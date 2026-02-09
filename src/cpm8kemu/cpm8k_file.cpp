#include "cpm8k_file.h"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

// Default segment for user-mode addresses (TPA merged I/D)
static constexpr uint8_t TPA_SEG = 0x0A;

CpmFileSystem::CpmFileSystem(SegmentedMemory& mem)
    : m_mem(mem), m_current_drive(0), m_user(0),
      m_dma_addr((uint32_t(TPA_SEG) << 16) | 0x0080),
      m_caller_seg(TPA_SEG), m_ro_vec(0)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++)
        m_files[i] = {nullptr, 0, 0, false, ""};
    m_search = {nullptr, "", 0, {}, false, 0};
    m_drive_paths[0] = "A";
    m_drive_paths[1] = "B";
    m_drive_paths[2] = "C";
    m_drive_paths[3] = "D";
}

CpmFileSystem::~CpmFileSystem()
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (m_files[i].active && m_files[i].fp)
            fclose(m_files[i].fp);
    }
    close_search();
}

void CpmFileSystem::set_drive_path(int drive, const std::string& path)
{
    if (drive >= 0 && drive < MAX_DRIVES)
        m_drive_paths[drive] = path;
}

const std::string& CpmFileSystem::get_drive_path(int drive) const
{
    static const std::string empty;
    if (drive >= 0 && drive < MAX_DRIVES)
        return m_drive_paths[drive];
    return empty;
}

// --- Memory helpers ---
// These use the caller's segment for FCB addresses.
// For DMA buffer access, use dma_read/dma_write which use the DMA segment.

uint8_t CpmFileSystem::mem_read(uint16_t addr)
{
    return m_mem.read_byte((uint32_t(m_caller_seg) << 16) | addr);
}

void CpmFileSystem::mem_write(uint16_t addr, uint8_t val)
{
    m_mem.write_byte((uint32_t(m_caller_seg) << 16) | addr, val);
}

void CpmFileSystem::mem_read_block(uint16_t addr, uint8_t* buf, int len)
{
    for (int i = 0; i < len; i++)
        buf[i] = mem_read(addr + i);
}

void CpmFileSystem::mem_write_block(uint16_t addr, const uint8_t* buf, int len)
{
    for (int i = 0; i < len; i++)
        mem_write(addr + i, buf[i]);
}

uint16_t CpmFileSystem::mem_read_word(uint16_t addr)
{
    return m_mem.read_word((uint32_t(m_caller_seg) << 16) | addr);
}

// DMA buffer helpers - use the segment embedded in m_dma_addr
uint8_t CpmFileSystem::dma_read(uint16_t offset)
{
    return m_mem.read_byte(m_dma_addr + offset);
}

void CpmFileSystem::dma_write(uint16_t offset, uint8_t val)
{
    m_mem.write_byte(m_dma_addr + offset, val);
}

void CpmFileSystem::dma_read_block(uint16_t offset, uint8_t* buf, int len)
{
    for (int i = 0; i < len; i++)
        buf[i] = m_mem.read_byte(m_dma_addr + offset + i);
}

void CpmFileSystem::dma_write_block(uint16_t offset, const uint8_t* buf, int len)
{
    for (int i = 0; i < len; i++)
        m_mem.write_byte(m_dma_addr + offset + i, buf[i]);
}

// --- Helper methods ---

int CpmFileSystem::resolve_drive(uint8_t fcb_drive)
{
    if (fcb_drive == 0) return m_current_drive;
    return fcb_drive - 1;
}

void CpmFileSystem::read_fcb_name(uint16_t fcb_addr, uint8_t name[11])
{
    for (int i = 0; i < 11; i++)
        name[i] = mem_read(fcb_addr + FCB_F1 + i) & 0x7F; // strip high bit
}

bool CpmFileSystem::match_pattern(const uint8_t pattern[11], const uint8_t name[11])
{
    // Check if extension is all spaces - treat as wildcard (match any extension).
    // The CCP searches with blank extension and manually checks directory entries
    // for .Z8K, .SUB, etc. (see cmd_file() search order in CCP source).
    bool ext_wildcard = (pattern[8] == ' ' && pattern[9] == ' ' && pattern[10] == ' ');

    for (int i = 0; i < 11; i++) {
        if (pattern[i] == '?') continue;
        if (ext_wildcard && i >= 8) continue; // blank ext = match any
        if (toupper(pattern[i]) != toupper(name[i])) return false;
    }
    return true;
}

