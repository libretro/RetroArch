/* Harness for the NFS cloud sync driver (network/cloud_sync/nfs.c).
 *
 * Runs the driver as shipped against an in-memory export (fake_nfs.c)
 * that keeps net_nfs3.c's status contract, and checks what the export
 * and the local disk hold after each call:
 *
 * - a download replaces the local file only once complete: a failed
 *   open, a read failing part way and an unanswered call each leave the
 *   local file as it was and no <file>.rafetching behind; an empty file
 *   comes back as an open, empty stream;
 * - only the server answering NOENT reads as "not there": an
 *   unanswered open, stat or unlink fails instead of reporting a
 *   missing file or a completed delete;
 * - an upload goes through <path>.rauploading and one RENAME over the
 *   old file - never an unlink first; with destructive sync off the
 *   old file is kept as <path>-<yymmdd-hhmmss>, a second backup in the
 *   same second gets its own name, and a stat, backup or final rename
 *   that fails leaves the old file in place and no temporary file;
 * - the server manifest is replaced without a backup;
 * - a name that does not fit is refused, never truncated;
 * - the configured subdirectory and the directories under it are made,
 *   including one another client makes at the same moment.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <boolean.h>
#include <streams/file_stream.h>

#include "../../../network/cloud_sync/nfs.c"

#include "fake_nfs.h"

void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("WARN: "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("ERR: "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }

bool network_init(void) { return true; }

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

typedef struct
{
   unsigned calls;
   bool     success;
   RFILE   *file;
} done_t;

static void on_done(void *user_data, const char *path, bool success,
      RFILE *file)
{
   done_t *d = (done_t*)user_data;
   (void)path;
   d->calls++;
   d->success = success;
   d->file    = file;
}

static char dir[64];
static char upload_src[96];
static char local_save[96];
static char local_tmp[128];

static void write_local(const char *path, const char *text, size_t len)
{
   FILE *f = fopen(path, "wb");
   if (f)
   {
      fwrite(text, 1, len, f);
      fclose(f);
   }
}

/* the file's content, or NULL when it does not exist */
static char *read_local(const char *path)
{
   FILE *f = fopen(path, "rb");
   char *buf;
   long  len;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, 0, SEEK_SET);
   buf = (char*)calloc(1, (size_t)len + 1);
   if (fread(buf, 1, (size_t)len, f) != (size_t)len)
      buf[0] = '\0';
   fclose(f);
   return buf;
}

static bool local_is(const char *path, const char *text)
{
   char *s = read_local(path);
   bool  ok = s && !strcmp(s, text);
   free(s);
   return ok;
}

static bool remote_is(const char *path, const char *text)
{
   fake_node_t *n = fake_nfs_find(path);
   return n && !n->dir && n->size == strlen(text)
      && (!n->size || !memcmp(n->data, text, n->size));
}

static void begin(const char *subdir)
{
   settings_t *settings = config_get_ptr();
   done_t      d;
   fake_nfs_reset();
   memset(settings, 0, sizeof(*settings));
   strlcpy(settings->arrays.nfs_server, "server", sizeof(settings->arrays.nfs_server));
   strlcpy(settings->arrays.nfs_export, "/export", sizeof(settings->arrays.nfs_export));
   strlcpy(settings->arrays.nfs_subdir, subdir, sizeof(settings->arrays.nfs_subdir));
   memset(&d, 0, sizeof(d));
   nfs_sync_begin(on_done, &d);
   CHECK(d.calls == 1 && d.success, "begin");
}

static void end(void)
{
   done_t d;
   memset(&d, 0, sizeof(d));
   nfs_sync_end(on_done, &d);
   CHECK(d.calls == 1 && d.success, "end");
   fake_nfs_reset();
}

