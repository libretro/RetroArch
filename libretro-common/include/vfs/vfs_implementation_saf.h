/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation_saf.h).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __LIBRETRO_SDK_VFS_IMPLEMENTATION_SAF_H
#define __LIBRETRO_SDK_VFS_IMPLEMENTATION_SAF_H

#include <stdio.h>
#include <stdint.h>
#include <jni.h>
#include <libretro.h>
#include <retro_environment.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>

RETRO_BEGIN_DECLS

typedef struct libretro_vfs_implementation_saf_dir
{
   jobject directory_object;
   jstring dirent_name_object;
   const char *dirent_name;
   bool dirent_is_dir;
} libretro_vfs_implementation_saf_dir;

struct libretro_vfs_implementation_saf_path_split_result
{
   char *tree;
   char *path;
};

/*
 * Initialize this VFS backend. This must be called prior to using any of the VFS operations provided by this backend.
 * get_jni_env should be a function that returns the JNIEnv for the current thread.
 * activity_object should be the current Android android.app.Activity.
 */
bool retro_vfs_init_saf(JNIEnv *(*get_jni_env)(void), jobject activity_object);

/*
 * Deinitialize this VFS backend.
 */
bool retro_vfs_deinit_saf(void);

/*
 * Split a serialized "saf://" path string into tree and path components for use with this backend.
 * Returns true if successful or false if not.
 * The results will be returned in `out` and must be freed by the caller.
 */
bool retro_vfs_path_split_saf(struct libretro_vfs_implementation_saf_path_split_result *out, const char *serialized_path);

/*
 * Join the tree and path components into a single serialized "saf://" path string.
 * Returns the serialized path if successful or NULL if not.
 * The returned path must be freed by the caller.
 */
char *retro_vfs_path_join_saf(const char *tree, const char *path);

/*
 * Open a file, returning its file descriptor if successful or -1 if not.
 * The file descriptor can be operated on using the POSIX file system API (`read()`, `write()`, `lseek()`, `close()`, etc).
 * You can also turn the file descriptor into a `FILE *` by calling `fdopen()` on it.
 */
int retro_vfs_file_open_saf(const char *tree, const char *path, unsigned mode);

int retro_vfs_file_remove_saf(const char *tree, const char *path);

int retro_vfs_file_rename_saf(const char *old_tree, const char *old_path, const char *new_tree, const char *new_path);

int retro_vfs_stat_saf(const char *tree, const char *path, int32_t *size);

int retro_vfs_mkdir_saf(const char *tree, const char *dir);

libretro_vfs_implementation_saf_dir *retro_vfs_opendir_saf(const char *tree, const char *dir, bool include_hidden);

bool retro_vfs_readdir_saf(libretro_vfs_implementation_saf_dir *dirstream);

const char *retro_vfs_dirent_get_name_saf(libretro_vfs_implementation_saf_dir *dirstream);

bool retro_vfs_dirent_is_dir_saf(libretro_vfs_implementation_saf_dir *dirstream);

int retro_vfs_closedir_saf(libretro_vfs_implementation_saf_dir *dirstream);

RETRO_END_DECLS

#endif
