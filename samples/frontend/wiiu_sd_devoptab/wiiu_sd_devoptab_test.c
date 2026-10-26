/* os/wiiu/fs/sd_fat_devoptab.c against a stand-in Cafe FS that models how
 * a read lands: by DMA, with every cache line it covers invalidated
 * whole, so whatever the CPU last wrote to the rest of the last line is
 * lost. The devoptab's read must therefore leave every byte past the
 * requested length as it was, whatever the buffer's alignment; it
 * returns -1 with errno set when the FS fails; and a seek outside the
 * file, or with a bad origin, fails without moving the position or
 * leaving the device mutex held. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

/* The devoptab's allocations come from here on both hosts: every block
 * starts on a cache line, as memalign gives it on the console, and
 * Windows has no memalign whose result free() accepts. */
static void *test_memalign(size_t align, size_t n)
{
   uint8_t *raw = (uint8_t*)malloc(n + 2 * 64);
   uintptr_t p;
   (void)align;
   if (!raw)
      return NULL;
   p = ((uintptr_t)raw + sizeof(void*) + 63) & ~(uintptr_t)63;
   ((void**)p)[-1] = raw;
   return (void*)p;
}
static void *test_malloc(size_t n) { return test_memalign(64, n); }
static void test_free(void *p) { if (p) free(((void**)p)[-1]); }

#define memalign test_memalign
#define malloc   test_malloc
#define free     test_free
#include "../../../os/wiiu/fs/sd_fat_devoptab.c"
#undef memalign
#undef malloc
#undef free

#define LINE 64

static int failures;
#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

/* The file the stand-in FS serves. */
static uint8_t  file_data[4096];
static uint32_t file_size;
static uint32_t file_pos;
static int      fail_next_read;
static int      mutex_depth;

const devoptab_t *devoptab_list[STD_MAX];

void OSInitMutex(OSMutex *m)   { (void)m; }
void OSLockMutex(OSMutex *m)   { (void)m; mutex_depth++; }
void OSUnlockMutex(OSMutex *m) { (void)m; mutex_depth--; }

FSStatus FSReadFile(FSClient *client, FSCmdBlock *block, uint8_t *buffer,
      uint32_t size, uint32_t count, FSFileHandle handle, uint32_t unk1,
      uint32_t flags)
{
   uint32_t want = size * count, n;
   uintptr_t end, line_end;
   (void)client; (void)block; (void)handle; (void)unk1; (void)flags;
   if ((uintptr_t)buffer & (LINE - 1))
      return (FSStatus)-1;          /* Cafe FS refuses unaligned buffers */
   if (fail_next_read)
   {
      fail_next_read = 0;
      return (FSStatus)-1;
   }
   n = file_size - file_pos;
   if (n > want)
      n = want;
   memcpy(buffer, file_data + file_pos, n);
   file_pos += n;
   /* The invalidated tail of the last line the transfer covered. */
   end      = (uintptr_t)buffer + want;
   line_end = (end + LINE - 1) & ~(uintptr_t)(LINE - 1);
   memset((void*)end, 0xEE, line_end - end);
   return (FSStatus)n;
}

FSStatus FSSetPosFile(FSClient *client, FSCmdBlock *block,
      FSFileHandle handle, uint32_t pos, uint32_t flags)
{
   (void)client; (void)block; (void)handle; (void)flags;
   if (pos > file_size)
      return (FSStatus)-1;
   file_pos = pos;
   return (FSStatus)0;
}

/* Never reached by the paths under test. */
FSStatus FSWriteFile(FSClient *c, FSCmdBlock *b, uint8_t *p, uint32_t s,
      uint32_t n, FSFileHandle h, uint32_t u, uint32_t f)
{ (void)c; (void)b; (void)p; (void)s; (void)h; (void)u; (void)f; return (FSStatus)n; }
FSStatus FSAddClient(FSClient *c, uint32_t f) { (void)c; (void)f; return 0; }
FSStatus FSChangeDir(FSClient *c, FSCmdBlock *b, const char *p, uint32_t f) { (void)c; (void)b; (void)p; (void)f; return 0; }
FSStatus FSCloseDir(FSClient *c, FSCmdBlock *b, FSDirectoryHandle h, uint32_t f) { (void)c; (void)b; (void)h; (void)f; return 0; }
FSStatus FSCloseFile(FSClient *c, FSCmdBlock *b, FSFileHandle h, uint32_t f) { (void)c; (void)b; (void)h; (void)f; return 0; }
FSStatus FSDelClient(FSClient *c, uint32_t f) { (void)c; (void)f; return 0; }
FSStatus FSFlushFile(FSClient *c, FSCmdBlock *b, FSFileHandle h, uint32_t f) { (void)c; (void)b; (void)h; (void)f; return 0; }
FSStatus FSGetFreeSpaceSize(FSClient *c, FSCmdBlock *b, char *p, u64 *o, uint32_t f) { (void)c; (void)b; (void)p; (void)f; *o = 0; return 0; }
FSStatus FSGetStat(FSClient *c, FSCmdBlock *b, const char *p, FSStat *s, uint32_t f) { (void)c; (void)b; (void)p; (void)s; (void)f; return (FSStatus)-1; }
FSStatus FSGetStatFile(FSClient *c, FSCmdBlock *b, FSFileHandle h, FSStat *s, uint32_t f) { (void)c; (void)b; (void)h; (void)s; (void)f; return (FSStatus)-1; }
void FSInit(void) { }
void FSInitCmdBlock(FSCmdBlock *b) { (void)b; }
FSStatus FSMakeDir(FSClient *c, FSCmdBlock *b, const char *p, uint32_t f) { (void)c; (void)b; (void)p; (void)f; return 0; }
FSStatus FSOpenDir(FSClient *c, FSCmdBlock *b, const char *p, FSDirectoryHandle *h, uint32_t f) { (void)c; (void)b; (void)p; (void)h; (void)f; return (FSStatus)-1; }
FSStatus FSOpenFile(FSClient *c, FSCmdBlock *b, const char *p, const char *m, FSFileHandle *h, uint32_t f) { (void)c; (void)b; (void)p; (void)m; (void)h; (void)f; return (FSStatus)-1; }
FSStatus FSReadDir(FSClient *c, FSCmdBlock *b, FSDirectoryHandle h, FSDirectoryEntry *e, uint32_t f) { (void)c; (void)b; (void)h; (void)e; (void)f; return (FSStatus)-1; }
FSStatus FSRemove(FSClient *c, FSCmdBlock *b, const char *p, uint32_t f) { (void)c; (void)b; (void)p; (void)f; return 0; }
FSStatus FSRename(FSClient *c, FSCmdBlock *b, const char *o, const char *n, uint32_t f) { (void)c; (void)b; (void)o; (void)n; (void)f; return 0; }
FSStatus FSRewindDir(FSClient *c, FSCmdBlock *b, FSDirectoryHandle h, uint32_t f) { (void)c; (void)b; (void)h; (void)f; return 0; }
void FSShutdown(void) { }
FSStatus FSTruncateFile(FSClient *c, FSCmdBlock *b, FSFileHandle h, uint32_t f) { (void)c; (void)b; (void)h; (void)f; return 0; }
FSStatus FSUnmount(FSClient *c, FSCmdBlock *b, const char *t, uint32_t f) { (void)c; (void)b; (void)t; (void)f; return 0; }
int MountFS(void *c, void *b, char **p) { (void)c; (void)b; (void)p; return -1; }

