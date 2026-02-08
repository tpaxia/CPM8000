#include "cpm8k_bdos.h"
#include "cpm8k_console.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// CP/M-8000 version: 1.4, type 0x14, system type 0x22
static constexpr uint16_t CPM8K_VERSION = 0x1422;

// Current caller's segment (set at start of bdos_handler from PC)
static uint8_t s_caller_seg = 0x0A;

// Read/write bytes in the caller's segment
static uint8_t cpu_read_byte(SegmentedMemory& mem, uint16_t addr)
{
    return mem.read_byte((uint32_t(s_caller_seg) << 16) | addr);
}

static void cpu_write_byte(SegmentedMemory& mem, uint16_t addr, uint8_t val)
{
    mem.write_byte((uint32_t(s_caller_seg) << 16) | addr, val);
}

// BDOS console functions

static void bdos_c_read(z8002_device& cpu)
{
    uint8_t ch = console_in();
    console_out(ch); // Echo
    cpu.set_reg(7, ch);
}

static void bdos_c_write(z8002_device& cpu)
{
    uint8_t ch = cpu.get_reg(7) & 0xFF;
    console_out(ch);
}

static void bdos_c_rawio(z8002_device& cpu)
{
    uint8_t ch = cpu.get_reg(7) & 0xFF;
    if (ch == 0xFF) {
        // Input: return char if available, else 0
        if (console_status()) {
            ch = console_in();
            cpu.set_reg(7, ch);
        } else {
            cpu.set_reg(7, 0);
        }
    } else if (ch == 0xFE) {
        // Status only
        cpu.set_reg(7, console_status() ? 0xFF : 0);
    } else if (ch == 0xFD) {
        // Input (blocking)
        ch = console_in();
        cpu.set_reg(7, ch);
    } else {
        // Output
        console_out(ch);
    }
}

static void bdos_c_writestr(z8002_device& cpu, SegmentedMemory& mem)
{
    // rr6 = address of $-terminated string
    uint16_t addr = cpu.get_reg(7); // Low word of rr6
    for (int i = 0; i < 0xFFFF; i++) {
        uint8_t ch = cpu_read_byte(mem, addr + i);
        if (ch == '$') break;
        console_out(ch);
    }
}

static void bdos_c_readstr(z8002_device& cpu, SegmentedMemory& mem)
{
    // rr6 = address of buffer: [max_len][ret_len][data...]
    uint16_t buf_addr = cpu.get_reg(7);
    uint8_t max_len = cpu_read_byte(mem, buf_addr);
    if (max_len == 0) max_len = 1;

    uint8_t count = 0;
    uint16_t data_addr = buf_addr + 2;

    while (count < max_len) {
        uint8_t ch = console_in();

        if (console_eof()) {
            if (count == 0) {
                // EOF at start of input - exit emulator
                cpu_write_byte(mem, buf_addr + 1, 0);
                cpu_write_byte(mem, data_addr, 0); // null terminate
                cpu.set_reg(7, 0);
                cpu.request_halt();
                return;
            }
            // EOF with partial input - submit what we have
            break;
        }

        if (ch == 0x04) { // Ctrl-D - exit emulator
            console_out('\r');
            console_out('\n');
            cpu_write_byte(mem, buf_addr + 1, 0);
            cpu_write_byte(mem, data_addr, 0);
            cpu.set_reg(7, 0);
            cpu.request_halt();
            return;
        }

        if (ch == '\r' || ch == '\n') {
            console_out('\r');
            console_out('\n');
            break;
        }
        if (ch == 0x08 || ch == 0x7F) { // Backspace/Delete
            if (count > 0) {
                count--;
                console_out('\b');
                console_out(' ');
                console_out('\b');
            }
            continue;
        }
        if (ch == 0x03) { // Ctrl-C
            console_out('^');
            console_out('C');
            console_out('\r');
            console_out('\n');
            count = 0;
            break;
        }
        if (ch == 0x15) { // Ctrl-U (cancel line)
            console_out('\r');
            console_out('\n');
            count = 0;
            continue;
        }

        console_out(ch);
        cpu_write_byte(mem, data_addr + count, ch);
        count++;
    }

    // Null-terminate the buffer (CCP is written in C and expects this)
    cpu_write_byte(mem, data_addr + count, 0);

    cpu_write_byte(mem, buf_addr + 1, count);
    cpu.set_reg(7, count);

    // Check for EXIT command - exit the emulator cleanly
    if (count >= 4) {
        char cmd[5];
        for (int i = 0; i < 4 && i < count; i++)
            cmd[i] = toupper(cpu_read_byte(mem, data_addr + i));
        cmd[4] = '\0';

        // Check for "EXIT" optionally followed by spaces
        if (strncmp(cmd, "EXIT", 4) == 0) {
            bool is_exit = true;
            for (int i = 4; i < count; i++) {
                if (cpu_read_byte(mem, data_addr + i) != ' ') {
                    is_exit = false;
                    break;
                }
            }
            if (is_exit) {
                fprintf(stderr, "EXIT command - shutting down emulator\n");
                cpu.request_halt();
                return;
            }
        }
    }
}

