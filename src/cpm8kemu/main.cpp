// CP/M-8000 Emulator for macOS
// Runs CP/M-8000 CCP natively on an emulated Z8001.
// Host provides BIOS (SC #3) and BDOS (SC #2) services via I/O port bridging.
// SC instructions trigger the Z8001 native trap mechanism, dispatching to
// BIOS assembly handlers which bridge to C++ via OUT instructions.

#include "z8000.h"
#include "cpm8k_mem.h"
#include "cpm8k_bdos.h"
#include "cpm8k_bios.h"
#include "cpm8k_console.h"
#include "cpm8k_file.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <sys/stat.h>

// Segments (M20-like)
static constexpr uint8_t SEG_PSA  = 0x02; // PSA
static constexpr uint8_t SEG_TPA_SPLIT = 0x08; // TPA separated I/D
static constexpr uint8_t SEG_TPA  = 0x0A; // TPA merged I/D
static constexpr uint8_t SEG_SYS  = 0x0B; // System: CCP + BIOS data

// Physical offsets
static constexpr uint32_t PHYS_TPA = 0x10000;
static constexpr uint32_t PHYS_SYS = 0x30000;

// System stack top (within system segment)
static constexpr uint16_t SYS_STACK_TOP = 0xBFFE;

// PSA offset within its segment (aligned to 256 bytes)
static constexpr uint16_t PSA_OFFSET = 0x0100;

// I/O ports for C++ handler bridging (must match biosdef.s)
static constexpr uint16_t PORT_BDOS   = 0xF0;
static constexpr uint16_t PORT_BIOS   = 0xF2;
static constexpr uint16_t PORT_MAP    = 0xF4;
static constexpr uint16_t PORT_MEMCPY = 0xF6;

// Global pointers for cross-module access
SegmentedMemory* g_mem = nullptr;
uint16_t g_mrt_offset = 0;

// Warm boot flag (extern'd by cpm8k_bdos.cpp)
bool g_warm_boot = false;


// BIOS entry point (set after loading cpm.sys)
static uint16_t g_entry_point = 0;   // Cold boot: C runtime startup (clears BSS)
static uint16_t g_warm_entry  = 0;   // Warm boot: CCP function (preserves BSS)
static uint16_t g_ccp_size = 0;

// --- COFF loading ---

struct CoffHeader {
    uint16_t magic;
    uint16_t num_sections;
    uint32_t timestamp;
    uint32_t symtab_offset;
    uint32_t num_symbols;
    uint16_t opt_hdr_size;
    uint16_t flags;
};

struct CoffOptHeader {
    uint16_t magic;
    uint16_t version;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t entry_point;
    uint32_t text_start;
    uint32_t data_start;
};

struct CoffSection {
    char name[8];
    uint32_t phys_addr;
    uint32_t virt_addr;
    uint32_t size;
    uint32_t data_offset;
    uint32_t reloc_offset;
    uint32_t lineno_offset;
    uint16_t num_relocs;
    uint16_t num_linenos;
    uint32_t flags;
};

static uint16_t read_be16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
}

static uint32_t read_be32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
           (uint32_t(p[2]) << 8)  | p[3];
}

// Find a named symbol in the COFF symbol table.
// Returns the symbol's value (segmented address), or 0 if not found.
static uint32_t find_coff_symbol(const uint8_t* buf, long file_size,
                                  const CoffHeader& hdr, const char* name)
{
    if (hdr.symtab_offset == 0 || hdr.num_symbols == 0) return 0;

    size_t name_len = strlen(name);
    uint32_t strtab_off = hdr.symtab_offset + hdr.num_symbols * 18;

    uint32_t i = 0;
    while (i < hdr.num_symbols) {
        uint32_t off = hdr.symtab_offset + i * 18;
        if (off + 18 > (uint32_t)file_size) break;

        const uint8_t* entry = buf + off;

        bool match = false;
        uint32_t zeroes = read_be32(entry);
        if (zeroes == 0) {
            // String table reference
            uint32_t str_off = read_be32(entry + 4);
            uint32_t abs_off = strtab_off + str_off;
            if (abs_off + name_len < (uint32_t)file_size) {
                match = (memcmp(buf + abs_off, name, name_len) == 0 &&
                         buf[abs_off + name_len] == '\0');
            }
        } else {
            // Inline name (up to 8 chars, null-padded)
            if (name_len <= 8) {
                match = (memcmp(entry, name, name_len) == 0 &&
                         (name_len == 8 || entry[name_len] == '\0'));
            }
        }

        if (match)
            return read_be32(entry + 8); // value field

        uint8_t naux = entry[17];
        i += 1 + naux;
    }

    return 0;
}

