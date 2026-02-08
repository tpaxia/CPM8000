/*
 * xout2coff.c
 *
 * Convert a Zilog x.out object file to Z8000-COFF format.
 * Ported from Go xout2coff by 4sun5bu.
 * Original project: https://github.com/4sun5bu/xoututils (MIT license)
 *
 * Copyright (c) 2025, Salvatore Paxia
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage: xout2coff <input.rel>
 *   Produces <basename>.o in the current directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xout.h"
#include "coff.h"

/* ==== x.out reader (same as xoutdump.c, duplicated for standalone build) ==== */

static int xout_read_header(FILE *fp, xout_file_t *xf)
{
    uint8_t buf[XOUT_HDR_LEN];
    fseek(fp, 0, SEEK_SET);
    if (fread(buf, 1, XOUT_HDR_LEN, fp) != XOUT_HDR_LEN)
        return -1;
    xf->header.magic         = read_be16(buf);
    xf->header.num_segs      = read_be16s(buf + 2);
    xf->header.code_part_len = read_be32s(buf + 4);
    xf->header.relocs_len    = read_be32s(buf + 8);
    xf->header.symbs_len     = read_be32s(buf + 12);

    xf->num_segs = xf->header.num_segs;
    xf->code_pos = XOUT_HDR_LEN + XOUT_SEG_ENTRY_LEN * xf->num_segs;
    xf->reloc_tbl_pos = xf->code_pos + xf->header.code_part_len;
    xf->symb_tbl_pos  = xf->reloc_tbl_pos + xf->header.relocs_len;
    xf->num_relocs = xf->header.relocs_len / XOUT_RELOC_ITEM_LEN;
    xf->num_symbs  = xf->header.symbs_len  / XOUT_SYMB_ENTRY_LEN;
    return 0;
}

static int xout_read_seg_tbl(FILE *fp, xout_file_t *xf)
{
    fseek(fp, XOUT_HDR_LEN, SEEK_SET);
    xout_ensure_seg(xf, xf->num_segs);
    for (int i = 0; i < xf->num_segs; i++) {
        uint8_t buf[XOUT_SEG_ENTRY_LEN];
        if (fread(buf, 1, XOUT_SEG_ENTRY_LEN, fp) != XOUT_SEG_ENTRY_LEN)
            return -1;
        xf->seg_tbl[i].number = buf[0];
        xf->seg_tbl[i].type   = buf[1];
        xf->seg_tbl[i].length = read_be16(buf + 2);
    }
    return 0;
}

static int xout_read_code_part(FILE *fp, xout_file_t *xf)
{
    xf->code_part_len = xf->header.code_part_len;
    xf->code_part = (uint8_t *)malloc(xf->code_part_len);
    if (!xf->code_part) return -1;
    fseek(fp, (long)xf->code_pos, SEEK_SET);
    if ((int32_t)fread(xf->code_part, 1, xf->code_part_len, fp) != xf->code_part_len)
        return -1;
    return 0;
}

static int xout_read_reloc_tbl(FILE *fp, xout_file_t *xf)
{
    fseek(fp, (long)xf->reloc_tbl_pos, SEEK_SET);
    int raw_count = xf->header.relocs_len / XOUT_RELOC_ITEM_LEN;
    xout_ensure_reloc(xf, raw_count);
    int kept = 0;
    for (int i = 0; i < raw_count; i++) {
        uint8_t buf[XOUT_RELOC_ITEM_LEN];
        if (fread(buf, 1, XOUT_RELOC_ITEM_LEN, fp) != XOUT_RELOC_ITEM_LEN)
            return -1;
        uint8_t type = buf[1];
        if (type == 0)
            continue;
        xout_ensure_reloc(xf, kept + 1);
        xf->reloc_tbl[kept].seg_idx  = buf[0];
        xf->reloc_tbl[kept].type     = type;
        xf->reloc_tbl[kept].location = read_be16(buf + 2);
        xf->reloc_tbl[kept].symb_idx = read_be16(buf + 4);
        kept++;
    }
    xf->num_relocs = kept;
    return 0;
}

