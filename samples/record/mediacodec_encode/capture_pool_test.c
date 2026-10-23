/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifdef NDEBUG
#error Assertions are required
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_android_capture_pool.h"
#include "../../../gfx/video_defines.h"
struct record_android_capture_images
{
   unsigned live;
};
struct record_android_capture_submit
{
   bool live, prepared;
};
struct record_gpu_dispatch_mediacodec
{
   enum record_dispatch_mediacodec_state state;
};
struct record_android_capture_pipe
{
   unsigned live;
};
static struct record_android_capture_images images;
static struct record_android_capture_submit entries[RECORD_GPU_QUEUE_SLOTS];
static struct record_gpu_dispatch_mediacodec gpu;
static struct record_android_capture_pipe pipe;
static struct model
{
   unsigned count, created, fail_entry, prepared, after, pipe_frees, pipe_steps;
   bool fail_images, fail_gpu, fail_pipe, gpu_live, pending, pipe_done;
   bool sources_done, cancelled, refuse_gpu_free, images_prepared, destroyed;
   bool copies_done;
   record_android_capture_source_t source;
   uint64_t expected_pts;
   unsigned records, commits;
   int record_result, commit_result;
   VkFormat format;
} m;
static void reset(unsigned mode)
{
   assert(!images.live && !m.gpu_live && !pipe.live);
   memset(&m, 0, sizeof(m));
   memset(entries, 0, sizeof(entries));
   m.count = 2;
   m.record_result = m.commit_result = 1;
   m.format = mode == 2 ? VK_FORMAT_R16G16B16A16_SFLOAT
         : mode == 1    ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        : VK_FORMAT_R8G8B8A8_UNORM;
}
record_android_capture_images_t *record_android_capture_images_new(
      VkPhysicalDevice physical, VkDevice device,
      const record_android_capture_image_ops_t *ops,
      const VkPhysicalDeviceMemoryProperties *memory, unsigned count,
      unsigned width, unsigned height, VkFormat format)
{
   assert(physical && device && ops && memory && count == m.count);
   assert(width == 128 && height == 96 && format == m.format);
   if (m.fail_images)
      return NULL;
   images.live = 1;
   return &images;
}
VkImage record_android_capture_images_image(
      const record_android_capture_images_t *p, unsigned slot)
{
   assert(p == &images && p->live && slot < m.count);
   return (VkImage)(uintptr_t)(slot + 1);
}
struct AHardwareBuffer *record_android_capture_images_buffer(
      const record_android_capture_images_t *p, unsigned slot)
{
   assert(p == &images && p->live && slot < m.count);
   return (struct AHardwareBuffer *)(uintptr_t)(slot + 1);
}
void record_android_capture_images_prepare_device_destroy(
      record_android_capture_images_t *p)
{
   assert(p == &images && p->live && m.prepared == m.count);
   m.images_prepared = true;
}
void record_android_capture_images_free(record_android_capture_images_t *p)
{
   unsigned i;
   if (!p)
      return;
   assert(p == &images && p->live && !m.gpu_live && !pipe.live);
   for (i = 0; i < m.count; i++)
      assert(!entries[i].live);
   assert(!m.images_prepared || m.destroyed);
   p->live = 0;
}
record_android_capture_submit_t *record_android_capture_submit_new(
      VkDevice device, VkQueue queue, unsigned family, VkImage image,
      VkFormat format, unsigned width, unsigned height,
      const record_android_capture_submit_ops_t *ops)
{
   unsigned i = m.created++;
   assert(device && queue && family == 3
         && image == (VkImage)(uintptr_t)(i + 1));
   assert(format == m.format && width == 128 && height == 96 && ops);
   if (m.fail_entry == i + 1)
      return NULL;
   entries[i].live = true;
   return &entries[i];
}
bool record_android_capture_submit_free(record_android_capture_submit_t *e)
{
   if (!e)
      return true;
   assert(e->live && !pipe.live && !m.gpu_live);
   e->live = false;
   return true;
}
bool record_android_capture_submit_prepare_device_destroy(
      record_android_capture_submit_t *e, bool quiesced)
{
   if (!e)
      return true;
   assert(e->live);
   if (!quiesced && !m.copies_done)
      return false;
   if (m.fail_entry && e == &entries[m.fail_entry - 1])
      return false;
   if (!e->prepared)
   {
      e->prepared = true;
      m.prepared++;
   }
   return true;
}
bool record_android_capture_submit_after_device_destroy(
      record_android_capture_submit_t *e)
{
   if (!e)
      return true;
   assert(m.images_prepared && m.destroyed && e->live && e->prepared);
   m.after++;
   return true;
}
record_gpu_dispatch_mediacodec_t *record_gpu_dispatch_mediacodec_new(
      record_dispatch_mediacodec_t *io,
      const record_gpu_mediacodec_config_t *config)
{
   unsigned i;
   assert(io && images.live && m.created == m.count);
   for (i = 0; i < m.count; i++)
      assert(
            config->buffers[i] == (struct AHardwareBuffer *)(uintptr_t)(i + 1));
   if (m.fail_gpu)
      return NULL;
   m.gpu_live = true;
   gpu.state = RECORD_MC_DISPATCH_STARTING;
   return &gpu;
}
enum record_dispatch_mediacodec_state record_gpu_dispatch_mediacodec_status(
      const record_gpu_dispatch_mediacodec_t *g, int *error, int *system_error)
{
   if (error)
      *error = 17;
   if (system_error)
      *system_error = 19;
   if (!g)
      return RECORD_MC_DISPATCH_FAILED;
   assert(g == &gpu && m.gpu_live);
   return g->state;
}
bool record_gpu_dispatch_mediacodec_free(record_gpu_dispatch_mediacodec_t *g)
{
   assert(g == &gpu && m.gpu_live && !pipe.live);
   if (m.refuse_gpu_free)
      return false;
   m.gpu_live = false;
   return true;
}
void record_gpu_dispatch_mediacodec_cancel(record_gpu_dispatch_mediacodec_t *g)
{
   assert(g == &gpu);
   m.cancelled = true;
}
bool record_gpu_dispatch_mediacodec_finish(
      record_gpu_dispatch_mediacodec_t *g, int64_t end)
{
   assert(g == &gpu && end >= 0);
   return !m.pending;
}
record_android_capture_pipe_t *record_android_capture_pipe_new(
      record_gpu_dispatch_mediacodec_t *g,
      record_android_capture_submit_t *const *e, unsigned count,
      void (*close_fd)(void *, int), void *context)
{
   (void)context;
   assert(g == &gpu && e[0] == &entries[0] && count == m.count && close_fd);
   if (m.fail_pipe)
      return NULL;
   pipe.live = 1;
   return &pipe;
}
int record_android_capture_pipe_push(record_android_capture_pipe_t *p,
      const record_android_capture_source_t *s, uint64_t pts, uint64_t duration)
{
   assert(
         p == &pipe && p->live && s && pts == m.expected_pts && duration == 10);
   m.records++;
   if (m.record_result == 1)
      m.source = *s;
   return m.record_result;
}
int record_android_capture_pipe_record(record_android_capture_pipe_t *p,
      const record_android_capture_source_t *source, uint64_t pts,
      uint64_t duration, VkCommandBuffer command)
{
   assert(command == (VkCommandBuffer)(uintptr_t)77);
   return record_android_capture_pipe_push(p, source, pts, duration);
}
int record_android_capture_pipe_commit(record_android_capture_pipe_t *p)
{
   assert(p == &pipe && p->live);
   m.commits++;
   return m.commit_result;
}
bool record_android_capture_pipe_abort_recorded(
      record_android_capture_pipe_t *p)
{
   assert(p == &pipe && p->live);
   return true;
}
int record_android_capture_pipe_step(record_android_capture_pipe_t *p)
{
   assert(p == &pipe && p->live);
   m.pipe_steps++;
   return m.pipe_done ? 1 : 0;
}
bool record_android_capture_pipe_sources_retired(
      const record_android_capture_pipe_t *p)
{
   return !p || m.sources_done;
}
bool record_android_capture_pipe_finish(
      record_android_capture_pipe_t *p, int64_t end)
{
   assert(p == &pipe && p->live && end >= 0);
   return !m.pending;
}
void record_android_capture_pipe_cancel(record_android_capture_pipe_t *p)
{
   assert(p == &pipe && p->live);
   m.cancelled = true;
}
bool record_android_capture_pipe_free(record_android_capture_pipe_t *p)
{
   assert(p == &pipe && p->live && m.pipe_done);
   p->live = 0;
   m.pipe_frees++;
   return true;
}
#include "../../../record/record_android_capture_device.c"
#include "../../../record/record_android_capture_pool.c"
static void close_fd(void *context, int fd)
{
   (void)context;
   (void)fd;
}
static record_android_capture_pool_config_t config(unsigned mode)
{
   record_android_capture_pool_config_t c;
   static VkPhysicalDeviceMemoryProperties memory;
   memset(&c, 0, sizeof(c));
   c.physical = (VkPhysicalDevice)(uintptr_t)1;
   c.device = (VkDevice)(uintptr_t)2;
   c.queue = (VkQueue)(uintptr_t)3;
   c.family = 3;
   c.memory = &memory;
   c.submit.close_fd = close_fd;
   c.gpu.count = m.count;
   c.gpu.width = 128;
   c.gpu.height = 96;
   c.gpu.mode = mode;
   return c;
}
static void normal(unsigned mode, unsigned count)
{
   static record_android_capture_pool_config_t c;
   record_android_capture_pool_t *p;
   record_android_capture_source_t source;
   int error, system_error;
   reset(mode);
   m.count = count;
   c = config(mode);
   memset(&source, 0, sizeof(source));
   assert(record_android_capture_pool_new(
         (record_dispatch_mediacodec_t *)&m, &c, &p));
   assert(p && !record_android_capture_pool_free(p));
   assert(record_android_capture_pool_status(p, &error, &system_error)
               == RECORD_MC_DISPATCH_STARTING
         && error == 17 && system_error == 19);
   assert(record_android_capture_pool_push(p, &source, 0, 10) == 1);
   assert(record_android_capture_pool_record(
                p, &source, 0, 10, (VkCommandBuffer)(uintptr_t)77)
         == 1);
   assert(record_android_capture_pool_abort_recorded(p));
   assert(record_android_capture_pool_commit(p) == 1);
   assert(!record_android_capture_pool_sources_retired(p));
   m.sources_done = true;
   assert(record_android_capture_pool_sources_retired(p));
   m.pending = true;
   assert(!record_android_capture_pool_finish(p, 10));
   m.pending = false;
   assert(!record_android_capture_pool_prepare_device_destroy(p, true));
   assert(record_android_capture_pool_finish(p, 10));
   assert(!record_android_capture_pool_finish(p, 11));
   assert(record_android_capture_pool_push(p, &source, 0, 10) == -1);
   assert(record_android_capture_pool_record(
                p, &source, 0, 10, (VkCommandBuffer)(uintptr_t)77)
         == -1);
   assert(record_android_capture_pool_commit(p) == 1);
   assert(record_android_capture_pool_abort_recorded(p));
   gpu.state = RECORD_MC_DISPATCH_DONE;
   assert(!record_android_capture_pool_free(p));
   m.pipe_done = true;
   m.refuse_gpu_free = true;
   assert(!record_android_capture_pool_free(p) && m.pipe_frees == 1);
   m.refuse_gpu_free = false;
   assert(record_android_capture_pool_free(p));
}
static void faults(void)
{
   static record_android_capture_pool_config_t c;
   record_android_capture_pool_t *p;
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      reset(0);
      c = config(0);
      m.fail_images = i == 0;
      m.fail_entry = i == 1 ? 2 : 0;
      m.fail_gpu = i == 2;
      m.fail_pipe = i == 3;
      assert(!record_android_capture_pool_new(
            (record_dispatch_mediacodec_t *)&m, &c, &p));
      if (i != 3)
         assert(!p && !images.live && !m.cancelled);
      else
      {
         assert(p && m.cancelled && images.live);
         assert(record_android_capture_pool_step(p) == 0);
         assert(!record_android_capture_pool_free(p));
         gpu.state = RECORD_MC_DISPATCH_FAILED;
         assert(record_android_capture_pool_free(p));
      }
   }
}
static void device_loss(unsigned mode, bool quiesced)
{
   static record_android_capture_pool_config_t c;
   record_android_capture_pool_t *p;
   unsigned i;
   reset(mode);
   c = config(mode);
   assert(record_android_capture_pool_new(
         (record_dispatch_mediacodec_t *)&m, &c, &p));
   assert(!record_android_capture_pool_after_device_destroy(p));
   assert(!record_android_capture_pool_prepare_device_destroy(p, true));
   record_android_capture_pool_cancel(p);
   assert(record_android_capture_pool_finish(p, 0));
   assert(!record_android_capture_pool_prepare_device_destroy(p, false));
   assert(!m.prepared && !m.images_prepared);
   assert(record_android_capture_pool_commit(p) == -1);
   assert(!record_android_capture_pool_abort_recorded(p));
   m.copies_done = !quiesced;
   m.fail_entry = 2;
   assert(!record_android_capture_pool_prepare_device_destroy(p, quiesced));
   assert(m.prepared == 1 && !m.images_prepared);
   for (i = 0; i < 100; i++)
   {
      assert(record_android_capture_pool_step(p) == 0);
      assert(!record_android_capture_pool_free(p));
      assert(!m.pipe_steps && !m.pipe_frees && images.live && m.gpu_live);
   }
   m.fail_entry = 0;
   assert(record_android_capture_pool_prepare_device_destroy(p, quiesced));
   assert(m.prepared == 2 && m.images_prepared);
   assert(record_android_capture_pool_prepare_device_destroy(p, false));
   gpu.state = RECORD_MC_DISPATCH_READY;
   assert(record_android_capture_pool_step(p) == 0);
   assert(!record_android_capture_pool_free(p));
   assert(!m.pipe_steps && images.live && m.gpu_live);
   gpu.state = RECORD_MC_DISPATCH_DRAINING;
   m.destroyed = true;
   assert(record_android_capture_pool_after_device_destroy(p) && m.after == 2);
   for (i = 0; i < 100; i++)
   {
      assert(record_android_capture_pool_step(p) == 0);
      assert(!record_android_capture_pool_free(p));
      assert(!m.pipe_steps && !m.pipe_frees && images.live && m.gpu_live);
   }
   gpu.state = RECORD_MC_DISPATCH_FAILED;
   assert(record_android_capture_pool_step(p) == 0);
   assert(!record_android_capture_pool_free(p));
   m.pipe_done = true;
   assert(record_android_capture_pool_step(p) == 1);
   assert(record_android_capture_pool_free(p));
}
static void renderer_binding(unsigned mode, unsigned fault)
{
   static record_android_capture_pool_config_t c;
   record_android_capture_pool_t *p;
   record_android_capture_renderer_t binding;
   record_android_capture_source_t source;
   unsigned dims = VIDEO_SCALE_PACK(128, 96);
   reset(mode);
   c = config(mode);
   c.fail_on_drop = fault == 6;
   if (fault == 6)
      m.record_result = 0;
   memset(&binding, 0, sizeof(binding));
   memset(&source, 0, sizeof(source));
   assert(record_android_capture_pool_new(
         (record_dispatch_mediacodec_t *)&m, &c, &p));
   assert(!record_android_capture_pool_bind_renderer(NULL, 10, &binding));
   assert(!record_android_capture_pool_bind_renderer(p, 0, &binding));
   assert(!record_android_capture_pool_bind_renderer(p, 1000001, &binding));
   assert(!record_android_capture_pool_bind_renderer(p, 10, NULL));
   assert(record_android_capture_pool_bind_renderer(p, 10, &binding));
   assert(binding.context == p && binding.record && binding.submitted
         && binding.service && binding.release);
   assert(!record_android_capture_pool_bind_renderer(p, 10, &binding));
   assert(binding.context == p);
   source.owner = binding.context;
   source.release = binding.release;
   source.image = (VkImage)(uintptr_t)99;
   source.format = m.format;
   source.width = 128;
   source.height = 96;
   source.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   if (fault == 3 || fault == 4 || fault == 6)
   {
      assert(binding.record(binding.context, &source,
                   (VkCommandBuffer)(uintptr_t)77, 0,
                   fault == 3 ? VIDEO_SCALE_PACK(64, 48) : dims,
                   fault == 4 ? (mode + 1) % 3 : mode)
            == -1);
      assert(m.cancelled && m.records == (fault == 6 ? 1u : 0u)
            && !m.commits && p->closed);
   }
   else
   {
      if (fault == 5)
      {
         m.record_result = 0;
         assert(binding.record(binding.context, &source,
                      (VkCommandBuffer)(uintptr_t)77, 0, dims, mode)
               == 0);
         assert(binding.record(binding.context, &source,
                      (VkCommandBuffer)(uintptr_t)77, 0, dims, mode)
               == 0);
         assert(m.records == 1 && !p->renderer_leases);
         m.record_result = 1;
         m.expected_pts = 1;
      }
      assert(binding.record(binding.context, &source,
                   (VkCommandBuffer)(uintptr_t)77, m.expected_pts, dims, mode)
            == 1);
      assert(p->renderer_leases == 1 && p->renderer_recorded);
      assert(binding.record(binding.context, &source,
                   (VkCommandBuffer)(uintptr_t)77, m.expected_pts, dims, mode)
            == 0);
      if (fault == 1)
         m.commit_result = -1;
      binding.submitted(
            binding.context, fault == 2 ? VK_ERROR_DEVICE_LOST : VK_SUCCESS);
      assert(!p->renderer_recorded && p->renderer_leases == 1);
      assert(m.commits == (fault == 2 ? 0u : 1u));
      binding.submitted(binding.context, VK_SUCCESS);
      assert(m.commits == (fault == 2 ? 0u : 1u));
      if (fault == 1 || fault == 2)
         assert(m.cancelled && p->closed);
      else
      {
         assert(!m.cancelled);
         binding.service(binding.context);
         assert(record_android_capture_pool_finish(p, 10 + m.expected_pts));
      }
   }
   if (m.cancelled)
   {
      assert(record_android_capture_pool_prepare_device_destroy(p, true));
      m.destroyed = true;
      assert(record_android_capture_pool_after_device_destroy(p));
   }
   if (p->renderer_leases)
   {
      assert(m.source.owner == p && m.source.release == binding.release);
      m.source.release(m.source.owner, m.destroyed);
      assert(!p->renderer_leases);
   }
   gpu.state
         = m.cancelled ? RECORD_MC_DISPATCH_FAILED : RECORD_MC_DISPATCH_DONE;
   m.pipe_done = true;
   assert(record_android_capture_pool_free(p));
}
static void device_registration(unsigned mode)
{
   static record_android_capture_pool_config_t c;
   record_android_capture_pool_t *p;
   unsigned i;
   record_android_capture_source_t source;
   reset(mode);
   c = config(mode);
   assert(record_android_capture_pool_new(
         (record_dispatch_mediacodec_t *)&m, &c, &p));
   /* No renderer binding or frame exists yet; startup is still protected. */
   assert(record_android_capture_device_pending(c.device));
   assert(!record_android_capture_device_pending((VkDevice)(uintptr_t)999));
   assert(!record_android_capture_device_destroyed(c.device));
   memset(&source, 0, sizeof(source));
   if (mode == 1)
      assert(record_android_capture_pool_push(p, &source, 0, 10) == 1);
   if (mode == 2)
      assert(record_android_capture_pool_record(p, &source, 0, 10,
                   (VkCommandBuffer)(uintptr_t)77) == 1);
   assert(p->renderer_end == (mode ? 10u : 0u));
   m.pending = true;
   assert(!record_android_capture_device_prepare(c.device, true));
   assert(!p->closed && p->preparing && !p->device_owner.prepared);
   assert(record_android_capture_pool_step(p) == 0);
   assert(record_android_capture_pool_push(p, &source, 0, 10) == -1);
   assert(record_android_capture_pool_record(p, &source, 0, 10,
                (VkCommandBuffer)(uintptr_t)77) == -1);
   m.pending = false;
   assert(!record_android_capture_device_prepare(c.device, false));
   assert(p->closed && p->preparing && !p->device_owner.prepared);
   assert(record_android_capture_device_prepare(c.device, true));
   assert(p->device_owner.prepared && !m.after);
   assert(record_android_capture_device_pending(c.device));
   m.destroyed = true;
   assert(record_android_capture_device_destroyed(c.device));
   assert(m.after == c.gpu.count);
   assert(!record_android_capture_device_pending(c.device));
   assert(record_android_capture_device_destroyed(c.device));
   assert(m.after == c.gpu.count);
   for (i = 0; i < 100; i++)
   {
      assert(!record_android_capture_pool_free(p));
      assert(!m.pipe_steps && images.live && m.gpu_live);
   }
   gpu.state = RECORD_MC_DISPATCH_FAILED;
   m.pipe_done = true;
   assert(record_android_capture_pool_free(p));
   assert(!record_acdevice_owners);
}