// Load a COFF binary into memory at the system segment.
// Returns the entry point offset, or -1 on error.
static int load_coff(SegmentedMemory& mem, const char* path, uint32_t phys_base)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", path);
        return -1;
    }

    // Read entire file
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t* buf = new uint8_t[file_size];
    if (fread(buf, 1, file_size, fp) != (size_t)file_size) {
        fprintf(stderr, "Error reading %s\n", path);
        delete[] buf;
        fclose(fp);
        return -1;
    }
    fclose(fp);

    // Parse COFF header
    if (file_size < 20) {
        fprintf(stderr, "%s: too small for COFF\n", path);
        delete[] buf;
        return -1;
    }

    CoffHeader hdr;
    const uint8_t* p = buf;
    hdr.magic = read_be16(p); p += 2;
    hdr.num_sections = read_be16(p); p += 2;
    hdr.timestamp = read_be32(p); p += 4;
    hdr.symtab_offset = read_be32(p); p += 4;
    hdr.num_symbols = read_be32(p); p += 4;
    hdr.opt_hdr_size = read_be16(p); p += 2;
    hdr.flags = read_be16(p); p += 2;

    // Z8K COFF magic: 0x8000 (z8k-coff)
    if (hdr.magic != 0x8000) {
        fprintf(stderr, "%s: not a Z8K COFF file (magic=0x%04X)\n", path, hdr.magic);
        delete[] buf;
        return -1;
    }

    uint32_t entry_point = 0;

    // Parse optional header if present
    if (hdr.opt_hdr_size >= 28) {
        const uint8_t* op = p;
        CoffOptHeader opt;
        opt.magic = read_be16(op); op += 2;
        opt.version = read_be16(op); op += 2;
        opt.text_size = read_be32(op); op += 4;
        opt.data_size = read_be32(op); op += 4;
        opt.bss_size = read_be32(op); op += 4;
        opt.entry_point = read_be32(op); op += 4;
        opt.text_start = read_be32(op); op += 4;
        opt.data_start = read_be32(op); op += 4;
        entry_point = opt.entry_point;
    }
    p += hdr.opt_hdr_size;

    // Parse sections and load them
    uint32_t highest_addr = 0;
    for (int i = 0; i < hdr.num_sections; i++) {
        CoffSection sec;
        memcpy(sec.name, p, 8); p += 8;
        sec.phys_addr = read_be32(p); p += 4;
        sec.virt_addr = read_be32(p); p += 4;
        sec.size = read_be32(p); p += 4;
        sec.data_offset = read_be32(p); p += 4;
        sec.reloc_offset = read_be32(p); p += 4;
        sec.lineno_offset = read_be32(p); p += 4;
        sec.num_relocs = read_be16(p); p += 2;
        sec.num_linenos = read_be16(p); p += 2;
        sec.flags = read_be32(p); p += 4;

        // The virt_addr in COFF is the segmented address (seg << 16 | offset)
        // We extract just the offset for loading
        uint16_t offset = sec.virt_addr & 0xFFFF;

        if (sec.size > 0 && sec.data_offset > 0 && sec.data_offset + sec.size <= (uint32_t)file_size) {
            uint32_t dest = phys_base + offset;
            if (dest + sec.size <= MEM_SIZE) {
                memcpy(mem.data() + dest, buf + sec.data_offset, sec.size);
                fprintf(stderr, "  Loaded section %.8s: offset=0x%04X size=0x%X\n",
                        sec.name, offset, sec.size);
            }
        } else if (sec.size > 0 && sec.data_offset == 0) {
            // BSS section - zero fill
            uint32_t dest = phys_base + offset;
            if (dest + sec.size <= MEM_SIZE) {
                memset(mem.data() + dest, 0, sec.size);
                fprintf(stderr, "  BSS section %.8s: offset=0x%04X size=0x%X\n",
                        sec.name, offset, sec.size);
            }
        }

        uint32_t end = offset + sec.size;
        if (end > highest_addr) highest_addr = end;
    }

    // Look for CCP warm boot entry point in symbol table.
    // On warm boot, we jump directly to ccp() to skip the C runtime
    // BSS clearing — CCP state (submit flags, subfcb) must survive.
    uint32_t ccp_sym = find_coff_symbol(buf, file_size, hdr, "ccp");
    if (ccp_sym) {
        g_warm_entry = ccp_sym & 0xFFFF;
        fprintf(stderr, "  Warm boot entry: ccp at 0x%04X\n", g_warm_entry);
    }

    delete[] buf;

    // entry_point is a segmented address - extract offset
    g_entry_point = entry_point & 0xFFFF;
    if (g_warm_entry == 0)
        g_warm_entry = g_entry_point; // Fallback if symbol not found
    g_ccp_size = highest_addr;

    fprintf(stderr, "Loaded %s: entry=0x%04X, size=0x%X\n", path, g_entry_point, highest_addr);
    return g_entry_point;
}

