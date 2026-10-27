/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_android_capture_request.c"

struct record_android_capture_pool
{
   bool live, closed, prepared, dead;
};
static struct record_android_capture_pool pool;
static video_driver_state_t video;
static video_device_info_t device;
static video_thread_async_load_t *pending;
static const void *expected_binding;
static VkPhysicalDeviceMemoryProperties source_memory;
static bool threaded, in_flight, post_allowed, egl_allowed, egl_free_allowed;
static bool io_cancelled;
static bool pool_allowed, partial_pool, finish_allowed, reads_retired, ready;
static bool locked, queue_busy, gpu_done, gpu_released, gpu_claimed;
static unsigned egl_creates, pool_creates, pool_steps, pool_frees, lock_calls;
static unsigned configured_memory;
static int64_t gpu_end;
static enum record_dispatch_mediacodec_state io_state;
static int egl_storage, io_storage;

video_driver_state_t *video_state_get_ptr(void)
{
   return &video;
}
bool video_driver_thread_wrapper_active(void)
{
   return threaded;
}
bool video_thread_record_gpu_in_flight(void *data, const void *binding)
{
   assert(data == video.data && binding == expected_binding);
   return in_flight;
}
bool video_thread_async_post(video_thread_async_load_t *node)
{
   assert(!pending && node->kind == VIDEO_THREAD_ASYNC_JOB);
   if (!post_allowed)
      return false;
   pending = node;
   return true;
}
bool slock_try_lock(slock_t *lock)
{
   assert(lock == device.queue_lock && !locked);
   if (queue_busy)
      return false;
   locked = true;
   lock_calls++;
   return true;
}
void slock_unlock(slock_t *lock)
{
   assert(lock == device.queue_lock && locked);
   locked = false;
}
bool record_android_capture_pool_closed(const record_android_capture_pool_t *p)
{
   return p && p->closed;
}
static bool get_device(void *data, video_device_info_t *out)
{
   assert(data == video.data);
   *out = device;
   return true;
}
static const video_poke_interface_t *test_poke(void)
{
   static video_poke_interface_t poke;
   memset(&poke, 0, sizeof(poke));
   poke.get_device = get_device;
   return &poke;
}
bool record_android_capture_egl_new(
      unsigned mode, record_android_capture_egl_t **out)
{
   assert(mode <= 2 && !locked);
   egl_creates++;
   *out = (record_android_capture_egl_t *)&egl_storage;
   return egl_allowed;
}
bool record_android_capture_egl_configure(
      const record_android_capture_egl_t *egl,
      record_gpu_mediacodec_config_t *config)
{
   assert(egl == (record_android_capture_egl_t *)&egl_storage);
   config->display = (EGLDisplay)(uintptr_t)66;
   return true;
}
bool record_android_capture_egl_free(record_android_capture_egl_t *egl)
{
   assert(egl == (record_android_capture_egl_t *)&egl_storage && !pool.live);
   return egl_free_allowed;
}
bool record_android_capture_pool_new(record_dispatch_mediacodec_t *io,
      const record_android_capture_pool_config_t *config,
      record_android_capture_pool_t **out)
{
   assert(io == (record_dispatch_mediacodec_t *)&io_storage && !locked);
   assert(config->memory != &source_memory);
   assert(config->memory->memoryTypeCount == configured_memory);
   assert(config->gpu.display == (EGLDisplay)(uintptr_t)66);
   pool_creates++;
   *out = pool_allowed || partial_pool ? &pool : NULL;
   pool.live = *out != NULL;
   if (pool.live)
      gpu_claimed = true;
   return pool_allowed;
}
bool record_android_capture_pool_bind_renderer(record_android_capture_pool_t *p,
      unsigned duration, record_android_capture_renderer_t *binding)
{
   assert(p == &pool && duration == 10);
   binding->context = p;
   return true;
}
enum record_dispatch_mediacodec_state record_android_capture_pool_status(
      const record_android_capture_pool_t *p, int *error, int *system)
{
   (void)error;
   (void)system;
   assert(p == &pool);
   return ready ? RECORD_MC_DISPATCH_READY : RECORD_MC_DISPATCH_STARTING;
}
bool record_android_capture_pool_device_state(
      const record_android_capture_pool_t *p, bool *destroyed)
{
   assert(p == &pool);
   *destroyed = p->dead;
   return p->prepared;
}
void record_android_capture_pool_cancel(record_android_capture_pool_t *p)
{
   assert(p == &pool && (locked || p->dead));
}
bool record_android_capture_pool_finish(
      record_android_capture_pool_t *p, int64_t end)
{
   assert(p == &pool && end == 40 && (locked || p->dead));
   if (!finish_allowed)
      return false;
   p->closed = true;
   return true;
}
int record_android_capture_pool_step(record_android_capture_pool_t *p)
{
   assert(p == &pool && (locked || p->dead));
   pool_steps++;
   if (p->prepared && !p->dead)
      return 0;
   return p->closed && reads_retired ? 1 : 0;
}
bool record_android_capture_pool_prepare_device_destroy(
      record_android_capture_pool_t *p, bool queue_quiesced)
{
   assert(p == &pool && locked && !queue_quiesced && p->closed);
   p->prepared = true;
   return true;
}
bool record_android_capture_pool_free(record_android_capture_pool_t *p)
{
   assert(p == &pool && (locked || p->dead));
   if (!p->closed || !reads_retired || (p->prepared && !p->dead))
      return false;
   p->live = false;
   pool_frees++;
   gpu_done = gpu_released = true;
   gpu_claimed = false;
   gpu_end = 40;
   return true;
}
bool record_dispatch_mediacodec_cancelled(
      const record_dispatch_mediacodec_t *io)
{
   assert(io == (record_dispatch_mediacodec_t *)&io_storage);
   return io_cancelled;
}
enum record_dispatch_mediacodec_state record_dispatch_mediacodec_status(
      const record_dispatch_mediacodec_t *io, int *error, int *file,
      int *system)
{
   (void)error;
   (void)file;
   (void)system;
   assert(io == (record_dispatch_mediacodec_t *)&io_storage);
   return io_state;
}
bool record_dispatch_mediacodec_gpu_claim(record_dispatch_mediacodec_t *io)
{
   assert(io == (record_dispatch_mediacodec_t *)&io_storage);
   if (gpu_claimed || gpu_done)
      return false;
   gpu_claimed = true;
   return true;
}
void record_dispatch_mediacodec_gpu_release_claim(
      record_dispatch_mediacodec_t *io)
{
   assert(io == (record_dispatch_mediacodec_t *)&io_storage && gpu_claimed);
   gpu_claimed = false;
}
bool record_dispatch_mediacodec_gpu_retired(
      record_dispatch_mediacodec_t *io, int64_t end)
{
   assert(io == (record_dispatch_mediacodec_t *)&io_storage);
   if (gpu_done)
      return gpu_end == end;
   gpu_done = true;
   gpu_end = end;
   return true;
}
bool record_dispatch_mediacodec_gpu_released(record_dispatch_mediacodec_t *io)
{
   assert(io == (record_dispatch_mediacodec_t *)&io_storage && gpu_done);
   gpu_released = true;
   return true;
}
static void flush(bool discard, bool execute)
{
   video_thread_async_load_t *node = pending;
   const video_thread_async_job_t *job;
   uintptr_t result = 0;
   assert(node);
   pending = NULL;
   job = (const video_thread_async_job_t *)node->img;
   if (execute)
      result = job->run(video.data, video.poke, job->user);
   node->img = NULL;
   node->done(node->user, discard ? 0 : result);
}
static record_android_capture_request_t *new_request_source(unsigned mode, bool menu_source)
{
   static record_android_capture_pool_config_t config;
   record_android_capture_request_t *r;
   memset(&pool, 0, sizeof(pool));
   memset(&video, 0, sizeof(video));
   memset(&device, 0, sizeof(device));
   memset(&config, 0, sizeof(config));
   memset(&source_memory, 0, sizeof(source_memory));
   configured_memory = source_memory.memoryTypeCount = 3;
   config.physical = (VkPhysicalDevice)(uintptr_t)1;
   config.device = (VkDevice)(uintptr_t)2;
   config.queue = (VkQueue)(uintptr_t)3;
   config.memory = &source_memory;
   config.family = 4;
   config.gpu.width = 128;
   config.gpu.height = 96;
   config.gpu.count = 2;
   config.gpu.mode = mode;
   device.api = VIDEO_DEVICE_VULKAN;
   device.device = (void *)config.device;
   device.physical_device = (void *)config.physical;
   device.queue = (void *)config.queue;
   device.queue_family = config.family;
   device.queue_lock = (void *)(uintptr_t)5;
   video.data = &video;
   video.poke = test_poke();
   pending = NULL;
   threaded = post_allowed = egl_allowed = egl_free_allowed = true;
   io_cancelled = false;
   pool_allowed = finish_allowed = true;
   in_flight = partial_pool = reads_retired = ready = locked = false;
   queue_busy = gpu_done = gpu_released = gpu_claimed = false;
   egl_creates = pool_creates = pool_steps = pool_frees = lock_calls = 0;
   io_state = RECORD_MC_DISPATCH_STARTING;
   r = record_android_capture_request_new(
         (record_dispatch_mediacodec_t *)&io_storage, &config, 10, true, menu_source);
   assert(r);
   expected_binding = &r->binding;
   source_memory.memoryTypeCount = 9;
   return r;
}
static record_android_capture_request_t *new_request(unsigned mode)
{
   return new_request_source(mode, false);
}
static void start(record_android_capture_request_t *r)
{
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   assert(!record_android_capture_request_free(r));
   assert(!record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_SERVICE, 0));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 0 && !egl_creates);
   io_state = RECORD_MC_DISPATCH_READY;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   assert(!record_android_capture_request_binding(r));
   ready = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 1);
   assert(record_android_capture_request_binding(r) == &r->binding);
   assert(r->binding.context == &pool && egl_creates == 1 && pool_creates == 1);
}
static void normal(unsigned mode)
{
   record_android_capture_request_t *r = new_request(mode);
   start(r);
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_SERVICE, 0));
   flush(false, true);
   assert(pool_steps == 1 && lock_calls == 1);
   in_flight = true;
   assert(!record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_FINISH, 40));
   assert(!record_android_capture_request_binding(r));
   assert(!record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_FINISH, 41));
   in_flight = false;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_FINISH, 40));
   flush(false, true);
   assert(pool.closed && !record_android_capture_request_free(r));
   assert(!record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(!pool_frees);
   reads_retired = true;
   egl_free_allowed = false;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(pool_frees == 1 && !record_android_capture_request_free(r));
   egl_free_allowed = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_free(r) && gpu_end == 40);
}
static void busy_queue(void)
{
   record_android_capture_request_t *r = new_request(0);
   start(r);
   queue_busy = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 0);
   assert(pool.live && !pool.closed && !pool_steps && !pool_frees);
   assert(gpu_claimed && !gpu_done && !gpu_released && !lock_calls);
   assert(!record_android_capture_request_free(r));
   queue_busy = false;
   reads_retired = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 1);
   assert(pool_frees == 1 && lock_calls == 1 && !locked);
   assert(record_android_capture_request_free(r));
}
static void teardown(bool execute)
{
   record_android_capture_request_t *r = new_request(0);
   io_state = RECORD_MC_DISPATCH_READY;
   ready = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   if (execute)
   {
      const video_thread_async_job_t *job
            = (const video_thread_async_job_t *)pending->img;
      job->run(video.data, video.poke, job->user);
      pool.prepared = pool.dead = true;
   }
   flush(true, false);
   assert(record_android_capture_request_result(r) == -1);
   if (execute)
   {
      device.device = (void *)(uintptr_t)222;
      reads_retired = true;
      assert(!record_android_capture_request_free(r));
      assert(record_android_capture_request_post(
            r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
      flush(false, true);
      assert(!lock_calls && pool_frees == 1);
   }
   assert(record_android_capture_request_free(r));
   assert(gpu_done && gpu_released && gpu_end == (execute ? 40 : 0));
}
static void prepare(void)
{
   record_android_capture_request_t *r = new_request(2);
   start(r);
   finish_allowed = false;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_PREPARE_DEVICE, 40));
   flush(false, true);
   assert(!pool.prepared);
   finish_allowed = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_PREPARE_DEVICE, 40));
   flush(false, true);
   assert(pool.prepared && !pool.dead);
   assert(record_android_capture_request_result(r) == 1);
   device.device = (void *)(uintptr_t)222;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(!pool_frees);
   pool.dead = true;
   reads_retired = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(pool_frees == 1);
   assert(record_android_capture_request_free(r));
}
static void startup_fault(bool partial)
{
   record_android_capture_request_t *r = new_request(0);
   io_state = RECORD_MC_DISPATCH_READY;
   pool_allowed = false;
   partial_pool = partial;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   assert(record_android_capture_request_result(r) == -1);
   assert(!record_android_capture_request_free(r));
   reads_retired = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CANCEL, 40));
   flush(false, true);
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_free(r));
   assert(gpu_end == (partial ? 40 : 0));
}
static void post_refusal(void)
{
   record_android_capture_request_t *r = new_request(0);
   start(r);
   post_allowed = false;
   assert(!record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_SERVICE, 0));
   assert(!record_android_capture_request_binding(r));
   assert(record_android_capture_request_result(r) == -1);
   post_allowed = true;
   reads_retired = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_free(r));
}

