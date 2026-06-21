#ifndef CPM8K_BIOS_H
#define CPM8K_BIOS_H

#include "z8000.h"
#include "cpm8k_mem.h"

// BIOS function handler for SC #3
// caller_seg is the segment of the caller (passed from assembly trap handler).
// Returns true if the syscall was handled.
// warm_boot is set to true if the handler requests a warm boot.
bool bios_handler(z8002_device& cpu, SegmentedMemory& mem, bool& warm_boot, uint8_t caller_seg);

// Initialize BIOS disk subsystem: open every IMAGE-backed drive from the
// drive table and build its DPH/DPB in Z8001 memory. HOST_DIR drives are
// serviced via CpmFileSystem and skipped here.
// base_offset = first free offset in system segment (after MRT).
// Returns offset past the last byte used.
uint16_t bios_init_disks(SegmentedMemory& mem, uint16_t base_offset);
void bios_cleanup_disks();

// Enable/disable BIOS call tracing to stderr
void bios_set_trace(bool enable);

#endif // CPM8K_BIOS_H
