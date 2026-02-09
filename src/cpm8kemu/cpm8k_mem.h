#ifndef CPM8K_MEM_H
#define CPM8K_MEM_H

#include "z8000_intf.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Total physical memory: 512KB
static constexpr size_t MEM_SIZE = 512 * 1024;

// Number of Z8001 segments (7-bit segment number)
static constexpr int NUM_SEGS = 128;

// FCW flags (from z8000cpu.h)
static constexpr uint16_t FCW_SEG = 0x8000;
static constexpr uint16_t FCW_S_N = 0x4000;

// Segmented memory with separate instruction/data address spaces.
// Models a Z8001 MMU with dual mapping tables (I-space and D-space)
// per segment, plus separate system/normal mode tables for segment 0
// (matching real Z8001 MMU behavior).
// Not a z8000_memory_bus — SegBus adapters provide the bus interface.
class SegmentedMemory {
public:
    SegmentedMemory() {
        m_data = new uint8_t[MEM_SIZE];
        memset(m_data, 0, MEM_SIZE);
        for (int i = 0; i < NUM_SEGS; i++) {
            m_imap[i] = 0xFFFFFFFF;
            m_dmap[i] = 0xFFFFFFFF;
            m_sys_imap[i] = 0xFFFFFFFF;
            m_sys_dmap[i] = 0xFFFFFFFF;
        }
    }

    ~SegmentedMemory() { delete[] m_data; }

    // Configure segment with separate I-space and D-space physical bases
    void set_segment(int seg, uint32_t i_phys, uint32_t d_phys) {
        if (seg >= 0 && seg < NUM_SEGS) {
            m_imap[seg] = i_phys;
            m_dmap[seg] = d_phys;
        }
    }

    // Configure segment with unified I/D mapping
    void set_segment_unified(int seg, uint32_t phys) {
        set_segment(seg, phys, phys);
    }

    // Configure system-mode segment override.
    // When the CPU is in system mode, these mappings take priority.
    // This models the M20 BIOS behavior where system code and user code
    // use different segment-to-physical mappings.
    void set_sys_segment(int seg, uint32_t i_phys, uint32_t d_phys) {
        if (seg >= 0 && seg < NUM_SEGS) {
            m_sys_imap[seg] = i_phys;
            m_sys_dmap[seg] = d_phys;
        }
    }

    void set_sys_segment_unified(int seg, uint32_t phys) {
        set_sys_segment(seg, phys, phys);
    }

    // Query segment mappings
    uint32_t get_imap(int seg) const {
        return (seg >= 0 && seg < NUM_SEGS) ? m_imap[seg] : 0xFFFFFFFF;
    }
    uint32_t get_dmap(int seg) const {
        return (seg >= 0 && seg < NUM_SEGS) ? m_dmap[seg] : 0xFFFFFFFF;
    }

    // Translate segmented address to physical offset.
    // space: 0=data/stack, 1=instruction fetch
    // system_mode: true if CPU is in system mode (FCW.S/N set)
    uint32_t translate(uint32_t addr, int space = 0, bool system_mode = false) const {
        uint8_t seg = (addr >> 16) & 0x7F;
        uint16_t off = addr & 0xFFFF;
        uint32_t base = 0xFFFFFFFF;
        // In system mode, check system-mode overrides first
        if (system_mode) {
            base = space ? m_sys_imap[seg] : m_sys_dmap[seg];
        }
        // Fall back to normal-mode mapping
        if (base == 0xFFFFFFFF) {
            base = space ? m_imap[seg] : m_dmap[seg];
        }
        // Unmapped segments fall back to segment 0x0B (system)
        if (base == 0xFFFFFFFF) {
            base = space ? m_imap[0x0B] : m_dmap[0x0B];
            if (base == 0xFFFFFFFF) return 0;
        }
        return (base + off) & (MEM_SIZE - 1);
    }

    // Convenience helpers — default space=0 (data) keeps all existing callers
    // (BDOS, file system) working unchanged.
    uint8_t read_byte(uint32_t addr, int space = 0) const {
        return m_data[translate(addr, space)];
    }
    uint16_t read_word(uint32_t addr, int space = 0) const {
        uint32_t phys = translate(addr, space) & ~1;
        return (uint16_t(m_data[phys]) << 8) | m_data[phys + 1];
    }
    void write_byte(uint32_t addr, uint8_t val, int space = 0) {
        m_data[translate(addr, space)] = val;
    }
    void write_word(uint32_t addr, uint16_t val, int space = 0) {
        uint32_t phys = translate(addr, space) & ~1;
        m_data[phys] = (val >> 8) & 0xFF;
        m_data[phys + 1] = val & 0xFF;
    }

    // Direct access to physical memory
    uint8_t* data() { return m_data; }
    const uint8_t* data() const { return m_data; }
    size_t size() const { return MEM_SIZE; }