static int xout_read_symb_tbl(FILE *fp, xout_file_t *xf)
{
    fseek(fp, (long)xf->symb_tbl_pos, SEEK_SET);
    int count = xf->header.symbs_len / XOUT_SYMB_ENTRY_LEN;
    xout_ensure_symb(xf, count);
    for (int i = 0; i < count; i++) {
        uint8_t buf[XOUT_SYMB_ENTRY_LEN];
        if (fread(buf, 1, XOUT_SYMB_ENTRY_LEN, fp) != XOUT_SYMB_ENTRY_LEN)
            return -1;
        xf->symb_tbl[i].seg_idx = buf[0];
        xf->symb_tbl[i].type    = buf[1];
        xf->symb_tbl[i].value   = read_be16(buf + 2);
        memcpy(xf->symb_tbl[i].name, buf + 4, XOUT_NAME_LEN);
    }
    xf->num_symbs = count;
    return 0;
}

static int xout_read(FILE *fp, xout_file_t *xf)
{
    memset(xf, 0, sizeof(*xf));
    if (xout_read_header(fp, xf) != 0) return -1;
    if (xout_read_seg_tbl(fp, xf) != 0) return -1;
    if (xout_read_code_part(fp, xf) != 0) return -1;
    if (xout_read_reloc_tbl(fp, xf) != 0) return -1;
    if (xout_read_symb_tbl(fp, xf) != 0) return -1;
    return 0;
}

static void xout_free(xout_file_t *xf)
{
    free(xf->seg_tbl);
    free(xf->code_part);
    free(xf->reloc_tbl);
    free(xf->symb_tbl);
    memset(xf, 0, sizeof(*xf));
}

/* ==== Conversion helpers ==== */

static const char *conv_seg_name(uint8_t seg_type)
{
    switch (seg_type) {
    case XOUT_SEG_CODE:  return ".text";
    case XOUT_SEG_DATA:  return ".data";
    case XOUT_SEG_CONST: return ".rdata";
    case XOUT_SEG_BSS:   return ".bss";
    default:             return "";
    }
}

static uint32_t conv_seg_type(uint8_t seg_type)
{
    switch (seg_type) {
    case XOUT_SEG_BSS:   return COFF_SECT_BSS;
    case XOUT_SEG_CODE:  return COFF_SECT_TEXT;
    case XOUT_SEG_DATA:
    case XOUT_SEG_CONST: return COFF_SECT_DATA;
    default:             return 0;
    }
}

static uint16_t calc_addr(xout_file_t *xf, int seg, uint16_t offset)
{
    uint16_t pos = 0;
    for (int i = 0; i < seg; i++)
        pos += xf->seg_tbl[i].length;
    return pos + offset;
}

/* ==== Preparation steps (modify xout in place) ==== */

static void assign_bss(xout_file_t *xf)
{
    /* Find existing BSS segment, or create one */
    int bss = 0;
    for (bss = 0; bss < xf->num_segs; bss++) {
        if (xf->seg_tbl[bss].type == XOUT_SEG_BSS)
            break;
    }
    if (bss == xf->num_segs) {
        xout_ensure_seg(xf, xf->num_segs + 1);
        xf->seg_tbl[bss].number = 0xff;
        xf->seg_tbl[bss].type   = XOUT_SEG_BSS;
        xf->seg_tbl[bss].length = 0;
        xf->num_segs++;
        xf->header.num_segs++;
    }

    /* Convert undef externals with non-zero value into BSS globals */
    for (int i = 0; i < xf->num_symbs; i++) {
        xout_symb_entry_t *s = &xf->symb_tbl[i];
        if (s->seg_idx == 0xff && s->type == XOUT_SYMB_UNDEF_EX && s->value != 0) {
            s->type    = XOUT_SYMB_GLOBAL;
            s->seg_idx = (uint8_t)bss;
            uint16_t size = s->value;
            s->value = xf->seg_tbl[bss].length;
            xf->seg_tbl[bss].length += size;
        }
    }
}

