#ifndef CPM8K_BDOS_H
#define CPM8K_BDOS_H

#include "z8000.h"
#include "cpm8k_file.h"

// Enable/disable BDOS call tracing to stderr
void bdos_set_trace(bool enable);

// BDOS SC #2 router. Decides, per call, whether the request targets a
// HOST_DIR drive (serviced here via CpmFileSystem) or should fall through
// to the native CP/M-8000 BDOS (IMAGE drives + all console/system calls).
// caller_seg is the segment of the caller (from the assembly trap handler).
// Returns true if handled in C++ (assembly must skip the native BDOS),
// false to defer to the native BDOS. On a handled call the result is left
// in the CPU's rr6 (r6:r7).
bool bdos_route(z8002_device& cpu, CpmFileSystem& fs, uint8_t caller_seg);

#endif // CPM8K_BDOS_H
