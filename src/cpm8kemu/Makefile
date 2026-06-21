# Makefile for CP/M-8000 emulator host program

CXX ?= c++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
Z8K_EMU = ../../z8000_emu

# Build output directory
BUILDDIR ?= ../../build/emu

# Z8000 emulator build directory (may be overridden by parent)
Z8K_BUILDDIR ?= ../../build/z8000_emu

# Include paths: z8000_emu reorganised layout (lib/include/z8000).
# lib/include lets the headers' internal <z8000/...> includes resolve;
# lib/include/z8000 lets our sources use "z8000.h" directly.
INCLUDES = -I$(Z8K_EMU)/lib/include -I$(Z8K_EMU)/lib/include/z8000

SOURCES = main.cpp cpm8k_bdos.cpp cpm8k_bios.cpp cpm8k_console.cpp cpm8k_file.cpp
OBJECTS = $(addprefix $(BUILDDIR)/,$(SOURCES:.cpp=.o))
TARGET = $(BUILDDIR)/cpm8k

# Z8000 emulator library (CMake puts archives under <build>/lib)
Z8K_LIB = $(Z8K_BUILDDIR)/lib/libz8000.a

.PHONY: all clean z8klib dirs

all: $(TARGET)

dirs:
	@mkdir -p $(BUILDDIR)

# Build z8000 library if needed (CMake build, library target only)
z8klib:
	cmake -S $(Z8K_EMU) -B $(Z8K_BUILDDIR) -DCMAKE_BUILD_TYPE=Release -DBUILD_DRIVER=OFF -DBUILD_TOOLS=OFF
	cmake --build $(Z8K_BUILDDIR) --target z8000

$(Z8K_LIB): z8klib

$(TARGET): dirs $(OBJECTS) $(Z8K_LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS) $(Z8K_LIB)

$(BUILDDIR)/%.o: %.cpp | dirs
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	$(RM) -f $(TARGET) $(OBJECTS)