static void add_seg_symb(xout_file_t *xf)
{
    for (int seg_idx = 0; seg_idx < xf->num_segs; seg_idx++) {
        /* Check if a segment symbol already exists for this segment */
        int found = -1;
        for (int si = 0; si < xf->num_symbs; si++) {
            if (xf->symb_tbl[si].type == XOUT_SYMB_SEG &&
                xf->symb_tbl[si].seg_idx == (uint8_t)seg_idx) {
                found = si;
                break;
            }
        }
        const char *name = conv_seg_name(xf->seg_tbl[seg_idx].type);
        char padded[8];
        memset(padded, 0, 8);
        /* name might be shorter than 8 */
        size_t nlen = strlen(name);
        if (nlen > 8) nlen = 8;
        memcpy(padded, name, nlen);

        if (found >= 0) {
            /* Update the existing segment symbol's name */
            memcpy(xf->symb_tbl[found].name, padded, 8);
        } else {
            /* Create a new segment symbol */
            xout_ensure_symb(xf, xf->num_symbs + 1);
            xout_symb_entry_t *s = &xf->symb_tbl[xf->num_symbs];
            s->type    = XOUT_SYMB_SEG;
            s->seg_idx = (uint8_t)seg_idx;
            s->value   = 0;
            memcpy(s->name, padded, 8);
            xf->num_symbs++;
        }
    }
}

static void add_seg_top_symb(xout_file_t *xf)
{
    for (int idx = 0; idx < xf->num_segs; idx++) {
        char name[9];
        snprintf(name, sizeof(name), "SEG%d0000", idx);

        xout_ensure_symb(xf, xf->num_symbs + 1);
        xout_symb_entry_t *s = &xf->symb_tbl[xf->num_symbs];
        s->seg_idx = (uint8_t)idx;
        s->type    = XOUT_SYMB_LOCAL;
        s->value   = 0x0000;
        memset(s->name, 0, 8);
        memcpy(s->name, name, strlen(name) > 8 ? 8 : strlen(name));
        xf->num_symbs++;
    }
}

/* ==== COFF construction ==== */

static void coff_init(coff_file_t *cf)
{
    memset(cf, 0, sizeof(*cf));
}

static void coff_free(coff_file_t *cf)
{
    free(cf->sect_tbl);
    free(cf->reloc_tbl);
    free(cf->symb_tbl);
    memset(cf, 0, sizeof(*cf));
}

/* Add a symbol entry to the COFF symbol table */
static void coff_add_symb_entry(coff_file_t *cf, const coff_symb_entry_t *e)
{
    coff_ensure_symb(cf, cf->num_symbs + 1);
    cf->symb_tbl[cf->num_symbs].kind = COFF_SYMB_TYPE_ENTRY;
    cf->symb_tbl[cf->num_symbs].u.entry = *e;
    cf->num_symbs++;
}

static void coff_add_symb_aux_sect(coff_file_t *cf, const coff_symb_aux_sect_t *a)
{
    coff_ensure_symb(cf, cf->num_symbs + 1);
    cf->symb_tbl[cf->num_symbs].kind = COFF_SYMB_TYPE_AUX_SECT;
    cf->symb_tbl[cf->num_symbs].u.aux_sect = *a;
    cf->num_symbs++;
}

static void coff_add_symb_aux_file(coff_file_t *cf, const coff_symb_aux_file_t *a)
{
    coff_ensure_symb(cf, cf->num_symbs + 1);
    cf->symb_tbl[cf->num_symbs].kind = COFF_SYMB_TYPE_AUX_FILE;
    cf->symb_tbl[cf->num_symbs].u.aux_file = *a;
    cf->num_symbs++;
}