// --- Memory Region Table ---
// Build the MRT that the CCP/BDOS expects to find via BIOS GMRTA call.
// Format (from System Guide Section 4, Function 18):
//   uint16_t  entry_count = 4
//   uint32_t  region1_base   (merged I/D segment)
//   uint32_t  region1_length
//   uint32_t  region2_base   (split I/D: program segment for PC)
//   uint32_t  region2_length
//   uint32_t  region3_base   (split I/D: data segment)
//   uint32_t  region3_length
//   uint32_t  region4_base   (data-space access to region 2)
//   uint32_t  region4_length
// All addresses are segmented (segment << 16 | offset).
// Total: 2 + 4*8 = 34 bytes.

static void write_be16(uint8_t* p, uint16_t v)
{
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

static void write_be32(uint8_t* p, uint32_t v)
{
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static uint16_t build_mrt(SegmentedMemory& mem, uint16_t offset)
{
    uint8_t* base = mem.data() + PHYS_SYS + offset;
    memset(base, 0, 34);

    // Entry count = 4
    write_be16(base, 4);
    base += 2;

    // Region 1: Merged I/D segment (SEG_TPA = 0x0A)
    // Programs with merged code+data run here
    write_be32(base, (uint32_t(SEG_TPA) << 16) | 0x0000);
    write_be32(base + 4, 0x0000FFFE);
    base += 8;

    // Region 2: Split I/D program segment (SEG_TPA_SPLIT = 0x08)
    // Code segment for programs with separate I/D spaces (goes in PC)
    write_be32(base, (uint32_t(SEG_TPA_SPLIT) << 16) | 0x0000);
    write_be32(base + 4, 0x0000FFFE);
    base += 8;

    // Region 3: Split I/D data segment (SEG_TPA_SPLIT = 0x08)
    // Data segment for programs with separate I/D spaces
    // Seg 0x08 d_map → 0x20000 = split data area
    write_be32(base, (uint32_t(SEG_TPA_SPLIT) << 16) | 0x0000);
    write_be32(base + 4, 0x0000FFFE);
    base += 8;

    // Region 4: Data-space access to region 2
    // Allows loading code into the instruction segment via data accesses
    write_be32(base, (uint32_t(SEG_TPA) << 16) | 0x0000);
    write_be32(base + 4, 0x0000FFFE);

    return offset;
}

// --- Basepage setup ---
// The basepage is at offset 0 of the TPA segment (0x0A).
// Structure (from startup.8kn):
//   0x00: reserved (2 bytes)
//   0x02: __exit return address
//   ...
//   0x18: lbss - pointer to BSS start
//   0x1C: bsslen - BSS length
//   0x80: command tail (128 bytes): [length][data...]

// --- Emulator I/O bus ---
// Assembly trap handlers bridge to C++ via OUT instructions to these ports.
// The OUT executes synchronously during the trap handler, so we can
// read/write CPU registers directly.

class EmuIO : public z8000_io_bus {
    z8002_device& m_cpu;
    CpmFileSystem& m_fs;
    SegmentedMemory& m_mem;
    bool& m_warm_boot;

public:
    EmuIO(z8002_device& cpu, CpmFileSystem& fs, SegmentedMemory& mem, bool& warm_boot)
        : m_cpu(cpu), m_fs(fs), m_mem(mem), m_warm_boot(warm_boot) {}

    uint8_t read_byte(uint16_t, int) override { return 0xFF; }
    uint16_t read_word(uint16_t, int) override { return 0xFFFF; }
    void write_byte(uint16_t, uint8_t, int) override {}

    void write_word(uint16_t port, uint16_t, int) override {
        switch (port) {
        case PORT_BDOS:   handle_bdos(); break;
        case PORT_BIOS:   handle_bios(); break;
        case PORT_MAP:    handle_map_adr(); break;
        case PORT_MEMCPY: handle_mem_cpy(); break;
        }
    }

private:
    // BDOS handler: r4=caller_seg_word, r5=func, r6=param_seg, r7=param_off
    // scseg is a Z8001 segmented PC word: (seg|0x80)<<8 in high byte
    void handle_bdos() {
        uint8_t caller_seg = (m_cpu.get_reg(4) >> 8) & 0x7F;
        bdos_handler(m_cpu, m_fs, caller_seg);
    }

    // BIOS handler: r2=caller_seg_word, r3=func, rr4=P1, rr6=P2
    void handle_bios() {
        uint8_t caller_seg = (m_cpu.get_reg(2) >> 8) & 0x7F;
        bios_handler(m_cpu, m_mem, m_warm_boot, caller_seg);
    }

    // map_adr: r4=caller_seg_word, r5=space, rr6=addr
    // Returns mapped segment in r6 (plain segment number; BIOS asm handles format conversion)
    void handle_map_adr() {
        uint16_t r4 = m_cpu.get_reg(4);
        uint16_t r5 = m_cpu.get_reg(5);
        uint16_t r6 = m_cpu.get_reg(6);

        uint8_t caller_seg = (r4 >> 8) & 0x7F;
        uint8_t space = r5 & 0xFF;
        bool true_prog = (r5 >> 8) != 0;

        uint8_t seg;
        switch (space) {
        case 0: // caller data
        case 1: // caller program
            seg = caller_seg;
            break;
        case 2: // system data
        case 3: // system program
            seg = SEG_SYS;
            break;
        case 4: // TPA data
            seg = SEG_TPA;
            break;
        case 5: // TPA program (I-space)
            if (true_prog) {
                seg = r6 & 0x7F;
            } else {
                seg = SEG_TPA;
            }
            break;
        default:
            seg = SEG_TPA;
            break;
        }

        // When dispatching a program (space=5, true_prog), reconfigure
        // normal-mode segment 0 to match the program's I/D layout.
        // Non-segmented programs use segment 0 for all accesses.
        if (space == 5 && true_prog) {
            if (seg == SEG_TPA_SPLIT) {
                // Split I/D: I-space = code (0x10000), D-space = data (0x20000)
                m_mem.set_segment(0x00, 0x10000, 0x20000);
            } else {
                // Merged I/D: both I and D at TPA (0x10000)
                m_mem.set_segment_unified(0x00, 0x10000);
            }
        }

        m_cpu.set_reg(6, seg);
        // r7 (offset) stays the same
    }

    // mem_cpy: rr2=length, rr4=dest, rr6=source
    // Returns rr6 = dest + length
    void handle_mem_cpy() {
        uint16_t r2 = m_cpu.get_reg(2);
        uint16_t r3 = m_cpu.get_reg(3);
        uint16_t r4 = m_cpu.get_reg(4);
        uint16_t r5 = m_cpu.get_reg(5);
        uint16_t r6 = m_cpu.get_reg(6);
        uint16_t r7 = m_cpu.get_reg(7);

        uint32_t length = (uint32_t(r2) << 16) | r3;
        uint32_t src = (uint32_t(r6) << 16) | r7;
        uint32_t dst = (uint32_t(r4) << 16) | r5;

        for (uint32_t i = 0; i < length && i < 0x10000; i++) {
            uint32_t src_phys = m_mem.translate(src + i, 0);
            uint32_t dst_phys = m_mem.translate(dst + i, 0);
            m_mem.data()[dst_phys] = m_mem.data()[src_phys];
        }

        uint32_t result = dst + length;
        m_cpu.set_reg(6, (result >> 16) & 0xFFFF);
        m_cpu.set_reg(7, result & 0xFFFF);
    }
};

// Global filesystem pointer for bdos_handler
CpmFileSystem* g_fs = nullptr;

static void usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [options] [cpm.sys]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -b         Enable BDOS call trace\n");
    fprintf(stderr, "  -t         Enable CPU instruction trace\n");
    fprintf(stderr, "  -r         Enable register trace\n");
    fprintf(stderr, "  -m         Enable memory bus trace\n");
    fprintf(stderr, "  -A dir     Host directory for drive A (default: drives/A)\n");
    fprintf(stderr, "  -B dir     Host directory for drive B (default: drives/B)\n");
    fprintf(stderr, "  -C dir     Host directory for drive C (default: drives/C)\n");
    fprintf(stderr, "  -D dir     Host directory for drive D (default: drives/D)\n");
    fprintf(stderr, "  -h         Show this help\n");
}

int main(int argc, char* argv[])
{
    const char* sys_file = nullptr;
    bool bdos_trace = false;
    bool trace = false;
    bool reg_trace = false;
    bool mem_trace = false;
    std::string drive_paths[4] = {"drives/A", "drives/B", "drives/C", "drives/D"};

    int opt;
    while ((opt = getopt(argc, argv, "btrmA:B:C:D:h")) != -1) {
        switch (opt) {
        case 'b': bdos_trace = true; break;
        case 't': trace = true; break;
        case 'r': reg_trace = true; break;
        case 'm': mem_trace = true; break;
        case 'A': drive_paths[0] = optarg; break;
        case 'B': drive_paths[1] = optarg; break;
        case 'C': drive_paths[2] = optarg; break;
        case 'D': drive_paths[3] = optarg; break;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }

    if (optind < argc) {
        sys_file = argv[optind];
    }

    // If no cpm.sys specified, look for it in default locations
    if (!sys_file) {
        static const char* defaults[] = {"cpm.sys", "build/bios-emu/cpm.sys", "bios/emu/cpm.sys", nullptr};
        for (const char** p = defaults; *p; p++) {
            struct stat st;
            if (stat(*p, &st) == 0) {
                sys_file = *p;
                break;
            }
        }
    }

    if (!sys_file) {
        fprintf(stderr, "Error: no cpm.sys found. Build it with 'make bios-emu' or specify path.\n");
        return 1;
    }

    // Validate drive directories - create if missing, error if path is invalid
    for (int i = 0; i < 4; i++) {
        struct stat st;
        if (stat(drive_paths[i].c_str(), &st) != 0) {
            if (mkdir(drive_paths[i].c_str(), 0755) != 0) {
                fprintf(stderr, "Error: cannot create drive %c: directory '%s': %s\n",
                        'A' + i, drive_paths[i].c_str(), strerror(errno));
                return 1;
            }
        } else if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Error: drive %c: path '%s' is not a directory\n",
                    'A' + i, drive_paths[i].c_str());
            return 1;
        }
    }

    // --- Create system components ---
    SegmentedMemory mem;
    g_mem = &mem;

    // Configure MMU segment table (separate I-space and D-space per segment)
    //   Seg   I-space   D-space   Purpose
    //   0x00  0x10000   0x10000   Non-seg fallback (= TPA for normal-mode programs)
    //   0x02  0x00000   0x00000   PSA
    //   0x08  0x10000   0x20000   Split I/D execution (I != D)
    //   0x0A  0x10000   0x10000   TPA merged / data-access to split code
    //   0x0B  0x30000   0x30000   System (CCP)
    // System-mode segment 0 maps to system area (0x30000) so BIOS handlers
    // running in non-segmented system mode access system data correctly.
    mem.set_segment_unified(0x00, 0x10000);
    mem.set_sys_segment_unified(0x00, 0x30000);
    mem.set_segment_unified(0x02, 0x00000);
    mem.set_segment(0x08, 0x10000, 0x20000);
    mem.set_segment_unified(0x0A, 0x10000);
    mem.set_sys_segment_unified(0x0A, 0x30000); // CCP code also lives in seg 0x0A
    mem.set_segment_unified(0x0B, 0x30000);

    // Create separate bus adapters for instruction fetch and data/stack
    SegBus i_bus(mem, 1); // instruction fetch
    SegBus d_bus(mem, 0); // data + stack
    i_bus.set_trace(mem_trace);
    d_bus.set_trace(mem_trace);

    z8001_device cpu;
    cpu.set_program_memory(&i_bus);
    cpu.set_data_memory(&d_bus);
    cpu.set_stack_memory(&d_bus);
    cpu.set_trace(trace);
    cpu.set_reg_trace(reg_trace);
    bdos_set_trace(bdos_trace);

    // Wire FCW pointer so SegBus can distinguish system/normal mode for segment 0
    i_bus.set_fcw_ptr(cpu.get_fcw_ptr());
    d_bus.set_fcw_ptr(cpu.get_fcw_ptr());

    CpmFileSystem fs(mem);
    for (int i = 0; i < 4; i++)
        fs.set_drive_path(i, drive_paths[i]);
    g_fs = &fs;

    EmuIO io(cpu, fs, mem, g_warm_boot);
    cpu.set_io(&io);

    // --- Load cpm.sys into system segment ---
    fprintf(stderr, "Loading %s...\n", sys_file);
    int entry = load_coff(mem, sys_file, PHYS_SYS);
    if (entry < 0) {
        fprintf(stderr, "Failed to load %s\n", sys_file);
        return 1;
    }

    // --- Set up BIOS data structures ---
    // Place MRT after the loaded CCP code
    uint16_t mrt_offset = (g_ccp_size + 0xFF) & ~0xFF; // Align to 256
    g_mrt_offset = build_mrt(mem, mrt_offset);
    fprintf(stderr, "MRT at system offset 0x%04X\n", g_mrt_offset);

    // Initialize BDOS state (TPA limits from MRT region 1)
    bdos_init((uint32_t(SEG_TPA) << 16) | 0x0000,
              (uint32_t(SEG_TPA) << 16) | 0xFFFE);

    // --- Initialize and run ---
    console_init();

    // Cold boot: set initial drive to A
    fs.set_current_drive(0);

    // Boot loop: CCP runs, warm boot restarts it
    bool cold_boot = true;
    uint16_t saved_psap_seg = 0;
    uint16_t saved_psap_off = 0;
    do {
        g_warm_boot = false;

        mem.set_segment_unified(0x00, 0x10000);

        // Cold boot: C runtime startup (clears BSS, calls main → ccp)
        // Warm boot: jump directly to ccp() to preserve BSS state
        // (CCP keeps submit flags in .data and subfcb in .bss — BSS
        // must not be cleared on warm boot for SUBMIT to work)
        uint16_t entry = cold_boot ? g_entry_point : g_warm_entry;
        uint32_t pc = (uint32_t(SEG_SYS) << 16) | entry;

        // On warm boot, preserve PSAP from the previous run.
        // The CCP's C runtime sets PSAP to point to its own PSA vectors
        // (sc_trap, etc.) at startup. If we reset PSAP to the initial
        // values, SC trap dispatch would read garbage instead of the
        // real trap handler addresses.
        uint16_t psap_seg = cold_boot ? SEG_PSA : saved_psap_seg;
        uint16_t psap_off = cold_boot ? PSA_OFFSET : saved_psap_off;

        // Cold boot: FCW=0xC000 (segmented + system mode) — _start is
        // assembly that sets up the stack and PSAP before bios clears
        // FCW.SEG with `res r0, #0xf`, switching to non-segmented mode.
        // Warm boot: FCW=0x4000 (system mode, NO segmented mode) — we
        // jump directly to ccp() which immediately enters C code.  C code
        // needs SP=R15 (non-segmented), not SP=R14 (segmented), because
        // csv sets R14=R15 as frame pointer — if SP were R14, subsequent
        // CALL/PUSH would interpret the frame-pointer value as a segment
        // number, corrupting the stack.
        uint16_t fcw = cold_boot ? 0xC000 : 0x4000;

        cpu.init_state(
            fcw,              // FCW: see above
            pc,               // PC: segmented address
            psap_seg,         // PSAP segment
            psap_off,         // PSAP offset
            SEG_SYS,          // NSP segment
            SYS_STACK_TOP     // NSP offset
        );

        cold_boot = false;

        // Reset DMA for CCP (runs in system segment)
        // Current drive persists across warm boots (matching real BDOS)
        fs.set_caller_seg(SEG_SYS);
        fs.set_dma((uint32_t(SEG_SYS) << 16) | 0x0080);

        // Refresh __exit handler at TPA offset 0x0002 (warm boot may have loaded
        // a program that overwrote it; this ensures a valid fallback).
        // Programs loaded by PGMLD include their own __exit from startup.8kn,
        // but the fallback is needed if CCP re-dispatches without loading.
        static const uint8_t exit_handler[] = {0xBD, 0x50, 0x7F, 0x02, 0x7A, 0x00};
        memcpy(mem.data() + PHYS_TPA + 0x0002, exit_handler, sizeof(exit_handler));

        // Run until CPU halts (run(-1) only executes ~1M cycles per call)
        while (!cpu.is_halted()) {
            cpu.run(-1);
        }

        // Save PSAP for next warm boot (set by CCP's C runtime during init)
        saved_psap_seg = cpu.get_psap_seg();
        saved_psap_off = cpu.get_psap_off();
    } while (g_warm_boot);

    console_restore();
    return 0;
}
