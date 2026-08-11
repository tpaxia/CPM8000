/*
 * xout2flat.c
 *
 * Convert a CP/M-8000 x.out executable into a flat boot image for the Z8002
 * board monitor's "Z" (boot) command.  The monitor loads raw sectors from the
 * SD into system bank 0 and jumps to offset 0, so the on-disk image must be the
 * exact runtime memory picture:
 *
 *     [ initialized code+const+data (x_init bytes) ][ zeroed BSS ]
 *
 * padded up to a 512-byte sector boundary.  The BSS must be present and zeroed
 * because the grafted Z8002 biosboot does NOT clear it (unlike a hosted loader,
 * and unlike the emulator whose RAM starts zeroed).
 *
 * This matches run_z8002()'s loader in cpm8kemu (init image at physical 0) but
 * adds the explicit BSS zero-fill that real RAM needs.
 *
 * Copyright (c) 2025, Salvatore Paxia
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Usage: xout2flat <cpm.sys> <out.img>
 *   Prints the flat image size and populated sector count.  Target packaging
 *   may add further padding to a fixed-size monitor boot area.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xout.h"

#define SECTOR 512

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <cpm.sys> <out.img>\n", argv[0]);
        return 2;
    }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) { perror(argv[1]); return 1; }

    uint8_t hdr[XOUT_HDR_LEN];
    if (fread(hdr, 1, XOUT_HDR_LEN, fp) != XOUT_HDR_LEN) {
        fprintf(stderr, "%s: short header\n", argv[1]); return 1;
    }
    uint16_t magic  = read_be16(hdr);
    int      nseg   = read_be16s(hdr + 2);
    uint32_t xinit  = read_be32s(hdr + 4);   /* initialized bytes in file */
    if ((magic & 0xFF00) != 0xEE00) {
        fprintf(stderr, "%s: not an x.out file (magic=0x%04X)\n", argv[1], magic);
        return 1;
    }
    if (magic != XOUT_MAGIC_NONSEG_X)
        fprintf(stderr, "warning: magic 0x%04X is not non-seg executable "
                        "(0xEE03); flat boot image assumes non-segmented\n", magic);

    /* Sum BSS/stack segment lengths -- memory beyond the initialized image
     * that must exist (zeroed) at run time. */
    uint32_t bss = 0;
    for (int i = 0; i < nseg; i++) {
        uint8_t s[XOUT_SEG_ENTRY_LEN];
        if (fread(s, 1, XOUT_SEG_ENTRY_LEN, fp) != XOUT_SEG_ENTRY_LEN) {
            fprintf(stderr, "%s: short segment table\n", argv[1]); return 1;
        }
        uint8_t  type = s[1];
        uint16_t len  = read_be16(s + 2);
        if (type == XOUT_SEG_BSS || type == XOUT_SEG_STACK)
            bss += len;
    }

    uint32_t memsz  = xinit + bss;                       /* runtime footprint */
    uint32_t padded = (memsz + SECTOR - 1) & ~(SECTOR - 1);
    uint32_t nsect  = padded / SECTOR;

    uint8_t *img = calloc(padded, 1);                    /* zero-filled (BSS) */
    if (!img) { fprintf(stderr, "out of memory\n"); return 1; }

    /* Copy the initialized image (skip header + segment table). */
    if (fseek(fp, XOUT_HDR_LEN + nseg * XOUT_SEG_ENTRY_LEN, SEEK_SET) != 0) {
        fprintf(stderr, "%s: seek\n", argv[1]); return 1;
    }
    if (fread(img, 1, xinit, fp) != xinit) {
        fprintf(stderr, "%s: short init image\n", argv[1]); return 1;
    }
    fclose(fp);

    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 1; }
    if (fwrite(img, 1, padded, out) != padded) {
        fprintf(stderr, "%s: write\n", argv[2]); return 1;
    }
    fclose(out);
    free(img);

    printf("%s: init=%u bss=%u  mem=%u -> %s (%u bytes, %u sectors)\n",
           argv[1], xinit, bss, memsz, argv[2], padded, nsect);
    return 0;
}
