# Logical development-media formats

Media generation is intentionally separate from system generation:

- `make system` links target-specific guest binaries.
- `make media` copies the common development tree plus a target's BIOS source
  overlay into one or more logical CP/M filesystems.

A format is a directory containing `format.conf`.  The descriptor supplies:

```sh
CPM_FORMAT=...       # diskdef name used by cpmtools
DISKDEFS=...         # repository-relative diskdefs file
LAYOUT=single|split  # one filesystem or a capacity-split set
IMAGE_BASENAME=...  # output filename stem
IMAGE_SIZE=...       # exact logical image size in bytes
```

Target packages advertise compatible formats through their `media-formats`
Make target.  For example, `src/bios/m20/Makefile` declares
`m20-floppy-set m20-hd`.

The output is a logical CP/M filesystem image.  This layer does not install a
boot sector, invoke `putboot`, or wrap an image in an emulator-specific format
such as CHD.

`z8002-demo-hd` describes only the filesystem after the machine's 64 KiB
system prefix.  `make z8002-demo-image` is the target-specific packaging step:
it combines that filesystem with the padded system payload and wraps the exact
8 MiB ATA image in an uncompressed CHD.
