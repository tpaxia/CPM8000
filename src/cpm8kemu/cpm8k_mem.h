#ifndef CPM8K_MEM_H
#define CPM8K_MEM_H

#include "z8000_intf.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

static constexpr size_t MEM_SIZE = 512 * 1024;
static constexpr int NUM_SEGS = 128;
static constexpr uint16_t FCW_SEG = 0x8000;
static constexpr uint16_t FCW_S_N = 0x4000;

class CpmAddressSpace {
public:
    virtual ~CpmAddressSpace() = default;
    virtual uint8_t read_byte(uint32_t addr, int space = 0) const = 0;
    virtual uint16_t read_word(uint32_t addr, int space = 0) const = 0;
    virtual void write_byte(uint32_t addr, uint8_t val, int space = 0) = 0;
    virtual void write_word(uint32_t addr, uint16_t val, int space = 0) = 0;
    virtual void configure_program(uint16_t, uint16_t) {}

    void read_block(uint32_t addr, uint8_t* dst, size_t len) const {
        for (size_t i = 0; i < len; ++i)
            dst[i] = read_byte(addr + i);
    }
    void write_block(uint32_t addr, const uint8_t* src, size_t len) {
        for (size_t i = 0; i < len; ++i)
            write_byte(addr + i, src[i]);
    }
    void clear_block(uint32_t addr, size_t len) {
        for (size_t i = 0; i < len; ++i)
            write_byte(addr + i, 0);
    }
};

// Both hosted targets use the same physical RAM and bus implementation.  The
// target-specific memory class supplies only logical-to-physical translation.
class HostedMemory : public CpmAddressSpace {
public:
    HostedMemory() { m_data.fill(0); }

    uint8_t read_byte(uint32_t addr, int space = 0) const override {
        return m_data[tagged_physical(addr, space)];
    }
    uint16_t read_word(uint32_t addr, int space = 0) const override {
        uint32_t p = tagged_physical(addr, space) & ~1u;
        return (uint16_t(m_data[p]) << 8) | m_data[p + 1];
    }
    void write_byte(uint32_t addr, uint8_t val, int space = 0) override {
        m_data[tagged_physical(addr, space)] = val;
    }
    void write_word(uint32_t addr, uint16_t val, int space = 0) override {
        uint32_t p = tagged_physical(addr, space) & ~1u;
        m_data[p] = val >> 8;
        m_data[p + 1] = val;
    }

    uint8_t* data() { return m_data.data(); }
    const uint8_t* data() const { return m_data.data(); }
    size_t size() const { return m_data.size(); }

    bool load_physical(uint32_t addr, const uint8_t* src, size_t len) {
        if (addr + len > m_data.size())
            return false;
        memcpy(m_data.data() + addr, src, len);
        return true;
    }

    uint8_t read_phys_byte(uint32_t addr) const {
        return m_data[addr & (MEM_SIZE - 1)];
    }
    uint16_t read_phys_word(uint32_t addr) const {
        addr &= (MEM_SIZE - 1) & ~1u;
        return (uint16_t(m_data[addr]) << 8) | m_data[addr + 1];
    }
    void write_phys_byte(uint32_t addr, uint8_t val) {
        m_data[addr & (MEM_SIZE - 1)] = val;
    }
    void write_phys_word(uint32_t addr, uint16_t val) {
        addr &= (MEM_SIZE - 1) & ~1u;
        m_data[addr] = val >> 8;
        m_data[addr + 1] = val;
    }

    virtual uint32_t cpu_physical(uint32_t addr, int space,
                                  bool system) const = 0;

protected:
    virtual uint32_t tagged_physical(uint32_t addr, int space) const = 0;

private:
    std::array<uint8_t, MEM_SIZE> m_data;
};

class SegmentedMemory final : public HostedMemory {
public:
    SegmentedMemory() {
        m_imap.fill(UNMAPPED);
        m_dmap.fill(UNMAPPED);
        m_sys_imap.fill(UNMAPPED);
        m_sys_dmap.fill(UNMAPPED);
    }

    void set_segment(int seg, uint32_t i_phys, uint32_t d_phys) {
        if (seg >= 0 && seg < NUM_SEGS) {
            m_imap[seg] = i_phys;
            m_dmap[seg] = d_phys;
        }
    }
    void set_segment_unified(int seg, uint32_t phys) {
        set_segment(seg, phys, phys);
    }
    void set_sys_segment(int seg, uint32_t i_phys, uint32_t d_phys) {
        if (seg >= 0 && seg < NUM_SEGS) {
            m_sys_imap[seg] = i_phys;
            m_sys_dmap[seg] = d_phys;
        }
    }
    void set_sys_segment_unified(int seg, uint32_t phys) {
        set_sys_segment(seg, phys, phys);
    }
    uint32_t get_imap(int seg) const {
        return seg >= 0 && seg < NUM_SEGS ? m_imap[seg] : UNMAPPED;
    }
    uint32_t get_dmap(int seg) const {
        return seg >= 0 && seg < NUM_SEGS ? m_dmap[seg] : UNMAPPED;
    }

