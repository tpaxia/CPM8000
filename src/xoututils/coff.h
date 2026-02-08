/*
 * coff.h
 *
 * Z8000 COFF (Common Object File Format) structures and constants.
 * Ported from Go binlib/coff.go by 4sun5bu.
 * Original project: https://github.com/4sun5bu/xoututils (MIT license)
 *
 * Copyright (c) 2025, Salvatore Paxia
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef COFF_H
#define COFF_H

#include <stdint.h>
#include <string.h>

/* Lengths */
#define COFF_HDR_LEN          20
#define COFF_SECT_HDR_LEN     40
#define COFF_RELOC_ITEM_LEN   16
#define COFF_SYMB_ENTRY_LEN   18
#define COFF_NAME_LEN          8

/* Section flags */
#define COFF_SECT_TEXT  0x0020
#define COFF_SECT_DATA  0x0040
#define COFF_SECT_BSS   0x0080

/* Symbol storage classes */
#define COFF_SYMB_CLASS_AUTO      0x01
#define COFF_SYMB_CLASS_GLOBAL    0x02
#define COFF_SYMB_CLASS_STATIC    0x03
#define COFF_SYMB_CLASS_EXTERNAL  0x05
#define COFF_SYMB_CLASS_LABEL     0x06
#define COFF_SYMB_CLASS_FILE      0x67

/* Symbol section numbers (special) */
#define COFF_SYMB_SCN_EXT   0
#define COFF_SYMB_SCN_ABS  (-1)

/* ---- In-memory structures ---- */

typedef struct {
    uint16_t magic;
    uint16_t num_sects;
    uint32_t date;
    int32_t  symb_tbl_fpos;
    uint32_t num_symbs;
    uint16_t opt_hdr_len;
    uint16_t flags;
} coff_hdr_t;

typedef struct {
    char     name[COFF_NAME_LEN];
    uint32_t paddr;
    uint32_t vaddr;
    uint32_t length;
    int32_t  fpos;
    int32_t  reloc_tbl_fpos;
    int32_t  line_nums_fpos;
    uint16_t num_relocs;
    uint16_t num_lines;
    uint32_t flags;
} coff_sect_hdr_t;

typedef struct {
    uint32_t vaddr;
    uint32_t symb_idx;
    uint32_t offset;
    uint16_t type;
    uint16_t stuff;
} coff_reloc_item_t;

/* Symbol entry types (discriminated union) */
typedef enum {
    COFF_SYMB_TYPE_ENTRY,
    COFF_SYMB_TYPE_AUX_SECT,
    COFF_SYMB_TYPE_AUX_FILE
} coff_symb_kind_t;

typedef struct {
    char     name[COFF_NAME_LEN];
    uint32_t value;
    int16_t  sect_no;
    uint16_t type;
    uint8_t  strg_class;
    uint8_t  num_aux;
} coff_symb_entry_t;

typedef struct {
    uint32_t length;
    uint16_t num_relocs;
    uint16_t num_lines;
    uint8_t  dummy[10];
} coff_symb_aux_sect_t;

typedef struct {
    char name[18];
} coff_symb_aux_file_t;

/* Tagged union for symbol table entries */
typedef struct {
    coff_symb_kind_t kind;
    union {
        coff_symb_entry_t    entry;
        coff_symb_aux_sect_t aux_sect;
        coff_symb_aux_file_t aux_file;
    } u;
} coff_symb_t;

/* In-memory COFF file representation */
typedef struct {
    coff_hdr_t        header;

    coff_sect_hdr_t  *sect_tbl;
    int               num_sects;
    int               sect_cap;

    coff_reloc_item_t *reloc_tbl;
    int                num_relocs;
    int                reloc_cap;

    coff_symb_t       *symb_tbl;
    int                num_symbs;
    int                symb_cap;

    uint8_t           *code_part;   /* shared pointer to xout code_part */
    int32_t            code_part_len;
} coff_file_t;

/* ---- Dynamic array helpers ---- */

static inline void coff_ensure_sect(coff_file_t *cf, int need) {
    if (need > cf->sect_cap) {
        cf->sect_cap = need * 2;
        cf->sect_tbl = (coff_sect_hdr_t *)realloc(cf->sect_tbl,
                        cf->sect_cap * sizeof(coff_sect_hdr_t));
    }
}

static inline void coff_ensure_reloc(coff_file_t *cf, int need) {
    if (need > cf->reloc_cap) {
        cf->reloc_cap = need * 2;
        cf->reloc_tbl = (coff_reloc_item_t *)realloc(cf->reloc_tbl,
                         cf->reloc_cap * sizeof(coff_reloc_item_t));
    }
}

static inline void coff_ensure_symb(coff_file_t *cf, int need) {
    if (need > cf->symb_cap) {
        cf->symb_cap = need * 2;
        cf->symb_tbl = (coff_symb_t *)realloc(cf->symb_tbl,
                        cf->symb_cap * sizeof(coff_symb_t));
    }
}

#endif /* COFF_H */
