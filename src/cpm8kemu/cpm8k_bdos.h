#ifndef CPM8K_BDOS_H
#define CPM8K_BDOS_H

#include "z8000.h"
#include "cpm8k_file.h"

// BDOS function handler for SC #2
// caller_seg is the segment of the caller (passed from assembly trap handler).
// Returns true if the syscall was handled.
bool bdos_handler(z8002_device& cpu, CpmFileSystem& fs, uint8_t caller_seg);

#endif // CPM8K_BDOS_H
