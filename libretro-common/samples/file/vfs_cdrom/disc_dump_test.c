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
static cdrom_toc_t reopened_toc;
static unsigned toc_queries;
static int changed_geometry, bad_size;
static int read_fail, write_fail, open_fail, alloc_fail, flush_fail;
static int output_close_fail, input_close_fail, publish_fail, short_read;
static RFILE *output_handles[4];
static RFILE *image_input;
static unsigned write_opens, publish_calls;
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
   {
      RFILE *file = filestream_open(path, mode, hints);
      if (file && medium == 0x10 && !strcmp(path, "dump_source.bin"))
         image_input = file;
      if (file && mode == RETRO_VFS_FILE_ACCESS_WRITE)
         output_handles[write_opens - 1] = file;
      return file;
   }
}
static int dump_close(RFILE *file)
{
   unsigned i;
   int result;
   int fail = input_close_fail && medium == 8 && toc_queries == 1;
   if (file == image_input)
   {
      image_input = NULL;
      fail = input_close_fail;
   }
   for (i = 0; i < 4; i++)
      if (output_handles[i] == file)
      {
         output_handles[i] = NULL;
         fail = output_close_fail == (int)i + 1;
         break;
      }
   result = filestream_close(file);
   return fail ? -1 : result;
}
static int64_t dump_read(RFILE *file, void *buf, int64_t len)
{
   CHECK(((uintptr_t)buf & 4095) == 0 || medium == 8);
   CHECK(len <= filestream_get_size(file) - filestream_tell(file));
   if (read_fail) return -1;
   if (short_read) return filestream_read(file, buf, len / 2);
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
static const cdrom_toc_t *dump_toc(const libretro_vfs_implementation_file *file)
{
   (void)file;
   if (!toc_queries++) return &fake_toc;
   if (changed_geometry && toc_queries == 3)
   {
      if (changed_geometry == 1) reopened_toc.num_tracks--;
      if (changed_geometry == 2) reopened_toc.track[1].lba++;
      if (changed_geometry == 3) reopened_toc.track[1].mode++;
      if (changed_geometry == 4) return NULL;
      if (changed_geometry == 5) reopened_toc.track[0].track_bytes++;
   }
   return &reopened_toc;
}
static bool dump_atip(libretro_vfs_implementation_file *file)
{ (void)file; return false; }
static void *dump_alloc(size_t align, size_t bytes)
{ return alloc_fail ? NULL : memalign_alloc(align, bytes); }
static int dump_finish(const char *path, bool ok)
{
   if (ok && ++publish_calls == (unsigned)publish_fail) return 1;
   return filestream_finish_atomic(path, ok);
}
static int64_t dump_size(RFILE *file)
{
   int64_t size = filestream_get_size(file);
   if (bad_size == 1) return -1;
   if (bad_size == 2) return 0;
   if (bad_size == 3) return size - 1;
   if (bad_size == 4) return size + 2352;
   return size;
}
#define filestream_get_size dump_size
#define filestream_finish_atomic dump_finish
#define filestream_open dump_open
#define filestream_read dump_read
#define filestream_write dump_write
#define filestream_flush dump_flush
#define filestream_close dump_close
#define cdrom_get_media_profile dump_profile
#define retro_vfs_file_get_cdrom_toc dump_toc
#define cdrom_has_atip dump_atip
#define memalign_alloc dump_alloc
#include "../../../../tasks/task_content_disc.c"
#undef filestream_get_size
#undef filestream_finish_atomic
#undef filestream_open
#undef filestream_read
#undef filestream_write
#undef memalign_alloc
#undef filestream_flush
#undef filestream_close
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
   if (medium == 0x10) fixture("dump_result.iso", 17);
   else
   {
      fixture("dump_result.cue", 17);
      fixture("dump_result (Track 01).bin", 17);
      fixture("dump_result (Track 02).bin", 17);
   }
   publish_calls = 0;
   write_opens = 0;
   fake_toc.num_tracks = 2;
   reopened_toc = fake_toc;
   toc_queries = 0;
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
      if (medium == 8 && tick == 2)
         fake_toc.num_tracks = 0; /* unrelated metadata refresh */
      if (cancel && tick == (cancel == 1 ? 2u : cancel == 2 ? 4u : 7u))
         task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
   }
   CHECK(tick < 32 && !state->file && !state->output_file);
   if (read_fail || write_fail || open_fail || alloc_fail || flush_fail || cancel || changed_geometry || output_close_fail || input_close_fail || publish_fail || short_read || bad_size)
   {
      CHECK(finished_logs == logs);
      if (bad_size) CHECK(state->failed && write_opens == (medium == 8 ? 1u : 0u));
      if (medium == 0x10) verify("dump_result.iso", 17);
      else
      {
         verify("dump_result (Track 01).bin", state->cur_track > 1 ? 4704 : 17);
         verify("dump_result (Track 02).bin", state->cur_track > 2 ? 7056 : 17);
         verify("dump_result.cue", 17);
      }
      if ((output_close_fail || input_close_fail || short_read) && !cancel)
         CHECK(state->failed);
      if (changed_geometry)
      {
         CHECK(state->failed && state->disc_read_bytes == 4704);
         CHECK(write_opens == 2); /* CUE and first track only */
      }
   }
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
         {
            void *text = NULL;
            int64_t len = 0;
            CHECK(filestream_read_file("dump_result.cue", &text, &len));
            CHECK(text && len > 17 && strstr((char*)text, "TRACK 02"));
            free(text);
         }
      }
   }
   CHECK(!filestream_exists("dump_result.cue.tmp"));
   CHECK(!filestream_exists("dump_result.iso.tmp"));
   CHECK(!filestream_exists("dump_result (Track 01).bin.tmp"));
   CHECK(!filestream_exists("dump_result (Track 02).bin.tmp"));
   task_cdrom_dump_callback(task, NULL, NULL, NULL);
   CHECK(!output_handles[0] && !output_handles[1] && !output_handles[2]);
   task_free_title(task);
   task_free_error(task);
   free(task);
   remove("dump_result.iso");
   remove("dump_result.cue");
   remove("dump_result (Track 01).bin");
   remove("dump_result (Track 02).bin");
}
static void test_geometry(void)
{
   static const size_t fields[] = {
      offsetof(cdrom_track_t, lba_start), offsetof(cdrom_track_t, lba),
      offsetof(cdrom_track_t, track_size), offsetof(cdrom_track_t, track_bytes),
      offsetof(cdrom_track_t, track_num), offsetof(cdrom_track_t, min),
      offsetof(cdrom_track_t, sec), offsetof(cdrom_track_t, frame),
      offsetof(cdrom_track_t, mode), offsetof(cdrom_track_t, audio)
   };
   unsigned i;
   CHECK(!task_cdrom_dump_same_geometry(NULL, &fake_toc));
   CHECK(!task_cdrom_dump_same_geometry(&fake_toc, NULL));
   for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++)
   {
      reopened_toc = fake_toc;
      ((unsigned char*)&reopened_toc.track[0])[fields[i]] ^= 1;
      CHECK(!task_cdrom_dump_same_geometry(&fake_toc, &reopened_toc));
   }
   reopened_toc = fake_toc;
   reopened_toc.drive = 'X';
   reopened_toc.timeouts.g1_timeout = 1;
   CHECK(task_cdrom_dump_same_geometry(&fake_toc, &reopened_toc));
   reopened_toc.num_tracks = 0;
   CHECK(!task_cdrom_dump_same_geometry(&fake_toc, &reopened_toc));
   reopened_toc.num_tracks = 100;
   CHECK(!task_cdrom_dump_same_geometry(&reopened_toc, &reopened_toc));
}

