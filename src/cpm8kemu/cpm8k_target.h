#ifndef CPM8K_TARGET_H
#define CPM8K_TARGET_H

struct SegmentedTarget {
    using Cpu = z8001_device;
    using Memory = SegmentedMemory;
    using Bus = HostedBus<Memory>;
    using Loader = Z8001ProgramLoader;

    struct BootState {
        uint16_t psap_seg = 0;
        uint16_t psap_off = 0;
    };

    static constexpr const char* name = "z8001";
    static constexpr const char* banner =
        "\r\nHosted CPU: Z8001 segmented mode\r\n";
    static constexpr uint16_t system_tag = SEG_SYS;
    static constexpr uint32_t load_base = uint32_t(SEG_SYS) << 16;

    static void configure(Memory& mem) {
        mem.set_segment_unified(0x00, 0x10000);
        mem.set_sys_segment_unified(0x00, 0x30000);
        mem.set_segment_unified(SEG_PSA, 0x00000);
        mem.set_segment(SEG_TPA_SPLIT, 0x10000, 0x20000);
        mem.set_segment_unified(SEG_TPA, 0x10000);
        mem.set_sys_segment_unified(SEG_TPA, 0x30000);
        mem.set_segment_unified(SEG_SYS, 0x30000);
    }

    static uint16_t build_mrt(Memory& mem, uint16_t offset) {
        return ::build_mrt(mem, offset);
    }

    static uint16_t bdos_caller(Cpu& cpu, Memory&) {
        return (cpu.get_reg(4) >> 8) & 0x7f;
    }

    static bool handle_bios(Cpu& cpu, Memory& mem, bool& warm_boot) {
        uint16_t caller = (cpu.get_reg(2) >> 8) & 0x7f;
        bios_handler(cpu, mem, warm_boot, system_tag, caller);
        return true;
    }

    static void map_address(Cpu& cpu, Memory& mem) {
        uint8_t caller = (cpu.get_reg(4) >> 8) & 0x7f;
        uint16_t request = cpu.get_reg(5);
        uint8_t space = request & 0xff;
        bool true_program = request >> 8;
        uint8_t tag;

        switch (space) {
        case 0:
        case 1: tag = caller; break;
        case 2:
        case 3: tag = SEG_SYS; break;
        case 4: tag = SEG_TPA; break;
        case 5: tag = true_program ? cpu.get_reg(6) & 0x7f : SEG_TPA; break;
        default: tag = SEG_TPA; break;
        }

        if (space == 5 && true_program) {
            if (tag == SEG_TPA_SPLIT) {
                mem.set_segment(0, 0x10000, 0x20000);
                g_last_prog_split = true;
            } else {
                mem.set_segment_unified(0, 0x10000);
                g_last_prog_split = false;
            }
        }
        cpu.set_reg(6, tag);
    }

    static void prepare_boot(Memory& mem, BdosRouter& bdos, bool cold) {
        if (!cold && g_last_prog_split && bdos.chain_pending())
            mem.set_segment(0, 0x10000, 0x20000);
        else
            mem.set_segment_unified(0, 0x10000);
        bdos.clear_chain_pending();
    }

    static void init_cpu(Cpu& cpu, BootState& state, bool cold) {
        uint32_t pc = (uint32_t(SEG_SYS) << 16) |
                      (cold ? g_entry_point : g_warm_entry);
        cpu.init_state(cold ? FCW_SEG | FCW_S_N : FCW_S_N, pc,
                       cold ? SEG_PSA : state.psap_seg,
                       cold ? PSA_OFFSET : state.psap_off,
                       SEG_SYS, SYS_STACK_TOP);
    }

    static void after_run(Cpu& cpu, BootState& state) {
        state.psap_seg = cpu.get_psap_seg();
        state.psap_off = cpu.get_psap_off();
    }

    static void refresh_exit_handler(Memory& mem) {
        static const uint8_t handler[] = {0xbd, 0x50, 0x7f, 0x02, 0x7a, 0x00};
        mem.write_block((uint32_t(SEG_TPA) << 16) | 2,
                        handler, sizeof(handler));
    }

    static const char* find_system() {
        static const char* paths[] = {
            "cpm.sys", "build/bios-emu-z8001/cpm.sys",
            "src/cpm8kemu/bios-z8001/cpm.sys", nullptr
        };
        return find_existing(paths);
    }
};