static void bad_inputs(void)
{
   static record_android_capture_pool_config_t c;
   record_android_capture_pool_t *p = NULL;
   reset(0);
   c = config(0);
   assert(!record_android_capture_pool_new(NULL, &c, &p) && !p);
   assert(!record_android_capture_pool_new(
                (record_dispatch_mediacodec_t *)&m, NULL, &p)
         && !p);
   assert(!record_android_capture_pool_new(
         (record_dispatch_mediacodec_t *)&m, &c, NULL));
   c.gpu.mode = 3;
   assert(!record_android_capture_pool_new(
                (record_dispatch_mediacodec_t *)&m, &c, &p)
         && !p);
   c.gpu.mode = 0;
   c.gpu.count = 0;
   assert(!record_android_capture_pool_new(
                (record_dispatch_mediacodec_t *)&m, &c, &p)
         && !p);
   assert(!images.live);
}
int main(void)
{
   unsigned mode, fault;
   assert(record_android_capture_pool_status(NULL, NULL, NULL)
         == RECORD_MC_DISPATCH_FAILED);
   bad_inputs();
   device_registration(0);
   device_registration(1);
   device_registration(2);
   normal(0, 1);
   normal(0, 16);
   normal(1, 2);
   normal(2, 2);
   faults();
   device_loss(0, false);
   device_loss(0, true);
   device_loss(1, false);
   device_loss(1, true);
   device_loss(2, false);
   device_loss(2, true);
   for (mode = 0; mode < 3; mode++)
      for (fault = 0; fault < 7; fault++)
         renderer_binding(mode, fault);
   assert(!record_acdevice_owners);
   assert(record_android_capture_pool_free(NULL));
   puts("PASS: joint capture pool construction, partial startup and ordered "
        "collection; native owners mocked");
   return 0;
}
