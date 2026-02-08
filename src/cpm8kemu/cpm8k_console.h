#ifndef CPM8K_CONSOLE_H
#define CPM8K_CONSOLE_H

#include <cstdint>

// Initialize raw terminal mode; registers atexit handler to restore.
void console_init();

// Restore terminal to original settings.
void console_restore();

// Read one character from console (blocking).
uint8_t console_in();

// Write one character to console.
void console_out(uint8_t ch);

// Return true if a character is available (non-blocking).
bool console_status();

// Return true if stdin has reached EOF (pipe closed).
bool console_eof();

#endif // CPM8K_CONSOLE_H