struct NonSegmentedTarget {
    using Cpu = z8002_device;
    using Memory = Z8002Memory;
    using Bus = HostedBus<Memory>;
    using Loader = Z8002ProgramLoader;
    struct BootState {};

    static constexpr const char* name = "z8002";
    static constexpr const char* banner =
        "\r\nHosted CPU: Z8002 non-segmented mode\r\n";
    static constexpr uint16_t system_tag = 0;
    static constexpr uint32_t load_base = 0;

    static void configure(Memory&) {}
    static uint16_t build_mrt(Memory& mem, uint16_t offset) {
        return build_z8002_mrt(mem, offset);
    }

    static uint16_t caller_tag(Cpu& cpu, Memory& mem, int reg) {
        return (cpu.get_reg(reg) & FCW_S_N) ? 0 : mem.user_data_tag();
    }

    static uint16_t bdos_caller(Cpu& cpu, Memory& mem) {
        uint16_t tag = caller_tag(cpu, mem, 4);
        if (tag)
            cpu.set_reg(6, tag);
        return tag;
    }

    static bool handle_bios(Cpu& cpu, Memory& mem, bool& warm_boot) {
        if (cpu.get_reg(3) == 1) {
            bios_prepare_warm_boot();
            return true;
        }
        bios_handler(cpu, mem, warm_boot, 0, caller_tag(cpu, mem, 2));
        return true;
    }

    static void map_address(Cpu& cpu, Memory& mem) {
        uint16_t saved_fcw = cpu.get_reg(4);
        int16_t space = int16_t(cpu.get_reg(5));
        uint16_t tag = cpu.get_reg(6);
        switch (space) {
        case -1: mem.configure_program(tag, tag); break;
        case 0: tag = (saved_fcw & FCW_S_N) ? 0 : mem.user_data_tag(); break;
        case 1: tag = (saved_fcw & FCW_S_N) ? 0 : mem.user_code_tag(); break;
        case 2:
        case 3: tag = 0; break;
        case 4: tag = mem.user_data_tag(); break;
        case 5: tag = mem.user_code_tag(); break;
        }
        cpu.set_reg(6, tag);
    }

    static void prepare_boot(Memory&, BdosRouter& bdos, bool) {
        bdos.clear_chain_pending();
    }
    static void init_cpu(Cpu& cpu, BootState&, bool cold) {
        cpu.init_state(FCW_S_N, cold ? g_entry_point : g_warm_entry,
                       0, 0, 0, 0xff00);
    }
    static void after_run(Cpu&, BootState&) {}
    static void refresh_exit_handler(Memory&) {}

    static const char* find_system() {
        static const char* paths[] = {
            "build/bios-emu-z8002/cpm.sys", nullptr
        };
        return find_existing(paths);
    }
};

#if defined(CPM8K_SEGMENTED)
using HostedTarget = SegmentedTarget;
#elif defined(CPM8K_NONSEGMENTED)
using HostedTarget = NonSegmentedTarget;
#else
#error "Select CPM8K_SEGMENTED or CPM8K_NONSEGMENTED"
#endif

template<class Target>
class HostedIO final : public z8000_io_bus {
public:
    using Cpu = typename Target::Cpu;
    using Memory = typename Target::Memory;

    HostedIO(Cpu& cpu, Memory& mem, BdosRouter& bdos, bool& warm_boot)
        : m_cpu(cpu), m_mem(mem), m_bdos(bdos), m_warm_boot(warm_boot) {}

