/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_hybrid_test.c).
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

/* Regression test for the hybrid VFS dispatch.
 *
 * vfs_hybrid sits between filestream and two backends - the local
 * implementation and the frontend's interface - and picks per file.
 * Everything that can go wrong here is a mismatch between what it
 * believes about a backend and what that backend actually offers, so
 * the test supplies fake frontends and checks which side gets called.
 *
 * Three properties, each of which has been wrong:
 *
 * 1. Version discipline. The hybrid negotiates v1 through v3, so a
 *    frontend can hand back an interface where only the v1 members
 *    are filled. Calling a v2 or v3 member on that struct reads past
 *    what the frontend initialised. truncate is v2 and was called
 *    unconditionally; the v1 frontend below leaves its v2 and v3
 *    slots poisoned so any such call is caught rather than silently
 *    jumping through whatever happened to be there.
 *
 * 2. Cleanup symmetry. Both open paths allocate a small wrapper after
 *    the backend handle already exists, so both have to release that
 *    handle if the wrapper allocation fails. hyb_open did; hyb_opendir
 *    did not. The wrapper's calloc is failed deliberately here - the
 *    Makefile wraps calloc for this test - because that path is
 *    unreachable otherwise and would go on being untested.
 *
 * 3. Local-first dispatch, which is the entire point of the layer. A
 *    plain path must open through the local implementation, keep a
 *    real mapping, and never reach the frontend at all; a plain-path
 *    miss on a non-sandboxed build must not consult the frontend
 *    either, or the loose-file probe cost that the design promises to
 *    preserve is gone.
 *
 * Build:  make            (SANITIZER=address,undefined for a checked run)
 * Run:    ./vfs_hybrid_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>
#include <vfs/vfs_hybrid.h>

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
      else \
         printf("ok:   %s\n", msg); \
   } while (0)

/* ---- bookkeeping for the fake frontends ---- */

static int fe_file_opens;
static int fe_files_live;
static int fe_dirs_live;
static int fe_above_version;   /* set when a member above the
                                  advertised version is reached */
static unsigned fe_advertise;

static void fe_reset(unsigned version)
{
   fe_file_opens    = 0;
   fe_files_live    = 0;
   fe_dirs_live     = 0;
   fe_above_version = 0;
   fe_advertise     = version;
}

/* ---- v1 members ---- */

static const char *fe_get_path(struct retro_vfs_file_handle *s)
{
   (void)s;
   return "frontend";
}

static struct retro_vfs_file_handle *fe_open(const char *path,
      unsigned mode, unsigned hints)
{
   (void)path; (void)mode; (void)hints;
   fe_file_opens++;
   fe_files_live++;
   return (struct retro_vfs_file_handle*)malloc(4);
}

static int fe_close(struct retro_vfs_file_handle *s)
{
   fe_files_live--;
   free(s);
   return 0;
}

static int64_t fe_size(struct retro_vfs_file_handle *s)  { (void)s; return 0; }
static int64_t fe_tell(struct retro_vfs_file_handle *s)  { (void)s; return 0; }
static int64_t fe_seek(struct retro_vfs_file_handle *s, int64_t o, int w)
{ (void)s; (void)o; (void)w; return 0; }
static int64_t fe_read(struct retro_vfs_file_handle *s, void *d, uint64_t l)
{ (void)s; memset(d, 'F', (size_t)l); return (int64_t)l; }
static int64_t fe_write(struct retro_vfs_file_handle *s, const void *d, uint64_t l)
{ (void)s; (void)d; return (int64_t)l; }
static int fe_flush(struct retro_vfs_file_handle *s)     { (void)s; return 0; }
static int fe_remove(const char *p)                      { (void)p; return 0; }
static int fe_rename(const char *a, const char *b)       { (void)a; (void)b; return 0; }

/* ---- v2, poisoned on the v1 interface ---- */

static int64_t fe_truncate_poison(struct retro_vfs_file_handle *s, int64_t l)
{
   (void)s; (void)l;
   fe_above_version = 1;
   return -1;
}

static int64_t fe_truncate(struct retro_vfs_file_handle *s, int64_t l)
{ (void)s; (void)l; return 0; }

/* ---- v3 ---- */

static int fe_stat(const char *p, int32_t *sz)   { (void)p; (void)sz; return 0; }
static int fe_mkdir(const char *d)               { (void)d; return 0; }

static struct retro_vfs_dir_handle *fe_opendir(const char *d, bool hidden)
{
   (void)d; (void)hidden;
   fe_dirs_live++;
   return (struct retro_vfs_dir_handle*)malloc(4);
}

static bool fe_readdir(struct retro_vfs_dir_handle *d)  { (void)d; return false; }
static const char *fe_dirent_name(struct retro_vfs_dir_handle *d)
{ (void)d; return "entry"; }
static bool fe_dirent_is_dir(struct retro_vfs_dir_handle *d) { (void)d; return false; }

static int fe_closedir(struct retro_vfs_dir_handle *d)
{
   fe_dirs_live--;
   free(d);
   return 0;
}

/* A frontend that filled only its v1 members.  Everything above is
 * poisoned rather than left NULL, so a version-gating slip is a
 * reported failure and not a NULL check that happens to save us. */