std::string CpmFileSystem::fcb_to_host_path(uint16_t fcb_addr)
{
    uint8_t drive_code = mem_read(fcb_addr + FCB_DR);
    int drive = resolve_drive(drive_code);
    if (drive < 0 || drive >= MAX_DRIVES) drive = 0;

    uint8_t name[11];
    read_fcb_name(fcb_addr, name);

    // Build filename: trim trailing spaces from name and extension
    char fname[9], ext[4];
    int nlen = 8, elen = 3;
    while (nlen > 0 && name[nlen - 1] == ' ') nlen--;
    while (elen > 0 && name[8 + elen - 1] == ' ') elen--;

    for (int i = 0; i < nlen; i++) fname[i] = toupper(name[i]);
    fname[nlen] = '\0';
    for (int i = 0; i < elen; i++) ext[i] = toupper(name[8 + i]);
    ext[elen] = '\0';

    std::string path = m_drive_paths[drive] + "/";
    path += fname;
    if (elen > 0) {
        path += ".";
        path += ext;
    }
    return path;
}

void CpmFileSystem::filename_to_fcb(const char* host_name, uint8_t* entry)
{
    // Parse "NAME.EXT" into 11-byte FCB name field (space padded, uppercase)
    memset(entry, ' ', 11);
    int i = 0;
    const char* p = host_name;

    // Skip leading dot files
    if (*p == '.') return;

    // Name part (up to 8 chars)
    while (*p && *p != '.' && i < 8) {
        entry[i++] = toupper(*p++);
    }

    // Skip to extension
    if (*p == '.') p++;

    // Extension part (up to 3 chars)
    i = 8;
    while (*p && i < 11) {
        entry[i++] = toupper(*p++);
    }
}

int CpmFileSystem::find_open_slot()
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!m_files[i].active) return i;
    }
    return -1;
}

OpenFile* CpmFileSystem::find_file_by_fcb(uint16_t fcb_addr)
{
    // Read file handle from FCB allocation map byte AL[1] (offset 17).
    // We store (slot_index + 1) there during file_open/file_make, so
    // when the CCP copies an FCB, the handle travels with it.
    uint8_t handle = mem_read(fcb_addr + FCB_AL + 1);
    if (handle >= 1 && handle <= MAX_OPEN_FILES) {
        int slot = handle - 1;
        if (m_files[slot].active)
            return &m_files[slot];
    }
    return nullptr;
}

FILE* CpmFileSystem::get_open_fp(uint16_t fcb_addr)
{
    OpenFile* of = find_file_by_fcb(fcb_addr);
    return (of && of->active) ? of->fp : nullptr;
}

void CpmFileSystem::close_search()
{
    if (m_search.dirp) {
        closedir(m_search.dirp);
        m_search.dirp = nullptr;
    }
    m_search.active = false;
}

long CpmFileSystem::compute_file_offset(uint16_t fcb_addr)
{
    uint8_t ex = mem_read(fcb_addr + FCB_EX);
    uint8_t s2 = mem_read(fcb_addr + FCB_S2) & 0x3F; // Bits 6-7 are flags, not extent
    uint8_t cr = mem_read(fcb_addr + FCB_CR);
    int extent = (s2 << 5) | (ex & 0x1F);
    return (long)(extent * 128 + cr) * 128;
}

long CpmFileSystem::compute_random_offset(uint16_t fcb_addr)
{
    uint8_t r0 = mem_read(fcb_addr + FCB_R0);
    uint8_t r1 = mem_read(fcb_addr + FCB_R1);
    uint8_t r2 = mem_read(fcb_addr + FCB_R2);
    long record = r0 | (r1 << 8) | ((long)r2 << 16);
    return record * 128;
}

void CpmFileSystem::update_fcb_after_read(uint16_t fcb_addr, long offset)
{
    long record = offset / 128;
    int extent = record / 128;
    int cr = record % 128;
    mem_write(fcb_addr + FCB_EX, extent & 0x1F);
    mem_write(fcb_addr + FCB_S2, (extent >> 5) & 0x3F);
    mem_write(fcb_addr + FCB_CR, cr);
}

