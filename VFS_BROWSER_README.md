# VFS Browser - Virtual File System Browser for RetroArch

## Overview

The VFS Browser is a new menu feature in RetroArch that provides a file browser interface backed by the Virtual File System (VFS) layer. This allows users to browse and manage files not only on the local filesystem but also through various VFS backends such as:

- Local filesystem
- Network shares (SMB/CIFS)
- CD-ROM drives
- SAF (Storage Access Framework) on Android
- Other VFS implementations

## Features

### Core Functionality

- **Directory Navigation**: Browse directories using VFS opendir/readdir
- **File Information**: Display file names, types (file/directory), and sizes
- **Parent Navigation**: Navigate up to parent directories
- **Subdirectory Access**: Enter subdirectories by selection

### File Operations

- **Delete Files/Folders**: Remove files and empty directories
- **Rename**: Rename files and directories
- **Create Directory**: Create new folders
- **File Info**: View file metadata

### VFS-Specific Features

- **Scheme Detection**: Shows which VFS backend is being used (local, SMB, CDROM, SAF)
- **Backend Agnostic**: Works with any VFS implementation that RetroArch supports
- **Unified Interface**: Same UI regardless of storage backend

## Implementation Details

### Files Added

1. **menu/menu_vfs_browser.h** - Public API header
2. **menu/menu_vfs_browser.c** - Implementation

### Files Modified

1. **msg_hash.h** - Added MENU_ENUM_LABEL_FILE_BROWSER_VFS enum
2. **intl/msg_hash_us.c** - Added user-visible strings
3. **menu/menu_displaylist.c** - Integrated VFS Browser into main menu

### API Functions

```c
// Initialize/deinitialize
bool menu_vfs_browser_init(void);
void menu_vfs_browser_deinit(void);

// Navigation
bool menu_vfs_browser_open(const char *path);
bool menu_vfs_browser_parent(void);
bool menu_vfs_browser_subdir(const char *name);
const char* menu_vfs_browser_get_path(void);

// File operations
bool menu_vfs_browser_operation(unsigned operation, const char *name, const char *new_name);
void menu_vfs_browser_refresh(void);

// Entry access
size_t menu_vfs_browser_get_count(void);
const char* menu_vfs_browser_get_name(size_t index);
bool menu_vfs_browser_is_directory(size_t index);
uint64_t menu_vfs_browser_get_size(size_t index);
enum vfs_scheme menu_vfs_browser_get_scheme(void);
```

### Operation Codes

- `0` - Info (get file information)
- `1` - Open (open file with viewer)
- `2` - Delete (remove file/directory)
- `3` - Rename (rename file/directory)
- `4` - Create Directory (mkdir)

## Usage

### Accessing the VFS Browser

The VFS Browser appears in the main menu under "Load Content" section:

1. Open RetroArch
2. Navigate to "Load Content"
3. Select "VFS Browser"
4. Browse files using the VFS layer

### Keyboard/Gamepad Controls

Standard RetroArch menu controls apply:
- **Up/Down**: Navigate entries
- **Enter/A**: Select/Open
- **Back/B**: Go to parent directory
- **Context Menu**: File operations (delete, rename, etc.)

## Technical Notes

### VFS Integration

The VFS Browser uses the following VFS functions from `libretro-common/vfs/`:

- `retro_vfs_opendir_impl()` - Open directory
- `retro_vfs_readdir_impl()` - Read directory entries
- `retro_vfs_dirent_get_name_impl()` - Get entry name
- `retro_vfs_dirent_is_dir_impl()` - Check if directory
- `retro_vfs_stat_impl()` - Get file stats/size
- `retro_vfs_remove_impl()` - Delete file
- `retro_vfs_rename_impl()` - Rename file
- `retro_vfs_mkdir_impl()` - Create directory
- `retro_vfs_closedir_impl()` - Close directory

### Memory Management

The browser maintains an internal cache of directory entries:
- Entries are cached when a directory is opened
- Cache is refreshed on navigation or after file operations
- Memory is properly freed on deinitialization

### Error Handling

- Failed directory opens are logged but don't crash
- Invalid operations return false
- Out-of-bounds access is checked

## Future Enhancements

Potential improvements for future versions:

1. **File Copy/Move**: Add copy and move operations
2. **Multi-select**: Select multiple files for batch operations
3. **Search**: Search within current directory
4. **Sort Options**: Sort by name, size, date
5. **File Preview**: Preview file contents
6. **Bookmarks**: Save favorite locations
7. **Drag and Drop**: For platforms that support it

## Testing

### Test Cases

1. **Local Filesystem**
   - Browse local directories
   - Create/delete/rename files
   - Verify file sizes

2. **SMB Network Share** (if configured)
   - Connect to SMB share
   - Browse remote files
   - Test file operations

3. **CD-ROM** (if available)
   - Browse CD contents
   - Verify read-only behavior

4. **Android SAF** (on Android)
   - Access scoped storage
   - Test file operations

### Build Instructions

```bash
cd RetroArch
./configure
make -j$(nproc)
```

The VFS Browser will be included automatically in the build.

## Compatibility

- **Minimum VFS Version**: V3 (includes directory operations)
- **Platform Support**: All platforms supported by RetroArch
- **Menu Drivers**: Compatible with all menu drivers (rgui, glui, xmb, etc.)

## License

This implementation is part of RetroArch and is licensed under the GNU General Public License v3.0 or later.

## Credits

- Implementation: Based on RetroArch VFS infrastructure
- VFS Layer: libretro-common VFS implementation
- Menu System: RetroArch menu driver

## References

- Issue #4774: Add frontend to host VFS Layer
- VFS API: `libretro-common/include/vfs/vfs.h`
- VFS Implementation: `libretro-common/vfs/vfs_implementation.c`
- Menu System: `menu/menu_driver.c`