/* Look up a COFF symbol by 8-byte name, return its index or 0xffffffff */
static uint32_t conv_symb_idx(uint16_t x_idx, xout_file_t *xf, coff_file_t *cf)
{
    char xname[8];
    memcpy(xname, xf->symb_tbl[x_idx].name, 8);
    for (int i = 0; i < cf->num_symbs; i++) {
        if (cf->symb_tbl[i].kind == COFF_SYMB_TYPE_ENTRY &&
            memcmp(cf->symb_tbl[i].u.entry.name, xname, 8) == 0)
            return (uint32_t)i;
    }
    return 0xffffffff;
}

/* Look up COFF symbol for a segment-top anchor (SEGn0000) */
static uint32_t conv_seg_top_symb_idx(uint16_t x_idx, xout_file_t *xf __attribute__((unused)), coff_file_t *cf)
{
    char segname[9];
    snprintf(segname, sizeof(segname), "SEG%d0000", x_idx);
    /* pad to 8 bytes */
    char padded[8];
    memset(padded, 0, 8);
    memcpy(padded, segname, strlen(segname) > 8 ? 8 : strlen(segname));

    for (int i = 0; i < cf->num_symbs; i++) {
        if (cf->symb_tbl[i].kind == COFF_SYMB_TYPE_ENTRY) {
            /* Compare: ConvertName equivalent */
            char cname[9];
            xout_convert_name(cf->symb_tbl[i].u.entry.name, cname);
            char sname[9];
            xout_convert_name(padded, sname);
            if (strcmp(cname, sname) == 0)
                return (uint32_t)i;
        }
    }
    return 0xffffffff;
}

static void conv_sect_hdrs(xout_file_t *xf, coff_file_t *cf)
{
    int32_t sect_pos = COFF_HDR_LEN + xf->num_segs * COFF_SECT_HDR_LEN;
    int32_t offset = 0;

    for (int i = 0; i < xf->num_segs; i++) {
        coff_ensure_sect(cf, cf->num_sects + 1);
        coff_sect_hdr_t *s = &cf->sect_tbl[cf->num_sects];
        memset(s, 0, sizeof(*s));

        const char *name = conv_seg_name(xf->seg_tbl[i].type);
        memset(s->name, 0, COFF_NAME_LEN);
        size_t nlen = strlen(name);
        if (nlen > COFF_NAME_LEN) nlen = COFF_NAME_LEN;
        memcpy(s->name, name, nlen);

        s->vaddr  = 0;
        s->paddr  = 0;
        s->length = xf->seg_tbl[i].length;

        if (xf->seg_tbl[i].type == XOUT_SEG_BSS) {
            s->fpos = 0;
        } else {
            s->fpos = sect_pos + offset;
            offset += (int32_t)xf->seg_tbl[i].length;
        }
        s->reloc_tbl_fpos = 0;   /* set by finalize */
        s->line_nums_fpos = 0;
        s->num_relocs     = 0;   /* set by finalize */
        s->num_lines      = 0;
        s->flags = conv_seg_type(xf->seg_tbl[i].type);
        cf->num_sects++;
    }
}

