/* Harness for the SMB driver's uploads.
 *
 * Two ways the SMB driver could lose the server's copy of a save:
 *
 * - With destructive sync off, a delete kept the old file (renamed to
 *   <path>-<yymmdd-hhmmss>), but an upload overwrote it in place, so
 *   the setting protected against deletes and not against overwrites.
 * - The upload opened the server file, truncated it, then wrote.  A
 *   write that failed part way - a dropped connection, a full share -
 *   left the server holding a truncated save, in either mode.
 *
 * Uploads now write to <path>.rauploading and rename it into place once
 * complete; the file being replaced is first renamed to the backup name
 * (non-destructive) or removed (destructive), and put back if the final
 * rename fails.  The test runs smb.c against an in-memory libsmb2 and
 * checks what the share holds afterwards:
 *
 * - non-destructive: new content in place, old content kept beside it;
 * - destructive, a new file and the server manifest: no backup;
 * - a write that fails part way, a backup that cannot be made and a
 *   final rename that fails all leave the old file exactly as it was
 *   and no temporary file behind, reported as one failure;
 * - an empty upload, a subdirectory, and every result reported once.
 *
 * Includes the driver's translation unit so it is tested as shipped.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <boolean.h>
#include <streams/file_stream.h>

#include "../../../network/cloud_sync/smb.c"

#include "fake_smb.h"

void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("WARN: "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("ERR: "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }

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
} done_t;

static void on_done(void *user_data, const char *path, bool success,
      RFILE *file)
{
   done_t *d = (done_t*)user_data;
   (void)path; (void)file;
   d->calls++;
   d->success = success;
}

static char tmp[64];

static void upload(const char *path, const char *text, bool destructive,
      done_t *done)
{
   FILE  *f;
   RFILE *rfile;

   if ((f = fopen(tmp, "wb")))
   {
      fputs(text, f);
      fclose(f);
   }
   rfile = filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   /* Leave the stream where a caller might: at its end. */
   filestream_seek(rfile, 0, SEEK_END);

   config_get_ptr()->bools.cloud_sync_destructive = destructive;
   memset(done, 0, sizeof(*done));
   smb_update(path, rfile, on_done, done);
   filestream_close(rfile);
}

/* How many files are named <prefix>-yymmdd-hhmmss, and the first. */
static unsigned backups_of(const char *prefix, fake_node_t **first)
{
   unsigned n = 0;
   size_t   len = strlen(prefix);
   int      i, j;

   *first = NULL;
   for (i = 0; i < FAKE_SMB_MAX_NODES; i++)
   {
      fake_node_t *node = &fake_smb.nodes[i];
      const char  *p;
      bool         ok = true;
      if (!node->used || node->dir || strncmp(node->path, prefix, len)
            || node->path[len] != '-')
         continue;
      p = node->path + len + 1;
      for (j = 0; j < 13; j++)
         if (j == 6 ? p[j] != '-' : !isdigit((unsigned char)p[j]))
            ok = false;
      if (!ok || p[13])
         continue;
      if (!n++)
         *first = node;
   }
   return n;
}

static bool no_temp_files(void)
{
   int i;
   for (i = 0; i < FAKE_SMB_MAX_NODES; i++)
      if (fake_smb.nodes[i].used && strstr(fake_smb.nodes[i].path, ".rauploading"))
         return false;
   return true;
}

static void fresh_share(const char *subdir)
{
   fake_smb_reset();
   smb_st.ctx = fake_smb_ctx();
   strlcpy(smb_st.subdir, subdir, sizeof(smb_st.subdir));
}

static void test_keeps_old_copy(void)
{
   done_t       done;
   fake_node_t *backup;

   fresh_share("");
   fake_smb_put("saves/a.srm", "OLD-SAVE-DATA");
   upload("saves/a.srm", "NEW-SAVE-DATA!", false, &done);

   CHECK(done.calls == 1 && done.success, "non-destructive upload must succeed once");
   CHECK(fake_smb_is("saves/a.srm", "NEW-SAVE-DATA!"),
         "non-destructive upload must put the new content in place");
   CHECK(backups_of("saves/a.srm", &backup) == 1
         && backup && backup->len == 13 && !memcmp(backup->data, "OLD-SAVE-DATA", 13),
         "non-destructive upload must keep the old content as saves/a.srm-<yymmdd-hhmmss>");
   CHECK(no_temp_files(), "no temporary file may be left behind");
}

