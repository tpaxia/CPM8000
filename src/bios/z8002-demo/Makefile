# CP/M-8000 BIOS -- Z8002-demo machine.
#
# A classic (Zilog-pipeline) BIOS package: built with the in-emulator DR/Zilog
# toolchain (zcc, asz8k, ld8k) via sysgen, producing an x.out bios.rel that is
# linked with the non-segmented CCP+BDOS (cpmsys2.rel) into cpm.sys. This is
# an OVERLAY on the stock M20 tree (src/cpm8k); it supplies the whole Z8002
# BIOS:
#   bios.c        -- console/disk glue, memtab, mem_bcp (MMU aperture copies)
#   biosio.8kn    -- Z80-SIO channel B console, ATA task file
#   biosmem.8kn   -- SC #1 memory management via the banking MMU (TPA.md)
#   biostrap.8kn  -- traps, BDOS caller-bank injection, NBANK_I/NBANK_D
#   biosboot.8kn  -- transfer vector, stack and PSA
#
# Build:  make system NAME=z8002-demo
#
# Contract used by scripts/sysgen.sh:
#   make -C src/bios/z8002-demo bios.rel BUILDDIR=<dir> [HOST_CPU=z8001|z8002]
#     -> <dir>/bios.rel
# TARGET_CPU (this package's target CPU) is read from ./TARGET_CPU; HOST_CPU
# (the hosted emulator that runs the toolchain) defaults to it.

ROOT     := $(abspath $(CURDIR)/../../..)
BUILDDIR ?= $(ROOT)/build/bios/z8002-demo
SYSTEMDIR ?= $(ROOT)/build/system/z8002-demo
ROMDIR ?= $(ROOT)/build/roms/z8002demo
XOUT2FLAT := $(ROOT)/build/tools/xout2flat
TARGET_CPU := $(shell sed -n 1p $(CURDIR)/TARGET_CPU)
HOST_CPU ?= $(TARGET_CPU)
MONITOR := $(CURDIR)/z8kmon.bin
MEDIA_FORMATS := z8002-demo-hd

.PHONY: bios.rel system-artifacts media-formats clean

bios.rel:
	BIOS_OVERLAY=$(CURDIR) TARGET_CPU=$(TARGET_CPU) HOST_CPU=$(HOST_CPU) \
		$(ROOT)/scripts/build-bios.sh $(BUILDDIR)

system-artifacts: $(SYSTEMDIR)/z8002-demo.boot $(ROMDIR)/z8kmon.bin

$(ROMDIR)/z8kmon.bin: $(MONITOR)
	@mkdir -p $(@D)
	cp $< $@

$(SYSTEMDIR)/z8002-demo.boot: $(SYSTEMDIR)/cpm.sys
	$(MAKE) -C $(ROOT)/src/xoututils BUILDDIR=$(ROOT)/build/tools xout2flat
	$(XOUT2FLAT) $< $(SYSTEMDIR)/cpm.flat
	@test $$(wc -c < $(SYSTEMDIR)/cpm.flat) -le 45056 || \
		{ echo "error: flat CP/M system (incl. bss) reaches SYSPSA 0xB000 (biosboot.8kn)" >&2; exit 1; }
	dd if=/dev/zero of=$@ bs=65536 count=1 2>/dev/null
	dd if=$(SYSTEMDIR)/cpm.flat of=$@ conv=notrunc 2>/dev/null
	@echo "Z8002-demo boot payload: $@ (128 sectors)"

media-formats:
	@echo $(MEDIA_FORMATS)

clean:
	$(RM) -rf $(BUILDDIR)
