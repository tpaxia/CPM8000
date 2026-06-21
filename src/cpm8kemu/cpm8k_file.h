#ifndef CPM8K_FILE_H
#define CPM8K_FILE_H

#include "cpm8k_mem.h"
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <dirent.h>

// Maximum open files (CP/M allows up to ~8 concurrent FCBs)
static constexpr int MAX_OPEN_FILES = 16;
static constexpr int MAX_DRIVES = 16;

// CP/M FCB structure offsets (36 bytes total)
static constexpr int FCB_DR   = 0;   // Drive code (0=default, 1=A, 2=B...)
static constexpr int FCB_F1   = 1;   // Filename (8 bytes, space-padded)
static constexpr int FCB_T1   = 9;   // Type/extension (3 bytes, space-padded)
static constexpr int FCB_EX   = 12;  // Extent counter (low)
static constexpr int FCB_S1   = 13;  // Reserved
static constexpr int FCB_S2   = 14;  // Extent counter (high)
static constexpr int FCB_RC   = 15;  // Record count in current extent
static constexpr int FCB_AL   = 16;  // Allocation map (16 bytes)
static constexpr int FCB_CR   = 32;  // Current record
static constexpr int FCB_R0   = 33;  // Random record (MSB, unlike CP/M-80)
static constexpr int FCB_R1   = 34;  // Random record (mid byte)
static constexpr int FCB_R2   = 35;  // Random record (LSB)
static constexpr int FCB_SIZE = 36;

// Directory entry size
static constexpr int DIR_ENTRY_SIZE = 32;

struct OpenFile {
    FILE* fp;
    uint16_t fcb_addr;    // FCB address in Z8001 memory (offset only)
    uint8_t  fcb_seg;     // FCB segment
    bool     active;
    std::string host_path;
};

struct SearchState {
    DIR* dirp;
    std::string dir_path;
    uint8_t search_drive;
    uint8_t search_name[11]; // FCB pattern with ? wildcards
    bool active;
    int entry_index;         // CP/M directory entry counter
};

class CpmFileSystem {
public:
    CpmFileSystem(SegmentedMemory& mem);
    ~CpmFileSystem();

    // Set host directory for a drive (0=A, 1=B, 2=C, 3=D)
    void set_drive_path(int drive, const std::string& path);

    // Get host directory for a drive
    const std::string& get_drive_path(int drive) const;

    // DMA address management
    void set_dma(uint32_t addr) { m_dma_addr = addr; }
    uint32_t get_dma() const { return m_dma_addr; }

    // Set the caller's segment (extracted from PC when SC is intercepted)
    void set_caller_seg(uint8_t seg) { m_caller_seg = seg; }

    // Current/default drive
    void set_current_drive(int drive) { m_current_drive = drive; }
    int get_current_drive() const { return m_current_drive; }

    // User number
    void set_user(int user) { m_user = user; }
    int get_user() const { return m_user; }

    // Get the host FILE* for an open FCB (for PGMLD)
    FILE* get_open_fp(uint16_t fcb_addr);

    // File operations (return CP/M error codes)
    int file_open(uint16_t fcb_addr);       // Func 15
    int file_close(uint16_t fcb_addr);      // Func 16
    int file_search_first(uint16_t fcb_addr); // Func 17
    int file_search_next();                  // Func 18
    int file_delete(uint16_t fcb_addr);     // Func 19
    int file_read_seq(uint16_t fcb_addr);   // Func 20
    int file_write_seq(uint16_t fcb_addr);  // Func 21
    int file_make(uint16_t fcb_addr);       // Func 22
    int file_rename(uint16_t fcb_addr);     // Func 23
    int file_read_rand(uint16_t fcb_addr);  // Func 33
    int file_write_rand(uint16_t fcb_addr); // Func 34
    int file_size(uint16_t fcb_addr);       // Func 35
    int file_set_random(uint16_t fcb_addr); // Func 36
    int file_write_rand_zf(uint16_t fcb_addr); // Func 40

    // Drive operations
    uint16_t get_login_vector();             // Func 24
    uint16_t get_rovec();                    // Func 29
    void set_drive_ro(int drive);            // Func 28
    void reset_all_drives();                 // Func 13
    void reset_drives(uint16_t mask);        // Func 37
    void close_all_files();                  // Close all open files (cold boot)
    void close_user_files(uint8_t sys_seg);  // Close files not in sys_seg (warm boot)

private:
    SegmentedMemory& m_mem;
    std::string m_drive_paths[MAX_DRIVES];
    int m_current_drive;
    int m_user;
    uint32_t m_dma_addr; // segmented address for DMA
    uint8_t m_caller_seg; // caller's segment (from PC when SC intercepted)
    uint16_t m_ro_vec;   // read-only drive vector (bit per drive)

    OpenFile m_files[MAX_OPEN_FILES];
    SearchState m_search;

    // Helper methods
    std::string fcb_to_host_path(uint16_t fcb_addr);
    int resolve_drive(uint8_t fcb_drive);
    void read_fcb_name(uint16_t fcb_addr, uint8_t name[11]);
    bool match_pattern(const uint8_t pattern[11], const uint8_t name[11]);
    void filename_to_fcb(const char* host_name, uint8_t* entry);
    int find_open_slot();
    OpenFile* find_file_by_fcb(uint16_t fcb_addr);
    OpenFile* ensure_open(uint16_t fcb_addr);
    void close_stale(uint16_t fcb_addr);
    void close_search();
    long compute_file_offset(uint16_t fcb_addr);
    long compute_random_offset(uint16_t fcb_addr);
    void update_fcb_after_read(uint16_t fcb_addr, long offset);

    // Memory helpers - read/write using caller's segment (for FCB access)
    uint8_t mem_read(uint16_t addr);
    void mem_write(uint16_t addr, uint8_t val);
    void mem_read_block(uint16_t addr, uint8_t* buf, int len);
    void mem_write_block(uint16_t addr, const uint8_t* buf, int len);
    uint16_t mem_read_word(uint16_t addr);

    // DMA buffer helpers - read/write using segment from m_dma_addr
    uint8_t dma_read(uint16_t offset);
    void dma_write(uint16_t offset, uint8_t val);
    void dma_read_block(uint16_t offset, uint8_t* buf, int len);
    void dma_write_block(uint16_t offset, const uint8_t* buf, int len);
};

#endif // CPM8K_FILE_H