static sd_fat_private_t    dev;
static OSMutex             mutex;
static sd_fat_file_state_t file;

static void reopen(uint32_t size)
{
   uint32_t i;
   for (i = 0; i < sizeof(file_data); i++)
      file_data[i] = (uint8_t)(i * 7 + 1);
   file_size     = size;
   file_pos      = 0;
   dev.pMutex    = &mutex;
   memset(&file, 0, sizeof(file));
   file.dev      = &dev;
   file.read     = true;
   file.len      = size;
}

/* A read of len bytes into a buffer at offset off from a line boundary,
 * with a canary after it in the same line: the data arrives, the canary
 * survives, and the right count comes back. */
static void read_case(size_t off, size_t len, uint32_t size)
{
   uint8_t *block = (uint8_t*)test_memalign(LINE, 8 * 1024);
   uint8_t *buf   = block + off;
   struct _reent r;
   ssize_t got;
   size_t expect  = len < size ? len : size;
   size_t i;

   memset(block, 0x5A, 8 * 1024);
   reopen(size);
   r._errno = 0;
   got = sd_fat_read_r(&r, &file, (char*)buf, len);
   CHECK(got == (ssize_t)expect);
   CHECK(memcmp(buf, file_data, expect) == 0);
   for (i = len; i < len + LINE; i++)
      if (buf[i] != 0x5A)
      {
         fprintf(stderr, "FAIL off %u len %u: byte %u past the read "
               "overwritten\n", (unsigned)off, (unsigned)len, (unsigned)(i - len));
         failures++;
         break;
      }
   CHECK(file.pos == expect);
   CHECK(mutex_depth == 0);
   test_free(block);
}

int main(void)
{
   struct _reent r;
   char buf[256];
   off_t got;

   /* Aligned buffer, length off a line: the case a whole-file read hits
    * when malloc happens to return a line-aligned block. */
   read_case(0,   100, 1000);
   read_case(0,  1000, 1000);
   read_case(0,  1024, 1000);   /* short: end of file */
   read_case(0,    63, 1000);
   read_case(0,   128, 1000);
   read_case(4,   100, 1000);   /* unaligned: through the bounce buffer */
   read_case(36, 2000, 3000);
   read_case(0,     0, 1000);

   /* A failing FS read is -1 with errno set, not an end of file. */
   reopen(1000);
   fail_next_read = 1;
   r._errno = 0;
   CHECK(sd_fat_read_r(&r, &file, buf, sizeof(buf)) == -1);
   CHECK(r._errno != 0);
   CHECK(mutex_depth == 0);

   /* Seeks: inside the file moves; past it, before it or with a bad
    * origin fails, leaves the position alone and the mutex free. */
   reopen(1000);
   r._errno = 0;
   CHECK(sd_fat_seek_r(&r, &file, 0, SEEK_END) == 1000);
   CHECK(sd_fat_seek_r(&r, &file, 10, SEEK_SET) == 10);
   got = sd_fat_seek_r(&r, &file, 5000, SEEK_SET);
   CHECK(got == -1 && file.pos == 10);
   got = sd_fat_seek_r(&r, &file, -50, SEEK_CUR);
   CHECK(got == -1 && file.pos == 10);
   got = sd_fat_seek_r(&r, &file, 0, 77);
   CHECK(got == -1 && r._errno == EINVAL && file.pos == 10);
   CHECK(mutex_depth == 0);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("wiiu_sd_devoptab: ok\n");
   return 0;
}
