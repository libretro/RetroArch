/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __TASK_CLOUDSYNC_PATH_H
#define __TASK_CLOUDSYNC_PATH_H

/* Kept dependency-free (only <string.h> in the .c) so the regression test
 * samples/tasks/cloudsync/cloudsync_path_safety_test.c can compile the real
 * predicate standalone -- no libretro-common include path required. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * cloud_sync_manifest_key_path:
 * @key : a manifest key as supplied by the (untrusted) cloud-sync server,
 *        in "portable" format using '/' separators.
 *
 * Validates @key and returns the relative path portion -- the substring
 * after the first '/' -- suitable for joining onto the local cloud-sync
 * base directory. Returns NULL when @key is unsafe to use, i.e. when it is
 * NULL, contains no '/', has an empty path portion, has an absolute path
 * portion (leading '/'), or contains a ".." anywhere in the path portion.
 *
 * The NULL return is the traversal/malformed-key reject signal: a hostile
 * manifest must not be able to walk out of the cloud-sync directory via
 * fill_pathname_join_special. The returned pointer, when non-NULL, points
 * into @key (no allocation).
 */
const char *cloud_sync_manifest_key_path(const char *key);

#ifdef __cplusplus
}
#endif

#endif
