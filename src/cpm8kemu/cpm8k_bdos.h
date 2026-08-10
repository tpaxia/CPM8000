#ifndef CPM8K_BDOS_H
#define CPM8K_BDOS_H

#include "z8000.h"
#include "cpm8k_file.h"
#include "cpm8k_mem.h"

class CpmProgramLoader {
public:
    virtual ~CpmProgramLoader() = default;
    virtual int load(CpmAddressSpace& mem, CpmFileSystem& fs,
                     uint16_t caller_tag, uint16_t lpb_offset) = 0;
};

class Z8001ProgramLoader final : public CpmProgramLoader {
public:
    int load(CpmAddressSpace& mem, CpmFileSystem& fs,
             uint16_t caller_tag, uint16_t lpb_offset) override;
};

class Z8002ProgramLoader final : public CpmProgramLoader {
public:
    int load(CpmAddressSpace& mem, CpmFileSystem& fs,
             uint16_t caller_tag, uint16_t lpb_offset) override;
};

// BDOS SC #2 router. Decides, per call, whether the request targets a
// HOST_DIR drive (serviced here via CpmFileSystem) or should fall through
// to the native CP/M-8000 BDOS (IMAGE drives + all console/system calls).
// caller_seg is the segment of the caller (from the assembly trap handler).
// Returns true if handled in C++ (assembly must skip the native BDOS),
// false to defer to the native BDOS. On a handled call the result is left
// in the CPU's rr6 (r6:r7).
class BdosRouter {
public:
    BdosRouter(CpmAddressSpace& mem, CpmFileSystem& fs,
               CpmProgramLoader& loader, uint16_t system_tag)
        : m_mem(mem), m_fs(fs), m_loader(loader), m_system_tag(system_tag) {}

    void set_trace(bool enable) { m_trace = enable; }
    bool route(z8002_device& cpu, uint16_t caller_tag);
    bool chain_pending() const { return m_chain_pending; }
    void clear_chain_pending() { m_chain_pending = false; }

private:
    CpmAddressSpace& m_mem;
    CpmFileSystem& m_fs;
    CpmProgramLoader& m_loader;
    uint16_t m_system_tag;
    bool m_trace = false;
    bool m_last_search_host = false;
    bool m_chain_pending = false;
};

#endif // CPM8K_BDOS_H
