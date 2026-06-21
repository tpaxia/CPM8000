#ifndef CPM8K_DRIVES_H
#define CPM8K_DRIVES_H

#include <string>
#include <cstdint>

// Per-drive backend selection. The emulator can mix host directories
// ("local disk", serviced at the BDOS file level by CpmFileSystem) and
// disk image files ("image", serviced at the BIOS sector level by
// bios_handler) on a per-drive basis.
enum class DriveBackend { NONE, HOST_DIR, IMAGE };

// CP/M-8000 supports up to 16 drives (A..P).
static constexpr int NUM_DRIVES = 16;

// Configure / query the global drive table (index 0 = A .. 15 = P).
void drive_set(int idx, DriveBackend backend, const std::string& path);
DriveBackend drive_backend(int idx);
const std::string& drive_path(int idx);

inline bool drive_is_host(int idx)  { return drive_backend(idx) == DriveBackend::HOST_DIR; }
inline bool drive_is_image(int idx) { return drive_backend(idx) == DriveBackend::IMAGE; }
inline bool drive_present(int idx)  { return drive_backend(idx) != DriveBackend::NONE; }

// Login vector with one bit set per configured drive (bit 0 = A).
uint16_t drive_login_vector();

// Parse a "-d" spec of the form "X=dir:path" or "X=img:path".
// Returns true and configures the table on success; false on parse error.
bool drive_parse_spec(const char* spec);

#endif // CPM8K_DRIVES_H
