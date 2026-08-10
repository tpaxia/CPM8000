# CP/M-8000 BIOS -- M20 serial-console variant.
#
# Built with the in-emulator DR/Zilog toolchain (zcc, asz8k, ld8k), producing
# an x.out object. This is an OVERLAY package: the BIOS sources come from the
# stock M20 tree (src/cpm8k); bios.c selects the RS-232 port as the console and
# initializes it for 9600 baud so MAME can expose the guest through a host PTY.
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