static void conv_symb_tbl(xout_file_t *xf, coff_file_t *cf)
{
    /* 1. Add dummy .file symbol + aux */
    {
        coff_symb_entry_t e;
        memset(&e, 0, sizeof(e));
        memcpy(e.name, ".file\0\0\0", 8);
        e.value      = 0;
        e.sect_no    = -2;
        e.type       = 0;
        e.strg_class = COFF_SYMB_CLASS_FILE;
        e.num_aux    = 1;
        coff_add_symb_entry(cf, &e);

        coff_symb_aux_file_t af;
        memset(&af, 0, sizeof(af));
        memcpy(af.name, "fake", 4);
        coff_add_symb_aux_file(cf, &af);
    }

    /* 2. Convert local symbols (seg_idx != 255, type == LOCAL) */
    for (int i = 0; i < xf->num_symbs; i++) {
        xout_symb_entry_t *xs = &xf->symb_tbl[i];
        if (xs->seg_idx == 255 || xs->type != XOUT_SYMB_LOCAL)
            continue;
        coff_symb_entry_t e;
        memset(&e, 0, sizeof(e));
        memcpy(e.name, xs->name, 8);
        e.value      = (uint32_t)xs->value;
        e.sect_no    = (int16_t)(xs->seg_idx + 1);
        e.type       = 0;
        e.strg_class = COFF_SYMB_CLASS_STATIC;
        e.num_aux    = 0;
        coff_add_symb_entry(cf, &e);
    }

    /* 3. Convert section symbols (type == SEG) */
    for (int i = 0; i < xf->num_symbs; i++) {
        xout_symb_entry_t *xs = &xf->symb_tbl[i];
        if (xs->type != XOUT_SYMB_SEG)
            continue;

        coff_symb_entry_t e;
        memset(&e, 0, sizeof(e));
        memcpy(e.name, xs->name, 8);
        e.value      = 0;
        e.sect_no    = (int16_t)(xs->seg_idx + 1);
        e.type       = 0;
        e.strg_class = COFF_SYMB_CLASS_STATIC;
        e.num_aux    = 1;
        coff_add_symb_entry(cf, &e);

        /* Auxiliary section entry */
        coff_symb_aux_sect_t as;
        memset(&as, 0, sizeof(as));
        as.length = (uint32_t)xf->seg_tbl[xs->seg_idx].length;
        as.num_lines = 0;
        as.num_relocs = 0;
        for (int r = 0; r < xf->num_relocs; r++) {
            if (xf->reloc_tbl[r].seg_idx == xs->seg_idx)
                as.num_relocs++;
        }
        coff_add_symb_aux_sect(cf, &as);
    }

    /* 4. Convert global symbols (ordered by segment) */
    for (int seg = 0; seg < xf->num_segs; seg++) {
        for (int i = 0; i < xf->num_symbs; i++) {
            xout_symb_entry_t *xs = &xf->symb_tbl[i];
            if (xs->seg_idx == (uint8_t)seg && xs->type == XOUT_SYMB_GLOBAL) {
                coff_symb_entry_t e;
                memset(&e, 0, sizeof(e));
                memcpy(e.name, xs->name, 8);
                e.value      = (uint32_t)xs->value;
                e.sect_no    = (int16_t)(xs->seg_idx + 1);
                e.type       = 0;
                e.strg_class = COFF_SYMB_CLASS_GLOBAL;
                e.num_aux    = 0;
                coff_add_symb_entry(cf, &e);
            }
        }
    }

    /* 5. Convert external symbols and constants (seg_idx == 255) */
    for (int i = 0; i < xf->num_symbs; i++) {
        xout_symb_entry_t *xs = &xf->symb_tbl[i];
        if (xs->seg_idx != 255)
            continue;
        coff_symb_entry_t e;
        memset(&e, 0, sizeof(e));
        memcpy(e.name, xs->name, 8);
        e.type   = 0;
        e.num_aux = 0;

        switch (xs->type) {
        case XOUT_SYMB_UNDEF_EX:
            e.value      = 0;
            e.sect_no    = COFF_SYMB_SCN_EXT;
            e.strg_class = COFF_SYMB_CLASS_GLOBAL;
            coff_add_symb_entry(cf, &e);
            break;
        case XOUT_SYMB_LOCAL:
            e.value      = (uint32_t)xs->value;
            e.sect_no    = COFF_SYMB_SCN_ABS;
            e.strg_class = COFF_SYMB_CLASS_GLOBAL;
            coff_add_symb_entry(cf, &e);
            break;
        default:
            break;
        }
    }
}

