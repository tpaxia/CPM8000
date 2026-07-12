# CP/M-8000 BIOS -- M20 reference.
#
# Built with the in-emulator DR/Zilog toolchain (zcc, asz8k, ld8k), producing
# an x.out object. This is an OVERLAY package: the BIOS sources come from the
# stock M20 tree (src/cpm8k); this directory overlays only the files it changes.
# The stock reference changes nothing, so it has no overlay sources -- copy this
# dir and drop in your own .c/.8kn to bring up a board.
#
# Contract used by scripts/sysgen.sh:
#   make -C src/bios/m20 bios.rel BUILDDIR=<dir>   -> <dir>/bios.rel

ROOT     := $(abspath $(CURDIR)/../../..)
BUILDDIR ?= $(ROOT)/build/bios/m20

.PHONY: bios.rel clean

# Build the BIOS object with the guest toolchain, overlaying this package's
# sources (if any) onto the stock M20 sources.
bios.rel:
	BIOS_OVERLAY=$(CURDIR) $(ROOT)/scripts/build-bios.sh $(BUILDDIR)

clean:
	$(RM) -rf $(BUILDDIR)