    uint8_t read_byte(uint16_t, int) override { return 0xff; }
    uint16_t read_word(uint16_t, int) override { return 0xffff; }
    void write_byte(uint16_t, uint8_t, int) override {}
    void write_word(uint16_t port, uint16_t, int) override {
        switch (port) {
        case PORT_BDOS: handle_bdos(); break;
        case PORT_BIOS: Target::handle_bios(m_cpu, m_mem, m_warm_boot); break;
        case PORT_MAP: Target::map_address(m_cpu, m_mem); break;
        case PORT_MEMCPY: handle_memcpy(); break;
        }
    }

private:
    void handle_bdos() {
        if (sc_trace_fp) {
            fprintf(sc_trace_fp, "BDOS %2d param=%04X:%04X\n",
                    m_cpu.get_reg(5), m_cpu.get_reg(6), m_cpu.get_reg(7));
            fflush(sc_trace_fp);
        }
        uint16_t caller = Target::bdos_caller(m_cpu, m_mem);
        m_cpu.set_reg(0, m_bdos.route(m_cpu, caller) ? 1 : 0);
    }
    void handle_memcpy() {
        uint32_t length = m_cpu.get_reg_long(2);
        uint32_t dst = m_cpu.get_reg_long(4);
        uint32_t src = m_cpu.get_reg_long(6);
        for (uint32_t i = 0; i < length && i < 0x10000; ++i)
            m_mem.write_byte(dst + i, m_mem.read_byte(src + i));
        m_cpu.set_reg_long(6, dst + length);
    }

    Cpu& m_cpu;
    Memory& m_mem;
    BdosRouter& m_bdos;
    bool& m_warm_boot;
};

template<class Target>
class HostedMachine {
public:
    using Cpu = typename Target::Cpu;
    using Memory = typename Target::Memory;
    using Bus = typename Target::Bus;
    using Loader = typename Target::Loader;

    HostedMachine(bool bdos_trace, bool cpu_trace,
                  bool reg_trace, bool mem_trace)
        : m_fs(m_mem), m_bdos(m_mem, m_fs, m_loader, Target::system_tag),
          m_i_bus(m_mem, 1), m_d_bus(m_mem, 0),
          m_io(m_cpu, m_mem, m_bdos, g_warm_boot) {
        Target::configure(m_mem);
        m_bdos.set_trace(bdos_trace);
        bios_set_trace(bdos_trace);
        m_i_bus.set_trace(mem_trace);
        m_d_bus.set_trace(mem_trace);
        m_cpu.set_program_memory(&m_i_bus);
        m_cpu.set_data_memory(&m_d_bus);
        m_cpu.set_stack_memory(&m_d_bus);
        m_cpu.set_io(&m_io);
        m_cpu.set_trace(cpu_trace);
        m_cpu.set_reg_trace(reg_trace);
        m_i_bus.set_fcw_ptr(m_cpu.get_fcw_ptr());
        m_d_bus.set_fcw_ptr(m_cpu.get_fcw_ptr());
    }

    int run(const char* system, bool bdos_trace) {
        for (int d = 0; d < NUM_DRIVES; ++d)
            if (drive_is_host(d))
                m_fs.set_drive_path(d, drive_path(d));
        for (int d = 0; d < NUM_DRIVES; ++d)
            if (drive_present(d)) { m_fs.set_default_drive(d); break; }

        if (bdos_trace)
            sc_trace_fp = fopen("sc_trace.log", "w");
        m_cpu.set_trap_callback([](void* ctx, uint8_t sc, uint32_t pc) {
            FILE* fp = static_cast<FILE*>(ctx);
            if (fp) {
                fprintf(fp, "SC #%d from PC=%08X\n", sc, unsigned(pc));
                fflush(fp);
            }
            return true;
        }, sc_trace_fp);

        if (load_coff(m_mem, system, Target::load_base) < 0)
            return 1;
        uint16_t mrt = (g_ccp_size + 0xff) & ~0xff;
        g_mrt_offset = Target::build_mrt(m_mem, mrt);
        bios_init_disks(m_mem, Target::system_tag, mrt + 256);

        console_init();
        for (const char* p = Target::banner; *p; ++p)
            console_out(*p);
        bool cold = true;
        typename Target::BootState boot_state;
        do {
            g_warm_boot = false;
            Target::prepare_boot(m_mem, m_bdos, cold);
            Target::init_cpu(m_cpu, boot_state, cold);
            Target::refresh_exit_handler(m_mem);
            cold = false;
            while (!m_cpu.is_halted())
                m_cpu.run(-1);
            Target::after_run(m_cpu, boot_state);
        } while (g_warm_boot);

        bios_cleanup_disks();
        console_restore();
        return 0;
    }

private:
    Memory m_mem;
    CpmFileSystem m_fs;
    Loader m_loader;
    BdosRouter m_bdos;
    Bus m_i_bus;
    Bus m_d_bus;
    Cpu m_cpu;
    HostedIO<Target> m_io;
};

#endif
