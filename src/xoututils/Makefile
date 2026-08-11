CC = cc
CFLAGS = -Wall -Wextra -O2 -std=c99

# Build output directory
BUILDDIR ?= ../../build/tools

TOOLS = $(BUILDDIR)/xarch $(BUILDDIR)/xout2coff $(BUILDDIR)/xoutdump \
        $(BUILDDIR)/xout2flat

all: $(TOOLS)

dirs:
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/xarch: xarch.c xout.h | dirs
	$(CC) $(CFLAGS) -o $@ xarch.c

$(BUILDDIR)/xout2coff: xout2coff.c xout.h coff.h | dirs
	$(CC) $(CFLAGS) -o $@ xout2coff.c

$(BUILDDIR)/xoutdump: xoutdump.c xout.h | dirs
	$(CC) $(CFLAGS) -o $@ xoutdump.c

$(BUILDDIR)/xout2flat: xout2flat.c xout.h | dirs
	$(CC) $(CFLAGS) -o $@ xout2flat.c

# Convenience targets (allow "make xarch" etc.)
xarch: $(BUILDDIR)/xarch
xout2coff: $(BUILDDIR)/xout2coff
xoutdump: $(BUILDDIR)/xoutdump
xout2flat: $(BUILDDIR)/xout2flat

clean:
	rm -f $(TOOLS)

.PHONY: all clean dirs xarch xout2coff xoutdump xout2flat