// --- File operations ---

int CpmFileSystem::file_open(uint16_t fcb_addr)
{
    std::string path = fcb_to_host_path(fcb_addr);

    // Check file exists
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return 0xFF; // File not found
    }

    int slot = find_open_slot();
    if (slot < 0) return 0xFF;

    FILE* fp = fopen(path.c_str(), "r+b");
    if (!fp) {
        // Try read-only
        fp = fopen(path.c_str(), "rb");
        if (!fp) return 0xFF;
    }

    m_files[slot].fp = fp;
    m_files[slot].fcb_addr = fcb_addr;
    m_files[slot].fcb_seg = m_caller_seg;
    m_files[slot].active = true;
    m_files[slot].host_path = path;

    // Initialize FCB fields
    mem_write(fcb_addr + FCB_EX, 0);
    mem_write(fcb_addr + FCB_S1, 0);
    mem_write(fcb_addr + FCB_S2, 0x80); // Write flag: file not written to (matches real BDOS)
    mem_write(fcb_addr + FCB_CR, 0);

    // Set record count based on file size
    long file_size = st.st_size;
    int records = (file_size + 127) / 128;
    int rc = records > 128 ? 128 : records;
    mem_write(fcb_addr + FCB_RC, rc);

    // Clear allocation map and store file handle
    for (int i = 0; i < 16; i++)
        mem_write(fcb_addr + FCB_AL + i, 0);
    mem_write(fcb_addr + FCB_AL, 1);            // AL[0]: non-zero = file exists
    mem_write(fcb_addr + FCB_AL + 1, slot + 1); // AL[1]: file handle (1-based)

    return 0; // Directory code 0 = success
}

int CpmFileSystem::file_close(uint16_t fcb_addr)
{
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (!of) return 0xFF;

    if (of->fp) {
        fflush(of->fp);
        fclose(of->fp);
        of->fp = nullptr;
    }
    of->active = false;
    // Clear handle in FCB so stale copies don't match
    mem_write(fcb_addr + FCB_AL + 1, 0);
    return 0;
}

int CpmFileSystem::file_search_first(uint16_t fcb_addr)
{
    close_search();

    uint8_t drive_code = mem_read(fcb_addr + FCB_DR);

    // Drive code '?' (0x3F) means "search current drive, match any user"
    int drive;
    bool match_any_user = false;
    if (drive_code == '?') {
        drive = m_current_drive;
        match_any_user = true;
    } else {
        drive = resolve_drive(drive_code);
    }
    if (drive < 0 || drive >= MAX_DRIVES) {
        fprintf(stderr, "F_SFIRST: bad drive %d\n", drive);
        return 0xFF;
    }

    m_search.dir_path = m_drive_paths[drive];
    m_search.dirp = opendir(m_search.dir_path.c_str());
    if (!m_search.dirp) {
        return 0xFF; // Drive directory doesn't exist
    }

    read_fcb_name(fcb_addr, m_search.search_name);
    m_search.search_drive = drive;
    m_search.active = true;
    m_search.entry_index = 0;

    return file_search_next();
}

int CpmFileSystem::file_search_next()
{
    if (!m_search.active || !m_search.dirp) return 0xFF;

    struct dirent* de;
    while ((de = readdir(m_search.dirp)) != nullptr) {
        // Skip directories, hidden files
        if (de->d_name[0] == '.') continue;

        // Skip directories
        std::string full = m_search.dir_path + "/" + de->d_name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            continue;

        // Convert host name to FCB format and check match
        uint8_t fcb_name[11];
        filename_to_fcb(de->d_name, fcb_name);
        if (!match_pattern(m_search.search_name, fcb_name))
            continue;

        // Build directory entry in DMA buffer
        // Entry format: user(1) + name(8) + ext(3) + extent(1) + s1(1) + s2(1) + rc(1) + alloc(16) = 32 bytes
        // Which of the 4 entries in the DMA buffer to use
        int dir_index = m_search.entry_index % 4;
        uint16_t entry_off = dir_index * DIR_ENTRY_SIZE;

        // Clear entry
        for (int i = 0; i < DIR_ENTRY_SIZE; i++)
            dma_write(entry_off + i, 0);

        // User number
        dma_write(entry_off, m_user);
        // Filename + extension
        for (int i = 0; i < 11; i++)
            dma_write(entry_off + 1 + i, fcb_name[i]);

        // Record count
        int records = (st.st_size + 127) / 128;
        dma_write(entry_off + 15, records > 128 ? 128 : records);

        m_search.entry_index++;
        return dir_index; // Return index (0-3) of entry in DMA buffer
    }

    close_search();
    return 0xFF; // No more matches
}

