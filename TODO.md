# TODO

## Hosted emulator

- Ctrl-C / Ctrl-D do not quit on an interactive tty. Raw mode clears ICANON
  and ISIG, so they arrive as bytes 0x03 / 0x04. Quitting works only via
  `EXIT`/`QUIT` or stdin EOF; add a control-key quit or SIGINT handler.
- Host-directory drives report a fixed synthetic capacity (the M20 floppy
  geometry, ~270K) for STAT free space instead of the host's real free space.

## Plasmo

- Run CP/M-8000 on the physical board (so far tested only in MAME).
- Provide a way to load the system on the board: the monitor
  (`Z8002mon.txt`) has no command that loads a full 64K image and jumps to
  `0x0000`. Needs a monitor extension or another boot path.
- Check the CF layout (system in LBA 0-127, filesystem from LBA 128) against
  whatever boot path the real board ends up using.
- Commit the Plasmo docs (`src/bios/plasmo/*.md`).