    uint32_t translate(uint32_t addr, int space = 0,
                       bool system = false) const {
        uint8_t seg = (addr >> 16) & 0x7f;
        uint32_t base = UNMAPPED;
        if (system)
            base = space ? m_sys_imap[seg] : m_sys_dmap[seg];
        if (base == UNMAPPED)
            base = space ? m_imap[seg] : m_dmap[seg];
        if (base == UNMAPPED)
            base = space ? m_imap[0x0b] : m_dmap[0x0b];
        if (base == UNMAPPED)
            return 0;
        return (base + (addr & 0xffff)) & (MEM_SIZE - 1);
    }

    uint32_t cpu_physical(uint32_t addr, int space,
                          bool system) const override {
        return translate(addr, space, system);
    }

protected:
    uint32_t tagged_physical(uint32_t addr, int space) const override {
        return translate(addr, space, false);
    }

private:
    static constexpr uint32_t UNMAPPED = 0xffffffff;
    std::array<uint32_t, NUM_SEGS> m_imap;
    std::array<uint32_t, NUM_SEGS> m_dmap;
    std::array<uint32_t, NUM_SEGS> m_sys_imap;
    std::array<uint32_t, NUM_SEGS> m_sys_dmap;
};

class Z8002Memory final : public HostedMemory {
public:
    void configure_program(uint16_t code_tag, uint16_t data_tag) override {
        set_user_code_tag(code_tag);
        set_user_data_tag(data_tag);
    }
    void set_user_code_tag(uint16_t tag) { m_user_code_bank = bank(tag); }
    void set_user_data_tag(uint16_t tag) { m_user_data_bank = bank(tag); }
    uint16_t user_code_tag() const { return uint16_t(m_user_code_bank) << 8; }
    uint16_t user_data_tag() const { return uint16_t(m_user_data_bank) << 8; }

    uint32_t cpu_physical(uint32_t addr, int space,
                          bool system) const override {
        uint8_t b = system ? 0 : (space ? m_user_code_bank : m_user_data_bank);
        return (uint32_t(b) << 16) | (addr & 0xffff);
    }

protected:
    uint32_t tagged_physical(uint32_t addr, int) const override {
        return (uint32_t(bank(addr >> 16)) << 16) | (addr & 0xffff);
    }

private:
    static uint8_t bank(uint16_t tag) { return (tag >> 8) & 7; }
    uint8_t m_user_code_bank = 1;
    uint8_t m_user_data_bank = 1;
};

template<class Memory>
class HostedBus final : public z8000_memory_bus {
public:
    HostedBus(Memory& mem, int space) : m_mem(mem), m_space(space) {}

    void set_fcw_ptr(const uint16_t* fcw) { m_fcw = fcw; }
    void set_trace(bool enable) { m_trace = enable; }

    uint8_t read_byte(uint32_t addr) override {
        uint32_t p = physical(addr);
        uint8_t val = m_mem.read_phys_byte(p);
        trace("RD8 ", addr, p, val);
        return val;
    }
    uint16_t read_word(uint32_t addr) override {
        uint32_t p = physical(addr) & ~1u;
        uint16_t val = m_mem.read_phys_word(p);
        trace("RD16", addr, p, val);
        return val;
    }
    void write_byte(uint32_t addr, uint8_t val) override {
        uint32_t p = physical(addr);
        trace("WR8 ", addr, p, val);
        m_mem.write_phys_byte(p, val);
    }
    void write_word(uint32_t addr, uint16_t val) override {
        write_word(addr, val, 0xffff);
    }
    void write_word(uint32_t addr, uint16_t val, uint16_t mask) override {
        uint32_t p = physical(addr) & ~1u;
        uint16_t old = m_mem.read_phys_word(p);
        uint16_t next = (old & ~mask) | (val & mask);
        trace("WR16", addr, p, next);
        m_mem.write_phys_word(p, next);
    }

private:
    uint32_t physical(uint32_t addr) const {
        return m_mem.cpu_physical(addr, m_space,
                                  m_fcw && (*m_fcw & FCW_S_N));
    }
    void trace(const char* op, uint32_t addr, uint32_t phys,
               uint16_t val) const {
        if (m_trace)
            fprintf(stderr, "  %cBUS %s [%08X phys=%05X] %04X\n",
                    m_space ? 'I' : 'D', op, unsigned(addr), unsigned(phys),
                    unsigned(val));
    }

    Memory& m_mem;
    int m_space;
    const uint16_t* m_fcw = nullptr;
    bool m_trace = false;
};

using SegBus = HostedBus<SegmentedMemory>;
using Z8002Bus = HostedBus<Z8002Memory>;

#endif