int CpmFileSystem::file_delete(uint16_t fcb_addr)
{
    std::string path = fcb_to_host_path(fcb_addr);

    // Close if open
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (of) {
        if (of->fp) fclose(of->fp);
        of->active = false;
    }

    if (unlink(path.c_str()) == 0)
        return 0;
    return 0xFF;
}

int CpmFileSystem::file_read_seq(uint16_t fcb_addr)
{
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (!of || !of->fp) return 0xFF;

    long offset = compute_file_offset(fcb_addr);
    fseek(of->fp, offset, SEEK_SET);

    uint8_t buf[128];
    memset(buf, 0x1A, 128); // Fill with Ctrl-Z (EOF marker)
    size_t n = fread(buf, 1, 128, of->fp);
    if (n == 0) return 1; // EOF

    dma_write_block(0, buf, 128);

    // Advance sequential position
    offset += 128;
    update_fcb_after_read(fcb_addr, offset);

    return 0;
}

int CpmFileSystem::file_write_seq(uint16_t fcb_addr)
{
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (!of || !of->fp) return 0xFF;

    long offset = compute_file_offset(fcb_addr);
    fseek(of->fp, offset, SEEK_SET);

    uint8_t buf[128];
    dma_read_block(0, buf, 128);

    size_t n = fwrite(buf, 1, 128, of->fp);
    if (n != 128) return 1; // Write error

    // Advance sequential position
    offset += 128;
    update_fcb_after_read(fcb_addr, offset);

    return 0;
}

int CpmFileSystem::file_make(uint16_t fcb_addr)
{
    std::string path = fcb_to_host_path(fcb_addr);

    int slot = find_open_slot();
    if (slot < 0) return 0xFF;

    FILE* fp = fopen(path.c_str(), "w+b");
    if (!fp) return 0xFF;

    m_files[slot].fp = fp;
    m_files[slot].fcb_addr = fcb_addr;
    m_files[slot].fcb_seg = m_caller_seg;
    m_files[slot].active = true;
    m_files[slot].host_path = path;

    // Initialize FCB fields
    mem_write(fcb_addr + FCB_EX, 0);
    mem_write(fcb_addr + FCB_S1, 0);
    mem_write(fcb_addr + FCB_S2, 0);
    mem_write(fcb_addr + FCB_RC, 0);
    mem_write(fcb_addr + FCB_CR, 0);
    for (int i = 0; i < 16; i++)
        mem_write(fcb_addr + FCB_AL + i, 0);
    mem_write(fcb_addr + FCB_AL, 1);            // AL[0]: non-zero = file exists
    mem_write(fcb_addr + FCB_AL + 1, slot + 1); // AL[1]: file handle (1-based)

    return 0;
}

int CpmFileSystem::file_rename(uint16_t fcb_addr)
{
    // Old name at FCB+0, new name at FCB+16
    std::string old_path = fcb_to_host_path(fcb_addr);

    // Build new name from FCB+16
    uint8_t new_name[11];
    for (int i = 0; i < 11; i++)
        new_name[i] = mem_read(fcb_addr + 16 + 1 + i) & 0x7F;

    uint8_t drive_code = mem_read(fcb_addr + FCB_DR);
    int drive = resolve_drive(drive_code);
    if (drive < 0 || drive >= MAX_DRIVES) drive = 0;

    char fname[9], ext[4];
    int nlen = 8, elen = 3;
    while (nlen > 0 && new_name[nlen - 1] == ' ') nlen--;
    while (elen > 0 && new_name[8 + elen - 1] == ' ') elen--;
    for (int i = 0; i < nlen; i++) fname[i] = toupper(new_name[i]);
    fname[nlen] = '\0';
    for (int i = 0; i < elen; i++) ext[i] = toupper(new_name[8 + i]);
    ext[elen] = '\0';

    std::string new_path = m_drive_paths[drive] + "/" + fname;
    if (elen > 0) new_path += std::string(".") + ext;

    if (rename(old_path.c_str(), new_path.c_str()) == 0)
        return 0;
    return 0xFF;
}

