#ifndef CPM8K_BIOS_H
#define CPM8K_BIOS_H

#include "z8000.h"
#include "cpm8k_mem.h"

// BIOS function handler for SC #3
// caller_seg is the segment of the caller (passed from assembly trap handler).
// Returns true if the syscall was handled.
// warm_boot is set to true if the handler requests a warm boot.
bool bios_handler(z8002_device& cpu, SegmentedMemory& mem, bool& warm_boot, uint8_t caller_seg);

#endif // CPM8K_BIOS_H
