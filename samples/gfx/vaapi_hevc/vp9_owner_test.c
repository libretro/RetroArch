/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_vp9_owner.h"
static vaapi_vp9_runtime_t owner_dummy;
static vaapi_vp9_runtime_t *owner_runtime = &owner_dummy;
#define VAAPI_RUNTIME_TEST_MAIN vp9_runtime_unit_main
#define VAAPI_RUNTIME_TEST_CONTEXT owner_runtime
#include "runtime_test.c"
#undef runtime
#undef VAAPI_RUNTIME_TEST_MAIN
#undef VAAPI_RUNTIME_TEST_CONTEXT

static unsigned owner_allocations, owner_alloc_fail;
static void *owner_alloc(size_t count, size_t bytes)
{
   owner_allocations++;
   return owner_allocations == owner_alloc_fail ? NULL : calloc(count, bytes);
}
#define calloc owner_alloc
#include "../../../gfx/common/vaapi_vp9_owner.c"
#undef calloc

static vaapi_vp9_owner_t *owner;
static rvp9_hw_ops_t owner_ops;
static rvp9_hw_picture_t owner_picture;
static uint8_t owner_packet[128];
static unsigned owner_buffers, owner_drops, owner_syncs, owner_fds, owner_closes;
static unsigned owner_create_fail, owner_begin_fail;
static VAStatus owner_completion;

