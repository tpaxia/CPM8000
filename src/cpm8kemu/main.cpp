// CP/M-8000 Emulator for macOS
// Runs CP/M-8000 CCP+BDOS natively on an emulated Z8001.
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
#include "cpm8k_z8002.h"

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

class EmuIO : public z8000_io_bus {
    z8002_device& m_cpu;
    SegmentedMemory& m_mem;
    BdosRouter& m_bdos;
    bool& m_warm_boot;

public:
    EmuIO(z8002_device& cpu, SegmentedMemory& mem, BdosRouter& bdos,
          bool& warm_boot)
        : m_cpu(cpu), m_mem(mem), m_bdos(bdos), m_warm_boot(warm_boot) {}

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
    // BDOS router: a HOST_DIR-targeted call is serviced here (r0=1, the
    // assembly then skips the native BDOS); everything else defers (r0=0).
    void handle_bdos() {
        if (sc_trace_fp) {
            uint16_t func = m_cpu.get_reg(5);
            uint16_t p_hi = m_cpu.get_reg(6);
            uint16_t p_lo = m_cpu.get_reg(7);
            fprintf(sc_trace_fp, "BDOS %2d param=%04X:%04X\n", func, p_hi, p_lo);
            fflush(sc_trace_fp);
        }
        uint8_t caller_seg = (m_cpu.get_reg(4) >> 8) & 0x7F;
        bool claimed = m_bdos.route(m_cpu, caller_seg);
        m_cpu.set_reg(0, claimed ? 1 : 0);
    }

