#include "cpm8k_bdos.h"
#include "cpm8k_mem.h"
#include "cpm8k_drives.h"
#include "cpm8k_console.h"
#include "cpm8k_bios.h"
#include <cstdio>
#include <cstring>

// Current caller's segment (set at start of bdos_route from PC)
static uint8_t s_caller_seg = 0x0A;

// Backend of the most recent F_SFIRST, so F_SNEXT routes the same way.
static bool s_last_search_host = false;

// BDOS call trace flag
static bool s_bdos_trace = false;

// Parameter type for trace formatting
enum class BdosParam {
    NONE,   // no parameter
    BYTE,   // r7 low byte (character, drive number, user number)
    WORD,   // r7 word
    ADDR,   // rr6 segmented address (r6:r7)
    FCB,    // rr6 = FCB address, show filename
};

struct BdosFunc {
    const char* name;
    const char* description;
    BdosParam   param;
};

// Table indexed by BDOS function number (file/disk functions only —
// console/system functions are handled by the real native BDOS)
static const BdosFunc s_bdos_funcs[] = {
    [0]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [1]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [2]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [3]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [4]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [5]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [6]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [7]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [8]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [9]  = {nullptr,         nullptr,                        BdosParam::NONE},
    [10] = {nullptr,         nullptr,                        BdosParam::NONE},
    [11] = {nullptr,         nullptr,                        BdosParam::NONE},
    [12] = {nullptr,         nullptr,                        BdosParam::NONE},
    [13] = {"DRV_ALLRESET",  "Reset all disks",              BdosParam::NONE},
    [14] = {"DRV_SET",       "Select disk",                  BdosParam::BYTE},
    [15] = {"F_OPEN",        "Open file",                    BdosParam::FCB},
    [16] = {"F_CLOSE",       "Close file",                   BdosParam::FCB},
    [17] = {"F_SFIRST",      "Search for first",             BdosParam::FCB},
    [18] = {"F_SNEXT",       "Search for next",              BdosParam::NONE},
    [19] = {"F_DELETE",      "Delete file",                  BdosParam::FCB},
    [20] = {"F_READ",        "Read sequential",              BdosParam::FCB},
    [21] = {"F_WRITE",       "Write sequential",             BdosParam::FCB},
    [22] = {"F_MAKE",        "Create file",                  BdosParam::FCB},
    [23] = {"F_RENAME",      "Rename file",                  BdosParam::FCB},
    [24] = {"DRV_LOGINVEC",  "Return login vector",          BdosParam::NONE},
    [25] = {"DRV_GET",       "Return current disk",          BdosParam::NONE},
    [26] = {"F_DMAOFF",      "Set DMA address",              BdosParam::ADDR},
    [27] = {"DRV_ALLOCVEC",  "Get alloc vector addr",        BdosParam::NONE},
    [28] = {"DRV_SETRO",     "Write protect disk",           BdosParam::NONE},
    [29] = {"DRV_ROVEC",     "Get R/O vector",               BdosParam::NONE},
    [30] = {"F_ATTRIB",      "Set file attributes",          BdosParam::FCB},
    [31] = {"DRV_DPB",       "Get DPB address",              BdosParam::NONE},
    [32] = {nullptr,         nullptr,                        BdosParam::NONE},
    [33] = {"F_READRAND",    "Read random",                  BdosParam::FCB},
    [34] = {"F_WRITERAND",   "Write random",                 BdosParam::FCB},
    [35] = {"F_SIZE",        "Compute file size",            BdosParam::FCB},
    [36] = {"F_RANDREC",     "Set random record",            BdosParam::FCB},
    [37] = {"DRV_RESET",     "Reset specific drives",        BdosParam::WORD},
    [38] = {nullptr,         nullptr,                        BdosParam::NONE},
    [39] = {nullptr,         nullptr,                        BdosParam::NONE},
    [40] = {"F_WRITEZF",     "Write random zero fill",       BdosParam::FCB},
    [41] = {nullptr,         nullptr,                        BdosParam::NONE},
    [42] = {nullptr,         nullptr,                        BdosParam::NONE},
    [43] = {nullptr,         nullptr,                        BdosParam::NONE},
    [44] = {nullptr,         nullptr,                        BdosParam::NONE},
    [45] = {nullptr,         nullptr,                        BdosParam::NONE},
    [46] = {"DRV_FREESPACE", "Get disk free space",          BdosParam::BYTE},
    [47] = {nullptr,         nullptr,                        BdosParam::NONE},
    [48] = {"F_FLUSH",       "Flush buffers",                BdosParam::NONE},
    [49] = {nullptr,         nullptr,                        BdosParam::NONE},
    [50] = {nullptr,         nullptr,                        BdosParam::NONE},
    [51] = {nullptr,         nullptr,                        BdosParam::NONE},
    [52] = {nullptr,         nullptr,                        BdosParam::NONE},
    [53] = {nullptr,         nullptr,                        BdosParam::NONE},
    [54] = {nullptr,         nullptr,                        BdosParam::NONE},
    [55] = {nullptr,         nullptr,                        BdosParam::NONE},
    [56] = {nullptr,         nullptr,                        BdosParam::NONE},
    [57] = {nullptr,         nullptr,                        BdosParam::NONE},
    [58] = {nullptr,         nullptr,                        BdosParam::NONE},
    [59] = {"PGMLD",         "Load program",                 BdosParam::ADDR},
};
static constexpr int NUM_BDOS_FUNCS = sizeof(s_bdos_funcs) / sizeof(s_bdos_funcs[0]);