    // Load binary data at physical address
    bool load_physical(uint32_t phys_addr, const uint8_t* data, size_t len) {
        if (phys_addr + len > MEM_SIZE) return false;
        memcpy(&m_data[phys_addr], data, len);
        return true;
    }

    // Read/write at physical address (no translation)
    uint8_t read_phys_byte(uint32_t addr) const {
        return m_data[addr & (MEM_SIZE - 1)];
    }
    uint16_t read_phys_word(uint32_t addr) const {
        addr &= (MEM_SIZE - 1) & ~1;
        return (uint16_t(m_data[addr]) << 8) | m_data[addr + 1];
    }
    void write_phys_byte(uint32_t addr, uint8_t val) {
        m_data[addr & (MEM_SIZE - 1)] = val;
    }
    void write_phys_word(uint32_t addr, uint16_t val) {
        addr &= (MEM_SIZE - 1) & ~1;
        m_data[addr] = (val >> 8) & 0xFF;
        m_data[addr + 1] = val & 0xFF;
    }

private:
    uint8_t* m_data;
    uint32_t m_imap[NUM_SEGS]; // instruction-space segment map (normal mode)
    uint32_t m_dmap[NUM_SEGS]; // data-space segment map (normal mode)
    uint32_t m_sys_imap[NUM_SEGS]; // system-mode I-space overrides
    uint32_t m_sys_dmap[NUM_SEGS]; // system-mode D-space overrides
};

// Bus adapter: routes Z8001 bus accesses through SegmentedMemory
// with a fixed address space (instruction or data).
// Queries CPU FCW to determine system/normal mode for segment 0.
class SegBus : public z8000_memory_bus {
public:
    SegBus(SegmentedMemory& mem, int space)
        : m_mem(mem), m_space(space), m_fcw(nullptr), m_trace(false) {}

    // Set pointer to CPU's FCW register for mode-aware segment 0
    void set_fcw_ptr(const uint16_t* fcw) { m_fcw = fcw; }

    void set_trace(bool enable) { m_trace = enable; }

    uint8_t read_byte(uint32_t addr) override {
        uint32_t phys = m_mem.translate(addr, m_space, is_system_mode());
        uint8_t val = m_mem.data()[phys];
        if (m_trace)
            fprintf(stderr, "  %cBUS RD8  [%02X:%04X phys=%05X] -> %02X\n",
                    m_space ? 'I' : 'D',
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        return val;
    }

    uint16_t read_word(uint32_t addr) override {
        uint32_t phys = m_mem.translate(addr, m_space, is_system_mode()) & ~1;
        uint16_t val = (uint16_t(m_mem.data()[phys]) << 8) | m_mem.data()[phys + 1];
        if (m_trace)
            fprintf(stderr, "  %cBUS RD16 [%02X:%04X phys=%05X] -> %04X\n",
                    m_space ? 'I' : 'D',
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        return val;
    }

    void write_byte(uint32_t addr, uint8_t val) override {
        uint32_t phys = m_mem.translate(addr, m_space, is_system_mode());
        if (m_trace)
            fprintf(stderr, "  %cBUS WR8  [%02X:%04X phys=%05X] <- %02X\n",
                    m_space ? 'I' : 'D',
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        m_mem.data()[phys] = val;
    }

    void write_word(uint32_t addr, uint16_t val) override {
        uint32_t phys = m_mem.translate(addr, m_space, is_system_mode()) & ~1;
        if (m_trace)
            fprintf(stderr, "  %cBUS WR16 [%02X:%04X phys=%05X] <- %04X\n",
                    m_space ? 'I' : 'D',
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        m_mem.data()[phys] = (val >> 8) & 0xFF;
        m_mem.data()[phys + 1] = val & 0xFF;
    }

    void write_word(uint32_t addr, uint16_t val, uint16_t mask) override {
        uint32_t phys = m_mem.translate(addr, m_space, is_system_mode()) & ~1;
        uint16_t existing = (uint16_t(m_mem.data()[phys]) << 8) | m_mem.data()[phys + 1];
        uint16_t new_val = (existing & ~mask) | (val & mask);
        if (m_trace)
            fprintf(stderr, "  %cBUS WR16 [%02X:%04X phys=%05X] <- %04X (mask %04X)\n",
                    m_space ? 'I' : 'D',
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, new_val, mask);
        m_mem.data()[phys] = (new_val >> 8) & 0xFF;
        m_mem.data()[phys + 1] = new_val & 0xFF;
    }

private:
    bool is_system_mode() const {
        return m_fcw && (*m_fcw & FCW_S_N);
    }

    SegmentedMemory& m_mem;
    int m_space; // 0=data, 1=instruction
    const uint16_t* m_fcw; // pointer to CPU's FCW register
    bool m_trace;
};

#endif // CPM8K_MEM_H
