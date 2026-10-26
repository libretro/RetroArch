/* Read-ahead on a mapped file (FREQUENT_ACCESS).
 *
 * Reads that follow on from each other hint the next few MiB, so a
 * stream of reads through the mapping does not stall on the disk. On
 * Windows the hint is PrefetchVirtualMemory where it exists and a
 * thread that touches the view ahead of the reader where it does not;
 * RETRO_VFS_PREFETCH=touch selects the thread, =none neither.
 *
 *    - every mode reads the file back intact, in order and after jumps;
 *    - opening and closing with the thread running never hangs;
 *    - with the thread, pages ahead of the reader become resident in
 *      the process without being read (checked with QueryWorkingSetEx).
 *
 * Build:  make  (mingw: make CC=x86_64-w64-mingw32-gcc MMAP=0)
 * Run:    ./vfs_readahead_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <boolean.h>
#include <streams/file_stream.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define FIXTURE_LEN (16u * 1024u * 1024u + 333u)
#define CHUNK       (64u * 1024u)

static int test_fails;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL: %s\n", msg); \
         test_fails++; \
      } \
   } while (0)

static uint8_t pattern(size_t i)
{
   return (uint8_t)((i * 2654435761u) >> 13);
}

static bool write_fixture(const char *path)
{
   RFILE   *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   uint8_t *buf;
   size_t   i;
   bool     ok;

   if (!f)
      return false;
   if (!(buf = (uint8_t*)malloc(FIXTURE_LEN)))
   {
      filestream_close(f);
      return false;
   }
   for (i = 0; i < FIXTURE_LEN; i++)
      buf[i] = pattern(i);
   ok = filestream_write(f, buf, FIXTURE_LEN) == FIXTURE_LEN;
   free(buf);
   filestream_close(f);
   return ok;
}

static bool read_check(RFILE *f, uint64_t off, size_t len, uint8_t *buf)
{
   size_t i;
   if (filestream_seek(f, (int64_t)off, RETRO_VFS_SEEK_POSITION_START) != 0)
      return false;
   if (filestream_read(f, buf, len) != (int64_t)len)
      return false;
   for (i = 0; i < len; i++)
      if (buf[i] != pattern((size_t)off + i))
         return false;
   return true;
}

#ifdef _WIN32
typedef struct
{
   PVOID     VirtualAddress;
   ULONG_PTR VirtualAttributes;   /* bit 0: Valid */
} ws_ex_info_t;
typedef BOOL (WINAPI *ws_ex_t)(HANDLE, PVOID, DWORD);

static ws_ex_t ws_ex_resolve(void)
{
   HMODULE m = GetModuleHandleA("kernel32.dll");
   ws_ex_t f = m ? (ws_ex_t)GetProcAddress(m, "K32QueryWorkingSetEx") : NULL;
   if (!f && (m = LoadLibraryA("psapi.dll")))
      f = (ws_ex_t)GetProcAddress(m, "QueryWorkingSetEx");
   return f;
}

/* -1: cannot tell */
static int resident(ws_ex_t q, const uint8_t *p)
{
   ws_ex_info_t info;
   info.VirtualAddress    = (PVOID)p;
   info.VirtualAttributes = 0;
   if (!q || !q(GetCurrentProcess(), &info, sizeof(info)))
      return -1;
   return (int)(info.VirtualAttributes & 1);
}

static void check_touch(const char *path)
{
   ws_ex_t        q = ws_ex_resolve();
   RFILE         *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   const uint8_t *map;
   int64_t        maplen = 0;
   uint8_t       *buf    = (uint8_t*)malloc(CHUNK);
   int            before, after = 0, i;

   CHECK(f != NULL && buf != NULL, "touch: open");
   if (!f || !buf)
   {
      free(buf);
      if (f)
         filestream_close(f);
      return;
   }
   map = filestream_get_mapped_ptr(f, &maplen);
   CHECK(map != NULL && maplen == FIXTURE_LEN, "touch: mapped");
   before = map ? resident(q, map + 3u * 1024u * 1024u) : -1;
   if (map && before == 0)
   {
      CHECK(read_check(f, 0, CHUNK, buf), "touch: first read");
      /* the thread runs on its own; give it a few seconds at most */
      for (i = 0; i < 500; i++)
      {
         if (     (after = resident(q, map + 3u * 1024u * 1024u)) == 1
               && resident(q, map + 2u * 1024u * 1024u) == 1)
            break;
         Sleep(10);
      }
      CHECK(after == 1, "touch: pages ahead of the reader made resident");
   }
   else
      printf("SKIP: working-set residency not observable (%d)\n", before);
   free(buf);
   filestream_close(f);
}
#endif

static void check_reads(const char *path)
{
   RFILE   *f   = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
   uint8_t *buf = (uint8_t*)malloc(CHUNK);
   uint64_t off;
   bool     ok  = true;

   CHECK(f != NULL && buf != NULL, "reads: open");
   if (!f || !buf)
   {
      free(buf);
      if (f)
         filestream_close(f);
      return;
   }
   for (off = 0; ok && off + CHUNK <= FIXTURE_LEN; off += CHUNK)
      ok = read_check(f, off, CHUNK, buf);
   CHECK(ok, "reads: sequential");
   CHECK(read_check(f, FIXTURE_LEN - 333u, 333u, buf), "reads: tail");
   /* jumps back and forward, then in order again from there */
   CHECK(read_check(f, 12u * 1024u * 1024u + 7u, 4096, buf), "reads: jump");
   CHECK(read_check(f, 100, 4096, buf), "reads: jump back");
   CHECK(read_check(f, 100 + 4096, CHUNK, buf), "reads: on from jump");
   CHECK(read_check(f, 100 + 4096 + CHUNK + 1000, CHUNK, buf),
         "reads: small gap");
   free(buf);
   CHECK(filestream_close(f) == 0, "reads: close");
}

/* Close straight after the first hint, with the thread just started */
static void check_open_close(const char *path)
{
   uint8_t buf[256];
   int     i;
   bool    ok = true;

   for (i = 0; ok && i < 64; i++)
   {
      RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
      if (!f)
         ok = false;
      else
      {
         ok = read_check(f, (uint64_t)i * 4096u, sizeof(buf), buf);
         if (filestream_close(f) != 0)
            ok = false;
      }
   }
   CHECK(ok, "open/close with read-ahead running");
}

int main(void)
{
   const char *path = "vfs_readahead_fixture.bin";
   const char *mode = getenv("RETRO_VFS_PREFETCH");

   if (!write_fixture(path))
   {
      printf("FAIL: cannot write %s\n", path);
      return 1;
   }
   printf("mode: %s\n", mode ? mode : "default");
   check_reads(path);
   check_open_close(path);
#ifdef _WIN32
   if (mode && !strcmp(mode, "touch"))
      check_touch(path);
#endif
   filestream_delete(path);

   if (test_fails)
   {
      printf("%d check(s) failed\n", test_fails);
      return 1;
   }
   printf("PASS\n");
   return 0;
}