static void inline_owner(void)
{
   record_android_capture_request_t *r = new_request_source(1, true);
   threaded = false;
   io_state = RECORD_MC_DISPATCH_READY;
   ready = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   assert(record_android_capture_request_binding(r));
   assert(record_android_capture_request_binding(r)->flags
         & RECORD_ANDROID_CAPTURE_MENU);
   reads_retired = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   assert(record_android_capture_request_result(r) == 1);
   assert(record_android_capture_request_free(r));
}

static void unavailable_queue(void)
{
   record_android_capture_request_t *r = new_request(0);
   io_state = RECORD_MC_DISPATCH_READY;
   device.queue_lock = NULL;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   assert(record_android_capture_request_result(r) == -1 && !egl_creates);
   assert(record_android_capture_request_free(r));
}

static void unrelated_gpu_claim(void)
{
   record_android_capture_request_t *r = new_request(0);
   io_state = RECORD_MC_DISPATCH_READY;
   gpu_claimed = true;
   pool_allowed = false;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(!record_android_capture_request_free(r));
   assert(gpu_claimed && !gpu_done && !gpu_released);
   /* Only the unrelated I/O owner completes its epoch; this request cannot. */
   io_state = RECORD_MC_DISPATCH_DONE;
   gpu_done = gpu_released = true;
   gpu_end = 77;
   assert(record_android_capture_request_free(r));
   assert(gpu_claimed && gpu_end == 77);
}