static void bdos_c_stat(z8002_device& cpu)
{
    cpu.set_reg(7, console_status() ? 0xFF : 0);
}

// Extern reference to memory for bdos string functions
extern SegmentedMemory* g_mem;

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
    uint32_t tpa_len    = pgtop; // Length of TPA region

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

    // For non-segmented combined I/D: load everything sequentially into TPA
    const int BPLEN = 256;     // basepage size
    const int DEFSTACK = 256;  // default stack

    uint32_t load_offset = 0;  // Current offset within TPA
    uint32_t text_loc = 0, text_size = 0;
    uint32_t data_loc = 0, data_size = 0;
    uint32_t bss_loc = 0, bss_size = 0;
    uint32_t seg_size = 0;     // Total loaded size (for limit checking)

    // Maximum loadable area
    uint32_t max_load = (tpa_len > 0 ? tpa_len : 0xFFFE) - BPLEN - DEFSTACK;

    for (int i = 0; i < num_segs; i++) {
        uint16_t slen = segs[i].length;
        uint8_t  stype = segs[i].type;

        switch (stype) {
        case 3: // CODE
        case 6: // CDMIX
        case 7: // CDMIX_P
            if (text_size == 0) text_loc = load_offset;
            text_size += slen;
            // Copy code data to TPA
            for (uint16_t j = 0; j < slen; j++) {
                mem.write_byte((uint32_t(tpa_seg) << 16) | (tpa_base + load_offset + j),
                               code_data[code_offset + j]);
            }
            load_offset += slen;
            code_offset += slen;
            break;

        case 4: // CONST
        case 5: // DATA
            if (data_size == 0) data_loc = load_offset;
            data_size += slen;
            // Copy data to TPA
            for (uint16_t j = 0; j < slen; j++) {
                mem.write_byte((uint32_t(tpa_seg) << 16) | (tpa_base + load_offset + j),
                               code_data[code_offset + j]);
            }
            load_offset += slen;
            code_offset += slen;
            break;

        case 1: // BSS - no data in file
            if (bss_size == 0) bss_loc = load_offset;
            bss_size += slen;
            // Zero-fill BSS
            for (uint16_t j = 0; j < slen; j++) {
                mem.write_byte((uint32_t(tpa_seg) << 16) | (tpa_base + load_offset + j), 0);
            }
            load_offset += slen;
            break;

        case 2: // STACK - no data in file, just adjusts stack size
            break;
        }
        seg_size = load_offset;
    }

    if (seg_size > max_load) {
        fprintf(stderr, "PGMLD: program too large (%u > %u)\n",
                (unsigned)seg_size, (unsigned)max_load);
        delete[] fbuf;
        return 2; // NOMEM
    }

    // Calculate basepage and stack locations (matching real BDOS pgmld.c)
    // stkloc = segment_base + SEGLEN - BPLEN - stksiz
    // Real BDOS uses full 64K segment size, ignoring TPA limits in LPB
    const uint32_t SEGLEN = 0x10000;
    uint32_t segment_base = (uint32_t(tpa_seg) << 16) | tpa_base;
    uint32_t stkloc = segment_base + SEGLEN - BPLEN - DEFSTACK;

    uint32_t bp_addr = stkloc;
    // stackptr = stkloc - sizeof(ustack), where ustack is 4 bytes for non-seg
    uint32_t sp_addr = stkloc - (segmented ? 8 : 4);

    // Write basepage (256 bytes, big-endian)
    // Clear it first
    for (int i = 0; i < BPLEN; i++)
        mem.write_byte(bp_addr + i, 0);

    // Basepage fields (all big-endian 32-bit):
    uint32_t code_addr = (uint32_t(tpa_seg) << 16) | (tpa_base + text_loc);
    uint32_t data_addr_val = (uint32_t(tpa_seg) << 16) | (tpa_base + data_loc);
    uint32_t bss_addr  = (uint32_t(tpa_seg) << 16) | (tpa_base + bss_loc);
    // Free space = segment limit - loaded size (matching real BDOS)
    uint32_t seg_limit = SEGLEN - BPLEN - DEFSTACK;
    uint32_t free_len  = (seg_size < seg_limit) ? (seg_limit - seg_size) : 0;

    // 0x00: ltpa (low TPA address)
    mem.write_word(bp_addr + 0, tpa_seg);
    mem.write_word(bp_addr + 2, tpa_base);
    // 0x04: htpa (= stackptr, per real BDOS: bp.htpa = mylpb.stackptr)
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

    // pgldaddr: overwrite with actual starting PC address
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