// Extern references
extern SegmentedMemory* g_mem;
extern bool g_chain_pending;   // set on P_CHAIN; consumed by the warm-boot loop

// --- BDOS Function 59: Program Load (PGMLD) ---
// Loads an x.out format program into the TPA.
// Parameter: RR6 = address of Load Parameter Block (LPB)
// LPB layout (big-endian, 22 bytes):
//   0x00: fcbaddr  (4) - segmented address of opened FCB
//   0x04: pgldaddr (4) - TPA base address (from MRT)
//   0x08: pgtop    (4) - TPA length (from MRT)
//   0x0C: bpaddr   (4) - RETURN: basepage address
//   0x10: stackptr (4) - RETURN: initial stack pointer
//   0x14: flags    (2) - RETURN: SPLIT=0x4000, SEG=0x2000
// Returns: 0=success, 1=bad header, 2=no memory, 3=read error

static int bdos_pgmld(SegmentedMemory& mem, CpmFileSystem& fs, uint16_t lpb_offset)
{
    uint32_t lpb_addr = (uint32_t(s_caller_seg) << 16) | lpb_offset;

    // Read LPB fields
    uint32_t fcbaddr  = (uint32_t(mem.read_word(lpb_addr + 0)) << 16) |
                         mem.read_word(lpb_addr + 2);
    uint32_t pgldaddr = (uint32_t(mem.read_word(lpb_addr + 4)) << 16) |
                         mem.read_word(lpb_addr + 6);
    uint32_t pgtop    = (uint32_t(mem.read_word(lpb_addr + 8)) << 16) |
                         mem.read_word(lpb_addr + 10);

    uint16_t fcb_offset = fcbaddr & 0xFFFF;
    uint8_t  tpa_seg    = (pgldaddr >> 16) & 0x7F;
    uint16_t tpa_base   = pgldaddr & 0xFFFF;
    (void)pgtop; // TPA limits from LPB are ignored (matching real BDOS)

    // Find the open file
    FILE* fp = fs.get_open_fp(fcb_offset);
    if (!fp) {
        fprintf(stderr, "PGMLD: FCB %04X not open\n", fcb_offset);
        return 1;
    }

    // Read entire file
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t* fbuf = new uint8_t[file_size];
    if (fread(fbuf, 1, file_size, fp) != (size_t)file_size) {
        delete[] fbuf;
        return 3; // READERR
    }

    // Parse x.out header (16 bytes, big-endian)
    if (file_size < 16) { delete[] fbuf; return 1; }
    const uint8_t* p = fbuf;
    uint16_t magic    = (uint16_t(p[0]) << 8) | p[1];
    int16_t  num_segs = (int16_t)((uint16_t(p[2]) << 8) | p[3]);
    // code_part_len at p+4..7, relocs/symbs at p+8..15 (not needed)

    bool split = false, segmented = false;
    switch (magic) {
    case 0xee02: case 0xee03: break;                       // non-seg combined I/D
    case 0xee0a: case 0xee0b: split = true; break;         // non-seg split I/D
    case 0xee00: case 0xee01: segmented = true; break;     // segmented
    default:
        fprintf(stderr, "PGMLD: bad magic 0x%04X\n", magic);
        delete[] fbuf;
        return 1;
    }

    // Parse segment headers (4 bytes each)
    struct XSeg { uint8_t number; uint8_t type; uint16_t length; };
    XSeg segs[16];
    if (num_segs > 16) num_segs = 16;
    p = fbuf + 16;
    for (int i = 0; i < num_segs; i++) {
        segs[i].number = p[0];
        segs[i].type   = p[1];
        segs[i].length = (uint16_t(p[2]) << 8) | p[3];
        p += 4;
    }

    // Code data starts after headers
    const uint8_t* code_data = fbuf + 16 + num_segs * 4;
    long code_offset = 0;

    const int BPLEN = 256;     // basepage size
    const int DEFSTACK = 256;  // default stack
    const uint32_t SEGLEN = 0x10000;

    // For split I/D:
    //   code_load_seg = 0x0A (d_map -> 0x10000 = code area, same phys as seg 8 i_map)
    //   data_load_seg = 0x08 (d_map -> 0x20000 = data area)
    //   exec_seg      = 0x08 (execution: i_map -> code @ 0x10000, d_map -> data @ 0x20000)
    // For combined I/D: everything goes to tpa_seg (usually 0x0A)
    uint8_t code_load_seg = tpa_seg;  // segment used to LOAD code (via data writes)
    uint8_t data_load_seg = tpa_seg;  // segment used to LOAD data (via data writes)
    uint8_t exec_seg      = tpa_seg;  // segment used to EXECUTE (goes in PC/basepage)
    if (split) {
        code_load_seg = 0x0A; // d_map -> 0x10000 = same phys as seg 8 i_map
        data_load_seg = 0x08; // d_map -> 0x20000 = split data area
        exec_seg      = 0x08; // i_map -> 0x10000 (code), d_map -> 0x20000 (data)
    }

    uint32_t code_load_offset = 0; // Current offset within code segment
    uint32_t data_load_offset = 0; // Current offset within data segment
    uint32_t text_loc = 0, text_size = 0;
    uint32_t data_loc = 0, data_size = 0;
    uint32_t bss_loc = 0, bss_size = 0;
    uint32_t stk_size = DEFSTACK;

    for (int i = 0; i < num_segs; i++) {
        uint16_t slen = segs[i].length;
        uint8_t  stype = segs[i].type;

        // Determine target segment: for split I/D, CONST/DATA/BSS/STACK go to data seg
        bool to_data = split && (stype == 4 || stype == 5 || stype == 1 || stype == 2);
        uint8_t  target_seg = to_data ? data_load_seg : code_load_seg;
        uint32_t& load_off  = to_data ? data_load_offset : code_load_offset;

        switch (stype) {
        case 3: // CODE
        case 6: // CDMIX
        case 7: // CDMIX_P
            if (text_size == 0) text_loc = load_off;
            text_size += slen;
            for (uint16_t j = 0; j < slen; j++) {
                mem.write_byte((uint32_t(target_seg) << 16) | (tpa_base + load_off + j),
                               code_data[code_offset + j]);
            }
            load_off += slen;
            code_offset += slen;
            break;

        case 4: // CONST
        case 5: // DATA
            if (data_size == 0) data_loc = load_off;
            data_size += slen;
            for (uint16_t j = 0; j < slen; j++) {
                mem.write_byte((uint32_t(target_seg) << 16) | (tpa_base + load_off + j),
                               code_data[code_offset + j]);
            }
            load_off += slen;
            code_offset += slen;
            break;

        case 1: // BSS - no data in file
            if (bss_size == 0) bss_loc = load_off;
            bss_size += slen;
            for (uint16_t j = 0; j < slen; j++) {
                mem.write_byte((uint32_t(target_seg) << 16) | (tpa_base + load_off + j), 0);
            }
            load_off += slen;
            break;

        case 2: // STACK - no data in file, adjusts stack size
            stk_size += slen;
            break;
        }
    }

    // Check size limits
    uint32_t data_seg_used = split ? data_load_offset : code_load_offset;
    uint32_t data_seg_limit = SEGLEN - BPLEN - stk_size;
    if (data_seg_used > data_seg_limit) {
        fprintf(stderr, "PGMLD: program too large (%u > %u)\n",
                (unsigned)data_seg_used, (unsigned)data_seg_limit);
        delete[] fbuf;
        return 2; // NOMEM
    }

    // Calculate basepage and stack locations (matching real BDOS pgmld.c)
    // stkloc = data_segment_base + SEGLEN - BPLEN - stksiz
    // Basepage and stack always go in the data segment
    uint32_t data_segment_base = (uint32_t(data_load_seg) << 16) | tpa_base;
    uint32_t stkloc = data_segment_base + SEGLEN - BPLEN - stk_size;

    uint32_t bp_addr = stkloc;
    uint32_t sp_addr = stkloc - (segmented ? 8 : 4);

    // Write basepage (256 bytes, big-endian)
    for (int i = 0; i < BPLEN; i++)
        mem.write_byte(bp_addr + i, 0);

    // Basepage fields (matching real BDOS setbase())
    // For split I/D: lcode/ltpa use exec_seg, ldata/lbss use data_load_seg
    uint32_t code_addr = (uint32_t(exec_seg) << 16) | (tpa_base + text_loc);
    uint32_t data_addr_val = (uint32_t(data_load_seg) << 16) | (tpa_base + data_loc);
    uint32_t bss_addr;
    if (bss_size > 0)
        bss_addr = (uint32_t(data_load_seg) << 16) | (tpa_base + bss_loc);
    else
        bss_addr = data_addr_val + data_size; // bssloc = dataloc + datasiz
    uint32_t free_len = (data_seg_used < data_seg_limit) ? (data_seg_limit - data_seg_used) : 0;

    // 0x00: ltpa (= lcode for non-segmented, per real BDOS)
    mem.write_word(bp_addr + 0, code_addr >> 16);
    mem.write_word(bp_addr + 2, code_addr & 0xFFFF);
    // 0x04: htpa (= stackptr)
    mem.write_word(bp_addr + 4, (sp_addr >> 16) & 0xFFFF);
    mem.write_word(bp_addr + 6, sp_addr & 0xFFFF);
    // 0x08: lcode
    mem.write_word(bp_addr + 8, code_addr >> 16);
    mem.write_word(bp_addr + 10, code_addr & 0xFFFF);
    // 0x0C: codelen
    mem.write_word(bp_addr + 12, text_size >> 16);
    mem.write_word(bp_addr + 14, text_size & 0xFFFF);
    // 0x10: ldata
    mem.write_word(bp_addr + 16, data_addr_val >> 16);
    mem.write_word(bp_addr + 18, data_addr_val & 0xFFFF);
    // 0x14: datalen
    mem.write_word(bp_addr + 20, data_size >> 16);
    mem.write_word(bp_addr + 22, data_size & 0xFFFF);
    // 0x18: lbss
    mem.write_word(bp_addr + 24, bss_addr >> 16);
    mem.write_word(bp_addr + 26, bss_addr & 0xFFFF);
    // 0x1C: bsslen
    mem.write_word(bp_addr + 28, bss_size >> 16);
    mem.write_word(bp_addr + 30, bss_size & 0xFFFF);
    // 0x20: freelen
    mem.write_word(bp_addr + 32, free_len >> 16);
    mem.write_word(bp_addr + 34, free_len & 0xFFFF);

    // Write LPB return values
    uint16_t flags = (split ? 0x4000 : 0) | (segmented ? 0x2000 : 0);

    // pgldaddr: actual starting PC address (code segment)
    mem.write_word(lpb_addr + 4, code_addr >> 16);
    mem.write_word(lpb_addr + 6, code_addr & 0xFFFF);
    // bpaddr
    mem.write_word(lpb_addr + 12, bp_addr >> 16);
    mem.write_word(lpb_addr + 14, bp_addr & 0xFFFF);
    // stackptr
    mem.write_word(lpb_addr + 16, sp_addr >> 16);
    mem.write_word(lpb_addr + 18, sp_addr & 0xFFFF);
    // flags
    mem.write_word(lpb_addr + 20, flags);

    delete[] fbuf;
    return 0; // GOOD
}

