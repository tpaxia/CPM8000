# Makefile for CP/M-8000 emulator thin BIOS
# Links biosboot + biostrap with CCP (cpmsys.o) from build/lib/ to produce cpm.sys

AS = z8k-coff-as
LD = z8k-coff-ld
OBJCOPY = z8k-coff-objcopy

# Build output and library directories (overridden by top-level Makefile)
BUILDDIR ?= ../../../build/bios-emu-z8001
LIBDIR ?= ../../../build/lib
FPEDIR ?= ../../../build/fpe-z8001

SRC_OBJS = biosboot.o biostrap.o

OBJS = $(addprefix $(BUILDDIR)/,$(SRC_OBJS))

.PHONY: all clean dirs

all: $(BUILDDIR)/cpm.sys

dirs:
	@mkdir -p $(BUILDDIR)

# Link BIOS + CCP into COFF, then extract binary
$(BUILDDIR)/cpm.sys: $(OBJS) $(LIBDIR)/cpmsys.o $(FPEDIR)/fpe.o $(FPEDIR)/fpedep.o cpmsys.x | dirs
	$(LD) -mz8002 -T cpmsys.x -L$(LIBDIR) -o $(BUILDDIR)/cpmsys.coff \
		$(OBJS) $(LIBDIR)/cpmsys.o $(FPEDIR)/fpedep.o $(FPEDIR)/fpe.o -lcpm
	$(OBJCOPY) -S -O binary $(BUILDDIR)/cpmsys.coff $(BUILDDIR)/cpm.bin
	cp $(BUILDDIR)/cpmsys.coff $(BUILDDIR)/cpm.sys

$(BUILDDIR)/%.o: %.s | dirs
	$(AS) -z8002 -o $@ $<

clean:
	$(RM) -f $(BUILDDIR)/cpmsys.coff $(BUILDDIR)/cpm.sys $(BUILDDIR)/cpm.bin $(OBJS)
