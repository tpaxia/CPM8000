#include "cpm8k_bdos.h"
#include "cpm8k_mem.h"
#include "cpm8k_console.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// CP/M-8000 version: 1.4, type 0x14, system type 0x22
static constexpr uint16_t CPM8K_VERSION = 0x1422;

// Current caller's segment (set at start of bdos_handler from PC)
static uint8_t s_caller_seg = 0x0A;

// Console column tracking (for tab expansion, matching real BDOS GBL.column)
static uint16_t s_column = 0;

// Chain-to-program state (BDOS function 47)
// Real BDOS saves GBL.chainp pointing to the DMA buffer; on the next
// readline (BDOS func 10) it feeds the command from chainp instead of
// the console.  We copy the DMA content into a local buffer because
// our warm-boot path clears the basepage area.
static bool    s_chain_pending = false;
static uint8_t s_chain_buf[128];
static uint8_t s_chain_len = 0;

// Exception vector table (BDOS function 61)
// 18 entries: indices 0-9 for vecnum 2-11, indices 10-17 for vecnum 32-39
static uint32_t s_excvec[18] = {};

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

// Table indexed by BDOS function number
static const BdosFunc s_bdos_funcs[] = {
    [0]  = {"P_TERMCPM",     "Warm boot",                    BdosParam::NONE},
    [1]  = {"C_READ",        "Console input",                BdosParam::NONE},
    [2]  = {"C_WRITE",       "Console output",               BdosParam::BYTE},
    [3]  = {"A_READ",        "Aux input",                    BdosParam::NONE},
    [4]  = {"A_WRITE",       "Aux output",                   BdosParam::BYTE},
    [5]  = {"L_WRITE",       "List output",                  BdosParam::BYTE},
    [6]  = {"C_RAWIO",       "Direct console I/O",           BdosParam::BYTE},
    [7]  = {"GET_IOBYTE",    "Get I/O byte",                 BdosParam::NONE},
    [8]  = {"SET_IOBYTE",    "Set I/O byte",                 BdosParam::BYTE},
    [9]  = {"C_WRITESTR",    "Print string",                 BdosParam::ADDR},
    [10] = {"C_READSTR",     "Read console buffer",          BdosParam::ADDR},
    [11] = {"C_STAT",        "Console status",               BdosParam::NONE},
    [12] = {"S_BDOSVER",     "Return version number",        BdosParam::NONE},
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
    [32] = {"F_USERNUM",     "Get/set user number",          BdosParam::BYTE},
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
    [47] = {"P_CHAIN",       "Chain to program",             BdosParam::NONE},
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
    [60] = {nullptr,         nullptr,                        BdosParam::NONE},
    [61] = {"S_SETEXCVEC",   "Set exception vector",         BdosParam::ADDR},
    [62] = {nullptr,         nullptr,                        BdosParam::NONE},
    [63] = {"S_TPALIMITS",   "Get/set TPA limits",           BdosParam::ADDR},
};
static constexpr int NUM_BDOS_FUNCS = sizeof(s_bdos_funcs) / sizeof(s_bdos_funcs[0]);

// TPA limits (BDOS function 63)
// Temporary and permanent boundaries (segmented addresses)
static uint32_t s_tpa_lt = 0;  // TPA lower (temporary)
static uint32_t s_tpa_ht = 0;  // TPA upper (temporary)
static uint32_t s_tpa_lp = 0;  // TPA lower (permanent)
static uint32_t s_tpa_hp = 0;  // TPA upper (permanent)

