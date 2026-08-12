// Hosted CP/M-8000 emulator.
// The same sources build segmented Z8001 and non-segmented Z8002 targets.
// BIOS (SC #3) services bridge to C++ via I/O port OUT instructions.
// All BDOS (SC #2) calls go to the native BDOS; disk I/O uses BIOS
// block read/write against real Olivetti M20 disk image files.

#include "z8000.h"
#include "cpm8k_mem.h"
#include "cpm8k_bdos.h"
#include "cpm8k_bios.h"
#include "cpm8k_console.h"
#include "cpm8k_file.h"
#include "cpm8k_drives.h"

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

// Physical offset used by the Z8001 system-segment MRT builder.
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

uint16_t g_mrt_offset = 0;

// Verbose startup diagnostics (off by default; enabled with -v)
bool g_verbose = false;

// Warm boot flag
bool g_warm_boot = false;

// True if the most recently dispatched TPA program was split I/D (data at
// 0x20000). On the following warm boot the CCP reads the pending P_CHAIN
// command via segment 0's data map; a split program leaves that command in
// its data segment (0x20000), so seg 0 must map there, not to the merged
// TPA (0x10000), or multi-pass tools (e.g. the C compiler zcc1->zcc2->zcc3)
// lose the chain command and the pass never loads.
bool g_last_prog_split = false;

// BDOS state sync: offsets within system segment (from COFF symbol table)
// Kept for symbol table lookup diagnostics; no longer synced at runtime.
uint16_t g_bdos_dma_offset = 0;
uint16_t g_bdos_curdisk_offset = 0;
uint16_t g_gbls_offset = 0;

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
static int load_coff(CpmAddressSpace& mem, const char* path, uint32_t load_base)
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
            uint32_t dest = load_base + offset;
            if (uint32_t(offset) + sec.size <= 0x10000) {
                mem.write_block(dest, buf + sec.data_offset, sec.size);
                if (g_verbose)
                    fprintf(stderr, "  Loaded section %.8s: offset=0x%04X size=0x%X\n",
                            sec.name, offset, sec.size);
            }
        } else if (sec.size > 0 && sec.data_offset == 0) {
            // BSS section - zero fill
            uint32_t dest = load_base + offset;
            if (uint32_t(offset) + sec.size <= 0x10000) {
                mem.clear_block(dest, sec.size);
                if (g_verbose)
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
        if (g_verbose)
            fprintf(stderr, "  Warm boot entry: ccp at 0x%04X\n", g_warm_entry);
    }

    // Find BDOS state variables for sync between C++ file I/O and native BDOS
    uint32_t dma_sym = find_coff_symbol(buf, file_size, hdr, "_dma");
    if (dma_sym) g_bdos_dma_offset = dma_sym & 0xFFFF;
    uint32_t curdis_sym = find_coff_symbol(buf, file_size, hdr, "_cur_dis");
    if (curdis_sym) g_bdos_curdisk_offset = curdis_sym & 0xFFFF;
    uint32_t gbls_sym = find_coff_symbol(buf, file_size, hdr, "_gbls");
    if (gbls_sym) g_gbls_offset = gbls_sym & 0xFFFF;

    delete[] buf;

    // entry_point is a segmented address - extract offset
    g_entry_point = entry_point & 0xFFFF;
    if (g_warm_entry == 0)
        g_warm_entry = g_entry_point; // Fallback if symbol not found
    g_ccp_size = highest_addr;

    if (g_verbose)
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

static uint16_t build_z8002_mrt(CpmAddressSpace& mem, uint16_t offset)
{
    uint8_t table[34] = {};
    write_be16(table, 4);
    write_be32(table + 2, 0x01000000);  // merged I/D
    write_be32(table + 6, 0x00010000);
    write_be32(table + 10, 0x01000000); // split I
    write_be32(table + 14, 0x00010000);
    write_be32(table + 18, 0x02000000); // split D
    write_be32(table + 22, 0x00010000);
    write_be32(table + 26, 0x01000000); // I-space accessed as data
    write_be32(table + 30, 0x00010000);
    mem.write_block(offset, table, sizeof(table));
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

static FILE* sc_trace_fp = nullptr;

static const char* find_existing(const char* const* paths)
{
    for (const char* const* p = paths; *p; ++p) {
        struct stat st;
        if (stat(*p, &st) == 0)
            return *p;
    }
    return nullptr;
}

#include "cpm8k_target.h"

static void usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [options] [image_a [image_b]]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -d X=dir:PATH  Map drive X (A..P) to host directory PATH (local disk)\n");
    fprintf(stderr, "  -d X=img:PATH  Map drive X (A..P) to disk image file PATH\n");
    fprintf(stderr, "  -b         Enable BDOS call trace\n");
    fprintf(stderr, "  -t         Enable CPU instruction trace\n");
    fprintf(stderr, "  -r         Enable register trace\n");
    fprintf(stderr, "  -m         Enable memory bus trace\n");
    fprintf(stderr, "  -v         Verbose startup diagnostics\n");
    fprintf(stderr, "  CPU target: %s\n", HostedTarget::name);
    fprintf(stderr, "  -h         Show this help\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Drives can be mixed, e.g.:\n");
    fprintf(stderr, "  %s -d A=img:rel11a.img -d C=dir:drives/C\n", prog);
    fprintf(stderr, "Positional image_a/image_b are shorthand for -d A=img: / -d B=img:.\n");
}

int main(int argc, char* argv[])
{
    bool bdos_trace = false;
    bool trace = false;
    bool reg_trace = false;
    bool mem_trace = false;
    int opt;
    while ((opt = getopt(argc, argv, "btrmvhd:")) != -1) {
        switch (opt) {
        case 'b': bdos_trace = true; break;
        case 't': trace = true; break;
        case 'r': reg_trace = true; break;
        case 'm': mem_trace = true; break;
        case 'v': g_verbose = true; break;
        case 'd':
            if (!drive_parse_spec(optarg)) { usage(argv[0]); return 1; }
            break;
        case 'h': usage(argv[0]); return 0;
        default: usage(argv[0]); return 1;
        }
    }

    // Positional args are shorthand for image drives A and B, applied only
    // if those drives were not already configured with -d.
    if (optind < argc && !drive_present(0))
        drive_set(0, DriveBackend::IMAGE, argv[optind]);
    if (optind + 1 < argc && !drive_present(1))
        drive_set(1, DriveBackend::IMAGE, argv[optind + 1]);

    // At least one drive must be configured.
    if (drive_login_vector() == 0) {
        fprintf(stderr, "Error: no drives configured (use -d or give a disk image)\n");
        usage(argv[0]);
        return 1;
    }

    const char* sys_file = HostedTarget::find_system();

    if (!sys_file) {
        fprintf(stderr, "Error: no cpm.sys found. Build it with 'make bios-emu' or specify path.\n");
        return 1;
    }

    HostedMachine<HostedTarget> machine(bdos_trace, trace, reg_trace, mem_trace);
    return machine.run(sys_file, bdos_trace);
}
