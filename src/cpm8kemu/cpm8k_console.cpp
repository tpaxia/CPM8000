#include "cpm8k_console.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>

static struct termios orig_termios;
static bool raw_mode = false;
static bool eof_flag = false;
static bool stdin_is_tty = false;

void console_restore()
{
    if (raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode = false;
    }
}

void console_init()
{
    if (raw_mode) return;
    stdin_is_tty = isatty(STDIN_FILENO);
    if (!stdin_is_tty) {
        fprintf(stderr, "console: stdin not a tty (isatty=0) -- raw mode off\n");
        return;
    }

    if (tcgetattr(STDIN_FILENO, &orig_termios) != 0) {
        fprintf(stderr, "console: tcgetattr failed (%s)\n", strerror(errno));
        return;
    }
    atexit(console_restore);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        fprintf(stderr, "console: tcsetattr failed (%s)\n", strerror(errno));
        return;
    }
    // Confirm the kernel actually applied raw mode.
    struct termios check;
    if (tcgetattr(STDIN_FILENO, &check) == 0 && (check.c_lflag & ICANON))
        fprintf(stderr, "console: WARNING raw mode not applied (ICANON still set)\n");

    raw_mode = true;
}

uint8_t console_in()
{
    uint8_t ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) {
        eof_flag = true;
        return 0x1A; // EOF → Ctrl-Z
    }
    return ch;
}

void console_out(uint8_t ch)
{
    write(STDOUT_FILENO, &ch, 1);
}

bool console_status()
{
    if (eof_flag) return false;
    // When stdin is a pipe, don't report characters ready.
    // The BDOS checks console status during C_WRITE to look for Ctrl-S/C.
    // If we report ready on a pipe, the BDOS will consume pipe data
    // that should be reserved for C_READSTR.
    if (!stdin_is_tty) return false;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv = {0, 0};
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

bool console_eof()
{
    return eof_flag;
}