/* Relocation type mapping: xout type -> coff type */
static uint16_t reloc_type_map(uint8_t xtype)
{
    switch (xtype) {
    case 1: return 0x0001;  /* OFF  -> 16-bit */
    case 3: return 0x0011;  /* LSG  -> 32-bit */
    case 5: return 0x0001;  /* XOFF -> 16-bit */
    case 7: return 0x0011;  /* XLSG -> 32-bit */
    default: return 0xffff;
    }
}

/* Compare function for sorting relocations by location */
static int reloc_cmp(const void *a, const void *b)
{
    const xout_reloc_item_t *ra = (const xout_reloc_item_t *)a;
    const xout_reloc_item_t *rb = (const xout_reloc_item_t *)b;
    if (ra->location < rb->location) return -1;
    if (ra->location > rb->location) return  1;
    return 0;
}

static void conv_reloc_tbl(xout_file_t *xf, coff_file_t *cf)
{
    /* Sort relocations by location (matches Go sort.Slice) */
    qsort(xf->reloc_tbl, xf->num_relocs, sizeof(xout_reloc_item_t), reloc_cmp);

    /* Export per segment (in segment order, matching Go) */
    for (int seg = 0; seg < xf->num_segs; seg++) {
        for (int i = 0; i < xf->num_relocs; i++) {
            xout_reloc_item_t *xr = &xf->reloc_tbl[i];
            if (xr->seg_idx != (uint8_t)seg)
                continue;

            coff_reloc_item_t cr;
            memset(&cr, 0, sizeof(cr));
            cr.vaddr = (uint32_t)xr->location;
            cr.type  = reloc_type_map(xr->type);
            cr.stuff = 0x5343;

            /* Extract 16-bit offset from code */
            uint16_t pos = calc_addr(xf, (int)xr->seg_idx, xr->location);
            cr.offset = (uint32_t)xf->code_part[pos] * 256 +
                        (uint32_t)xf->code_part[pos + 1];

            if (xr->type == XOUT_RELOC_XOFF) {
                cr.symb_idx = conv_symb_idx(xr->symb_idx, xf, cf);
            } else if (xr->type == XOUT_RELOC_OFF) {
                cr.symb_idx = conv_seg_top_symb_idx(xr->symb_idx, xf, cf);
            }

            coff_ensure_reloc(cf, cf->num_relocs + 1);
            cf->reloc_tbl[cf->num_relocs++] = cr;
        }
    }
}

static void finalize(xout_file_t *xf, coff_file_t *cf)
{
    int reloc_fpos = COFF_HDR_LEN + COFF_SECT_HDR_LEN * cf->num_sects +
                     xf->code_part_len;
    int count = 0;

    for (int sect = 0; sect < cf->num_sects; sect++) {
        reloc_fpos += count * COFF_RELOC_ITEM_LEN;
        cf->sect_tbl[sect].reloc_tbl_fpos = (int32_t)reloc_fpos;
        count = 0;
        for (int r = 0; r < xf->num_relocs; r++) {
            if ((int)xf->reloc_tbl[r].seg_idx == sect)
                count++;
        }
        cf->sect_tbl[sect].num_relocs = (uint16_t)count;
        if (count == 0)
            cf->sect_tbl[sect].reloc_tbl_fpos = 0;
    }
}

static void conv_hdr(coff_file_t *cf)
{
    cf->header.magic       = 0x8000;
    cf->header.num_sects   = (uint16_t)cf->num_sects;
    cf->header.date        = 0;
    cf->header.symb_tbl_fpos = (int32_t)(COFF_HDR_LEN +
                               cf->num_sects * COFF_SECT_HDR_LEN +
                               cf->code_part_len +
                               cf->num_relocs * COFF_RELOC_ITEM_LEN);
    cf->header.num_symbs   = (uint32_t)cf->num_symbs;
    cf->header.opt_hdr_len = 0;
    cf->header.flags       = 0x2205;
}

/* ==== COFF writer ==== */

