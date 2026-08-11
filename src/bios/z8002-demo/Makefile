# CP/M-8000 BIOS -- Z8002-demo FPGA machine.
#
# A classic (Zilog-pipeline) BIOS package: built with the in-emulator DR/Zilog
# toolchain (zcc, asz8k, ld8k) via sysgen, producing an x.out bios.rel that is
# linked with the CCP+BDOS into cpm.sys. This is an OVERLAY on the stock M20
# tree (src/cpm8k): it overrides only the files that change for the Z8002
# machine (see Z8000_FPGA/z8000_examples/cpm8000_z8002/HARDWARE.md):
#   bios.c        -- Z80-SIO console, KFMMC IDE disk, Z8002 cold boot
#   biosmem.8kn   -- SC #1 memory management via the banking MMU (MEMORY_MODEL)
#   (+ any I/O-def overrides)
#
# Build:  make system NAME=z8002-demo
#
# Contract used by scripts/sysgen.sh:
#   make -C src/bios/z8002-demo bios.rel BUILDDIR=<dir> -> <dir>/bios.rel

ROOT     := $(abspath $(CURDIR)/../../..)
BUILDDIR ?= $(ROOT)/build/bios/z8002-demo
SYSTEMDIR ?= $(ROOT)/build/system/z8002-demo
XOUT2FLAT := $(ROOT)/build/tools/xout2flat
MEDIA_FORMATS := z8002-demo-hd

.PHONY: bios.rel system-artifacts media-formats clean

bios.rel:
	BIOS_OVERLAY=$(CURDIR) EMU_MODEL=z8002 $(ROOT)/scripts/build-bios.sh $(BUILDDIR)

system-artifacts: $(SYSTEMDIR)/z8002-demo.boot

$(SYSTEMDIR)/z8002-demo.boot: $(SYSTEMDIR)/cpm.sys
	$(MAKE) -C $(ROOT)/src/xoututils BUILDDIR=$(ROOT)/build/tools xout2flat
	$(XOUT2FLAT) $< $(SYSTEMDIR)/cpm.flat
	@test $$(wc -c < $(SYSTEMDIR)/cpm.flat) -le 65536 || \
		{ echo "error: flat CP/M system exceeds 64 KiB" >&2; exit 1; }
	dd if=/dev/zero of=$@ bs=65536 count=1 2>/dev/null
	dd if=$(SYSTEMDIR)/cpm.flat of=$@ conv=notrunc 2>/dev/null
	@echo "Z8002-demo boot payload: $@ (128 sectors)"

media-formats:
	@echo $(MEDIA_FORMATS)

clean:
	$(RM) -rf $(BUILDDIR)
