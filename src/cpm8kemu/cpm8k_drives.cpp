#include "cpm8k_drives.h"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {

struct DriveCfg {
    DriveBackend backend = DriveBackend::NONE;
    std::string  path;
};

DriveCfg     s_drives[NUM_DRIVES];
const std::string s_empty;

} // namespace

void drive_set(int idx, DriveBackend backend, const std::string& path)
{
    if (idx < 0 || idx >= NUM_DRIVES) return;
    s_drives[idx].backend = backend;
    s_drives[idx].path = path;
}

DriveBackend drive_backend(int idx)
{
    if (idx < 0 || idx >= NUM_DRIVES) return DriveBackend::NONE;
    return s_drives[idx].backend;
}

const std::string& drive_path(int idx)
{
    if (idx < 0 || idx >= NUM_DRIVES) return s_empty;
    return s_drives[idx].path;
}

uint16_t drive_login_vector()
{
    uint16_t v = 0;
    for (int i = 0; i < NUM_DRIVES; i++)
        if (drive_present(i)) v |= (1u << i);
    return v;
}

// "X=dir:path" or "X=img:path"  (X = drive letter A..P, case-insensitive)
bool drive_parse_spec(const char* spec)
{
    if (!spec) return false;

    char letter = std::toupper((unsigned char)spec[0]);
    if (letter < 'A' || letter > 'P' || spec[1] != '=') {
        fprintf(stderr, "drive spec '%s': expected form X=dir:path or X=img:path\n", spec);
        return false;
    }
    int idx = letter - 'A';

    const char* rest = spec + 2;
    DriveBackend backend;
    if (strncmp(rest, "dir:", 4) == 0) {
        backend = DriveBackend::HOST_DIR;
        rest += 4;
    } else if (strncmp(rest, "img:", 4) == 0) {
        backend = DriveBackend::IMAGE;
        rest += 4;
    } else {
        fprintf(stderr, "drive spec '%s': backend must be 'dir:' or 'img:'\n", spec);
        return false;
    }

    if (*rest == '\0') {
        fprintf(stderr, "drive spec '%s': missing path\n", spec);
        return false;
    }

    drive_set(idx, backend, rest);
    return true;
}
