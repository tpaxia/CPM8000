# CP/M-8000 BIOS -- M20 reference.
#
# Built with the in-emulator DR/Zilog toolchain (zcc, asz8k, ld8k), producing
# an x.out object. This is an OVERLAY package: the BIOS sources come from the
# stock M20 tree (src/cpm8k); this directory overlays only the files it changes.
# The stock reference changes nothing, so it has no overlay sources -- copy this
# dir, set TARGET_CPU, and drop in your own .c/.8kn to bring up a board.
#
# Contract used by scripts/sysgen.sh:
#   make -C src/bios/m20 bios.rel BUILDDIR=<dir> [HOST_CPU=z8001|z8002]
#     -> <dir>/bios.rel
# TARGET_CPU (this package's target CPU) is read from ./TARGET_CPU; HOST_CPU
# (the hosted emulator that runs the toolchain) defaults to it.

ROOT     := $(abspath $(CURDIR)/../../..)
BUILDDIR ?= $(ROOT)/build/bios/m20

TARGET_CPU := $(shell sed -n 1p $(CURDIR)/TARGET_CPU)
HOST_CPU ?= $(TARGET_CPU)

MEDIA_FORMATS := m20-floppy-set m20-hd

.PHONY: bios.rel media-formats clean

# Build the BIOS object with the guest toolchain, overlaying this package's
# sources (if any) onto the stock M20 sources.
bios.rel:
	BIOS_OVERLAY=$(CURDIR) TARGET_CPU=$(TARGET_CPU) HOST_CPU=$(HOST_CPU) \
		$(ROOT)/scripts/build-bios.sh $(BUILDDIR)

media-formats:
	@echo $(MEDIA_FORMATS)

clean:
	$(RM) -rf $(BUILDDIR)
