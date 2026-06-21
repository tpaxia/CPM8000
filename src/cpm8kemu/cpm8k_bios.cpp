#include "cpm8k_bios.h"
#include "cpm8k_console.h"
#include "cpm8k_drives.h"
#include <cstdio>
#include <cstring>

// System segment
static constexpr uint8_t SYS_SEG = 0x0B;

// Physical offset of system segment
static constexpr uint32_t PHYS_SYS = 0x30000;

// BIOS function numbers (from bios.s biostbl)
enum BiosFunc {
    BIOS_INIT    = 0,
    BIOS_WBOOT   = 1,
    BIOS_CONST   = 2,
    BIOS_CONIN   = 3,
    BIOS_CONOUT  = 4,
    BIOS_LIST    = 5,
    BIOS_PUNCH   = 6,
    BIOS_READER  = 7,
    BIOS_HOME    = 8,
    BIOS_SELDSK  = 9,
    BIOS_SETTRK  = 10,
    BIOS_SETSEC  = 11,
    BIOS_SETDMA  = 12,
    BIOS_READ    = 13,
    BIOS_WRITE   = 14,
    BIOS_LISTST  = 15,
    BIOS_SECTRAN = 16,
    BIOS_FLUSH   = 17,
    BIOS_GMRTA   = 18,
    BIOS_GMRT    = 19,
    BIOS_MAXDRV  = 20,
    BIOS_FLUSH2  = 21,
    BIOS_SETXVEC = 22,
};

// Memory region table address (set by main after loading cpm.sys)
extern uint16_t g_mrt_offset;

// BIOS trace flag and file
static bool s_bios_trace = false;
static FILE* s_trace_fp = nullptr;

void bios_set_trace(bool enable)
{
    s_bios_trace = enable;
    if (enable && !s_trace_fp) {
        s_trace_fp = fopen("bios_trace.log", "w");
    }
}

static const char* bios_func_name(int f)
{
    static const char* names[] = {
        "INIT","WBOOT","CONST","CONIN","CONOUT","LIST","PUNCH","READER",
        "HOME","SELDSK","SETTRK","SETSEC","SETDMA","READ","WRITE",
        "LISTST","SECTRAN","FLUSH","GMRTA","GMRT","MAXDRV","FLUSH2","SETXVEC"
    };
    if (f >= 0 && f < 23) return names[f];
    return "???";
}

// --- Disk I/O state (indexed by drive 0=A .. 15=P) ---
// Only IMAGE-backed drives have an open file here; HOST_DIR drives are
// serviced at the BDOS file level (CpmFileSystem) and never reach the
// BIOS sector handlers.
static FILE* s_disk_fp[NUM_DRIVES] = {nullptr};
static uint16_t s_dph_offset[NUM_DRIVES] = {0};
static int s_num_image_drives = 0;
static int s_max_drive = 0;       // highest image drive index + 1 (for MAXDRV)
static int s_drive = 0;
static int s_track = 0;
static int s_sector = 0;
static uint32_t s_dma_addr = 0;

// --- Big-endian memory helpers ---
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

// --- Disk subsystem initialization ---
// Build DPH, DPB, and buffer structures in Z8001 system segment memory,
// and open disk image files for every IMAGE-backed drive in the drive
// table. HOST_DIR drives are skipped here (serviced via CpmFileSystem).
//
// Layout at base_offset within system segment:
//   DPB    (18 bytes, shared by all image drives)
//   DIRBUF (128 bytes, shared)
//   then, per image drive: DPH (26) + CSV (16) + ALV (32)
//
// DPH layout (26 bytes, all pointers are 4-byte segmented longs):
//   0: xltp (4)     - sector translation table (NULL = identity)
//   4: dphscr (6)   - BDOS scratchpad (3 x int)
//  10: dirbufp (4)  - directory buffer pointer
//  14: dpbp (4)     - disk parameter block pointer
//  18: csvp (4)     - checksum vector pointer
//  22: alvp (4)     - allocation vector pointer