void bdos_set_trace(bool enable)
{
    s_bdos_trace = enable;
}

// Extract filename from an FCB at the given offset in the caller's segment
static void fcb_filename(SegmentedMemory& mem, uint16_t fcb_off, char* buf, size_t buflen)
{
    // FCB: byte 0 = drive, bytes 1-8 = name, bytes 9-11 = type
    uint8_t drv = mem.read_byte((uint32_t(s_caller_seg) << 16) | fcb_off);
    char name[9], type[4];
    for (int i = 0; i < 8; i++)
        name[i] = mem.read_byte((uint32_t(s_caller_seg) << 16) | (fcb_off + 1 + i)) & 0x7F;
    name[8] = '\0';
    for (int i = 0; i < 3; i++)
        type[i] = mem.read_byte((uint32_t(s_caller_seg) << 16) | (fcb_off + 9 + i)) & 0x7F;
    type[3] = '\0';
    // Trim trailing spaces
    for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
    for (int i = 2; i >= 0 && type[i] == ' '; i--) type[i] = '\0';
    char d = drv ? ('A' + drv - 1) : '?';
    snprintf(buf, buflen, "%c:%.8s.%.3s", d, name, type);
}

static void bdos_trace_call(uint16_t func, z8002_device& cpu, SegmentedMemory& mem)
{
    const BdosFunc* f = (func < NUM_BDOS_FUNCS) ? &s_bdos_funcs[func] : nullptr;
    const char* name = (f && f->name) ? f->name : "???";
    const char* desc = (f && f->description) ? f->description : "Unknown";
    BdosParam ptype = (f && f->name) ? f->param : BdosParam::WORD;

    fprintf(stderr, "BDOS %2d %-14s %-24s", func, name, desc);
    switch (ptype) {
    case BdosParam::NONE:
        break;
    case BdosParam::BYTE:
        fprintf(stderr, " %02X", cpu.get_reg(7) & 0xFF);
        if (func == 14) {
            fprintf(stderr, " (%c:)", 'A' + (cpu.get_reg(7) & 0x0F));
        }
        break;
    case BdosParam::WORD:
        fprintf(stderr, " %04X", cpu.get_reg(7));
        break;
    case BdosParam::ADDR:
        fprintf(stderr, " %02X:%04X", cpu.get_reg(6), cpu.get_reg(7));
        break;
    case BdosParam::FCB: {
        char fname[20];
        fcb_filename(mem, cpu.get_reg(7), fname, sizeof(fname));
        fprintf(stderr, " %02X:%04X %s", cpu.get_reg(6), cpu.get_reg(7), fname);
        break;
    }
    }
    fprintf(stderr, "\r\n");
}

