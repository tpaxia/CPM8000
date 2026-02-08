/*
 * xoutdump.c
 *
 * Dump x.out file information (header, segments, relocations, symbols).
 * Ported from Go xoutdump/xoutdump.go by 4sun5bu.
 * Original project: https://github.com/4sun5bu/xoututils (MIT license)
 *
 * Copyright (c) 2025, Salvatore Paxia
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage: xoutdump <file.o> [file2.o ...]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xout.h"

/* ---- Read an entire x.out file into memory ---- */

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
            continue;  /* skip type-0 entries, same as Go */
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

/* ---- Dump routines ---- */

static void dump_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "xoutdump: cannot open %s\n", path);
        return;
    }

    xout_file_t xf;
    if (xout_read(fp, &xf) != 0) {
        fprintf(stderr, "xoutdump: error reading %s\n", path);
        fclose(fp);
        return;
    }
    fclose(fp);

    printf("\nFile = %s\n", path);
    printf("  Magic = 0x%4x\n", xf.header.magic);
    printf("  nSegs = %d\n", xf.header.num_segs);
    printf("  SegInfo    FilePos = 0x%04x\n", XOUT_HDR_LEN);
    printf("  Code       FilePos = 0x%04x  Size = %d\n",
           (int)xf.code_pos, xf.header.code_part_len);
    printf("  RelocTable FilePos = 0x%04x  Size = %d\n",
           (int)xf.reloc_tbl_pos, xf.header.relocs_len);
    printf("  SymbTable  FilePos = 0x%04x  Size = %d\n",
           (int)xf.symb_tbl_pos, xf.header.symbs_len);
    printf("\n");

    /* Segment info */
    printf("Segment Info\n");
    for (int i = 0; i < xf.num_segs; i++) {
        printf(" %4d : No. = %1d, Type = %d, Size = %5d\n",
               i, xf.seg_tbl[i].number, xf.seg_tbl[i].type,
               xf.seg_tbl[i].length);
    }
    printf("\n");

    /* Relocation items */
    printf("Relocation items\n");
    for (int i = 0; i < xf.num_relocs; i++) {
        printf(" %4d : Seg = %3d, Type = %1d, Offset = 0x%04x, Symb = %d\n",
               i, xf.reloc_tbl[i].seg_idx, xf.reloc_tbl[i].type,
               xf.reloc_tbl[i].location, xf.reloc_tbl[i].symb_idx);
    }
    printf("\n");

    /* Symbol table */
    printf("Symbol table\n");
    for (int i = 0; i < xf.num_symbs; i++) {
        char nm[9];
        xout_convert_name(xf.symb_tbl[i].name, nm);
        printf(" %4d : Seg = %3d, Type = %1d,  Val = 0x%04x, Name = %-8s \n",
               i, xf.symb_tbl[i].seg_idx, xf.symb_tbl[i].type,
               xf.symb_tbl[i].value, nm);
    }
    printf("\n");

    xout_free(&xf);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: xoutdump <file.o> [file2.o ...]\n");
        return 1;
    }
    for (int i = 1; i < argc; i++)
        dump_file(argv[i]);
    return 0;
}
