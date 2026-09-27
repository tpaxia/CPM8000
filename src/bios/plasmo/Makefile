# CP/M-8000 BIOS -- Plasmo machine.
#
# A classic (Zilog-pipeline) BIOS package: built with the in-emulator DR/Zilog
# toolchain (zcc, asz8k, ld8k) via sysgen, producing an x.out bios.rel that is
# linked with the non-segmented CCP+BDOS (cpmsys2.rel) into cpm.sys. This is
# an OVERLAY on the stock M20 tree (src/cpm8k); it supplies the whole Z8002
# BIOS:
#   bios.c        -- console/disk glue, memtab, mem_bcp (bank-window copies)
#   biosio.8kn    -- CPLD UART console ($81/$83), CompactFlash task file
#   biosmem.8kn   -- SC #1 memory management via the port $85 window (TPA.md)
#   biostrap.8kn  -- traps, BDOS caller-bank injection
#   biosboot.8kn  -- transfer vector, stack and PSA
#
# Build:  make system NAME=plasmo
#
# Contract used by scripts/sysgen.sh:
#   make -C src/bios/plasmo bios.rel BUILDDIR=<dir> [HOST_CPU=z8001|z8002]
#     -> <dir>/bios.rel
# TARGET_CPU (this package's target CPU) is read from ./TARGET_CPU; HOST_CPU
# (the hosted emulator that runs the toolchain) defaults to it.

ROOT     := $(abspath $(CURDIR)/../../..)
BUILDDIR ?= $(ROOT)/build/bios/plasmo
SYSTEMDIR ?= $(ROOT)/build/system/plasmo
XOUT2FLAT := $(ROOT)/build/tools/xout2flat
TARGET_CPU := $(shell sed -n 1p $(CURDIR)/TARGET_CPU)
HOST_CPU ?= $(TARGET_CPU)
MEDIA_FORMATS := plasmo-hd

.PHONY: bios.rel system-artifacts media-formats clean

bios.rel:
	BIOS_OVERLAY=$(CURDIR) TARGET_CPU=$(TARGET_CPU) HOST_CPU=$(HOST_CPU) \
		$(ROOT)/scripts/build-bios.sh $(BUILDDIR)

# There is no ROM/monitor binary to install: the board's monitor cannot load
# a full system image yet (see README.md). system-artifacts only produces the
# flat boot payload, and fails if the image (incl. bss) reaches SYSPSA.
system-artifacts: $(SYSTEMDIR)/plasmo.boot

$(SYSTEMDIR)/plasmo.boot: $(SYSTEMDIR)/cpm.sys
	$(MAKE) -C $(ROOT)/src/xoututils BUILDDIR=$(ROOT)/build/tools xout2flat
	$(XOUT2FLAT) $< $(SYSTEMDIR)/cpm.flat
	@test $$(wc -c < $(SYSTEMDIR)/cpm.flat) -le 45056 || \
		{ echo "error: flat CP/M system (incl. bss) reaches SYSPSA 0xB000 (biosboot.8kn)" >&2; exit 1; }
	dd if=/dev/zero of=$@ bs=65536 count=1 2>/dev/null
	dd if=$(SYSTEMDIR)/cpm.flat of=$@ conv=notrunc 2>/dev/null
	@echo "Plasmo boot payload: $@ (128 sectors)"

media-formats:
	@echo $(MEDIA_FORMATS)

clean:
	$(RM) -rf $(BUILDDIR)
