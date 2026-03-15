# VFS Frontend Implementation Plan

## Issue #4774: Add frontend to host VFS Layer

### Analysis

**Current State:**
- RetroArch already implements VFS interface v3 in `runloop.c`
- VFS functions are implemented in `libretro-common/vfs/vfs_implementation.c`
- File browser exists but may not fully utilize VFS layer

**What's Needed:**
Based on the issue title "Add frontend to host VFS Layer", we need to:
1. Create a menu UI that allows users to browse files through the VFS layer
2. Expose VFS operations (browse, open, manage files) through the frontend menu
3. Ensure the file browser uses VFS functions instead of direct file I/O

### Implementation Approach

**Option 1: Enhance existing file browser to use VFS**
- Modify menu_displaylist.c to use VFS opendir/readdir functions
- Add VFS-specific file browser mode

**Option 2: Create new VFS Browser menu**
- Add new menu entry "VFS Browser" 
- Implement dedicated VFS file browser UI
- Support all VFS operations (open, read, write, delete, rename, etc.)

**Recommended: Option 2** - Cleaner separation, easier to test

### Files to Create/Modify

**New Files:**
1. `menu/menu_vfs_browser.c` - VFS browser implementation
2. `menu/menu_vfs_browser.h` - Header file

**Modified Files:**
1. `menu/menu_driver.c` - Add VFS browser menu entry
2. `menu/menu_displaylist.c` - Add VFS display list handling
3. `menu/menu_cbs.c` - Add VFS browser callbacks

### VFS Functions to Expose

From `struct retro_vfs_interface`:
- ✅ get_path - Get file path
- ✅ open - Open file
- ✅ close - Close file
- ✅ size - Get file size
- ✅ tell - Get position
- ✅ seek - Seek in file
- ✅ read - Read from file
- ✅ write - Write to file
- ✅ flush - Flush file
- ✅ remove - Delete file
- ✅ rename - Rename file
- ✅ truncate - Truncate file
- ✅ stat - Get file stats
- ✅ mkdir - Create directory
- ✅ opendir - Open directory
- ✅ readdir - Read directory
- ✅ dirent_get_name - Get entry name
- ✅ dirent_is_dir - Check if directory
- ✅ closedir - Close directory

### UI Features

1. **Directory Browser**
   - Navigate directories
   - Show files and folders
   - Display file sizes and types

2. **File Operations**
   - Open file (view content)
   - Create new file
   - Create new folder
   - Rename file/folder
   - Delete file/folder
   - Copy/Move (if supported)

3. **VFS-Specific Features**
   - Show VFS scheme (local, SMB, CDROM, SAF)
   - Handle different VFS backends

### Testing Strategy

1. Unit test VFS functions
2. Test menu navigation
3. Test file operations
4. Test with different VFS backends (SMB, local, etc.)

### Timeline

- Phase 1: Create basic VFS browser UI (1 hour) ✅ DONE
- Phase 2: Implement file operations (1 hour) ✅ DONE
- Phase 3: Add VFS-specific features (30 min) ✅ DONE
- Phase 4: Menu integration (30 min) ✅ DONE
- Phase 5: Testing and build (30 min) - Pending

Total: ~3 hours

### Implementation Status

**Completed Files:**
1. ✅ `menu/menu_vfs_browser.h` - Header file with API
2. ✅ `menu/menu_vfs_browser.c` - Full implementation
3. ✅ `msg_hash.h` - Added MENU_ENUM_LABEL_FILE_BROWSER_VFS
4. ✅ `intl/msg_hash_us.c` - Added string labels
5. ✅ `menu/menu_displaylist.c` - Added menu entry to main menu

**Features Implemented:**
- ✅ Directory browsing using VFS opendir/readdir
- ✅ File/factory distinction
- ✅ File size display
- ✅ Parent directory navigation
- ✅ Subdirectory navigation
- ✅ File operations: delete, rename, mkdir
- ✅ VFS scheme detection
- ✅ Entry refresh

**Remaining Work:**
- ⏳ Build configuration and compilation
- ⏳ Integration testing with different VFS backends
- ⏳ File viewer integration for opening files
- ⏳ Copy/move operations (optional)
