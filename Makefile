# Top-level Makefile: build the CP/M-8000 emulator and generate systems.
#
# Build the emulator (default target):
#   1. Build host tools (xarch, xout2coff) in src/xoututils/
#   2. Extract and convert libcpm.a from x.out to COFF in build/lib/
#   3. Convert the CCP+BDOS object (cpmsys.o) from x.out to COFF
#   4. Assemble the emulator's thin BIOS and build the emulator host program
#
# Generate a bootable system:  make system NAME=<name>   (see scripts/sysgen.sh)
#
# The CP/M-8000 sources in src/cpm8k/ are checked into the repository.

AR = z8k-coff-ar
SRCDIR = src/cpm8k
TOOLDIR = src/xoututils
BUILDDIR = build
XARCH = $(BUILDDIR)/tools/xarch
XOUT2COFF = $(BUILDDIR)/tools/xout2coff
LIBDIR = $(BUILDDIR)/lib
.PHONY: all clean tools lib bios-emu emu regenerate overlay cpm8k-src system

all: emu

# --- Regenerate the CP/M-8000 source tree from the distribution images ---
# Two auditable steps: (1) extract pristine files from the M20 disk images,
# (2) overlay the from-source linker. cpm8k-src runs both.
regenerate:
	scripts/regenerate-cpm8k.sh $(SRCDIR)

overlay:
	scripts/overlay-cpm8k.sh $(SRCDIR)

cpm8k-src: regenerate overlay

# --- System generation: build a bootable system for a chosen BIOS ---
# make system NAME=<name> [BIOS=<dir>] [LOADER=1]   (default M20)
system:
	scripts/sysgen.sh $(if $(BIOS),--bios $(BIOS),) $(if $(LOADER),--loader,) $(if $(NAME),$(NAME),m20)

# --- Build host tools ---
tools: $(XARCH) $(XOUT2COFF)

$(XARCH) $(XOUT2COFF):
	$(MAKE) -C $(TOOLDIR) BUILDDIR=$(abspath $(BUILDDIR)/tools)

# --- Extract archive and convert members ---
$(LIBDIR)/.done: $(XARCH) $(XOUT2COFF)
	mkdir -p $(LIBDIR)
	cp $(SRCDIR)/libcpm.a $(LIBDIR)/libcpm-xout.a
	cd $(LIBDIR) && $(abspath $(XARCH)) libcpm-xout.a
	@cd $(LIBDIR) && for f in *.o; do \
		base=$${f%.o}; \
		mv "$$f" "$${base}.rel"; \
		echo "  xout2coff $${base}.rel -> $${base}.o"; \
		$(abspath $(XOUT2COFF)) "$${base}.rel"; \
	done
	@touch $@

# --- Convert the CCP+BDOS object and create the library ---
$(LIBDIR)/cpmsys.o: $(XOUT2COFF)
	mkdir -p $(LIBDIR)
	cp $(SRCDIR)/cpmsys.rel $(LIBDIR)/cpmsys.rel
	cd $(LIBDIR) && $(abspath $(XOUT2COFF)) cpmsys.rel

$(LIBDIR)/libcpm.a: $(LIBDIR)/.done
	$(AR) rcs $@ $(LIBDIR)/*.o

lib: $(LIBDIR)/libcpm.a $(LIBDIR)/cpmsys.o

# --- Build thin BIOS for emulator ---
bios-emu: lib
	$(MAKE) -C src/cpm8kemu/bios BUILDDIR=$(abspath $(BUILDDIR)/bios-emu) LIBDIR=$(abspath $(LIBDIR))

# --- Build emulator host program ---
emu: bios-emu
	$(MAKE) -C src/cpm8kemu BUILDDIR=$(abspath $(BUILDDIR)/emu) Z8K_BUILDDIR=$(abspath $(BUILDDIR)/z8000_emu)

clean:
	rm -rf $(BUILDDIR)
