/*
 * xout.h
 *
 * x.out (Zilog) object file format structures and constants.
 * Ported from Go binlib/xout.go by 4sun5bu.
 * Original project: https://github.com/4sun5bu/xoututils (MIT license)
 *
 * Copyright (c) 2025, Salvatore Paxia
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef XOUT_H
#define XOUT_H

#include <stdint.h>

/* Lengths */
#define XOUT_HDR_LEN        16
#define XOUT_SEG_ENTRY_LEN   4
#define XOUT_RELOC_ITEM_LEN  6
#define XOUT_SYMB_ENTRY_LEN 12
#define XOUT_NAME_LEN        8

/* Magic numbers (big-endian on disk) */
#define XOUT_MAGIC_SEG           0xee00
#define XOUT_MAGIC_SEG_X         0xee01
#define XOUT_MAGIC_NONSEG        0xee02
#define XOUT_MAGIC_NONSEG_X      0xee03
#define XOUT_MAGIC_NONSEG_SHARED 0x0006
#define XOUT_MAGIC_NONSEG_XSHARED 0xee07
#define XOUT_MAGIC_NONSEG_SPLIT  0xee0a
#define XOUT_MAGIC_NONSEG_XSPLIT 0xee0b

/* Segment types */
#define XOUT_SEG_BSS      1
#define XOUT_SEG_STACK    2
#define XOUT_SEG_CODE     3
#define XOUT_SEG_CONST    4
#define XOUT_SEG_DATA     5
#define XOUT_SEG_CDMIX    6
#define XOUT_SEG_CDMIX_P  7
#define XOUT_SEG_UNDEF    0

/* Relocation types */
#define XOUT_RELOC_OFF   1   /* 16-bit non-segmented */
#define XOUT_RELOC_SSG   2   /* 16-bit short segmented */
#define XOUT_RELOC_LSG   3   /* 32-bit long segmented */
#define XOUT_RELOC_XOFF  5   /* 16-bit non-seg, external */
#define XOUT_RELOC_XSSG  6   /* short seg, external */
#define XOUT_RELOC_XLSG  7   /* long seg, external */

/* Symbol types */
#define XOUT_SYMB_LOCAL     1
#define XOUT_SYMB_UNDEF_EX  2
#define XOUT_SYMB_GLOBAL    3
#define XOUT_SYMB_SEG       4

/* Archive format */
#define AR_HDR_LEN   26
#define AR_FNAME_LEN 14
#define AR_MAGIC     0xff65

/* ---- on-disk structures (packed, big-endian) ---- */

/* We read these byte-by-byte, so no packed structs needed.
   These are in-memory representations after endian conversion. */

typedef struct {
    uint16_t magic;
    int16_t  num_segs;
    int32_t  code_part_len;
    int32_t  relocs_len;
    int32_t  symbs_len;
} xout_header_t;

typedef struct {
    uint8_t  number;
    uint8_t  type;
    uint16_t length;
} xout_seg_t;

typedef struct {
    uint8_t  seg_idx;
    uint8_t  type;
    uint16_t location;
    uint16_t symb_idx;
} xout_reloc_item_t;

typedef struct {
    uint8_t  seg_idx;
    uint8_t  type;
    uint16_t value;
    char     name[XOUT_NAME_LEN];
} xout_symb_entry_t;

typedef struct {
    char     name[AR_FNAME_LEN];
    uint32_t date;
    uint8_t  uid;
    uint8_t  gid;
    uint16_t mode;
    uint32_t size;
} ar_hdr_t;

/* In-memory x.out file representation */
typedef struct {
    xout_header_t      header;
    int                num_segs;    /* may grow if BSS added */
    int                num_relocs;
    int                num_symbs;
    int64_t            code_pos;
    int64_t            reloc_tbl_pos;
    int64_t            symb_tbl_pos;

    xout_seg_t        *seg_tbl;
    int                seg_cap;

    uint8_t           *code_part;
    int32_t            code_part_len;

    xout_reloc_item_t *reloc_tbl;
    int                reloc_cap;

    xout_symb_entry_t *symb_tbl;
    int                symb_cap;
} xout_file_t;

/* ---- Big-endian I/O helpers ---- */

static inline uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | p[3];
}

static inline int16_t read_be16s(const uint8_t *p) {
    return (int16_t)read_be16(p);
}

static inline int32_t read_be32s(const uint8_t *p) {
    return (int32_t)read_be32(p);
}

static inline void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xff;
    p[1] = v & 0xff;
}

static inline void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xff;
    p[1] = (v >> 16) & 0xff;
    p[2] = (v >> 8)  & 0xff;
    p[3] = v & 0xff;
}

/* ---- Utility: convert 8-byte name to C string ---- */
static inline void xout_convert_name(const char src[8], char dst[9]) {
    int i;
    for (i = 0; i < 8 && src[i] != 0; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/* ---- Dynamic array helpers ---- */

static inline void xout_ensure_seg(xout_file_t *xf, int need) {
    if (need > xf->seg_cap) {
        xf->seg_cap = need * 2;
        xf->seg_tbl = (xout_seg_t *)realloc(xf->seg_tbl,
                       xf->seg_cap * sizeof(xout_seg_t));
    }
}

static inline void xout_ensure_reloc(xout_file_t *xf, int need) {
    if (need > xf->reloc_cap) {
        xf->reloc_cap = need * 2;
        xf->reloc_tbl = (xout_reloc_item_t *)realloc(xf->reloc_tbl,
                         xf->reloc_cap * sizeof(xout_reloc_item_t));
    }
}

static inline void xout_ensure_symb(xout_file_t *xf, int need) {
    if (need > xf->symb_cap) {
        xf->symb_cap = need * 2;
        xf->symb_tbl = (xout_symb_entry_t *)realloc(xf->symb_tbl,
                        xf->symb_cap * sizeof(xout_symb_entry_t));
    }
}

#endif /* XOUT_H */