static int owner_node(const char *path, unsigned major_id, unsigned minor_id)
{
   assert(owner);
   owner_runtime = &owner->runtime;
   return node_open(path, major_id, minor_id);
}
static VAStatus owner_profiles(VADisplay d, VAProfile *p, int *n)
{
   VAStatus status = profiles(d, p, n);
   if (fault != NO_PROFILE)
   {
      p[0] = VAProfileVP9Profile0;
      p[1] = VAProfileVP9Profile2;
   }
   return status;
}
static VAStatus owner_entries(VADisplay d, VAProfile p, VAEntrypoint *v, int *n)
{
   assert(p == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0));
   return entrypoints(d, depth == 10 ? VAProfileHEVCMain10 : VAProfileHEVCMain, v, n);
}
static VAStatus owner_attributes(VADisplay d, VAProfile p, VAEntrypoint e,
      VAConfigAttrib *a, int n)
{
   check_display(d);
   assert(p == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0)
         && e == VAEntrypointVLD && n == 3);
   assert(a[0].type == VAConfigAttribRTFormat
         && a[1].type == VAConfigAttribMaxPictureWidth
         && a[2].type == VAConfigAttribMaxPictureHeight);
   a[0].value = rt_mask;
   a[1].value = max_width;
   a[2].value = 2160;
   return fault == ATTRIBUTES ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus owner_config(VADisplay d, VAProfile p, VAEntrypoint e,
      VAConfigAttrib *a, int n, VAConfigID *out)
{
   VAStatus status;
   unsigned saved = slice_mask;
   assert(p == (depth == 10 ? VAProfileVP9Profile2 : VAProfileVP9Profile0) && n == 1);
   slice_mask = VA_ATTRIB_NOT_SUPPORTED;
   status = create_config(d, depth == 10 ? VAProfileHEVCMain10 : VAProfileHEVCMain,
         e, a, n, out);
   slice_mask = saved;
   return status;
}
static VAStatus owner_create_buffer(VADisplay d, VAContextID context,
      VABufferType type, unsigned bytes, unsigned count, void *data, VABufferID *id)
{
   unsigned index = owner_buffers++;
   assert(d == owner->runtime.display && context == 22 && data && count == 1);
   if (!index)
      assert(type == VAPictureParameterBufferType
            && bytes == sizeof(VADecPictureParameterBufferVP9));
   else if (index == 1)
      assert(type == VASliceParameterBufferType
            && bytes == sizeof(VASliceParameterBufferVP9));
   else
      assert(index == 2 && type == VASliceDataBufferType && bytes == 128);
   *id = 1000 + index;
   return owner_create_fail == index + 1 ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus owner_drop_buffer(VADisplay d, VABufferID id)
{
   assert(d == owner->runtime.display && id >= 1000 && id < 1003);
   owner_drops++;
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_begin(VADisplay d, VAContextID context, VASurfaceID target)
{
   assert(d == owner->runtime.display && context == 22 && target == 100);
   return owner_begin_fail ? VA_STATUS_ERROR_OPERATION_FAILED : VA_STATUS_SUCCESS;
}
static VAStatus owner_render(VADisplay d, VAContextID context, VABufferID *ids, int count)
{
   assert(d == owner->runtime.display && context == 22 && count == 3 && ids[2] == 1002);
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_end(VADisplay d, VAContextID context)
{
   assert(d == owner->runtime.display && context == 22);
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_sync(VADisplay d, VASurfaceID target, uint64_t timeout)
{
   assert(d == owner->runtime.display && target == 100 && !timeout);
   owner_syncs++;
   return owner_completion;
}
static VAStatus owner_export(VADisplay d, VASurfaceID target,
      uint32_t memory, uint32_t flags, void *output)
{
   VADRMPRIMESurfaceDescriptor *v = (VADRMPRIMESurfaceDescriptor *)output;
   unsigned pitch = depth == 10 ? 128 : 64;
   assert(d == owner->runtime.display && target == 100 && !owner_fds);
   assert(memory == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2
         && flags == (VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS));
   owner_fds = 1;
   v->fourcc = depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   v->width = v->height = 64;
   v->num_objects = 1;
   v->objects[0].fd = 42;
   v->objects[0].size = pitch * 64 * 3 / 2;
   v->objects[0].drm_format_modifier = 0;
   v->num_layers = 2;
   v->layers[0].drm_format = depth == 10 ? UINT32_C(0x20363152) : UINT32_C(0x20203852);
   v->layers[1].drm_format = depth == 10 ? UINT32_C(0x32335247) : UINT32_C(0x38385247);
   v->layers[0].num_planes = v->layers[1].num_planes = 1;
   v->layers[0].object_index[0] = v->layers[1].object_index[0] = 0;
   v->layers[0].offset[0] = 0;
   v->layers[1].offset[0] = pitch * 64;
   v->layers[0].pitch[0] = v->layers[1].pitch[0] = pitch;
   return VA_STATUS_SUCCESS;
}
static bool owner_seal(int fd)
{
   assert(fd == 42 && owner_fds);
   return true;
}
static bool owner_identity(int fd, uint64_t *device, uint64_t *inode)
{
   assert(fd == 42 && owner_fds);
   *device = 3;
   *inode = 5;
   return true;
}
static void owner_close_fd(int fd)
{
   assert(fd == 42 && owner_fds);
   owner_fds = 0;
   owner_closes++;
}
static const vaapi_hevc_export_ops_t owner_exports = {
      owner_export, owner_seal, owner_close_fd, owner_identity};
static vaapi_vp9_runtime_io_t owner_io;

static void owner_setup(unsigned bits)
{
   unsigned i;
   owner = NULL;
   owner_runtime = &owner_dummy;
   depth = bits;
   runtime_reset();
   expected_slots = 12;
   owner_io = io;
   owner_io.open_node = owner_node;
   exports.session.ops.profiles = owner_profiles;
   exports.session.ops.entrypoints = owner_entries;
   exports.session.ops.attributes = owner_attributes;
   exports.session.ops.create_config = owner_config;
   exports.submit.create = owner_create_buffer;
   exports.submit.destroy = owner_drop_buffer;
   exports.submit.begin = owner_begin;
   exports.submit.render = owner_render;
   exports.submit.end = owner_end;
   exports.submit.sync = owner_sync;
   owner_allocations = owner_alloc_fail = 0;
   owner_buffers = owner_drops = owner_syncs = owner_fds = owner_closes = 0;
   owner_create_fail = owner_begin_fail = 0;
   owner_completion = VA_STATUS_SUCCESS;
   memset(&owner_ops, 0, sizeof(owner_ops));
   memset(&owner_picture, 0, sizeof(owner_picture));
   owner_picture.header.w = owner_picture.header.h = 64;
   owner_picture.header.profile = bits == 10 ? 2 : 0;
   owner_picture.header.bit_depth = (int)bits;
   owner_picture.header.subsampling_x = owner_picture.header.subsampling_y = 1;
   owner_picture.header.uncomp_size = 18;
   owner_picture.header.first_partition_size = 20;
   owner_picture.slot = 0;
   owner_picture.previous_slot = -1;
   for (i = 0; i < 8; i++) owner_picture.reference_slots[i] = -1;
   for (i = 0; i < 3; i++) owner_picture.named_slots[i] = -1;
}
static bool owner_open(void)
{
   return vaapi_vp9_owner_open_with_io(&owner, "/render-test", 226, 128,
         &owner_picture.header, NULL, 2, 10, 128, &owner_io, &owner_exports, &owner_ops);
}

static void owner_lifecycle(unsigned bits, bool claimed)
{
   video_hw_frame_t frame;
   vaapi_frame_view_t view;
   vaapi_hevc_export_t taken;
   owner_setup(bits);
   assert(owner_open() && owner->runtime.session.slots == 12);
   assert(!vaapi_vp9_owner_retire(owner));
   assert(owner_ops.decode(owner, owner_packet, 129, &owner_picture) < 0 && !owner_buffers);
   assert(!owner_ops.decode(owner, owner_packet, 128, &owner_picture));
   owner_completion = VA_STATUS_ERROR_TIMEDOUT;
   assert(owner_ops.output(owner, 0, &frame) == 1 && !owner_fds);
   owner_completion = VA_STATUS_SUCCESS;
   assert(!owner_ops.output(owner, 0, &frame) && owner_drops == 3);
   memset(&taken, 0, sizeof(taken));
   if (claimed)
   {
      assert(vaapi_frame_claim(frame.owner, frame.done, frame.done_value, frame.image, &view));
      assert(vaapi_frame_take(frame.owner, frame.done, frame.done_value, &taken));
   }
   assert(vaapi_vp9_owner_detach(&owner_ops) == owner);
   assert(!owner_ops.decoder && !owner_ops.decode && !owner_ops.output);
   assert(!vaapi_vp9_owner_retire(owner) && !term_calls && !fd_closes);
   video_hw_frame_abandon(&frame);
   if (claimed)
   {
      assert(!vaapi_vp9_owner_retire(owner) && !term_calls);
      assert(vaapi_frame_finish(frame.owner, frame.done, frame.done_value, &taken));
   }
   term_failures = 1;
   assert(!vaapi_vp9_owner_retire(owner) && term_calls == 1 && !fd_closes);
   assert(vaapi_vp9_owner_retire(owner) && term_calls == 2 && fd_closes == 1);
   assert(owner_closes == 1 && !owner_fds && loads == unloads);
}

static void owner_failures(void)
{
   unsigned i;
   owner_setup(8);
   owner_alloc_fail = 1;
   assert(!owner_open() && !owner && !loads);
   for (i = 1; i <= 2; i++)
   {
      owner_setup(8);
      failed_load = i;
      assert(!owner_open() && owner && !owner_ops.decoder);
      assert(vaapi_vp9_owner_retire(owner) && loads == unloads && fd_closes == 1);
   }
   owner_setup(8);
   fault = CONTEXT;
   assert(!owner_open() && owner && !owner_ops.decoder);
   assert(vaapi_vp9_owner_retire(owner) && !live_context && fd_closes == 1);
   owner_setup(8);
   fault = NO_PROFILE;
   assert(!owner_open() && owner && !owner_ops.decoder);
   assert(vaapi_vp9_owner_retire(owner) && !live_config && fd_closes == 1);
   for (i = 1; i <= 3; i++)
   {
      owner_setup(8);
      assert(owner_open());
      owner_create_fail = i;
      assert(owner_ops.decode(owner, owner_packet, 128, &owner_picture) < 0);
      assert(vaapi_vp9_owner_detach(&owner_ops) == owner);
      assert(vaapi_vp9_owner_retire(owner) && owner_drops == i && term_calls == 1);
   }
   owner_setup(8);
   assert(owner_open());
   owner_begin_fail = 1;
   assert(owner_ops.decode(owner, owner_packet, 128, &owner_picture) < 0);
   assert(vaapi_vp9_owner_detach(&owner_ops) == owner);
   term_failures = 1;
   assert(!vaapi_vp9_owner_retire(owner) && owner->pool.active && !owner_drops);
   assert(vaapi_vp9_owner_retire(owner) && term_calls == 2 && !owner_drops);
}

int main(void)
{
   owner_lifecycle(8, false);
   owner_lifecycle(10, true);
   owner_failures();
   assert(!vaapi_vp9_owner_detach(NULL) && vaapi_vp9_owner_retire(NULL));
   puts("PASS: VP9 native owner, held renderer frames, partial startup and termination retry");
   return 0;
}