static int coff_write_hdr(FILE *fp, coff_file_t *cf)
{
    uint8_t buf[COFF_HDR_LEN];
    write_be16(buf,      cf->header.magic);
    write_be16(buf + 2,  cf->header.num_sects);
    write_be32(buf + 4,  cf->header.date);
    write_be32(buf + 8,  (uint32_t)cf->header.symb_tbl_fpos);
    write_be32(buf + 12, cf->header.num_symbs);
    write_be16(buf + 16, cf->header.opt_hdr_len);
    write_be16(buf + 18, cf->header.flags);
    return fwrite(buf, 1, COFF_HDR_LEN, fp) == COFF_HDR_LEN ? 0 : -1;
}

static int coff_write_sect_tbl(FILE *fp, coff_file_t *cf)
{
    for (int i = 0; i < cf->num_sects; i++) {
        uint8_t buf[COFF_SECT_HDR_LEN];
        coff_sect_hdr_t *s = &cf->sect_tbl[i];
        memcpy(buf, s->name, COFF_NAME_LEN);
        write_be32(buf + 8,  s->paddr);
        write_be32(buf + 12, s->vaddr);
        write_be32(buf + 16, s->length);
        write_be32(buf + 20, (uint32_t)s->fpos);
        write_be32(buf + 24, (uint32_t)s->reloc_tbl_fpos);
        write_be32(buf + 28, (uint32_t)s->line_nums_fpos);
        write_be16(buf + 32, s->num_relocs);
        write_be16(buf + 34, s->num_lines);
        write_be32(buf + 36, s->flags);
        if (fwrite(buf, 1, COFF_SECT_HDR_LEN, fp) != COFF_SECT_HDR_LEN)
            return -1;
    }
    return 0;
}

static int coff_write_code_part(FILE *fp, coff_file_t *cf)
{
    if (cf->code_part_len > 0) {
        if ((int32_t)fwrite(cf->code_part, 1, cf->code_part_len, fp) != cf->code_part_len)
            return -1;
    }
    return 0;
}

static int coff_write_reloc_tbl(FILE *fp, coff_file_t *cf)
{
    for (int i = 0; i < cf->num_relocs; i++) {
        uint8_t buf[COFF_RELOC_ITEM_LEN];
        coff_reloc_item_t *r = &cf->reloc_tbl[i];
        write_be32(buf,      r->vaddr);
        write_be32(buf + 4,  r->symb_idx);
        write_be32(buf + 8,  r->offset);
        write_be16(buf + 12, r->type);
        write_be16(buf + 14, r->stuff);
        if (fwrite(buf, 1, COFF_RELOC_ITEM_LEN, fp) != COFF_RELOC_ITEM_LEN)
            return -1;
    }
    return 0;
}

static int coff_write_symb_entry(FILE *fp, const coff_symb_entry_t *e)
{
    uint8_t buf[COFF_SYMB_ENTRY_LEN];
    memcpy(buf, e->name, COFF_NAME_LEN);
    write_be32(buf + 8,  e->value);
    write_be16(buf + 12, (uint16_t)e->sect_no);
    write_be16(buf + 14, e->type);
    buf[16] = e->strg_class;
    buf[17] = e->num_aux;
    return fwrite(buf, 1, COFF_SYMB_ENTRY_LEN, fp) == COFF_SYMB_ENTRY_LEN ? 0 : -1;
}

static int coff_write_symb_aux_sect(FILE *fp, const coff_symb_aux_sect_t *a)
{
    uint8_t buf[COFF_SYMB_ENTRY_LEN];
    memset(buf, 0, COFF_SYMB_ENTRY_LEN);
    write_be32(buf,     a->length);
    write_be16(buf + 4, a->num_relocs);
    write_be16(buf + 6, a->num_lines);
    /* bytes 8..17 are zeros (dummy) */
    return fwrite(buf, 1, COFF_SYMB_ENTRY_LEN, fp) == COFF_SYMB_ENTRY_LEN ? 0 : -1;
}