uint16_t bios_init_disks(SegmentedMemory& mem, uint16_t base_offset)
{
    uint8_t* sysbase = mem.data() + PHYS_SYS;
    uint16_t off = base_offset;

    // --- Shared DPB (M20 dpb1: 280KB double-sided) ---
    // spt=32, bsh=4, blm=15, exm=1, pad=0, dsm=134, drm=63,
    // al0=0xC0, al1=0, cks=16, off=3, psh=0, psm=0
    uint16_t dpb_off = off; off += 18;
    uint8_t* p = sysbase + dpb_off;
    memset(p, 0, 18);
    write_be16(p + 0,  32);    // spt
    p[2]  = 4;                  // bsh
    p[3]  = 15;                 // blm
    p[4]  = 1;                  // exm
    write_be16(p + 6,  134);   // dsm
    write_be16(p + 8,  63);    // drm
    p[10] = 0xC0;               // al0
    write_be16(p + 12, 16);    // cks
    write_be16(p + 14, 3);     // off

    // --- Shared directory buffer ---
    uint16_t dirbuf_off = off; off += 128;
    memset(sysbase + dirbuf_off, 0, 128);

    // --- Per image drive: DPH + CSV + ALV, and open the image file ---
    for (int d = 0; d < NUM_DRIVES; d++) {
        s_disk_fp[d] = nullptr;
        s_dph_offset[d] = 0;
        if (!drive_is_image(d)) continue;

        FILE* fp = fopen(drive_path(d).c_str(), "r+b");
        if (!fp) {
            fprintf(stderr, "Cannot open image %c: %s\n", 'A' + d, drive_path(d).c_str());
            continue;
        }

        uint16_t dph_off = off; off += 26;
        uint16_t csv_off = off; off += 16;
        uint16_t alv_off = off; off += 32;
        memset(sysbase + dph_off, 0, 26 + 16 + 32);

        uint8_t* dph = sysbase + dph_off;
        write_be32(dph + 0,  0);  // xltp = NULL (no sector translation)
        write_be32(dph + 10, (uint32_t(SYS_SEG) << 16) | dirbuf_off);
        write_be32(dph + 14, (uint32_t(SYS_SEG) << 16) | dpb_off);
        write_be32(dph + 18, (uint32_t(SYS_SEG) << 16) | csv_off);
        write_be32(dph + 22, (uint32_t(SYS_SEG) << 16) | alv_off);

        s_disk_fp[d] = fp;
        s_dph_offset[d] = dph_off;
        s_num_image_drives++;
        if (d + 1 > s_max_drive) s_max_drive = d + 1;
        fprintf(stderr, "Drive %c: image %s\n", 'A' + d, drive_path(d).c_str());
    }

    fprintf(stderr, "BIOS disk data at 0x%04X-0x%04X (%d image drive%s)\n",
            base_offset, off, s_num_image_drives,
            s_num_image_drives == 1 ? "" : "s");
    return off;
}

void bios_cleanup_disks()
{
    for (int i = 0; i < NUM_DRIVES; i++) {
        if (s_disk_fp[i]) {
            fclose(s_disk_fp[i]);
            s_disk_fp[i] = nullptr;
        }
    }
}

// --- BIOS function handler ---