bool bdos_handler(z8002_device& cpu, CpmFileSystem& fs, uint8_t caller_seg)
{
    // Register conventions from startup.8kn:
    // r5 = function number, rr6 = parameter
    // Returns: r7 (word) or rr6 (long)
    uint16_t func = cpu.get_reg(5);
    uint16_t param_lo = cpu.get_reg(7); // Low word of rr6 (often the address)

    SegmentedMemory& mem = *g_mem;
    int result;

    // Caller's segment is passed from the assembly trap handler via r4
    s_caller_seg = caller_seg;
    fs.set_caller_seg(s_caller_seg);

    switch (func) {
    case 0: { // P_TERMCPM - warm boot
        // Signal warm boot so main loop restarts the CCP
        extern bool g_warm_boot;
        g_warm_boot = true;
        cpu.request_halt();
        return true;
    }

    case 1: // C_READ
        bdos_c_read(cpu);
        return true;

    case 2: // C_WRITE
        bdos_c_write(cpu);
        return true;

    case 3: // A_READ (aux input)
        cpu.set_reg(7, console_in());
        return true;

    case 4: // A_WRITE (aux output)
        console_out(cpu.get_reg(7) & 0xFF);
        return true;

    case 5: // L_WRITE (list/printer output) - stub
        return true;

    case 6: // C_RAWIO
        bdos_c_rawio(cpu);
        return true;

    case 9: // C_WRITESTR
        bdos_c_writestr(cpu, mem);
        return true;

    case 10: // C_READSTR
        bdos_c_readstr(cpu, mem);
        return true;

    case 11: // C_STAT
        bdos_c_stat(cpu);
        return true;

    case 12: // S_BDOSVER
        cpu.set_reg(7, CPM8K_VERSION);
        return true;

    case 13: // DRV_ALLRESET
        fs.reset_all_drives();
        cpu.set_reg(7, 0);
        return true;

    case 14: // DRV_SET
        fs.set_current_drive(param_lo & 0x0F);
        cpu.set_reg(7, 0);
        return true;

    case 15: // F_OPEN
        result = fs.file_open(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 16: // F_CLOSE
        result = fs.file_close(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 17: // F_SFIRST
        result = fs.file_search_first(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 18: // F_SNEXT
        result = fs.file_search_next();
        cpu.set_reg(7, result);
        return true;

    case 19: // F_DELETE
        result = fs.file_delete(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 20: // F_READ (sequential)
        result = fs.file_read_seq(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 21: // F_WRITE (sequential)
        result = fs.file_write_seq(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 22: // F_MAKE
        result = fs.file_make(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 23: // F_RENAME
        result = fs.file_rename(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 24: // DRV_LOGINVEC
        cpu.set_reg(7, fs.get_login_vector());
        return true;

    case 25: // DRV_GET
        cpu.set_reg(7, fs.get_current_drive());
        return true;

    case 26: { // F_DMAOFF - set DMA address
        // RR6 contains the full segmented DMA address (R6=segment, R7=offset)
        uint16_t dma_seg = cpu.get_reg(6);
        uint16_t dma_off = param_lo;
        uint32_t dma_addr = (uint32_t(dma_seg) << 16) | dma_off;
        fs.set_dma(dma_addr);
        cpu.set_reg(7, 0);
        return true;
    }

    case 27: // DRV_ALLOCVEC - stub (return 0)
        cpu.set_reg_long(6, 0);
        return true;

    case 29: // DRV_ROVEC
        cpu.set_reg(7, fs.get_rovec());
        return true;

    case 30: // F_ATTRIB - set file attributes (stub)
        cpu.set_reg(7, 0);
        return true;

    case 31: // DRV_DPB - return DPB address (stub - return 0)
        cpu.set_reg_long(6, 0);
        return true;

    case 32: // F_USERNUM - get/set user number
        if (param_lo == 0xFF) {
            cpu.set_reg(7, fs.get_user());
        } else {
            fs.set_user(param_lo & 0x0F);
            cpu.set_reg(7, 0);
        }
        return true;

    case 33: // F_READRAND
        result = fs.file_read_rand(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 34: // F_WRITERAND
        result = fs.file_write_rand(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 35: // F_SIZE
        result = fs.file_size(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 36: // F_RANDREC
        result = fs.file_set_random(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 37: // DRV_RESET
        fs.reset_drives(param_lo);
        cpu.set_reg(7, 0);
        return true;

    case 40: // F_WRITEZF
        result = fs.file_write_rand_zf(param_lo);
        cpu.set_reg(7, result);
        return true;

    case 59: // PGMLD - Program Load
        result = bdos_pgmld(mem, fs, param_lo);
        cpu.set_reg(7, result);
        return true;

    default:
        fprintf(stderr, "BDOS: unimplemented function %d\n", func);
        cpu.set_reg(7, 0);
        return true;
    }
}
