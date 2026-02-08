#include "cpm8k_bios.h"
#include "cpm8k_console.h"
#include <cstdio>

// System segment
static constexpr uint8_t SYS_SEG = 0x0B;

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
    (void)p1_hi;
    (void)caller_seg;

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

    case BIOS_CONIN:
        cpu.set_reg(7, console_in());
        return true;

    case BIOS_CONOUT:
        console_out(p1_lo & 0xFF); // Character in R5 (first param)
        return true;

    case BIOS_LIST:   // Printer output - stub
    case BIOS_PUNCH:  // Punch output - stub
        return true;

    case BIOS_READER:  // Reader input - return Ctrl-Z (EOF)
        cpu.set_reg(7, 0x1A);
        return true;

    case BIOS_HOME:
    case BIOS_SELDSK:
    case BIOS_SETTRK:
    case BIOS_SETSEC:
    case BIOS_SETDMA:
    case BIOS_READ:
    case BIOS_WRITE:
    case BIOS_SECTRAN:
        // Disk functions not needed - BDOS handles all file I/O
        cpu.set_reg(7, 0);
        return true;

    case BIOS_LISTST:
        cpu.set_reg(7, 0xFF); // List device always ready
        return true;

    case BIOS_FLUSH:
    case BIOS_FLUSH2:
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
        cpu.set_reg(7, 4); // 4 drives
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
