// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017 Valve Corporation
// Copyright (c) 2017 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Authors: Mark Young <marky@lunarg.com>
//                  Nat Brown <natb@valvesoftware.com>
//
// RetroArch: reimplemented on libretro-common file_path/retro_dirent,
// matching the std::filesystem semantics this build previously used,
// so <filesystem> and the per-platform fallbacks are gone. Two
// deliberate deltas: FindFilesInPath and GetCanonicalPath return false
// instead of throwing on a missing path, and ParsePathList no longer
// corrupts the middle entries of lists with three or more paths (an
// upstream substr count bug in every branch).

#include "filesystem_utils.hpp"

#include <string.h>

#include <file/file_path.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <vfs/vfs.h>

#if defined(XR_OS_WINDOWS)
#include <direct.h>
#define PATH_SEPARATOR ';'
#define DIRECTORY_SYMBOL '\\'
static bool isDirSep(char c) { return c == '\\' || c == '/'; }
#else
#include <unistd.h>
#define PATH_SEPARATOR ':'
#define DIRECTORY_SYMBOL '/'
static bool isDirSep(char c) { return c == '/'; }
#endif

bool FileSysUtilsIsRegularFile(const std::string& path) {
    int flags = path_stat(path.c_str());
    return (flags & RETRO_VFS_STAT_IS_VALID) && !(flags & RETRO_VFS_STAT_IS_DIRECTORY) &&
           !(flags & RETRO_VFS_STAT_IS_CHARACTER_SPECIAL);
}

bool FileSysUtilsIsDirectory(const std::string& path) { return path_is_directory(path.c_str()); }

bool FileSysUtilsPathExists(const std::string& path) { return (path_stat(path.c_str()) & RETRO_VFS_STAT_IS_VALID) != 0; }

bool FileSysUtilsIsAbsolutePath(const std::string& path) { return path_is_absolute(path.c_str()); }

bool FileSysUtilsGetCurrentPath(std::string& path) {
    std::string buf(PATH_MAX_LENGTH, '\0');
#if defined(XR_OS_WINDOWS)
    if (_getcwd(&buf[0], (int)buf.size() - 1) == NULL) return false;
#else
    if (getcwd(&buf[0], buf.size() - 1) == NULL) return false;
#endif
    path.assign(buf.c_str());
    return true;
}

// std::filesystem::path::parent_path(): trailing separators belong to
// the last component ("a/b/" -> "a/b"), a bare filename has an empty
// parent, and the root keeps itself.
bool FileSysUtilsGetParentPath(const std::string& file_path, std::string& parent_path) {
    std::string::size_type end = file_path.size();
    bool had_trailing_sep = end > 0 && isDirSep(file_path[end - 1]);
    while (end > 0 && isDirSep(file_path[end - 1])) end--;
    if (had_trailing_sep) {
        // "a/b/" decomposes as "a/b" plus an empty final element, and the
        // root is its own parent.
        parent_path = (end == 0) ? file_path.substr(0, 1) : file_path.substr(0, end);
        return true;
    }
    std::string::size_type pos = end;
    while (pos > 0 && !isDirSep(file_path[pos - 1])) pos--;
    if (pos == 0) {
        parent_path.clear();
        return true;
    }
    // strip the separator run, but keep the root ("/", "C:\")
    std::string::size_type root = pos;
    while (root > 1 && isDirSep(file_path[root - 1]) && isDirSep(file_path[root - 2])) root--;
    if (root >= 2 && file_path[root - 2] == ':') {
        parent_path = file_path.substr(0, root);  // "C:\"
        return true;
    }
    if (root == 1) {
        parent_path = file_path.substr(0, 1);  // "/"
        return true;
    }
    parent_path = file_path.substr(0, root - 1);
    return true;
}

// std::filesystem::absolute(): current_path()/p for a relative path,
// with no dot normalisation or symlink resolution.
bool FileSysUtilsGetAbsolutePath(const std::string& path, std::string& absolute) {
    if (path_is_absolute(path.c_str())) {
        absolute = path;
        return true;
    }
    std::string cwd;
    if (!FileSysUtilsGetCurrentPath(cwd)) return false;
    return FileSysUtilsCombinePaths(cwd, path, absolute);
}

bool FileSysUtilsGetCanonicalPath(const std::string& path, std::string& canonical) {
#if defined(XR_OS_WINDOWS)
    // Symbolic links are not important on Windows since the loader uses
    // the registry for indirection instead; keep the upstream no-op.
    canonical = path;
    return true;
#else
    std::string buf(PATH_MAX_LENGTH, '\0');
    strlcpy(&buf[0], path.c_str(), buf.size());
    if (!path_resolve_realpath(&buf[0], buf.size(), true)) return false;
    canonical.assign(buf.c_str());
    return true;
#endif
}

// std::filesystem's operator/: an absolute child replaces the parent,
// an empty parent yields the child unchanged.
bool FileSysUtilsCombinePaths(const std::string& parent, const std::string& child, std::string& combined) {
    if (parent.empty() || path_is_absolute(child.c_str())) {
        combined = child;
        return true;
    }
    std::string::size_type parent_len = parent.size();
    if (isDirSep(parent[parent_len - 1])) parent_len--;
    combined = parent.substr(0, parent_len) + DIRECTORY_SYMBOL + child;
    return true;
}

bool FileSysUtilsParsePathList(std::string& path_list, std::vector<std::string>& paths) {
    std::string::size_type start = 0;
    std::string::size_type location = path_list.find(PATH_SEPARATOR);
    while (location != std::string::npos) {
        paths.push_back(path_list.substr(start, location - start));
        start = location + 1;
        location = path_list.find(PATH_SEPARATOR, start);
    }
    paths.push_back(path_list.substr(start));
    return true;
}

bool FileSysUtilsFindFilesInPath(const std::string& path, std::vector<std::string>& files) {
    struct RDIR* dir = retro_opendir(path.c_str());
    if (!dir) return false;
    while (retro_readdir(dir)) {
        const char* name = retro_dirent_get_name(dir);
        if (!name || !strcmp(name, ".") || !strcmp(name, "..")) continue;
        files.push_back(name);
    }
    retro_closedir(dir);
    return true;
}