// Resolve the target drive (0=A .. 15=P) of an FCB at `fcb_off` in segment
// `seg`. The FCB drive byte is 0 for the current/default drive, else 1=A..16=P.
static int fcb_drive(SegmentedMemory& mem, uint8_t seg, uint16_t fcb_off,
                     CpmFileSystem& fs)
{
    uint8_t raw = mem.read_byte((uint32_t(seg) << 16) | fcb_off);
    if (raw == '?')                     // '?' = current drive, match any user
        return fs.get_current_drive();
    uint8_t d = raw & 0x1F;
    return d ? (d - 1) : fs.get_current_drive();
}

// True if the n console-line bytes at `addr`, once trimmed of surrounding
// spaces/tabs and upper-cased, exactly equal `word`.
static bool console_line_is(const SegmentedMemory& mem, uint32_t addr,
                            uint8_t n, const char* word)
{
    uint8_t start = 0, end = n;
    while (start < end) {
        uint8_t c = mem.read_byte(addr + start);
        if (c != ' ' && c != '\t') break;
        start++;
    }
    while (end > start) {
        uint8_t c = mem.read_byte(addr + end - 1);
        if (c != ' ' && c != '\t') break;
        end--;
    }
    size_t len = strlen(word);
    if ((size_t)(end - start) != len) return false;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = mem.read_byte(addr + start + i);
        if (c >= 'a' && c <= 'z') c = (uint8_t)(c - 32);   // upper-case
        if (c != (uint8_t)word[i]) return false;
    }
    return true;
}

