# Top-level Makefile: build CP/M-8000 system
#
# Pipeline:
#   1. Build host tools (xarch, xout2coff) in src/xoututils/
#   2. Extract and convert libcpm.a from x.out to COFF in build/lib/
#   3. Convert standalone x.out objects (fpe.o, fpedep.o, cpmsys.o)
#   4. Assemble BIOS and link CP/M-8000 system in src/bios/z8001/
#
# The CP/M-8000 sources in src/cpm8k/ are checked into the repository.

AR = z8k-coff-ar
SRCDIR = src/cpm8k
TOOLDIR = src/xoututils
BUILDDIR = build
XARCH = $(BUILDDIR)/tools/xarch
XOUT2COFF = $(BUILDDIR)/tools/xout2coff
LIBDIR = $(BUILDDIR)/lib
.PHONY: all clean tools lib bios bios-emu emu

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
	$(MAKE) -C src/bios/z8001 BUILDDIR=$(abspath $(BUILDDIR)/bios) LIBDIR=$(abspath $(LIBDIR))

# --- Build thin BIOS for emulator ---
bios-emu: lib
	$(MAKE) -C src/bios/emu BUILDDIR=$(abspath $(BUILDDIR)/bios-emu) LIBDIR=$(abspath $(LIBDIR))

# --- Build emulator host program ---
emu: bios-emu
	$(MAKE) -C src/cpm8kemu BUILDDIR=$(abspath $(BUILDDIR)/emu) Z8K_BUILDDIR=$(abspath $(BUILDDIR)/z8000_emu)

clean:
	rm -rf $(BUILDDIR)
