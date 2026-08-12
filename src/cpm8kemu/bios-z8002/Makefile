AS = z8k-coff-as
LD = z8k-coff-ld

BUILDDIR ?= ../../../build/bios-emu-z8002
LIBDIR ?= ../../../build/lib
FPEDIR ?= ../../../build/fpe-z8002

OBJS = $(BUILDDIR)/biosboot.o $(BUILDDIR)/biostrap.o

.PHONY: all dirs clean

all: $(BUILDDIR)/cpm.sys

dirs:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/cpm.sys: $(OBJS) $(LIBDIR)/cpmsys2.o $(FPEDIR)/fpe.o $(FPEDIR)/fpedep.o cpmsys.x | dirs
	$(LD) -mz8002 -T cpmsys.x -L$(LIBDIR) -o $@ \
		$(OBJS) $(LIBDIR)/cpmsys2.o $(FPEDIR)/fpedep.o $(FPEDIR)/fpe.o -lcpm

$(BUILDDIR)/%.o: %.s biosdef.s | dirs
	$(AS) -z8002 -I. -o $@ $<

clean:
	$(RM) -f $(BUILDDIR)/cpm.sys $(OBJS)