// BDOS func 10: C_READSTR -- read an edited console line into the caller's
// buffer (buf[0]=max length, buf[1]=count out, buf[2..]=chars). Serviced in
// C++ because the native BDOS gates its line read on console status, which
// reports not-ready for our host console, so it never reads a character.
static int bdos_read_console_line(z8002_device& cpu, SegmentedMemory& mem,
                                  uint8_t seg, uint16_t buf_off)
{
    uint32_t base = (uint32_t(seg) << 16) | buf_off;
    uint8_t maxlen = mem.read_byte(base + 0);
    if (maxlen == 0) maxlen = 1;

    uint8_t n = 0;
    for (;;) {
        uint8_t c = console_in();
        if (console_eof()) {        // pipe/EOF -> exit like the BIOS CONIN path
            cpu.request_halt();
            break;
        }
        if (c == 0x0D || c == 0x0A) {           // Enter -> end of line
            console_out(0x0D);
            console_out(0x0A);
            break;
        }
        if (c == 0x08 || c == 0x7F) {           // Backspace / Delete
            if (n > 0) {
                n--;
                console_out(0x08); console_out(' '); console_out(0x08);
            }
            continue;
        }
        if (c == 0x03) {                        // Ctrl-C -> abandon the line
            console_out('^'); console_out('C');
            console_out(0x0D); console_out(0x0A);
            n = 0;
            break;
        }
        if (n >= maxlen) continue;              // buffer full: ignore extra
        mem.write_byte(base + 2 + n, c);
        console_out(c);                         // echo (host raw mode has echo off)
        n++;
    }

    mem.write_byte(base + 1, n);

    // "EXIT" (or "QUIT") typed as the whole command line quits the emulator.
    // There is no EXIT.Z8K program; the emulator provides this at the line
    // read so it works on any drive and in interactive (raw-mode) sessions,
    // where Ctrl-D/Ctrl-C are just bytes and can't signal EOF. request_halt()
    // stops the run loop before the CCP acts on the line (same mechanism as
    // the EOF path above), so no "EXIT?" is printed.
    if (console_line_is(mem, base + 2, n, "EXIT") ||
        console_line_is(mem, base + 2, n, "QUIT"))
        cpu.request_halt();

    return 0;
}

