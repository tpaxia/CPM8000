# CP/M-8000 BIOS -- Plasmo machine.
#
# A classic (Zilog-pipeline) BIOS package: built with the in-emulator DR/Zilog
# toolchain (zcc, asz8k, ld8k) via sysgen, producing an x.out bios.rel that is
# linked with the CCP+BDOS into cpm.sys. This is an OVERLAY on the stock M20
# tree (src/cpm8k): it overrides only the files that change for the Z8002
# machine:
#   bios.c        -- Z80-SIO console, ATA disk, Z8002 cold boot
#   biosmem.8kn   -- SC #1 memory management via the banking MMU (see TPA.md)
#   (+ any I/O-def overrides)
#
# Build:  make system NAME=plasmo
#
# Contract used by scripts/sysgen.sh:
#   make -C src/bios/plasmo bios.rel BUILDDIR=<dir> -> <dir>/bios.rel

ROOT     := $(abspath $(CURDIR)/../../..)
BUILDDIR ?= $(ROOT)/build/bios/plasmo
SYSTEMDIR ?= $(ROOT)/build/system/plasmo
XOUT2FLAT := $(ROOT)/build/tools/xout2flat
MEDIA_FORMATS := plasmo-hd

.PHONY: bios.rel system-artifacts media-formats clean

bios.rel:
	BIOS_OVERLAY=$(CURDIR) EMU_MODEL=z8002 $(ROOT)/scripts/build-bios.sh $(BUILDDIR)

# Unlike Z8002-demo, there is no ROM/monitor binary to install here: this
# package doesn't include a compiled Plasmo monitor (see README.md's "Status
# and open items" -- the real board's monitor doesn't yet have a path that
# loads a full 64K system image and jumps to it). system-artifacts only
# produces the flat boot payload.
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
