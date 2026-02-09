#ifndef CPM8K_BDOS_H
#define CPM8K_BDOS_H

#include "z8000.h"
#include "cpm8k_file.h"

// Initialize BDOS state (TPA limits, exception vectors)
void bdos_init(uint32_t tpa_low, uint32_t tpa_high);

// Enable/disable BDOS call tracing to stderr
void bdos_set_trace(bool enable);

// BDOS function handler for SC #2
// caller_seg is the segment of the caller (passed from assembly trap handler).
// Returns true if the syscall was handled.
bool bdos_handler(z8002_device& cpu, CpmFileSystem& fs, uint8_t caller_seg);

#endif // CPM8K_BDOS_H