bool bdos_route(z8002_device& cpu, CpmFileSystem& fs, uint8_t caller_seg)
{
    // Register conventions: r5 = function number, rr6 = parameter.
    uint16_t func = cpu.get_reg(5);
    uint16_t param_lo = cpu.get_reg(7); // Low word of rr6 (often an address)

    SegmentedMemory& mem = *g_mem;

    s_caller_seg = caller_seg;
    fs.set_caller_seg(caller_seg);

    if (s_bdos_trace)
        bdos_trace_call(func, cpu, mem);

    // Return a handled byte/word result in rr6 (high word zeroed).
    auto handled = [&](int r) -> bool {
        cpu.set_reg(6, 0);
        cpu.set_reg(7, (uint16_t)r);
        return true;
    };

    switch (func) {
    // --- Console line input: serviced here (native BDOS won't read) ---
    case 10: // C_READSTR -- read an edited command line
        return handled(bdos_read_console_line(cpu, mem, caller_seg, param_lo));

    // --- Drive / DMA state: keep CpmFileSystem in sync with the system ---
    case 13: // DRV_ALLRESET
        fs.reset_all_drives();
        return false; // native BDOS also resets the image drives
    case 37: // DRV_RESET (specific drives)
        fs.reset_drives(param_lo);
        return false;

    case 14: { // DRV_SET (select disk)
        uint8_t drv = param_lo & 0x0F;
        fs.set_current_drive(drv);
        // A HOST_DIR current drive must not reach the native BDOS, whose
        // BIOS SELDSK would reject it. Handle it here.
        if (drive_is_host(drv))
            return handled(0);
        return false; // IMAGE/none: let the native BDOS select it
    }

    case 25: // DRV_GET (current disk) -- unified across both backends
        return handled(fs.get_current_drive());

    case 24: // DRV_LOGINVEC -- report every configured drive (host + image)
        return handled(drive_login_vector());

    case 27:   // DRV_ALLOCVEC -- pointer to allocation vector
    case 31: { // DRV_DPB -- pointer to disk parameter block
        // For HOST_DIR drives the native BDOS would log the drive in with
        // sector I/O, which they don't have. Serve the synthetic structures
        // built by bios_init_disks instead (the ALV stays zeroed; free-space
        // numbers on a host dir are nominal anyway). IMAGE drives keep the
        // native BDOS's real, login-maintained structures.
        int drv = fs.get_current_drive();
        if (!drive_is_host(drv))
            return false;
        uint16_t off = (func == 27) ? bios_alv_offset(drv) : bios_dpb_offset(drv);
        cpu.set_reg(6, off ? 0x0B : 0);  // system segment
        cpu.set_reg(7, off);
        return true;
    }

    case 26: { // F_DMAOFF -- mirror to fs; native BDOS still needs its own
        uint16_t dma_seg = cpu.get_reg(6);
        fs.set_dma((uint32_t(dma_seg) << 16) | param_lo);
        return false;
    }

    // --- FCB file operations: route by the FCB's target drive ---
    case 15: case 16: case 17: case 19: case 20: case 21:
    case 22: case 23: case 30: case 33: case 34: case 35:
    case 36: case 40: {
        int drv = fcb_drive(mem, caller_seg, param_lo, fs);
        if (!drive_is_host(drv))
            return false; // IMAGE drive -> native BDOS -> BIOS sector I/O
        if (func == 17) s_last_search_host = true;
        switch (func) {
        case 15: return handled(fs.file_open(param_lo));
        case 16: return handled(fs.file_close(param_lo));
        case 17: return handled(fs.file_search_first(param_lo));
        case 19: return handled(fs.file_delete(param_lo));
        case 20: return handled(fs.file_read_seq(param_lo));
        case 21: return handled(fs.file_write_seq(param_lo));
        case 22: return handled(fs.file_make(param_lo));
        case 23: return handled(fs.file_rename(param_lo));
        case 30: return handled(0); // F_ATTRIB (stub)
        case 33: return handled(fs.file_read_rand(param_lo));
        case 34: return handled(fs.file_write_rand(param_lo));
        case 35: return handled(fs.file_size(param_lo));
        case 36: return handled(fs.file_set_random(param_lo));
        case 40: return handled(fs.file_write_rand_zf(param_lo));
        }
        return false; // unreachable
    }

    case 46: { // DRV_FREESPACE -- free record count into the current DMA
        uint8_t drv = param_lo & 0x0F;
        if (!drive_is_host(drv))
            return false;   // image: native BDOS computes from the real ALV
        // A host directory has no real CP/M allocation state; report the
        // synthetic disk's full capacity as a 32-bit big-endian record count
        // at the DMA, matching the native BDOS's result format. (This is a
        // stub -- a host directory isn't bounded by a CP/M allocation map.)
        uint32_t recs = bios_drive_total_recs(drv);
        uint32_t dma = fs.get_dma();
        for (int i = 0; i < 4; i++)
            mem.write_byte(dma + i, (recs >> (24 - 8 * i)) & 0xFF);
        return handled(0);
    }

    case 18: // F_SNEXT -- continue the most recent search on its backend
        if (s_last_search_host)
            return handled(fs.file_search_next());
        return false;

    case 59: { // PGMLD -- route by the program file's drive
        uint32_t lpb = (uint32_t(caller_seg) << 16) | param_lo;
        uint8_t  fcb_seg = mem.read_word(lpb + 0) & 0x7F;
        uint16_t fcb_off = mem.read_word(lpb + 2);
        int drv = fcb_drive(mem, fcb_seg, fcb_off, fs);
        if (!drive_is_host(drv))
            return false; // image program -> native BDOS PGMLD
        return handled(bdos_pgmld(mem, fs, param_lo));
    }

    case 47:  // P_CHAIN -- serviced by the native BDOS, but flag it so the
        // next warm boot preserves a split-I/D program's data mapping (the
        // pending chain command lives in its data segment). See main.cpp.
        g_chain_pending = true;
        return false;

    default:
        // Console / system calls and image-only services: native BDOS.
        return false;
    }
}
