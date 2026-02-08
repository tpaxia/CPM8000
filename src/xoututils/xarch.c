/*
 * xarch.c
 *
 * Extract object files from an x.out archive (Zilog library format).
 * Ported from Go xarch/xarch.go by 4sun5bu.
 * Original project: https://github.com/4sun5bu/xoututils (MIT license)
 *
 * Copyright (c) 2025, Salvatore Paxia
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage: xarch <archive.a>
 *   Extracts all member .o files into the current directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xout.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: xarch <archive>\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "xarch: cannot open %s\n", argv[1]);
        return 1;
    }

    /* Check archive magic */
    uint8_t mbuf[2];
    if (fread(mbuf, 1, 2, fp) != 2) {
        fprintf(stderr, "xarch: read error\n");
        fclose(fp);
        return 1;
    }
    uint16_t magic = read_be16(mbuf);
    if (magic != AR_MAGIC) {
        fprintf(stderr, "xarch: not a library file (magic=0x%04x)\n", magic);
        fclose(fp);
        return 1;
    }

    /* Read members */
    for (;;) {
        uint8_t hbuf[AR_HDR_LEN];
        if (fread(hbuf, 1, AR_HDR_LEN, fp) != AR_HDR_LEN)
            break;

        ar_hdr_t hdr;
        memcpy(hdr.name, hbuf, AR_FNAME_LEN);
        hdr.date = read_be32(hbuf + 14);
        hdr.uid  = hbuf[18];
        hdr.gid  = hbuf[19];
        hdr.mode = read_be16(hbuf + 20);
        hdr.size = read_be32(hbuf + 22);

        if (hdr.name[0] == 0 || hdr.size == 0)
            break;

        /* Build null-terminated filename */
        char fname[AR_FNAME_LEN + 1];
        int i;
        for (i = 0; i < AR_FNAME_LEN && hdr.name[i] != 0; i++)
            fname[i] = hdr.name[i];
        fname[i] = '\0';

        printf("%s\n", fname);

        /* Read member data */
        uint8_t *data = (uint8_t *)malloc(hdr.size);
        if (!data) {
            fprintf(stderr, "xarch: out of memory\n");
            fclose(fp);
            return 1;
        }
        if (fread(data, 1, hdr.size, fp) != hdr.size) {
            fprintf(stderr, "xarch: truncated member %s\n", fname);
            free(data);
            break;
        }

        /* Write member to file */
        FILE *out = fopen(fname, "wb");
        if (!out) {
            fprintf(stderr, "xarch: cannot create %s\n", fname);
            free(data);
            fclose(fp);
            return 1;
        }
        fwrite(data, 1, hdr.size, out);
        fclose(out);
        free(data);
    }

    fclose(fp);
    return 0;
}
