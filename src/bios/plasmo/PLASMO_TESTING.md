# Running the Plasmo BIOS in MAME

The `plasmo` machine driver (`src/mame/zilog/plasmo.cpp`) lives on the
`z8002-plasmo` branch of the `tpaxia/mame` fork. It models the board from
its schematics (see README.md and TPA.md): Z8002, CPLD UART at `$81`/`$83`,
CompactFlash on the ATA task file at `$10-$1E`, and the port `$85` bank
register.

## Building MAME

From a checkout of that branch (only the `plasmo` driver is linked, but the
full core is still built):

```sh
make SOURCES=src/mame/zilog/plasmo.cpp -j8
```

## Building the system and disk

From the CPM8000 root:

```sh
make plasmo-image
```

This produces `build/system/plasmo/plasmo.boot` (the system image) and
`build/media/plasmo/plasmo-hd/plasmo.chd` (the CF disk).

## Booting

The board's boot ROM is logic inside the CPLD and has not been dumped, so
the driver has no ROM. Instead `-quik <file>` loads a system image into RAM
at `0x0000` and starts the CPU there. This stands in for a bootstrap; it
does not model the real board's boot sequence.

```sh
CPM8000=/path/to/CPM8000
SDL_VIDEODRIVER=dummy ./mame plasmo \
  -quik $CPM8000/build/system/plasmo/plasmo.boot \
  -hard $CPM8000/build/media/plasmo/plasmo-hd/plasmo.chd \
  -video none -sound none -nothrottle -seconds_to_run 8 -log
```

With `-video none` the emulated terminal isn't visible. The driver logs every
byte the BIOS sends to the console as `console tx:` lines in `error.log`:

```sh
grep "console tx" error.log
```

Without `-video none` (and without `SDL_VIDEODRIVER=dummy`) the terminal is
shown normally and can be typed into.

## Scripted commands

`-autoboot_command` types into the console. Put blank lines between
commands: the UART has a single-byte receive latch, as on the real board,
so characters typed while a command is still running are lost (and STAT
aborts on a keypress). Use a copy of the CHD for anything that writes.

```sh
PAD='\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n'
cp $CPM8000/build/media/plasmo/plasmo-hd/plasmo.chd /tmp/scratch.chd
SDL_VIDEODRIVER=dummy ./mame plasmo \
  -quik $CPM8000/build/system/plasmo/plasmo.boot -hard /tmp/scratch.chd \
  -autoboot_delay 2 \
  -autoboot_command "pip copy.sub=wump.sub\n${PAD}type copy.sub\n" \
  -video none -sound none -nothrottle -seconds_to_run 60 -log
```

## Verified

Boot to `A>`, `dir`, `type`, `stat`, `submit` (a two-line `.SUB`), and a
`pip` copy read back with `type` and `dir`.
