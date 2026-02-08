# Top-level Makefile: build CP/M-8000 system
#
# Pipeline:
#   1. Build host tools (xarch, xout2coff) in src/xoututils/
#   2. Extract and convert libcpm.a from x.out to COFF in build/lib/
#   3. Convert standalone x.out objects (fpe.o, fpedep.o, cpmsys.o)
#   4. Assemble BIOS and link CP/M-8000 system in bios/z8001/
#
# The CP/M-8000 sources in src/cpm8k/ are checked into the repository.
# To re-extract pristine originals from the distribution disk images,
# run: make re-extract

AR = z8k-coff-ar
SRCDIR = src/cpm8k
TOOLDIR = src/xoututils
BUILDDIR = build
XARCH = $(BUILDDIR)/tools/xarch
XOUT2COFF = $(BUILDDIR)/tools/xout2coff
LIBDIR = $(BUILDDIR)/lib
DISTDIR = distribution/CPM_8000_1.1
IMGS = $(wildcard $(DISTDIR)/*.IMG)

.PHONY: all clean tools lib bios bios-emu emu re-extract

all: bios

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

# --- Convert standalone x.out objects and create library ---
$(LIBDIR)/fpe.o: $(XOUT2COFF) | $(LIBDIR)/.done
	cp $(SRCDIR)/fpe.o $(LIBDIR)/fpe.rel
	cd $(LIBDIR) && $(abspath $(XOUT2COFF)) fpe.rel

$(LIBDIR)/fpedep.o: $(XOUT2COFF) | $(LIBDIR)/.done
	cp $(SRCDIR)/fpedep.o $(LIBDIR)/fpedep.rel
	cd $(LIBDIR) && $(abspath $(XOUT2COFF)) fpedep.rel

$(LIBDIR)/cpmsys.o: $(XOUT2COFF)
	mkdir -p $(LIBDIR)
	cp $(SRCDIR)/cpmsys.rel $(LIBDIR)/cpmsys.rel
	cd $(LIBDIR) && $(abspath $(XOUT2COFF)) cpmsys.rel

$(LIBDIR)/libcpm.a: $(LIBDIR)/.done
	$(AR) rcs $@ $(LIBDIR)/*.o

lib: $(LIBDIR)/libcpm.a $(LIBDIR)/fpe.o $(LIBDIR)/fpedep.o $(LIBDIR)/cpmsys.o

# --- Build BIOS and link CP/M-8000 system ---
bios: lib
	$(MAKE) -C bios/z8001 BUILDDIR=$(abspath $(BUILDDIR)/bios) LIBDIR=$(abspath $(LIBDIR))

# --- Build thin BIOS for emulator ---
bios-emu: lib
	$(MAKE) -C bios/emu BUILDDIR=$(abspath $(BUILDDIR)/bios-emu) LIBDIR=$(abspath $(LIBDIR))

# --- Build emulator host program ---
emu: bios-emu
	$(MAKE) -C src/cpm8kemu BUILDDIR=$(abspath $(BUILDDIR)/emu) Z8K_BUILDDIR=$(abspath $(BUILDDIR)/z8000_emu)

clean:
	rm -rf $(BUILDDIR)

# --- Re-extract sources from distribution disk images ---
# Overwrites src/cpm8k/ with pristine files from the distribution.
# Requires cpmtools (cpmls, cpmcp).
re-extract:
	rm -rf $(SRCDIR)
	mkdir -p $(SRCDIR)
	@for img in $(DISTDIR)/*.IMG; do \
		echo "=== $$(basename $$img) ==="; \
		cpmls "$$img" | grep -v ':' | while read -r file; do \
			[ -z "$$file" ] && continue; \
			echo "  $$file"; \
			cpmcp "$$img" "0:$$file" "$(SRCDIR)/$$file"; \
		done; \
	done
	@echo "Done. Review changes with: git diff src/cpm8k/"