bool bios_handler(z8002_device& cpu, SegmentedMemory& mem, bool& warm_boot, uint8_t caller_seg)
{
    // BIOS calling convention (from biostrap.s biossc):
    // r3 = function number
    // rr4 = P1 (parameter 1)
    // rr6 = P2 (parameter 2)
    // Returns: rr6 (long)
    uint16_t func = cpu.get_reg(3);
    uint16_t p1_hi = cpu.get_reg(4);
    uint16_t p1_lo = cpu.get_reg(5);
    uint16_t p2_hi = cpu.get_reg(6);
    uint16_t p2_lo = cpu.get_reg(7);
    (void)caller_seg;

    if (s_bios_trace && s_trace_fp && func != BIOS_CONST) {
        fprintf(s_trace_fp, "BIOS %2d %-8s P1=%04X:%04X P2=%04X:%04X\n",
                func, bios_func_name(func), p1_hi, p1_lo, p2_hi, p2_lo);
        if (func == BIOS_CONOUT) {
            fprintf(s_trace_fp, "  REGS:");
            for (int i = 0; i < 16; i++)
                fprintf(s_trace_fp, " R%d=%04X", i, cpu.get_reg(i));
            fprintf(s_trace_fp, "\n");
            // biossc R15 points to the frame (+ 4 for calr return address)
            // Original R15 (at time of SC #3) = current R15 + frame overhead
            //   Frame overhead: hardware(8) + sc_trap push(2) + sub(30) + dispatch calr(4) = 44
            uint16_t cur_r15 = cpu.get_reg(15);
            uint16_t orig_r15 = cur_r15 + 44;
            // Read frame fields for context
            uint32_t frame_phys = 0x30000 + cur_r15 + 4; // +4 for calr ret addr
            uint16_t scfcw = (mem.data()[frame_phys + 34] << 8) | mem.data()[frame_phys + 35];
            uint16_t scseg = (mem.data()[frame_phys + 36] << 8) | mem.data()[frame_phys + 37];
            uint16_t scpc = (mem.data()[frame_phys + 38] << 8) | mem.data()[frame_phys + 39];
            fprintf(s_trace_fp, "  scfcw=%04X caller=%04X:%04X orig_R15=%04X\n",
                    scfcw, scseg, scpc, orig_r15);
            // Dump stack at original R15 (the _bios stack frame before SC #3)
            uint32_t stk_phys = 0x30000 + orig_r15;
            fprintf(s_trace_fp, "  STK @%04X:", orig_r15);
            for (int i = 0; i < 24; i++)
                fprintf(s_trace_fp, " %02X", mem.data()[stk_phys + i]);
            fprintf(s_trace_fp, "\n");
            // Dump caller code around the return address (0x1480)
            static int code_dump_count = 0;
            if (code_dump_count < 2) {
                code_dump_count++;
                uint16_t ret_addr = (mem.data()[stk_phys] << 8) | mem.data()[stk_phys + 1];
                uint32_t caller_phys = 0x30000 + ret_addr;
                // Dump 32 bytes before return address (the call site and arg setup)
                fprintf(s_trace_fp, "  CALLER @%04X:", ret_addr - 32);
                for (int i = -32; i < 8; i++)
                    fprintf(s_trace_fp, " %02X", mem.data()[caller_phys + i]);
                fprintf(s_trace_fp, "\n");
                // Also dump _bios function code
                fprintf(s_trace_fp, "  _bios @02D4:");
                for (int i = 0; i < 24; i++)
                    fprintf(s_trace_fp, " %02X", mem.data()[0x30000 + 0x02D4 + i]);
                fprintf(s_trace_fp, "\n");
            }
        }
        fflush(s_trace_fp);
    }

    switch (func) {
    case BIOS_INIT:
        // Return drive A, user 0 in RR6
        cpu.set_reg_long(6, 0);
        return true;

    case BIOS_WBOOT:
        warm_boot = true;
        cpu.request_halt();
        return true;

    case BIOS_CONST:
        cpu.set_reg(7, console_status() ? 0xFF : 0);
        return true;

    case BIOS_CONIN: {
        uint8_t ch = console_in();
        if (console_eof()) {
            // EOF on stdin -> exit emulator cleanly (halt without warm boot)
            cpu.request_halt();
            return true;
        }
        cpu.set_reg(7, ch);
        return true;
    }

    case BIOS_CONOUT: {
        // The output char arrives in R5 from direct / C-runtime callers
        // (e.g. the cold-boot banner), but the native BDOS's C_WRITE path
        // delivers it in R10. Prefer R5; fall back to R10 when R5 is empty.
        uint8_t ch = p1_lo & 0xFF;
        if (ch == 0) ch = cpu.get_reg(10) & 0xFF;
        console_out(ch);
        return true;
    }

    case BIOS_LIST:   // Printer output - stub
    case BIOS_PUNCH:  // Punch output - stub
        return true;

    case BIOS_READER:  // Reader input - return Ctrl-Z (EOF)
        cpu.set_reg(7, 0x1A);
        return true;

    case BIOS_HOME:
        s_track = 0;
        return true;

    case BIOS_SELDSK: {
        uint8_t drive = p1_lo & 0xFF;
        if (drive < NUM_DRIVES && s_disk_fp[drive]) {
            s_drive = drive;
            cpu.set_reg_long(6, (uint32_t(SYS_SEG) << 16) | s_dph_offset[drive]);
        } else {
            cpu.set_reg_long(6, 0);  // invalid drive
        }
        return true;
    }

    case BIOS_SETTRK:
        s_track = p1_lo;
        return true;

    case BIOS_SETSEC:
        s_sector = p1_lo;
        return true;

    case BIOS_SETDMA:
        s_dma_addr = (uint32_t(p1_hi) << 16) | p1_lo;
        return true;

    case BIOS_READ: {
        if (s_drive >= NUM_DRIVES || !s_disk_fp[s_drive]) {
            cpu.set_reg(7, 1);
            return true;
        }
        long offset = (long)s_track * 4096 + (long)s_sector * 128;
        uint8_t buf[128];
        if (fseek(s_disk_fp[s_drive], offset, SEEK_SET) != 0 ||
            fread(buf, 1, 128, s_disk_fp[s_drive]) != 128) {
            cpu.set_reg(7, 1);
            return true;
        }
        // Copy to Z8001 memory at DMA address
        uint32_t phys = mem.translate(s_dma_addr, 0);
        memcpy(mem.data() + phys, buf, 128);
        cpu.set_reg(7, 0);
        return true;
    }

    case BIOS_WRITE: {
        if (s_drive >= NUM_DRIVES || !s_disk_fp[s_drive]) {
            cpu.set_reg(7, 1);
            return true;
        }
        long offset = (long)s_track * 4096 + (long)s_sector * 128;
        uint8_t buf[128];
        // Read from Z8001 memory at DMA address
        uint32_t phys = mem.translate(s_dma_addr, 0);
        memcpy(buf, mem.data() + phys, 128);
        if (fseek(s_disk_fp[s_drive], offset, SEEK_SET) != 0 ||
            fwrite(buf, 1, 128, s_disk_fp[s_drive]) != 128) {
            cpu.set_reg(7, 1);
            return true;
        }
        cpu.set_reg(7, 0);
        return true;
    }

    case BIOS_LISTST:
        cpu.set_reg(7, 0xFF); // List device always ready
        return true;

    case BIOS_SECTRAN: {
        uint16_t sector = p1_lo;
        uint32_t xlt = (uint32_t(p2_hi) << 16) | p2_lo;
        if (xlt == 0) {
            cpu.set_reg_long(6, (uint32_t)sector);
        } else {
            // Read translated sector from XLT table in Z8001 memory
            uint32_t phys = mem.translate(xlt + sector, 0);
            cpu.set_reg_long(6, (uint32_t)mem.data()[phys]);
        }
        return true;
    }

    case BIOS_FLUSH:
    case BIOS_FLUSH2:
        if (s_drive < NUM_DRIVES && s_disk_fp[s_drive])
            fflush(s_disk_fp[s_drive]);
        return true;

    case BIOS_GMRTA: {
        // Return memory region table address as long (seg:offset)
        uint32_t addr = (uint32_t(SYS_SEG) << 16) | g_mrt_offset;
        cpu.set_reg_long(6, addr);
        return true;
    }

    case BIOS_GMRT:
        // Return individual MRT entry - stub
        cpu.set_reg_long(6, 0);
        return true;

    case BIOS_MAXDRV:
        cpu.set_reg(7, s_max_drive);
        return true;

    case BIOS_SETXVEC:
        // Set exception vector - stub for emulator
        cpu.set_reg_long(6, 0);
        return true;

    default:
        fprintf(stderr, "BIOS: unimplemented function %d\n", func);
        cpu.set_reg(7, 0);
        return true;
    }
}
