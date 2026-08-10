#ifndef CPM8K_Z8002_H
#define CPM8K_Z8002_H

#include "cpm8k_mem.h"
#include <array>

class Z8002Memory final : public CpmAddressSpace {
public:
    Z8002Memory() { m_data.fill(0); }

    uint8_t read_byte(uint32_t addr, int = 0) const override {
        return m_data[tagged_physical(addr)];
    }
    uint16_t read_word(uint32_t addr, int = 0) const override {
        uint32_t p = tagged_physical(addr) & ~1u;
        return (uint16_t(m_data[p]) << 8) | m_data[p + 1];
    }
    void write_byte(uint32_t addr, uint8_t val, int = 0) override {
        m_data[tagged_physical(addr)] = val;
    }
    void write_word(uint32_t addr, uint16_t val, int = 0) override {
        uint32_t p = tagged_physical(addr) & ~1u;
        m_data[p] = val >> 8;
        m_data[p + 1] = val;
    }

    uint8_t cpu_read_byte(uint32_t addr, int space, bool system) const {
        return m_data[cpu_physical(addr, space, system)];
    }
    uint16_t cpu_read_word(uint32_t addr, int space, bool system) const {
        uint32_t p = cpu_physical(addr, space, system) & ~1u;
        return (uint16_t(m_data[p]) << 8) | m_data[p + 1];
    }
    void cpu_write_byte(uint32_t addr, uint8_t val, int space, bool system) {
        m_data[cpu_physical(addr, space, system)] = val;
    }
    void cpu_write_word(uint32_t addr, uint16_t val, uint16_t mask,
                        int space, bool system) {
        uint32_t p = cpu_physical(addr, space, system) & ~1u;
        uint16_t old = (uint16_t(m_data[p]) << 8) | m_data[p + 1];
        uint16_t next = (old & ~mask) | (val & mask);
        m_data[p] = next >> 8;
        m_data[p + 1] = next;
    }

    void set_user_code_tag(uint16_t tag) { m_user_code_bank = bank(tag); }
    void set_user_data_tag(uint16_t tag) { m_user_data_bank = bank(tag); }
    void configure_program(uint16_t code_tag, uint16_t data_tag) override {
        set_user_code_tag(code_tag);
        set_user_data_tag(data_tag);
    }
    uint16_t user_code_tag() const { return uint16_t(m_user_code_bank) << 8; }
    uint16_t user_data_tag() const { return uint16_t(m_user_data_bank) << 8; }

private:
    static uint8_t bank(uint16_t tag) { return (tag >> 8) & 7; }
    static uint32_t tagged_physical(uint32_t addr) {
        return (uint32_t(bank(addr >> 16)) << 16) | (addr & 0xFFFF);
    }
    uint32_t cpu_physical(uint32_t addr, int space, bool system) const {
        uint8_t b = system ? 0 : (space ? m_user_code_bank : m_user_data_bank);
        return (uint32_t(b) << 16) | (addr & 0xFFFF);
    }

    std::array<uint8_t, MEM_SIZE> m_data;
    uint8_t m_user_code_bank = 1;
    uint8_t m_user_data_bank = 1;
};

class Z8002Bus final : public z8000_memory_bus {
public:
    Z8002Bus(Z8002Memory& mem, int space) : m_mem(mem), m_space(space) {}
    void set_fcw_ptr(const uint16_t* fcw) { m_fcw = fcw; }
    void set_trace(bool enable) { m_trace = enable; }

    uint8_t read_byte(uint32_t addr) override {
        uint8_t val = m_mem.cpu_read_byte(addr, m_space, system());
        trace("RD8 ", addr, val);
        return val;
    }
    uint16_t read_word(uint32_t addr) override {
        uint16_t val = m_mem.cpu_read_word(addr, m_space, system());
        trace("RD16", addr, val);
        return val;
    }
    void write_byte(uint32_t addr, uint8_t val) override {
        trace("WR8 ", addr, val);
        m_mem.cpu_write_byte(addr, val, m_space, system());
    }
    void write_word(uint32_t addr, uint16_t val) override {
        write_word(addr, val, 0xFFFF);
    }
    void write_word(uint32_t addr, uint16_t val, uint16_t mask) override {
        trace("WR16", addr, val);
        m_mem.cpu_write_word(addr, val, mask, m_space, system());
    }

private:
    bool system() const { return m_fcw && (*m_fcw & FCW_S_N); }
    void trace(const char* op, uint32_t addr, uint16_t val) const {
        if (m_trace)
            fprintf(stderr, "  Z2%c %s [%04X] %04X\n",
                    m_space ? 'I' : 'D', op, unsigned(addr & 0xFFFF), val);
    }

    Z8002Memory& m_mem;
    int m_space;
    const uint16_t* m_fcw = nullptr;
    bool m_trace = false;
};

#endif