int main(void)
{
   fixture("dump_source.bin", 133120);
   fixture("dump_track1.bin", 4704);
   fixture("dump_track2.bin", 7056);
   fake_toc.num_tracks = 2;
   fake_toc.track[0].track_bytes = 4704;
   fake_toc.track[1].track_bytes = 7056;
   test_geometry();
   run_dump(0);
   read_fail = 1; run_dump(0); read_fail = 0;
   write_fail = 1; run_dump(0); write_fail = 0;
   open_fail = 1; run_dump(0); open_fail = 0;
   alloc_fail = 1; run_dump(0); alloc_fail = 0;
   flush_fail = 1; run_dump(0); flush_fail = 0;
   output_close_fail = 1; run_dump(0); run_dump(1); output_close_fail = 0;
   run_dump(1);
   publish_fail = 1; run_dump(0); publish_fail = 0;
   input_close_fail = 1; run_dump(0); input_close_fail = 0;
   for (bad_size = 1; bad_size <= 4; bad_size++) run_dump(0);
   bad_size = 0;
   medium = 8;
   run_dump(0);
   for (bad_size = 1; bad_size <= 4; bad_size++) run_dump(0);
   bad_size = 0;
   short_read = 1; run_dump(0); short_read = 0;
   run_dump(2);
   run_dump(3);
   publish_fail = 1; run_dump(0); publish_fail = 0;
   publish_fail = 2; run_dump(0); publish_fail = 0;
   publish_fail = 3; run_dump(0); publish_fail = 0;
   for (output_close_fail = 1; output_close_fail <= 3; output_close_fail++)
      run_dump(0);
   output_close_fail = 0;
   input_close_fail = 1; run_dump(0); input_close_fail = 0;
   for (changed_geometry = 1; changed_geometry <= 5; changed_geometry++)
      run_dump(0);
   changed_geometry = 0;
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
