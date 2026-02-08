# Makefile for CP/M-8000 BIOS
# Assembles BIOS sources and links with cpmsys.o and libcpm.a from build/lib/

AS = z8k-coff-as
LD = z8k-coff-ld
OBJCOPY = z8k-coff-objcopy

# Build output and library directories (overridden by top-level Makefile)
BUILDDIR ?= ../../build/bios
LIBDIR ?= ../../build/lib

SRC_OBJS = biosboot.o biosif.o biostrap.o biosmem.o \
       bios.o bioscall1.o bioscall2.o bioscall3.o bioscall4.o \
       i8251.o \
       diskio.o put.o

OBJS = $(addprefix $(BUILDDIR)/,$(SRC_OBJS))

.PHONY: all clean dirs

all: $(BUILDDIR)/cpm8k.bin

dirs:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/cpm8k.bin: $(OBJS) biosdef.s $(LIBDIR)/cpmsys.o $(LIBDIR)/libcpm.a \
           $(LIBDIR)/fpe.o $(LIBDIR)/fpedep.o cpmsys.x | dirs
	$(LD) -mz8002 -T cpmsys.x -L$(LIBDIR) -o $(BUILDDIR)/cpm8k \
		$(OBJS) $(LIBDIR)/fpe.o $(LIBDIR)/fpedep.o \
		$(LIBDIR)/cpmsys.o -lcpm
	$(OBJCOPY) -S -O binary $(BUILDDIR)/cpm8k $(BUILDDIR)/cpm8k.bin

$(BUILDDIR)/%.o: %.s | dirs
	$(AS) -z8002 -o $@ $<

clean:
	$(RM) -f $(BUILDDIR)/cpm8k $(BUILDDIR)/cpm8k.bin $(OBJS)
