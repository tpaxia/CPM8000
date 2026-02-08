#ifndef CPM8K_MEM_H
#define CPM8K_MEM_H

#include "z8000_intf.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

// Total physical memory: 512KB
static constexpr size_t MEM_SIZE = 512 * 1024;

// Segment → physical offset mapping (M20-like)
// Segment 0x02 → 0x00000 (64K) - PSA
// Segment 0x08 → 0x10000 (64K) - TPA split I/D
// Segment 0x0A → 0x20000 (64K) - TPA merged I/D
// Segment 0x0B → 0x30000 (64K) - System (CCP + BIOS data)
static constexpr int NUM_SEGS = 128;

// Segmented memory bus: translates Z8001 segmented addresses
// (seg << 16 | offset) to a flat 512K array via a lookup table.
class SegmentedMemory : public z8000_memory_bus {
public:
    SegmentedMemory() : m_trace(false) {
        m_data = new uint8_t[MEM_SIZE];
        memset(m_data, 0, MEM_SIZE);
        // Initialize segment map: all unmapped (0xFFFFFFFF)
        for (int i = 0; i < NUM_SEGS; i++)
            m_seg_map[i] = 0xFFFFFFFF;
        // Default M20-like mapping
        m_seg_map[0x00] = 0x20000; // Seg 0: non-segmented access → TPA
        m_seg_map[0x02] = 0x00000;
        m_seg_map[0x08] = 0x10000;
        m_seg_map[0x0A] = 0x20000;
        m_seg_map[0x0B] = 0x30000;
    }

    ~SegmentedMemory() { delete[] m_data; }

    void set_trace(bool enable) { m_trace = enable; }

    void set_segment(int seg, uint32_t phys_base) {
        if (seg >= 0 && seg < NUM_SEGS)
            m_seg_map[seg] = phys_base;
    }

    // Translate segmented address to physical offset
    // Z8001 addresses: bits 22..16 = segment number, bits 15..0 = offset
    uint32_t translate(uint32_t addr) const {
        uint8_t seg = (addr >> 16) & 0x7F;
        uint16_t off = addr & 0xFFFF;
        uint32_t base = m_seg_map[seg];
        if (base == 0xFFFFFFFF) {
            // Unmapped segment - try segment 0x0B as fallback for non-seg code
            base = m_seg_map[0x0B];
            if (base == 0xFFFFFFFF) return 0;
        }
        return (base + off) & (MEM_SIZE - 1);
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

    // z8000_memory_bus interface
    uint8_t read_byte(uint32_t addr) override {
        uint32_t phys = translate(addr);
        uint8_t val = m_data[phys];
        if (m_trace)
            fprintf(stderr, "  MEM RD8  [%02X:%04X phys=%05X] -> %02X\n",
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        return val;
    }

    uint16_t read_word(uint32_t addr) override {
        uint32_t phys = translate(addr) & ~1;
        uint16_t val = (uint16_t(m_data[phys]) << 8) | m_data[phys + 1];
        if (m_trace)
            fprintf(stderr, "  MEM RD16 [%02X:%04X phys=%05X] -> %04X\n",
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        return val;
    }

    void write_byte(uint32_t addr, uint8_t val) override {
        uint32_t phys = translate(addr);
        if (m_trace)
            fprintf(stderr, "  MEM WR8  [%02X:%04X phys=%05X] <- %02X\n",
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        m_data[phys] = val;
    }

    void write_word(uint32_t addr, uint16_t val) override {
        uint32_t phys = translate(addr) & ~1;
        if (m_trace)
            fprintf(stderr, "  MEM WR16 [%02X:%04X phys=%05X] <- %04X\n",
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, val);
        m_data[phys] = (val >> 8) & 0xFF;
        m_data[phys + 1] = val & 0xFF;
    }

    void write_word(uint32_t addr, uint16_t val, uint16_t mask) override {
        uint32_t phys = translate(addr) & ~1;
        uint16_t existing = (uint16_t(m_data[phys]) << 8) | m_data[phys + 1];
        uint16_t new_val = (existing & ~mask) | (val & mask);
        if (m_trace)
            fprintf(stderr, "  MEM WR16 [%02X:%04X phys=%05X] <- %04X (mask %04X)\n",
                    (addr >> 16) & 0x7F, addr & 0xFFFF, phys, new_val, mask);
        m_data[phys] = (new_val >> 8) & 0xFF;
        m_data[phys + 1] = new_val & 0xFF;
    }

private:
    uint8_t* m_data;
    uint32_t m_seg_map[NUM_SEGS];
    bool m_trace;
};

#endif // CPM8K_MEM_H