static void upload(const char *path, const char *text, bool destructive,
      done_t *done)
{
   RFILE *rfile;
   write_local(upload_src, text, strlen(text));
   rfile = filestream_open(upload_src, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   /* left where a caller might leave it: at its end */
   filestream_seek(rfile, 0, SEEK_END);
   config_get_ptr()->bools.cloud_sync_destructive = destructive;
   memset(done, 0, sizeof(*done));
   nfs_update(path, rfile, on_done, done);
   filestream_close(rfile);
}

static void download(const char *path, done_t *done)
{
   memset(done, 0, sizeof(*done));
   nfs_read(path, local_save, on_done, done);
}

static void delete_remote(const char *path, bool destructive, done_t *done)
{
   config_get_ptr()->bools.cloud_sync_destructive = destructive;
   memset(done, 0, sizeof(*done));
   nfs_free(path, on_done, done);
}

/* files named <prefix>-yymmdd-hhmmss[-n] */
static unsigned backups_of(const char *prefix, fake_node_t **first)
{
   unsigned n = 0;
   size_t   len = strlen(prefix);
   int      i, j;

   if (first)
      *first = NULL;
   for (i = 0; i < FAKE_NFS_MAX_NODES; i++)
   {
      fake_node_t *node = &fake_nfs.nodes[i];
      const char  *p;
      bool         ok = true;
      if (!node->used || node->dir || strncmp(node->path, prefix, len)
            || node->path[len] != '-')
         continue;
      p = node->path + len + 1;
      for (j = 0; j < 13; j++)
         if (j == 6 ? p[j] != '-' : !isdigit((unsigned char)p[j]))
            ok = false;
      if (!ok || (p[13] && !(p[13] == '-' && isdigit((unsigned char)p[14]) && !p[15])))
         continue;
      if (!n++ && first)
         *first = node;
   }
   return n;
}

static bool no_temp_files(void)
{
   int i;
   for (i = 0; i < FAKE_NFS_MAX_NODES; i++)
   {
      const char *p = fake_nfs.nodes[i].path;
      size_t      l = strlen(p);
      if (fake_nfs.nodes[i].used && l >= 12 && !strcmp(p + l - 12, ".rauploading"))
         return false;
   }
   return access(local_tmp, F_OK) != 0;
}

static void test_download(void)
{
   done_t   d;
   char    *big;
   unsigned i;

   begin("");
   CHECK(fake_nfs_find("cloud_sync") && fake_nfs_find("cloud_sync")->dir,
         "base directory made");

   /* complete download replaces the local file */
   write_local(local_save, "local", 5);
   fake_nfs_put("cloud_sync/saves/a.srm", "remote save");
   download("saves/a.srm", &d);
   CHECK(d.calls == 1 && d.success && d.file, "download reported");
   if (d.file)
   {
      char buf[32] = {0};
      CHECK(filestream_read(d.file, buf, sizeof(buf)) == 11
            && !strcmp(buf, "remote save"), "returned stream reads the download");
      filestream_close(d.file);
   }
   CHECK(local_is(local_save, "remote save"), "local file replaced");
   CHECK(no_temp_files(), "no temporary after a download");

   /* not on the server: success, no file, local untouched */
   write_local(local_save, "local", 5);
   download("saves/missing.srm", &d);
   CHECK(d.calls == 1 && d.success && !d.file, "missing file reported as missing");
   CHECK(local_is(local_save, "local"), "missing file leaves the local file");

   /* an unanswered open is a failure, not "missing", and touches nothing */
   fake_nfs_fail(FAKE_OP_OPEN, fake_nfs.calls[FAKE_OP_OPEN] + 1, RNFS_STATUS_NONE);
   download("saves/a.srm", &d);
   CHECK(d.calls == 1 && !d.success && !d.file, "unanswered open fails");
   CHECK(local_is(local_save, "local"), "unanswered open leaves the local file");
   CHECK(no_temp_files(), "no temporary after an unanswered open");

   /* a refused open (not NOENT) also fails */
   fake_nfs_fail(FAKE_OP_OPEN, fake_nfs.calls[FAKE_OP_OPEN] + 1, 13);
   download("saves/a.srm", &d);
   CHECK(d.calls == 1 && !d.success, "refused open fails");
   CHECK(local_is(local_save, "local"), "refused open leaves the local file");

   /* a read failing after the first 64 KiB chunk */
   big = (char*)malloc(200001);
   for (i = 0; i < 200000; i++)
      big[i] = (char)('a' + i % 26);
   big[200000] = '\0';
   fake_nfs_put("cloud_sync/saves/big.srm", big);
   fake_nfs_fail(FAKE_OP_READ, fake_nfs.calls[FAKE_OP_READ] + 2, RNFS_STATUS_NONE);
   download("saves/big.srm", &d);
   CHECK(d.calls == 1 && !d.success && !d.file, "partial read fails");
   CHECK(local_is(local_save, "local"), "partial read leaves the local file");
   CHECK(no_temp_files(), "no temporary after a partial read");

   /* and the same file in full */
   fake_nfs.fail_op = -1;
   download("saves/big.srm", &d);
   CHECK(d.calls == 1 && d.success && d.file, "large download");
   if (d.file)
      filestream_close(d.file);
   CHECK(local_is(local_save, big), "large download content");
   free(big);

   /* an empty file comes back as an open, empty stream */
   fake_nfs_put("cloud_sync/manifest.server", "");
   download("manifest.server", &d);
   CHECK(d.calls == 1 && d.success && d.file
         && filestream_get_size(d.file) == 0, "empty download");
   if (d.file)
      filestream_close(d.file);
   end();
}

static void test_upload(void)
{
   done_t       d;
   fake_node_t *bk;

   begin("");

   /* new file, non-destructive: no backup, parent directories made */
   upload("saves/core/a.srm", "v1", false, &d);
   CHECK(d.calls == 1 && d.success, "new upload");
   CHECK(remote_is("cloud_sync/saves/core/a.srm", "v1"), "new upload content");
   CHECK(backups_of("cloud_sync/saves/core/a.srm", NULL) == 0, "no backup for a new file");
   CHECK(no_temp_files(), "no temporary after a new upload");

   /* replace, non-destructive: old kept */
   upload("saves/core/a.srm", "v2", false, &d);
   CHECK(d.calls == 1 && d.success, "replace upload");
   CHECK(remote_is("cloud_sync/saves/core/a.srm", "v2"), "replaced content");
   CHECK(backups_of("cloud_sync/saves/core/a.srm", &bk) == 1
         && bk && bk->size == 2 && !memcmp(bk->data, "v1", 2), "old copy kept");

   /* again in the same second: a second backup, the first untouched */
   upload("saves/core/a.srm", "v3", false, &d);
   CHECK(d.calls == 1 && d.success, "second replace");
   CHECK(remote_is("cloud_sync/saves/core/a.srm", "v3"), "second replaced content");
   CHECK(backups_of("cloud_sync/saves/core/a.srm", NULL) == 2, "both backups kept");

   /* destructive: one RENAME over the old file, no unlink, no backup */
   fake_nfs.calls[FAKE_OP_UNLINK] = 0;
   upload("saves/core/b.srm", "b1", true, &d);
   upload("saves/core/b.srm", "b2", true, &d);
   CHECK(d.calls == 1 && d.success, "destructive replace");
   CHECK(remote_is("cloud_sync/saves/core/b.srm", "b2"), "destructive content");
   CHECK(fake_nfs.calls[FAKE_OP_UNLINK] == 0, "the old file was never unlinked");
   CHECK(backups_of("cloud_sync/saves/core/b.srm", NULL) == 0, "no backup when destructive");

   /* the server manifest is replaced without a backup */
   upload(CLOUD_SYNC_SERVER_MANIFEST, "m1", false, &d);
   upload(CLOUD_SYNC_SERVER_MANIFEST, "m2", false, &d);
   CHECK(d.success && remote_is("cloud_sync/" CLOUD_SYNC_SERVER_MANIFEST, "m2"), "manifest replaced");
   CHECK(backups_of("cloud_sync/" CLOUD_SYNC_SERVER_MANIFEST, NULL) == 0, "no manifest backup");

   /* an empty upload */
   upload("saves/core/empty.srm", "", false, &d);
   CHECK(d.success && remote_is("cloud_sync/saves/core/empty.srm", ""), "empty upload");

   /* a write failing part way: old file intact, no temporary */
   fake_nfs_fail(FAKE_OP_WRITE, fake_nfs.calls[FAKE_OP_WRITE] + 1, RNFS_STATUS_NONE);
   upload("saves/core/b.srm", "b3", false, &d);
   CHECK(d.calls == 1 && !d.success, "failed write reported");
   CHECK(remote_is("cloud_sync/saves/core/b.srm", "b2"), "failed write leaves the old file");
   CHECK(no_temp_files(), "no temporary after a failed write");

   /* an unanswered stat of the old file: fail closed, no unguarded replace */
   fake_nfs_fail_path(FAKE_OP_STAT, "cloud_sync/saves/core/b.srm", RNFS_STATUS_NONE);
   upload("saves/core/b.srm", "b3", false, &d);
   fake_nfs_reset_fail();
   CHECK(d.calls == 1 && !d.success, "unanswered stat fails the upload");
   CHECK(remote_is("cloud_sync/saves/core/b.srm", "b2"), "unanswered stat leaves the old file");
   CHECK(backups_of("cloud_sync/saves/core/b.srm", NULL) == 0, "unanswered stat makes no backup");
   CHECK(no_temp_files(), "no temporary after an unanswered stat");

   /* the backup rename failing */
   fake_nfs_fail(FAKE_OP_RENAME, fake_nfs.calls[FAKE_OP_RENAME] + 1, 13);
   upload("saves/core/b.srm", "b3", false, &d);
   CHECK(d.calls == 1 && !d.success, "failed backup reported");
   CHECK(remote_is("cloud_sync/saves/core/b.srm", "b2"), "failed backup leaves the old file");
   CHECK(no_temp_files(), "no temporary after a failed backup");

   /* the final rename failing: the old file is put back */
   fake_nfs_fail(FAKE_OP_RENAME, fake_nfs.calls[FAKE_OP_RENAME] + 2, RNFS_STATUS_NONE);
   upload("saves/core/b.srm", "b3", false, &d);
   CHECK(d.calls == 1 && !d.success, "failed final rename reported");
   CHECK(remote_is("cloud_sync/saves/core/b.srm", "b2"), "failed final rename restores the old file");
   CHECK(backups_of("cloud_sync/saves/core/b.srm", NULL) == 0, "restored backup is gone");
   CHECK(no_temp_files(), "no temporary after a failed final rename");
   end();
}

static void test_delete(void)
{
   done_t d;

   begin("");
   fake_nfs_put("cloud_sync/saves/a.srm", "a");
   fake_nfs_put("cloud_sync/saves/b.srm", "b");
   fake_nfs_mkdir_node("cloud_sync/saves");

   delete_remote("saves/missing.srm", false, &d);
   CHECK(d.calls == 1 && d.success, "missing file counts as removed");
   delete_remote("saves/missing.srm", true, &d);
   CHECK(d.calls == 1 && d.success, "missing file counts as removed (destructive)");

   fake_nfs_fail_path(FAKE_OP_STAT, "cloud_sync/saves/a.srm", RNFS_STATUS_NONE);
   delete_remote("saves/a.srm", false, &d);
   fake_nfs_reset_fail();
   CHECK(d.calls == 1 && !d.success, "unanswered stat is not a delete");
   CHECK(remote_is("cloud_sync/saves/a.srm", "a"), "unanswered stat keeps the file");

   fake_nfs_fail_path(FAKE_OP_UNLINK, "cloud_sync/saves/a.srm", RNFS_STATUS_NONE);
   delete_remote("saves/a.srm", true, &d);
   fake_nfs_reset_fail();
   CHECK(d.calls == 1 && !d.success, "unanswered unlink is not a delete");
   CHECK(remote_is("cloud_sync/saves/a.srm", "a"), "unanswered unlink keeps the file");

   delete_remote("saves/a.srm", false, &d);
   CHECK(d.calls == 1 && d.success, "non-destructive delete");
   CHECK(!fake_nfs_find("cloud_sync/saves/a.srm")
         && backups_of("cloud_sync/saves/a.srm", NULL) == 1, "delete keeps a backup");

   delete_remote("saves/b.srm", true, &d);
   CHECK(d.calls == 1 && d.success, "destructive delete");
   CHECK(!fake_nfs_find("cloud_sync/saves/b.srm")
         && backups_of("cloud_sync/saves/b.srm", NULL) == 0, "destructive delete keeps nothing");
   end();
}

static void test_paths(void)
{
   done_t   d;
   char    *key;
   unsigned before;

   /* subdirectory with surrounding slashes, made level by level */
   begin("/games//retro/");
   CHECK(fake_nfs_find("games") && fake_nfs_find("games/retro")
         && fake_nfs_find("games/retro/cloud_sync"), "subdirectory made");
   upload("saves/x.srm", "x", false, &d);
   CHECK(d.success && remote_is("games/retro/cloud_sync/saves/x.srm", "x"), "upload under the subdirectory");
   end();

   /* a directory another client makes between the lookup and MKDIR */
   begin("");
   fake_nfs.mkdir_race = 1;
   upload("states/y.state", "y", false, &d);
   CHECK(d.success && remote_is("cloud_sync/states/y.state", "y"), "mkdir race accepted");
   fake_nfs.mkdir_race = 0;

   /* a name that fits but whose temporary name does not: refused, and
    * nothing written under a truncated name */
   key = (char*)malloc(PATH_MAX_LENGTH);
   memset(key, 'k', PATH_MAX_LENGTH - 20);
   key[PATH_MAX_LENGTH - 20] = '\0';
   before = fake_nfs_count();
   upload(key, "z", false, &d);
   CHECK(d.calls == 1 && !d.success, "over-long temporary name refused");
   CHECK(fake_nfs_count() == before, "nothing written for an over-long name");
   /* one that does not fit at all */
   memset(key, 'k', PATH_MAX_LENGTH - 5);
   key[PATH_MAX_LENGTH - 5] = '\0';
   write_local(local_save, "local", 5);
   download(key, &d);
   CHECK(d.calls == 1 && !d.success, "over-long name refused on download");
   CHECK(local_is(local_save, "local"), "over-long download leaves the local file");
   delete_remote(key, true, &d);
   CHECK(d.calls == 1 && !d.success, "over-long name refused on delete");
   CHECK(fake_nfs.calls[FAKE_OP_UNLINK] == 0, "nothing unlinked for an over-long name");
   free(key);
   end();
}

int main(void)
{
   strcpy(dir, "/tmp/nfs_backup_XXXXXX");
   if (!mkdtemp(dir))
      return 2;
   snprintf(upload_src, sizeof(upload_src), "%s/upload", dir);
   snprintf(local_save, sizeof(local_save), "%s/save.srm", dir);
   snprintf(local_tmp, sizeof(local_tmp), "%s" NFS_LOCAL_TMP_SUFFIX, local_save);

   test_download();
   test_upload();
   test_delete();
   test_paths();

   unlink(upload_src);
   unlink(local_save);
   unlink(local_tmp);
   rmdir(dir);
   if (failures)
   {
      printf("[FAIL] nfs_backup_test: %u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] nfs_backup_test\n");
   return 0;
}