static void test_no_backup_cases(void)
{
   done_t       done;
   fake_node_t *backup;

   fresh_share("");
   fake_smb_put("saves/a.srm", "OLD");
   upload("saves/a.srm", "NEW", true, &done);
   CHECK(done.calls == 1 && done.success && fake_smb_is("saves/a.srm", "NEW")
         && !backups_of("saves/a.srm", &backup) && no_temp_files(),
         "destructive upload must replace without a backup");

   fresh_share("");
   upload("saves/b.srm", "FIRST", false, &done);
   CHECK(done.calls == 1 && done.success && fake_smb_is("saves/b.srm", "FIRST")
         && !backups_of("saves/b.srm", &backup) && no_temp_files(),
         "uploading a new file must not invent a backup");

   fresh_share("");
   fake_smb_put(CLOUD_SYNC_SERVER_MANIFEST, "[old]");
   upload(CLOUD_SYNC_SERVER_MANIFEST, "[new]", false, &done);
   CHECK(done.calls == 1 && done.success
         && fake_smb_is(CLOUD_SYNC_SERVER_MANIFEST, "[new]")
         && !backups_of(CLOUD_SYNC_SERVER_MANIFEST, &backup) && no_temp_files(),
         "the server manifest must be replaced without a backup");
}

static void test_failed_write_keeps_old(bool destructive)
{
   done_t       done;
   fake_node_t *backup;
   const char  *mode = destructive ? "destructive" : "non-destructive";

   fresh_share("");
   fake_smb_put("saves/a.srm", "OLD-SAVE-DATA");
   /* Chunks are 4 bytes; the second write fails. */
   fake_smb.fail_write_after = 4;
   upload("saves/a.srm", "NEW-SAVE-DATA!", destructive, &done);

   CHECK(done.calls == 1 && !done.success,
         "%s: a failed write must report failure once", mode);
   CHECK(fake_smb_is("saves/a.srm", "OLD-SAVE-DATA"),
         "%s: a failed write must leave the server's file untouched", mode);
   CHECK(!backups_of("saves/a.srm", &backup),
         "%s: a failed write must not move the old file aside", mode);
   CHECK(no_temp_files(), "%s: a failed write must not leave a temporary file", mode);
}

static void test_failed_backup_keeps_old(void)
{
   done_t done;

   fresh_share("");
   fake_smb_put("saves/a.srm", "OLD-SAVE-DATA");
   fake_smb.fail_rename_number = 1; /* old -> backup */
   upload("saves/a.srm", "NEW-SAVE-DATA!", false, &done);

   CHECK(done.calls == 1 && !done.success,
         "a backup that cannot be made must fail the upload once");
   CHECK(fake_smb_is("saves/a.srm", "OLD-SAVE-DATA"),
         "a backup that cannot be made must leave the old file in place");
   CHECK(no_temp_files(), "a failed backup must not leave a temporary file");

   fresh_share("");
   fake_smb_put("saves/a.srm", "OLD-SAVE-DATA");
   fake_smb.fail_unlink = true;
   upload("saves/a.srm", "NEW-SAVE-DATA!", true, &done);
   CHECK(done.calls == 1 && !done.success && fake_smb_is("saves/a.srm", "OLD-SAVE-DATA")
         && no_temp_files(),
         "destructive: an old file that cannot be removed must fail the upload and stay");
}

static void test_failed_final_rename_restores_old(void)
{
   done_t       done;
   fake_node_t *backup;

   fresh_share("");
   fake_smb_put("saves/a.srm", "OLD-SAVE-DATA");
   fake_smb.fail_rename_number = 2; /* temp -> path, after old -> backup */
   upload("saves/a.srm", "NEW-SAVE-DATA!", false, &done);

   CHECK(done.calls == 1 && !done.success,
         "a final rename that fails must report failure once");
   CHECK(fake_smb_is("saves/a.srm", "OLD-SAVE-DATA"),
         "a final rename that fails must put the old file back");
   CHECK(!backups_of("saves/a.srm", &backup),
         "the old file must be back in place, not left under the backup name");
   CHECK(no_temp_files(), "a failed final rename must not leave a temporary file");
}

static void test_empty_and_subdir(void)
{
   done_t       done;
   fake_node_t *backup;

   fresh_share("retro");
   fake_smb_put("retro/saves/a.srm", "OLD");
   upload("saves/a.srm", "", false, &done);

   CHECK(done.calls == 1 && done.success && fake_smb_is("retro/saves/a.srm", ""),
         "an empty upload must leave an empty file");
   CHECK(backups_of("retro/saves/a.srm", &backup) == 1,
         "the backup must sit beside the file inside the configured subdirectory");
   CHECK(no_temp_files(), "no temporary file may be left behind");
}

int main(void)
{
   snprintf(tmp, sizeof(tmp), "/tmp/smb_backup_%ld", (long)getpid());

   test_keeps_old_copy();
   test_no_backup_cases();
   test_failed_write_keeps_old(false);
   test_failed_write_keeps_old(true);
   test_failed_backup_keeps_old();
   test_failed_final_rename_restores_old();
   test_empty_and_subdir();

   fake_smb_reset();
   remove(tmp);

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall SMB upload checks passed\n");
   return 0;
}