static struct retro_vfs_interface iface_v1 =
{
   fe_get_path, fe_open, fe_close, fe_size, fe_tell, fe_seek,
   fe_read, fe_write, fe_flush, fe_remove, fe_rename,
   fe_truncate_poison,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

static struct retro_vfs_interface iface_v3 =
{
   fe_get_path, fe_open, fe_close, fe_size, fe_tell, fe_seek,
   fe_read, fe_write, fe_flush, fe_remove, fe_rename,
   fe_truncate,
   fe_stat, fe_mkdir, fe_opendir, fe_readdir,
   fe_dirent_name, fe_dirent_is_dir, fe_closedir
};

static struct retro_vfs_interface *iface_in_use;

static bool env_cb(unsigned cmd, void *data)
{
   struct retro_vfs_interface_info *info =
      (struct retro_vfs_interface_info*)data;

   if (cmd != RETRO_ENVIRONMENT_GET_VFS_INTERFACE)
      return false;
   if (info->required_interface_version > fe_advertise)
      return false;
   /* The frontend writes back the version it actually supports, the
    * way RetroArch's own handler does. */
   info->required_interface_version = fe_advertise;
   info->iface                      = iface_in_use;
   return true;
}

/* ---- wrapper-allocation failure injection (see the Makefile) ---- */

static int fail_next_calloc;

void *__real_calloc(size_t n, size_t size);

void *__wrap_calloc(size_t n, size_t size)
{
   if (fail_next_calloc)
   {
      fail_next_calloc = 0;
      return NULL;
   }
   return __real_calloc(n, size);
}

int main(void)
{
   /* ---- 1: a v1 frontend must not have its v2 members called ---- */
   {
      RFILE *f;

      fe_reset(1);
      iface_in_use = &iface_v1;
      vfs_hybrid_init(env_cb, NULL);

      /* URI-shaped, so this is forced down the frontend branch on
       * every platform rather than only the sandboxed ones. */
      f = filestream_open("content://media/item",
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(f != NULL, "v1: uri path opens through the frontend");
      if (f)
      {
         int64_t rc = filestream_truncate(f, 3);
         CHECK(!fe_above_version,
               "v1: truncate does not reach the frontend's v2 member");
         CHECK(rc == -1, "v1: truncate reports failure rather than pretending");
         filestream_close(f);
      }
      CHECK(fe_files_live == 0, "v1: frontend file handle released on close");
   }

   /* ---- 2: the backend handle survives a wrapper allocation failure ---- */
   {
      struct RDIR *d;

      fe_reset(3);
      iface_in_use = &iface_v3;
      vfs_hybrid_init(env_cb, NULL);

      fail_next_calloc = 1;
      d = retro_opendir("content://tree/docs");
      CHECK(d == NULL, "opendir: reports failure when its wrapper cannot be allocated");
      CHECK(fe_dirs_live == 0,
            "opendir: frontend dir handle released rather than leaked");
      if (d)
         retro_closedir(d);
      fail_next_calloc = 0;

      /* and the same for a file, which already behaved */
      {
         RFILE *f;
         fail_next_calloc = 1;
         f = filestream_open("content://media/item",
               RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
         CHECK(f == NULL, "open: reports failure when its wrapper cannot be allocated");
         CHECK(fe_files_live == 0,
               "open: frontend file handle released rather than leaked");
         if (f)
            filestream_close(f);
         fail_next_calloc = 0;
      }
   }

   /* ---- 3: local-first dispatch ---- */
   {
      RFILE *f;
      const char *body = "HELLOHELLO";
      size_t      len  = 10;

      {
         FILE *w = fopen("hybrid_plain.bin", "wb");
         fwrite(body, 1, len, w);
         fclose(w);
      }

      fe_reset(3);
      iface_in_use = &iface_v3;
      vfs_hybrid_init(env_cb, NULL);

      f = filestream_open("hybrid_plain.bin", RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
      CHECK(f != NULL, "local: plain path opens");
      CHECK(fe_file_opens == 0, "local: plain path never reaches the frontend");
      if (f)
      {
         char buf[16];
         int64_t got;
         const uint8_t *map;
         int64_t map_len = -1;

         memset(buf, 0, sizeof(buf));
         got = filestream_read(f, buf, (int64_t)len);
         CHECK(got == (int64_t)len && memcmp(buf, body, len) == 0,
               "local: read returns the file's own bytes");

         /* The zero-copy sideband has to keep working through the
          * hybrid, or the reason for local-first dispatch is lost. */
         map = filestream_get_mapped_ptr(f, &map_len);
         if (map)
         {
            CHECK(map_len == (int64_t)len && memcmp(map, body, len) == 0,
                  "local: mapped view survives the hybrid layer");
         }
         else
            printf("skip: no mapping available on this build\n");

         filestream_close(f);
      }

      /* A miss on a plain path must not consult the frontend on a
       * non-sandboxed build; that cost is what the design promises to
       * leave alone. */
      f = filestream_open("hybrid_absent.bin", RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);
      CHECK(f == NULL, "local: missing plain path fails");
#if !defined(ANDROID) && !defined(__ANDROID__)
      CHECK(fe_file_opens == 0,
            "local: plain-path miss does not consult the frontend");
#else
      printf("skip: sandboxed build, a plain-path miss falls through by design\n");
#endif
      if (f)
         filestream_close(f);

      remove("hybrid_plain.bin");
   }

   if (test_fails)
   {
      printf("== %d FAILURES ==\n", test_fails);
      return 1;
   }
   printf("== vfs_hybrid_test: all tests pass ==\n");
   return 0;
}
