/* The frontend, transfer, codec and task-queue surface the windowed
 * feeder references, stubbed down to what the race under test needs.
 *
 * data_transfer_open_window() refusing is what keeps the handler's
 * non-dead path short: it bails to the classic loader, whose task_init()
 * here returns none, so a pass costs a read of dead and little else.
 * That read is the whole point, so nothing below should do real work. */
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

#include <boolean.h>
#include <queues/task_queue.h>

void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

/* Transfer windows: no window, so the feeder bails on its first tick. */
void  *data_transfer_open_window(const char *p, size_t keep) { (void)p; (void)keep; return NULL; }
void   data_transfer_free(void *dt) { (void)dt; }
const uint8_t *data_transfer_window_base(void *dt, size_t *len) { (void)dt; if (len) *len = 0; return NULL; }
bool   data_transfer_window_feed(void *dt, int64_t want) { (void)dt; (void)want; return false; }
bool   data_transfer_window_grow_keep(void *dt, size_t keep) { (void)dt; (void)keep; return false; }
void   data_transfer_window_punch(void *dt, size_t lo, size_t hi) { (void)dt; (void)lo; (void)hi; }

/* The mixer side of the borrow. */
bool   audio_driver_mixer_add_stream(void *params) { (void)params; return false; }
void   audio_driver_mixer_remove_stream(unsigned i) { (void)i; }
int64_t audio_driver_mixer_stream_byte_tell(unsigned i) { (void)i; return -1; }
void   audio_driver_mixer_stream_set_avail(unsigned i, size_t avail) { (void)i; (void)avail; }

/* Codec probes the feeder uses to decide whether a file is windowable. */
int    audio_transfer_ogg_audio_type(const uint8_t *b, size_t l) { (void)b; (void)l; return 0; }
bool   rwav_parse(const void *b, size_t l, void *out) { (void)b; (void)l; (void)out; return false; }
void  *rwebm_open_memory_avail(const uint8_t *b, size_t l, size_t a) { (void)b; (void)l; (void)a; return NULL; }
void   rwebm_close(void *w) { (void)w; }
unsigned rwebm_num_tracks(void *w) { (void)w; return 0; }
void  *rwebm_get_track(void *w, unsigned i) { (void)w; (void)i; return NULL; }
int64_t rwebm_media_floor(void *w, int64_t t) { (void)w; (void)t; return 0; }

/* nbio, for the classic loader the bail path falls back to. */
void  *nbio_xfer_ptr(void *n, size_t *len) { (void)n; if (len) *len = 0; return NULL; }
void   nbio_xfer_close(void *n) { (void)n; }
bool   task_file_load_handler(retro_task_t *task) { (void)task; return true; }

/* No task is ever handed out, so the bail path's push returns at once. */
retro_task_t *task_init(void) { return NULL; }
bool   task_queue_push(retro_task_t *task) { (void)task; return false; }
void   task_set_data(retro_task_t *task, void *data) { (void)task; (void)data; }

/* task_queue.c is not linked - it owns task_init() too - so the two
 * flag helpers the feeder uses come from here, on the task's own word. */
void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{
   if (!task)
      return;
   if (set)
      task->flags |=  flags;
   else
      task->flags &= ~flags;
}

uint8_t task_get_flags(retro_task_t *task)
{
   return task ? task->flags : 0;
}

/* file_path.c has path_get_size(), but it wants the VFS wired up; the
 * feeder only asks for a size it then bails on. */
int32_t path_get_size(const char *path) { (void)path; return -1; }
void    mem_stats_free(void *p) { free(p); }
