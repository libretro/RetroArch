/* Real disc-dump state machine, real file I/O and task accessors.
 * Optical media and failure points are simulated. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <streams/file_stream.h>
#include <cdrom/cdrom.h>
#include <vfs/vfs_implementation_cdrom.h>
#include <queues/task_queue.h>
#include <memalign.h>
#include "../../../../msg_hash.h"
#include "../../../../verbosity.h"

static uint16_t medium = 0x10;
static cdrom_toc_t fake_toc;
static int read_fail, write_fail, open_fail, alloc_fail, flush_fail;
static unsigned write_opens;
static unsigned failures, finished_logs, cue_opens;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   if (msg == MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE) return "read failed";
   if (msg == MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK) return "write failed";
   return "open failed";
}
void RARCH_LOG(const char *fmt, ...)
{ if (strstr(fmt, "Dump finished")) finished_logs++; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

static RFILE *dump_open(const char *path, unsigned mode, unsigned hints)
{
   if (strstr(path, "cdrom://"))
   {
      if (strstr(path, ".cue")) cue_opens++;
      if (strstr(path, ".iso")) path = "dump_source.bin";
      else if (strstr(path, "track02")) path = "dump_track2.bin";
      else path = "dump_track1.bin";
   }
   if (mode == RETRO_VFS_FILE_ACCESS_WRITE
         && ++write_opens == (unsigned)open_fail)
      return NULL;
   return filestream_open(path, mode, hints);
}
static int64_t dump_read(RFILE *file, void *buf, int64_t len)
{
   CHECK(((uintptr_t)buf & 4095) == 0 || medium == 8);
   if (read_fail) return -1;
   return filestream_read(file, buf, len);
}
static int64_t dump_write(RFILE *file, const void *buf, int64_t len)
{
   if (write_fail) return filestream_write(file, buf, len / 2);
   return filestream_write(file, buf, len);
}
static int dump_flush(RFILE *file)
{ return flush_fail ? -1 : filestream_flush(file); }
static int dump_profile(libretro_vfs_implementation_file *file, uint16_t *profile)
{ (void)file; *profile = medium; return 0; }
static const cdrom_toc_t *dump_toc(void) { return &fake_toc; }
static bool dump_atip(libretro_vfs_implementation_file *file)
{ (void)file; return false; }
static void *dump_alloc(size_t align, size_t bytes)
{ return alloc_fail ? NULL : memalign_alloc(align, bytes); }
#define filestream_open dump_open
#define filestream_read dump_read
#define filestream_write dump_write
#define filestream_flush dump_flush
#define cdrom_get_media_profile dump_profile
#define retro_vfs_file_get_cdrom_toc dump_toc
#define cdrom_has_atip dump_atip
#define memalign_alloc dump_alloc
#include "../../../../tasks/task_content_disc.c"
#undef filestream_open
#undef filestream_read
#undef filestream_write
#undef memalign_alloc
#undef filestream_flush
settings_t *config_get_ptr(void) { return NULL; }

static void fixture(const char *path, unsigned size)
{
   FILE *file = fopen(path, "wb");
   unsigned i;
   CHECK(file != NULL);
   if (!file) return;
   for (i = 0; i < size; i++) fputc((unsigned char)i, file);
   CHECK(fclose(file) == 0);
}
static void verify(const char *path, unsigned size)
{
   FILE *file = fopen(path, "rb");
   unsigned i;
   CHECK(file != NULL);
   if (!file) return;
   for (i = 0; i < size; i++) CHECK(fgetc(file) == (unsigned char)i);
   CHECK(fgetc(file) == EOF);
   fclose(file);
}
static void run_dump(int cancel)
{
   retro_task_t *task = task_init();
   task_cdrom_dump_state_t *state = (task_cdrom_dump_state_t*)calloc(1, sizeof(*state));
   unsigned tick = 0, logs = finished_logs;
   write_opens = 0;
   CHECK(task && state);
   if (!task || !state) return;
   state->drive_letter[0] =
#ifdef _WIN32
      'D';
#else
      '1';
#endif
   strcpy(state->title, "dump_result");
   strcpy(state->dir_core_assets, ".");
   task->state = state;
   while (!(task_get_flags(task) & RETRO_TASK_FLG_FINISHED) && tick++ < 32)
   {
      task_cdrom_dump_handler(task);
      if (cancel && tick == 2)
         task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
   }
   CHECK(tick < 32 && !state->file && !state->output_file);
   if (read_fail || write_fail || open_fail || alloc_fail || flush_fail || cancel)
      CHECK(finished_logs == logs);
   else
   {
      CHECK(finished_logs == logs + 1);
      if (medium == 0x10)
      {
         CHECK(cue_opens == 0 && state->disc_read_bytes == 133120);
         verify("dump_result.iso", 133120);
      }
      else
      {
         CHECK(state->disc_read_bytes == 11760);
         verify("dump_result (Track 01).bin", 4704);
         verify("dump_result (Track 02).bin", 7056);
      }
   }
   task_cdrom_dump_callback(task, NULL, NULL, NULL);
   task_free_title(task);
   task_free_error(task);
   free(task);
   remove("dump_result.iso");
   remove("dump_result.cue");
   remove("dump_result (Track 01).bin");
   remove("dump_result (Track 02).bin");
}
int main(void)
{
   fixture("dump_source.bin", 133120);
   fixture("dump_track1.bin", 4704);
   fixture("dump_track2.bin", 7056);
   fake_toc.num_tracks = 2;
   fake_toc.track[0].track_bytes = 4704;
   fake_toc.track[1].track_bytes = 7056;
   run_dump(0);
   read_fail = 1; run_dump(0); read_fail = 0;
   write_fail = 1; run_dump(0); write_fail = 0;
   open_fail = 1; run_dump(0); open_fail = 0;
   alloc_fail = 1; run_dump(0); alloc_fail = 0;
   flush_fail = 1; run_dump(0); flush_fail = 0;
   run_dump(1);
   medium = 8;
   run_dump(0);
   open_fail = 1; run_dump(0); open_fail = 0;
   open_fail = 3; run_dump(0); open_fail = 0;
   write_fail = 1; run_dump(0); write_fail = 0;
   task_push_cdrom_dump(NULL);
   task_push_cdrom_dump("");
   remove("dump_source.bin");
   remove("dump_track1.bin");
   remove("dump_track2.bin");
   printf("disc dump tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