// Cooked console output: expands tabs, tracks column (matches real BDOS cookdout)
static void console_cooked_out(uint8_t ch)
{
    if (ch == 0x09) { // Tab: expand to next multiple of 8
        do {
            console_out(' ');
            s_column++;
        } while (s_column & 7);
    } else {
        console_out(ch);
        if (ch >= ' ') s_column++;
        else if (ch == 0x0D) s_column = 0;
        else if (ch == 0x08) { if (s_column > 0) s_column--; }
    }
}

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
    console_cooked_out(ch);
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
        console_cooked_out(ch);
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

    // Chain-to-program: feed command from chain buffer instead of console
    // (matches real BDOS readline() behavior when GBL.chainp is set)
    if (s_chain_pending) {
        count = s_chain_len;
        if (count > max_len) count = max_len;
        for (uint8_t i = 0; i < count; i++) {
            uint8_t ch = s_chain_buf[i];
            console_cooked_out(ch);
            cpu_write_byte(mem, data_addr + i, ch);
        }
        cpu_write_byte(mem, data_addr + count, 0);
        cpu_write_byte(mem, buf_addr + 1, count);
        cpu.set_reg(7, count);
        s_chain_pending = false;
        return;
    }

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
        if (ch == 0x03) { // Ctrl-C - warmboot (matches real BDOS)
            console_cooked_out('^');
            console_cooked_out('C');
            console_cooked_out('\r');
            console_cooked_out('\n');
            extern bool g_warm_boot;
            g_warm_boot = true;
            cpu.request_halt();
            return;
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
    //   code_load_seg = 0x0A (d_map → 0x10000 = code area, same phys as seg 8 i_map)
    //   data_load_seg = 0x08 (d_map → 0x20000 = data area)
    //   exec_seg      = 0x08 (execution: i_map → code @ 0x10000, d_map → data @ 0x20000)
    // For combined I/D: everything goes to tpa_seg (usually 0x0A)
    uint8_t code_load_seg = tpa_seg;  // segment used to LOAD code (via data writes)
    uint8_t data_load_seg = tpa_seg;  // segment used to LOAD data (via data writes)
    uint8_t exec_seg      = tpa_seg;  // segment used to EXECUTE (goes in PC/basepage)
    if (split) {
        code_load_seg = 0x0A; // d_map → 0x10000 = same phys as seg 8 i_map
        data_load_seg = 0x08; // d_map → 0x20000 = split data area
        exec_seg      = 0x08; // i_map → 0x10000 (code), d_map → 0x20000 (data)
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

// --- BDOS Function 47: P_CHAIN (Chain to Program) ---
// Saves the command line from the DMA buffer and warm boots.
// On the next readline (BDOS func 10), the saved command is fed to
// the CCP instead of reading from the console.  The CCP then loads
// and runs the named program normally.
// (Matches real BDOS: GBL.chainp = GBL.dmaadr; warmboot(0);)

static void bdos_p_chain(SegmentedMemory& mem, CpmFileSystem& fs,
                         z8002_device& cpu)
{
    // Read command line from DMA buffer: [length][chars...]
    uint32_t dma_addr = fs.get_dma();
    s_chain_len = mem.read_byte(dma_addr);
    if (s_chain_len > 126) s_chain_len = 126;
    for (int i = 0; i < s_chain_len; i++)
        s_chain_buf[i] = mem.read_byte(dma_addr + 1 + i);
    s_chain_pending = true;

    // Warm boot (matching real BDOS: warmboot(0))
    extern bool g_warm_boot;
    g_warm_boot = true;
    cpu.request_halt();
}

void bdos_init(uint32_t tpa_low, uint32_t tpa_high)
{
    s_tpa_lt = s_tpa_lp = tpa_low;
    s_tpa_ht = s_tpa_hp = tpa_high;
    memset(s_excvec, 0, sizeof(s_excvec));
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
        if (func == 2 || func == 4 || func == 5) {
            uint8_t ch = cpu.get_reg(7) & 0xFF;
            if (ch >= 0x20 && ch < 0x7F)
                fprintf(stderr, " '%c'", ch);
        } else if (func == 14) {
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

    if (s_bdos_trace)
        bdos_trace_call(func, cpu, mem);

    switch (func) {
    case 0: { // P_TERMCPM - warm boot
        // Reset exception vectors and restore TPA limits (matching real BDOS)
        memset(s_excvec, 0, sizeof(s_excvec));
        s_tpa_lt = s_tpa_lp;
        s_tpa_ht = s_tpa_hp;
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

    case 7: // Get I/O Byte (return 0 — no physical I/O redirection)
        cpu.set_reg(7, 0);
        return true;

    case 8: // Set I/O Byte (ignore — no physical I/O redirection)
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

    case 15: { // F_OPEN
        result = fs.file_open(param_lo);
        cpu.set_reg(7, result);
        return true;
    }

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

    case 28: // DRV_SETRO - write protect current disk
        fs.set_drive_ro(fs.get_current_drive());
        cpu.set_reg(7, 0);
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
        // Real BDOS: any value <= 15 sets the user number; always returns current
        if ((param_lo & 0xFF) <= 15)
            fs.set_user(param_lo & 0xFF);
        cpu.set_reg(7, fs.get_user());
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

    case 46: { // DRV_FREESPACE - get disk free space
        // Write free sector count (4 bytes, big-endian) to DMA buffer.
        // We use host filesystem so report a large fixed value (~8MB).
        uint32_t dma = fs.get_dma();
        uint32_t free_sectors = 0x0000FFFE; // ~8MB in 128-byte sectors
        mem.write_word(dma, free_sectors >> 16);
        mem.write_word(dma + 2, free_sectors & 0xFFFF);
        cpu.set_reg(7, 0);
        return true;
    }

    case 47: // P_CHAIN - Chain to Program
        bdos_p_chain(mem, fs, cpu);
        return true;

    case 48: // F_FLUSH - flush buffers (no-op, host stdio handles flushing)
        cpu.set_reg(7, 0);
        return true;

    case 59: // PGMLD - Program Load
        result = bdos_pgmld(mem, fs, param_lo);
        cpu.set_reg(7, result);
        return true;

    case 61: { // Set Exception Vector
        // EPB: [vecnum:2][newvec:4][oldvec:4] at rr6 in caller's segment
        uint32_t epb = (uint32_t(s_caller_seg) << 16) | param_lo;
        int16_t vecnum = (int16_t)mem.read_word(epb);
        uint32_t newvec = (uint32_t(mem.read_word(epb + 2)) << 16) |
                           mem.read_word(epb + 4);
        int i = vecnum - 2;
        if (i == 32 || i == 33) { cpu.set_reg(7, 0xFFFF); return true; }
        if (i >= 30 && i <= 37) i -= 20;
        else if (i < 0 || i > 9) { cpu.set_reg(7, 0xFFFF); return true; }
        // Return old vector, install new
        uint32_t oldvec = s_excvec[i];
        s_excvec[i] = newvec;
        mem.write_word(epb + 6, oldvec >> 16);
        mem.write_word(epb + 8, oldvec & 0xFFFF);
        cpu.set_reg(7, 0);
        return true;
    }

    case 63: { // Get/Set TPA Limits
        // TPAB: [parms:2][low:4][high:4] at rr6 in caller's segment
        uint32_t tpab = (uint32_t(s_caller_seg) << 16) | param_lo;
        uint16_t parms = mem.read_word(tpab);
        if (parms & 1) {
            // Set TPA limits
            s_tpa_lt = (uint32_t(mem.read_word(tpab + 2)) << 16) |
                        mem.read_word(tpab + 4);
            s_tpa_ht = (uint32_t(mem.read_word(tpab + 6)) << 16) |
                        mem.read_word(tpab + 8);
            if (parms & 2) { // Sticky (permanent)
                s_tpa_lp = s_tpa_lt;
                s_tpa_hp = s_tpa_ht;
            }
        } else {
            // Get TPA limits
            mem.write_word(tpab + 2, s_tpa_lt >> 16);
            mem.write_word(tpab + 4, s_tpa_lt & 0xFFFF);
            mem.write_word(tpab + 6, s_tpa_ht >> 16);
            mem.write_word(tpab + 8, s_tpa_ht & 0xFFFF);
        }
        cpu.set_reg(7, 0);
        return true;
    }

    default:
        fprintf(stderr, "BDOS: unimplemented function %d\n", func);
        cpu.set_reg(7, 0);
        return true;
    }
}