static int coff_write_symb_aux_file(FILE *fp, const coff_symb_aux_file_t *a)
{
    uint8_t buf[COFF_SYMB_ENTRY_LEN];
    memset(buf, 0, COFF_SYMB_ENTRY_LEN);
    memcpy(buf, a->name, COFF_SYMB_ENTRY_LEN);
    return fwrite(buf, 1, COFF_SYMB_ENTRY_LEN, fp) == COFF_SYMB_ENTRY_LEN ? 0 : -1;
}

static int coff_write_symb_tbl(FILE *fp, coff_file_t *cf)
{
    for (int i = 0; i < cf->num_symbs; i++) {
        int rc;
        switch (cf->symb_tbl[i].kind) {
        case COFF_SYMB_TYPE_ENTRY:
            rc = coff_write_symb_entry(fp, &cf->symb_tbl[i].u.entry);
            break;
        case COFF_SYMB_TYPE_AUX_SECT:
            rc = coff_write_symb_aux_sect(fp, &cf->symb_tbl[i].u.aux_sect);
            break;
        case COFF_SYMB_TYPE_AUX_FILE:
            rc = coff_write_symb_aux_file(fp, &cf->symb_tbl[i].u.aux_file);
            break;
        default:
            rc = -1;
        }
        if (rc != 0) return -1;
    }
    return 0;
}

/* ==== Main ==== */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: xout2coff <input.rel>\n");
        return 1;
    }

    const char *inpath = argv[1];
    FILE *infp = fopen(inpath, "rb");
    if (!infp) {
        fprintf(stderr, "xout2coff: cannot open %s\n", inpath);
        return 1;
    }

    /* Build output filename: strip extension, add .o */
    const char *base = strrchr(inpath, '/');
    base = base ? base + 1 : inpath;
    size_t blen = strlen(base);
    const char *dot = strrchr(base, '.');
    size_t stem_len = dot ? (size_t)(dot - base) : blen;
    char *outpath = (char *)malloc(stem_len + 3);
    memcpy(outpath, base, stem_len);
    memcpy(outpath + stem_len, ".o", 3);

    /* Read x.out */
    xout_file_t xf;
    if (xout_read(infp, &xf) != 0) {
        fprintf(stderr, "xout2coff: error reading %s\n", inpath);
        fclose(infp);
        free(outpath);
        return 1;
    }
    fclose(infp);

    /* Prepare */
    assign_bss(&xf);
    add_seg_symb(&xf);
    add_seg_top_symb(&xf);

    /* Convert */
    coff_file_t cf;
    coff_init(&cf);

    conv_sect_hdrs(&xf, &cf);
    cf.code_part     = xf.code_part;
    cf.code_part_len = xf.code_part_len;
    conv_symb_tbl(&xf, &cf);
    conv_reloc_tbl(&xf, &cf);
    finalize(&xf, &cf);
    conv_hdr(&cf);

    /* Write */
    FILE *outfp = fopen(outpath, "wb");
    if (!outfp) {
        fprintf(stderr, "xout2coff: cannot create %s\n", outpath);
        xout_free(&xf);
        coff_free(&cf);
        free(outpath);
        return 1;
    }

    if (coff_write_hdr(outfp, &cf) != 0 ||
        coff_write_sect_tbl(outfp, &cf) != 0 ||
        coff_write_code_part(outfp, &cf) != 0 ||
        coff_write_reloc_tbl(outfp, &cf) != 0 ||
        coff_write_symb_tbl(outfp, &cf) != 0) {
        fprintf(stderr, "xout2coff: write error\n");
        fclose(outfp);
        xout_free(&xf);
        coff_free(&cf);
        free(outpath);
        return 1;
    }

    fclose(outfp);
    xout_free(&xf);
    coff_free(&cf);
    free(outpath);
    return 0;
}