    // BIOS handler: r2=caller_seg_word, r3=func, rr4=P1, rr6=P2
    void handle_bios() {
        uint8_t caller_seg = (m_cpu.get_reg(2) >> 8) & 0x7F;
        bios_handler(m_cpu, m_mem, m_warm_boot, SEG_SYS, caller_seg);
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
                g_last_prog_split = true;
            } else {
                // Merged I/D: both I and D at TPA (0x10000)
                m_mem.set_segment_unified(0x00, 0x10000);
                g_last_prog_split = false;
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

class Z8002IO final : public z8000_io_bus {
public:
    Z8002IO(z8002_device& cpu, Z8002Memory& mem, BdosRouter& bdos,
            bool& warm_boot)
        : m_cpu(cpu), m_mem(mem), m_bdos(bdos), m_warm_boot(warm_boot) {}

    uint8_t read_byte(uint16_t, int) override { return 0xFF; }
    uint16_t read_word(uint16_t, int) override { return 0xFFFF; }
    void write_byte(uint16_t, uint8_t, int) override {}
    void write_word(uint16_t port, uint16_t, int) override {
        switch (port) {
        case PORT_BDOS: handle_bdos(); break;
        case PORT_BIOS: handle_bios(); break;
        case PORT_MAP: handle_map(); break;
        case PORT_MEMCPY: handle_memcpy(); break;
        }
    }

private:
    void handle_bdos() {
        uint16_t saved_fcw = m_cpu.get_reg(4);
        bool system = saved_fcw & FCW_S_N;
        uint16_t caller_tag = system ? 0 : m_mem.user_data_tag();
        if (!system)
            m_cpu.set_reg(6, caller_tag);
        m_cpu.set_reg(0, m_bdos.route(m_cpu, caller_tag) ? 1 : 0);
    }
    void handle_bios() {
        uint16_t saved_fcw = m_cpu.get_reg(2);
        uint16_t caller_tag = (saved_fcw & FCW_S_N) ? 0 : m_mem.user_data_tag();
        if (m_cpu.get_reg(3) == 1) {
            bios_prepare_warm_boot();
            return;
        }
        bios_handler(m_cpu, m_mem, m_warm_boot, 0, caller_tag);
    }
    void handle_map() {
        uint16_t saved_fcw = m_cpu.get_reg(4);
        int16_t space = int16_t(m_cpu.get_reg(5));
        uint16_t tag = m_cpu.get_reg(6);

        switch (space) {
        case -1:
            m_mem.configure_program(tag, tag);
            break;
        case 0:
            tag = (saved_fcw & FCW_S_N) ? 0 : m_mem.user_data_tag();
            break;
        case 1:
            tag = (saved_fcw & FCW_S_N) ? 0 : m_mem.user_code_tag();
            break;
        case 2:
        case 3:
            tag = 0;
            break;
        case 4:
            tag = 0x0200;
            m_mem.set_user_data_tag(tag);
            break;
        case 5:
            tag = m_mem.user_code_tag();
            break;
        }
        m_cpu.set_reg(6, tag);
    }
    void handle_memcpy() {
        uint32_t length = m_cpu.get_reg_long(2);
        uint32_t dst = m_cpu.get_reg_long(4);
        uint32_t src = m_cpu.get_reg_long(6);
        for (uint32_t i = 0; i < length && i < 0x10000; ++i)
            m_mem.write_byte(dst + i, m_mem.read_byte(src + i));
        m_cpu.set_reg_long(6, dst + length);
    }

    z8002_device& m_cpu;
    Z8002Memory& m_mem;
    BdosRouter& m_bdos;
    bool& m_warm_boot;
};

class Z8002HostedMachine {
public:
    Z8002HostedMachine(bool bdos_trace, bool cpu_trace,
                       bool reg_trace, bool mem_trace)
        : m_fs(m_mem), m_bdos(m_mem, m_fs, m_loader, 0),
          m_i_bus(m_mem, 1), m_d_bus(m_mem, 0),
          m_io(m_cpu, m_mem, m_bdos, g_warm_boot)
    {
        m_bdos.set_trace(bdos_trace);
        bios_set_trace(bdos_trace);
        m_i_bus.set_trace(mem_trace);
        m_d_bus.set_trace(mem_trace);
        m_cpu.set_program_memory(&m_i_bus);
        m_cpu.set_data_memory(&m_d_bus);
        m_cpu.set_stack_memory(&m_d_bus);
        m_cpu.set_io(&m_io);
        m_cpu.set_trace(cpu_trace);
        m_cpu.set_reg_trace(reg_trace);
        m_i_bus.set_fcw_ptr(m_cpu.get_fcw_ptr());
        m_d_bus.set_fcw_ptr(m_cpu.get_fcw_ptr());
    }

    int run(const char* sys_file, bool bdos_trace) {
        for (int d = 0; d < NUM_DRIVES; ++d)
            if (drive_is_host(d))
                m_fs.set_drive_path(d, drive_path(d));
        for (int d = 0; d < NUM_DRIVES; ++d)
            if (drive_present(d)) { m_fs.set_default_drive(d); break; }

        if (bdos_trace)
            sc_trace_fp = fopen("sc_trace.log", "w");
        m_cpu.set_trap_callback([](void* ctx, uint8_t sc, uint32_t pc) {
            FILE* fp = static_cast<FILE*>(ctx);
            if (fp) { fprintf(fp, "SC #%d from PC=%04X\n", sc, unsigned(pc)); fflush(fp); }
            return true;
        }, sc_trace_fp);

        int entry = load_coff(m_mem, sys_file, 0);
        if (entry < 0) return 1;
        uint16_t mrt = (g_ccp_size + 0xFF) & ~0xFF;
        g_mrt_offset = build_z8002_mrt(m_mem, mrt);
        bios_init_disks(m_mem, 0, mrt + 256);

        console_init();
        bool cold = true;
        do {
            g_warm_boot = false;
            uint16_t pc = cold ? g_entry_point : g_warm_entry;
            m_cpu.init_state(FCW_S_N, pc, 0, 0, 0, 0xFF00);
            cold = false;
            while (!m_cpu.is_halted())
                m_cpu.run(-1);
            m_bdos.clear_chain_pending();
        } while (g_warm_boot);

        bios_cleanup_disks();
        console_restore();
        return 0;
    }

private:
    Z8002Memory m_mem;
    CpmFileSystem m_fs;
    Z8002ProgramLoader m_loader;
    BdosRouter m_bdos;
    Z8002Bus m_i_bus;
    Z8002Bus m_d_bus;
    z8002_device m_cpu;
    Z8002IO m_io;
};

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
    fprintf(stderr, "  -M cpu     Hosted CPU: z8001 (default) or z8002\n");
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
    const char* machine = "z8001";

    int opt;
    while ((opt = getopt(argc, argv, "btrmvhd:M:")) != -1) {
        switch (opt) {
        case 'b': bdos_trace = true; break;
        case 't': trace = true; break;
        case 'r': reg_trace = true; break;
        case 'm': mem_trace = true; break;
        case 'v': g_verbose = true; break;
        case 'M': machine = optarg; break;
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

    if (strcmp(machine, "z8001") != 0 && strcmp(machine, "z8002") != 0) {
        fprintf(stderr, "Error: unknown hosted CPU '%s'\n", machine);
        return 1;
    }

    // Auto-detect the CPU-specific cpm.sys.
    const char* sys_file = nullptr;
    {
        static const char* z8001_defaults[] = {"cpm.sys", "build/bios-emu-z8001/cpm.sys", "src/cpm8kemu/bios-z8001/cpm.sys", nullptr};
        static const char* z8002_defaults[] = {"build/bios-emu-z8002/cpm.sys", nullptr};
        const char** defaults = strcmp(machine, "z8002") == 0 ? z8002_defaults : z8001_defaults;
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

    if (strcmp(machine, "z8002") == 0) {
        Z8002HostedMachine hosted(bdos_trace, trace, reg_trace, mem_trace);
        return hosted.run(sys_file, bdos_trace);
    }

    // --- Create system components ---
    SegmentedMemory mem;

    // Host-directory backend (services BDOS file ops for HOST_DIR drives)
    CpmFileSystem fs(mem);
    Z8001ProgramLoader loader;
    BdosRouter bdos(mem, fs, loader, SEG_SYS);
    for (int d = 0; d < NUM_DRIVES; d++)
        if (drive_is_host(d))
            fs.set_drive_path(d, drive_path(d));

    // Default the current drive to the smallest configured drive, so the CCP
    // boots there instead of always assuming A (which errors if A is not
    // mapped). The CCP reads its cold-boot drive via DRV_GET, which returns
    // this. (drive_login_vector() != 0 was checked above, so one exists.)
    for (int d = 0; d < NUM_DRIVES; d++)
        if (drive_present(d)) { fs.set_default_drive(d); break; }

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
    bdos.set_trace(bdos_trace);
    bios_set_trace(bdos_trace);

    // Wire FCW pointer so SegBus can distinguish system/normal mode for segment 0
    i_bus.set_fcw_ptr(cpu.get_fcw_ptr());
    d_bus.set_fcw_ptr(cpu.get_fcw_ptr());

    EmuIO io(cpu, mem, bdos, g_warm_boot);
    cpu.set_io(&io);

    // Trap callback: log SC calls and allow all to proceed.
    if (bdos_trace)
        sc_trace_fp = fopen("sc_trace.log", "w");
    cpu.set_trap_callback([](void* ctx, uint8_t sc_num, uint32_t caller_pc) -> bool {
        FILE* fp = static_cast<FILE*>(ctx);
        if (fp) {
            fprintf(fp, "SC #%d from PC=%05X\n", sc_num, caller_pc);
            fflush(fp);
        }
        return true;
    }, sc_trace_fp);

    // --- Load cpm.sys into system segment ---
    if (g_verbose)
        fprintf(stderr, "Loading %s...\n", sys_file);
    int entry = load_coff(mem, sys_file, uint32_t(SEG_SYS) << 16);
    if (entry < 0) {
        fprintf(stderr, "Failed to load %s\n", sys_file);
        return 1;
    }

    // --- Set up BIOS data structures ---
    // Place MRT after the loaded CCP code
    uint16_t mrt_offset = (g_ccp_size + 0xFF) & ~0xFF; // Align to 256
    g_mrt_offset = build_mrt(mem, mrt_offset);
    if (g_verbose)
        fprintf(stderr, "MRT at system offset 0x%04X\n", g_mrt_offset);

    // Build BIOS disk data structures (DPH, DPB, buffers) and open images
    // for every IMAGE-backed drive in the drive table.
    uint16_t disk_data_offset = mrt_offset + 256;
    bios_init_disks(mem, SEG_SYS, disk_data_offset);

    // --- Initialize and run ---
    console_init();

    // Boot loop: CCP runs, warm boot restarts it
    bool cold_boot = true;
    uint16_t saved_psap_seg = 0;
    uint16_t saved_psap_off = 0;
    do {
        g_warm_boot = false;

        // Reset the normal-mode TPA (segment 0). After a split-I/D program the
        // CCP still needs to read that program's pending P_CHAIN command out of
        // its data segment (0x20000), so keep the split data map for one warm
        // boot; the next program dispatch (map_adr) reconfigures segment 0.
        if (!cold_boot && g_last_prog_split && bdos.chain_pending())
            mem.set_segment(0x00, 0x10000, 0x20000);
        else
            mem.set_segment_unified(0x00, 0x10000);
        bdos.clear_chain_pending();

        // Cold boot: C runtime startup (clears BSS, calls main -> ccp)
        // Warm boot: jump directly to ccp() to preserve BSS state
        // (CCP keeps submit flags in .data and subfcb in .bss)
        uint16_t entry = cold_boot ? g_entry_point : g_warm_entry;
        uint32_t pc = (uint32_t(SEG_SYS) << 16) | entry;

        // On warm boot, preserve PSAP from the previous run.
        uint16_t psap_seg = cold_boot ? SEG_PSA : saved_psap_seg;
        uint16_t psap_off = cold_boot ? PSA_OFFSET : saved_psap_off;

        // Cold boot: FCW=0xC000 (segmented + system mode)
        // Warm boot: FCW=0x4000 (system mode, NO segmented mode)
        uint16_t fcw = cold_boot ? 0xC000 : 0x4000;

        cpu.init_state(
            fcw,              // FCW
            pc,               // PC: segmented address
            psap_seg,         // PSAP segment
            psap_off,         // PSAP offset
            SEG_SYS,          // NSP segment
            SYS_STACK_TOP     // NSP offset
        );

        cold_boot = false;

        // Refresh __exit handler at TPA offset 0x0002
        static const uint8_t exit_handler[] = {0xBD, 0x50, 0x7F, 0x02, 0x7A, 0x00};
        mem.write_block((uint32_t(SEG_TPA) << 16) | 0x0002,
                        exit_handler, sizeof(exit_handler));

        // Run until CPU halts
        while (!cpu.is_halted()) {
            cpu.run(-1);
        }

        // Save PSAP for next warm boot
        saved_psap_seg = cpu.get_psap_seg();
        saved_psap_off = cpu.get_psap_off();
    } while (g_warm_boot);

    bios_cleanup_disks();
    console_restore();
    return 0;
}