int CpmFileSystem::file_read_rand(uint16_t fcb_addr)
{
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (!of || !of->fp) return 0xFF;

    long offset = compute_random_offset(fcb_addr);
    fseek(of->fp, offset, SEEK_SET);

    uint8_t buf[128];
    memset(buf, 0x1A, 128);
    size_t n = fread(buf, 1, 128, of->fp);
    if (n == 0) return 1; // EOF / record not found

    dma_write_block(0, buf, 128);

    // Update sequential position to match
    update_fcb_after_read(fcb_addr, offset + 128);

    return 0;
}

int CpmFileSystem::file_write_rand(uint16_t fcb_addr)
{
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (!of || !of->fp) return 0xFF;

    long offset = compute_random_offset(fcb_addr);
    fseek(of->fp, offset, SEEK_SET);

    uint8_t buf[128];
    dma_read_block(0, buf, 128);

    size_t n = fwrite(buf, 1, 128, of->fp);
    if (n != 128) return 1;

    update_fcb_after_read(fcb_addr, offset + 128);
    return 0;
}

int CpmFileSystem::file_size(uint16_t fcb_addr)
{
    std::string path = fcb_to_host_path(fcb_addr);
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0xFF;

    long records = (st.st_size + 127) / 128;
    mem_write(fcb_addr + FCB_R0, records & 0xFF);
    mem_write(fcb_addr + FCB_R1, (records >> 8) & 0xFF);
    mem_write(fcb_addr + FCB_R2, (records >> 16) & 0xFF);
    return 0;
}

int CpmFileSystem::file_set_random(uint16_t fcb_addr)
{
    long offset = compute_file_offset(fcb_addr);
    long record = offset / 128;
    mem_write(fcb_addr + FCB_R0, record & 0xFF);
    mem_write(fcb_addr + FCB_R1, (record >> 8) & 0xFF);
    mem_write(fcb_addr + FCB_R2, (record >> 16) & 0xFF);
    return 0;
}

int CpmFileSystem::file_write_rand_zf(uint16_t fcb_addr)
{
    // Same as random write but zero-fills gaps
    OpenFile* of = find_file_by_fcb(fcb_addr);
    if (!of || !of->fp) return 0xFF;

    long offset = compute_random_offset(fcb_addr);

    // Check current file size, zero-fill if needed
    fseek(of->fp, 0, SEEK_END);
    long cur_size = ftell(of->fp);
    if (offset > cur_size) {
        uint8_t zeros[128];
        memset(zeros, 0, 128);
        fseek(of->fp, cur_size, SEEK_SET);
        while (cur_size < offset) {
            size_t chunk = (offset - cur_size > 128) ? 128 : (offset - cur_size);
            fwrite(zeros, 1, chunk, of->fp);
            cur_size += chunk;
        }
    }

    fseek(of->fp, offset, SEEK_SET);
    uint8_t buf[128];
    dma_read_block(0, buf, 128);
    fwrite(buf, 1, 128, of->fp);

    update_fcb_after_read(fcb_addr, offset + 128);
    return 0;
}

uint16_t CpmFileSystem::get_login_vector()
{
    uint16_t vec = 0;
    for (int i = 0; i < MAX_DRIVES; i++) {
        struct stat st;
        if (stat(m_drive_paths[i].c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            vec |= (1 << i);
    }
    return vec;
}

uint16_t CpmFileSystem::get_rovec()
{
    return m_ro_vec;
}

void CpmFileSystem::set_drive_ro(int drive)
{
    if (drive >= 0 && drive < 16)
        m_ro_vec |= (1 << drive);
}

void CpmFileSystem::reset_all_drives()
{
    m_current_drive = 0;
    m_ro_vec = 0; // Clear read-only flags (matching real BDOS)
    // Real BDOS does NOT reset DMA address on func 13
}

void CpmFileSystem::reset_drives(uint16_t /*mask*/)
{
    // Nothing to reset for host directories
}
