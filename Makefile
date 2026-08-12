# Top-level Makefile: build the CP/M-8000 emulator and generate systems.
#
# Build the emulator (default target):
#   1. Build host tools (xarch, xout2coff) in src/xoututils/
#   2. Extract and convert libcpm.a from x.out to COFF in build/lib/
#   3. Convert the CCP+BDOS object (cpmsys.o) from x.out to COFF
#   4. Assemble the emulator's thin BIOS and build the emulator host program
#
# Generate guest system binaries: make system NAME=<name> (see scripts/sysgen.sh)
#
# The CP/M-8000 sources in src/cpm8k/ are checked into the repository.

AR = z8k-coff-ar
SRCDIR = src/cpm8k
TOOLDIR = src/xoututils
BUILDDIR = build
XARCH = $(BUILDDIR)/tools/xarch
XOUT2COFF = $(BUILDDIR)/tools/xout2coff
LIBDIR = $(BUILDDIR)/lib
FPE_Z8001_DIR = $(BUILDDIR)/fpe-z8001
.PHONY: all clean tools lib bios-emu bios-emu-z8001 bios-emu-z8002 emu regenerate overlay cpm8k-src system media media-formats z8002-demo-image submit-regression submit-regression-z8001 submit-regression-z8002 fpe-regression fpe-regression-z8001

all: emu

# --- Regenerate the CP/M-8000 source tree from the distribution images ---
# Two auditable steps: (1) extract pristine files from the M20 disk images,
# (2) overlay the from-source linker. cpm8k-src runs both.
regenerate:
	scripts/regenerate-cpm8k.sh $(SRCDIR)

overlay:
	scripts/overlay-cpm8k.sh $(SRCDIR)

cpm8k-src: regenerate overlay

# --- System generation: build guest binaries for a chosen BIOS ---
# make system NAME=<name> [BIOS=<dir>] [LOADER=1]   (default M20)
system:
	scripts/sysgen.sh $(if $(BIOS),--bios $(BIOS),) $(if $(LOADER),--loader,) $(if $(NAME),$(NAME),m20)

# Logical CP/M development media.  The target package declares the formats it
# supports; no boot sectors or emulator-specific containers are generated.
media:
	@test -n "$(FORMAT)" || { echo "usage: make media NAME=<name> FORMAT=<format> [BIOS=<dir>]" >&2; exit 2; }
	scripts/build-media.sh $(if $(NAME),$(NAME),m20) $(FORMAT) $(if $(BIOS),$(BIOS),src/bios/$(if $(NAME),$(NAME),m20))

media-formats:
	@$(MAKE) -s -C $(if $(BIOS),$(BIOS),src/bios/$(if $(NAME),$(NAME),m20)) media-formats

z8002-demo-image:
	scripts/build-z8002-demo-hd.sh

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

$(LIBDIR)/cpmsys2.o: $(XOUT2COFF)
	mkdir -p $(LIBDIR)
	cp $(SRCDIR)/cpmsys2.rel $(LIBDIR)/cpmsys2.rel
	cd $(LIBDIR) && $(abspath $(XOUT2COFF)) cpmsys2.rel

$(FPE_Z8001_DIR)/%.o: $(SRCDIR)/%.o $(XOUT2COFF)
	mkdir -p $(FPE_Z8001_DIR)
	cp $< $(FPE_Z8001_DIR)/$*.rel
	cd $(FPE_Z8001_DIR) && $(abspath $(XOUT2COFF)) $*.rel

$(LIBDIR)/libcpm.a: $(LIBDIR)/.done
	$(AR) rcs $@ $(LIBDIR)/*.o

lib: $(LIBDIR)/libcpm.a $(LIBDIR)/cpmsys.o

# --- Build thin BIOSes for emulator ---
bios-emu: bios-emu-z8001

bios-emu-z8001: lib $(FPE_Z8001_DIR)/fpe.o $(FPE_Z8001_DIR)/fpedep.o
	$(MAKE) -C src/cpm8kemu/bios-z8001 BUILDDIR=$(abspath $(BUILDDIR)/bios-emu-z8001) LIBDIR=$(abspath $(LIBDIR)) FPEDIR=$(abspath $(FPE_Z8001_DIR))

bios-emu-z8002: lib $(LIBDIR)/cpmsys2.o
	$(MAKE) -C src/cpm8kemu/bios-z8002 BUILDDIR=$(abspath $(BUILDDIR)/bios-emu-z8002) LIBDIR=$(abspath $(LIBDIR))

# --- Build emulator host program (cross-platform CMake build) ---
emu: bios-emu-z8001 bios-emu-z8002
	cmake -S . -B $(BUILDDIR)/emu -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILDDIR)/emu

submit-regression: submit-regression-z8001 submit-regression-z8002

submit-regression-z8001: emu
	scripts/test-submit-regression.sh z8001

submit-regression-z8002: emu
	scripts/test-submit-regression.sh z8002

fpe-regression: fpe-regression-z8001

fpe-regression-z8001: emu
	scripts/test-fpe.sh z8001

clean:
	rm -rf $(BUILDDIR)