static void quiesce_for_drain(void)
{
   record_android_capture_request_t *r = new_request(0);
   io_state = RECORD_MC_DISPATCH_READY;
   ready = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   finish_allowed = false;
   in_flight = true;
   assert(!record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE, 40));
   in_flight = false;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_QUIESCE_DEVICE, 40));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 1);
   assert(pool.live && !pool.prepared && !pool.dead && !pool.closed);
   assert(
         !record_android_capture_request_free(r) && !pool_steps && !pool_frees);
   /* Only modeled common teardown's actual drain can now prepare children. */
   pool.prepared = pool.dead = true;
   finish_allowed = true;
   reads_retired = true;
   device.device = (void *)(uintptr_t)222;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_free(r));
}
static void already_closed(void)
{
   record_android_capture_request_t *r = new_request(0);
   io_state = RECORD_MC_DISPATCH_READY;
   ready = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_CREATE, 0));
   flush(false, true);
   pool.closed = true;
   finish_allowed = false;
   reads_retired = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 0 && pool.live);
   io_cancelled = true;
   assert(record_android_capture_request_post(
         r, RECORD_ANDROID_CAPTURE_COLLECT, 40));
   flush(false, true);
   assert(record_android_capture_request_result(r) == 1);
   assert(!pool.live && record_android_capture_request_free(r));
}
int main(void)
{
   busy_queue();
   quiesce_for_drain();
   already_closed();
   unrelated_gpu_claim();
   post_refusal();
   inline_owner();
   unavailable_queue();
   normal(0);
   normal(1);
   normal(2);
   teardown(false);
   teardown(true);
   prepare();
   startup_fault(false);
   startup_fault(true);
   assert(record_android_capture_request_free(NULL));
   assert(record_android_capture_request_result(NULL) == -1);
   puts("PASS asynchronous capture requests: main/render handoff, startup, "
        "queued "
        "bindings, closure, existing queue lock and independent retirement; "
        "native owners mocked");
   return 0;
}
