CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99

# Build output directory
BUILDDIR ?= ../../build/tools

TOOLS = $(BUILDDIR)/xarch $(BUILDDIR)/xout2coff $(BUILDDIR)/xoutdump

all: $(TOOLS)

dirs:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/xarch: xarch.c xout.h | dirs
	$(CC) $(CFLAGS) -o $@ xarch.c

$(BUILDDIR)/xout2coff: xout2coff.c xout.h coff.h | dirs
	$(CC) $(CFLAGS) -o $@ xout2coff.c

$(BUILDDIR)/xoutdump: xoutdump.c xout.h | dirs
	$(CC) $(CFLAGS) -o $@ xoutdump.c

# Convenience targets (allow "make xarch" etc.)
xarch: $(BUILDDIR)/xarch
xout2coff: $(BUILDDIR)/xout2coff
xoutdump: $(BUILDDIR)/xoutdump

clean:
	rm -f $(TOOLS)

.PHONY: all clean dirs xarch xout2coff xoutdump
