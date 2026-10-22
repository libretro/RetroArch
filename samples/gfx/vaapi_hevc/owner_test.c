/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_runtime.h"
#include <formats/image_yuv_blit.h>
static vaapi_hevc_runtime_t *owner_test_runtime;
#define VAAPI_RUNTIME_TEST_MAIN runtime_unit_main
#define VAAPI_RUNTIME_TEST_CONTEXT owner_test_runtime
#include "runtime_test.c"
#undef runtime
#undef VAAPI_RUNTIME_TEST_MAIN
#undef VAAPI_RUNTIME_TEST_CONTEXT
static unsigned owner_allocations, owner_alloc_fail;

static void *owner_calloc(size_t count, size_t size)
{
   owner_allocations++;
   return owner_allocations == owner_alloc_fail ? NULL : calloc(count, size);
}

static void *owner_malloc(size_t size)
{
   owner_allocations++;
   return owner_allocations == owner_alloc_fail ? NULL : malloc(size);
}

#define calloc owner_calloc
#define malloc owner_malloc
#include "../../../gfx/common/vaapi_hevc_owner.c"
#undef malloc
#undef calloc

static vaapi_hevc_runtime_t owner_dummy;
static vaapi_hevc_owner_t *test_owner;
static rh265_hw_ops_t owner_ops;
static rh265_video_params_t owner_params;
static video_color_metadata_t owner_color;
static unsigned owner_fds, owner_fd_closes;
static unsigned owner_yuv;

static int owner_node_open(
      const char *path, unsigned major_id, unsigned minor_id)
{
   assert(test_owner);
   owner_test_runtime = &test_owner->runtime;
   return node_open(path, major_id, minor_id);
}

static VAStatus owner_export(VADisplay display, VASurfaceID surface,
      uint32_t memory, uint32_t flags, void *out)
{
   VADRMPRIMESurfaceDescriptor *d = (VADRMPRIMESurfaceDescriptor *)out;
   unsigned pitch                 = 64 * (depth == 10 ? 2 : 1);
   assert(display == test_owner->runtime.display && surface == 100);
   assert(memory == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2);
   assert(flags
          == (VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS));
   assert(!owner_fds && test_owner->pool.exports == 1);
   d->fourcc = depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   d->width = d->height              = 64;
   d->num_objects                    = 1;
   d->objects[0].fd                  = 42;
   d->objects[0].size                = pitch * 64 * 3 / 2;
   d->objects[0].drm_format_modifier = 0;
   d->num_layers                     = 2;
   d->layers[0].drm_format =
         depth == 10 ? UINT32_C(0x20363152) : UINT32_C(0x20203852);
   d->layers[1].drm_format =
         depth == 10 ? UINT32_C(0x32335247) : UINT32_C(0x38385247);
   d->layers[0].num_planes = d->layers[1].num_planes = 1;
   d->layers[0].object_index[0] = d->layers[1].object_index[0] = 0;
   d->layers[0].offset[0]                                      = 0;
   d->layers[1].offset[0]                                      = pitch * 64;
   d->layers[0].pitch[0] = d->layers[1].pitch[0] = pitch;
   owner_fds                                     = 1;
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
   *device = 1;
   *inode  = 2;
   return true;
}

static void owner_fd_close(int fd)
{
   assert(fd == 42 && owner_fds && !term_calls && !unloads && driver_live);
   owner_fds = 0;
   owner_fd_closes++;
}

static const vaapi_hevc_export_ops_t owner_exports = {
      owner_export, owner_seal, owner_fd_close, owner_identity};

static function_t owner_symbol(dylib_t lib, const char *name)
{
   if (!strcmp(name, "vaExportSurfaceHandle"))
      return NULL;
   return symbol(lib, name);
}

static const vaapi_hevc_runtime_io_t owner_io = {
      owner_node_open, node_close, load, owner_symbol, unload};

static void owner_reset(unsigned bits)
{
   owner_yuv          = 0;
   owner_test_runtime = &owner_dummy;
   runtime_reset();
   depth = bits;
   mode  = VA_DEC_SLICE_MODE_BASE;
   memset(&owner_ops, 0, sizeof(owner_ops));
   memset(&owner_params, 0, sizeof(owner_params));
   memset(&owner_color, 0, sizeof(owner_color));
   owner_params.width = owner_params.height = 64;
   owner_params.bit_depth                   = (int)bits;
   owner_params.chroma_format_idc           = 1;
   owner_color.primaries = owner_color.matrix = 1;
   owner_color.transfer                       = 13;
   owner_color.range                          = 2;
   test_owner                                 = NULL;
   owner_allocations = owner_alloc_fail = 0;
   owner_fds = owner_fd_closes = 0;
}

static bool owner_open(void)
{
   return vaapi_hevc_owner_open_with_io(&test_owner, "/render-test", 226, 128,
         &owner_params, &owner_color, owner_yuv, 4, 4096, &owner_io,
         &owner_exports, &owner_ops);
}

static void owner_failures(unsigned bits)
{
   unsigned i;
   for (i = 1; i <= 22; i++)
   {
      owner_reset(bits);
      missing_symbol = i;
      assert(!owner_open() && test_owner && !owner_ops.decoder);
      assert(vaapi_hevc_owner_retire(test_owner));
      assert(loads == unloads && !fd_live && !driver_live && !display_live);
   }
   for (i = 1; i <= 3; i++)
   {
      owner_reset(bits);
      failed_load = i;
      assert(!owner_open() && test_owner && !owner_ops.decoder);
      assert(vaapi_hevc_owner_retire(test_owner) && !fd_live
             && loads == unloads);
   }
   for (i = 1; i <= 3; i++)
   {
      owner_reset(bits);
      owner_alloc_fail = i;
      assert(!owner_open() && !owner_ops.decoder && !loads);
      assert((test_owner != NULL) == (i > 1));
      assert(vaapi_hevc_owner_retire(test_owner));
   }
   owner_reset(bits);
   assert(!vaapi_hevc_owner_open_with_io(&test_owner, "/render-test", 226, 128,
         &owner_params, &owner_color, 0, 4, 4096, &owner_io, NULL, &owner_ops));
   assert(test_owner && !owner_ops.decoder && loads == 3);
   assert(vaapi_hevc_owner_retire(test_owner) && !fd_live);
   owner_reset(bits);
   owner_params.crop_left = 1;
   assert(!owner_open() && test_owner && !owner_ops.decoder && loads == 3);
   assert(vaapi_hevc_owner_retire(test_owner) && !fd_live && !driver_live);
   owner_reset(bits);
   owner_yuv = IMAGE_YUV_FLAG_VU;
   assert(!owner_open() && !test_owner && !owner_ops.decoder && !loads);
   owner_yuv = IMAGE_YUV_FLAG_RGBA;
   assert(!owner_open() && !test_owner && !owner_ops.decoder && !loads);
   owner_reset(bits);
   owner_params.width = 7;
   assert(!owner_open() && !test_owner && !owner_ops.decoder && !loads);
   assert(vaapi_hevc_owner_retire(NULL));
}

static void owner_lifetime(unsigned bits, bool claimed)
{
   video_hw_frame_t frame;
   vaapi_frame_view_t view;
   vaapi_hevc_export_t taken;
   unsigned native_calls;
   owner_reset(bits);
   owner_yuv = IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709;
   assert(owner_open() && owner_ops.decoder == test_owner);
   assert(!vaapi_hevc_owner_retire(test_owner));
   assert(!owner_ops.drain(owner_ops.decoder));
   test_owner->pool.ready = test_owner->pool.valid = 1;
   test_owner->pool.generation[0]                  = 1;
   assert(!owner_ops.output(owner_ops.decoder, 0, &frame));
   assert(owner_fds && frame.width == 64 && frame.height == 64);
   assert(frame.yuv == owner_yuv);
   memset(&taken, 0, sizeof(taken));
   if (claimed)
   {
      assert(vaapi_frame_claim(
            frame.owner, frame.done, frame.done_value, frame.image, &view));
      assert(vaapi_frame_take(
            frame.owner, frame.done, frame.done_value, &taken));
   }
   assert(vaapi_hevc_owner_detach(&owner_ops) == test_owner);
   assert(!owner_ops.decoder && !owner_ops.decode && !owner_ops.output
          && !owner_ops.drain);
   assert(!vaapi_hevc_owner_retire(test_owner));
   assert(!term_calls && !unloads && driver_live && fd_live && owner_fds);
   video_hw_frame_abandon(&frame);
   if (claimed)
   {
      assert(!vaapi_hevc_owner_retire(test_owner));
      assert(vaapi_frame_finish(
            frame.owner, frame.done, frame.done_value, &taken));
   }
   term_failures = 2;
   assert(!vaapi_hevc_owner_retire(test_owner));
   assert(!owner_fds && owner_fd_closes == 1 && !unloads && fd_live
          && !driver_live);
   native_calls = calls;
   assert(!vaapi_hevc_owner_retire(test_owner) && calls == native_calls);
   assert(vaapi_hevc_owner_retire(test_owner) && calls == native_calls);
   assert(term_calls == 3 && !fd_live && !display_live && loads == unloads);
}

static unsigned owner_polls;
static bool owner_complete;

static VAStatus owner_sync(
      VADisplay display, VASurfaceID surface, uint64_t timeout)
{
   assert(display == test_owner->runtime.display && surface == 100 && !timeout);
   owner_polls++;
   return owner_complete ? VA_STATUS_SUCCESS : VA_STATUS_ERROR_TIMEDOUT;
}

static void owner_transaction(unsigned bits, bool poisoned)
{
   vaapi_hevc_submit_t *transaction;
   unsigned native_calls;
   owner_reset(bits);
   assert(owner_open());
   transaction             = &test_owner->pool.transactions[0];
   transaction->active     = true;
   transaction->submitted  = !poisoned;
   transaction->poisoned   = poisoned;
   transaction->surface    = 100;
   transaction->display    = test_owner->runtime.display;
   transaction->ops        = test_owner->runtime.submit;
   transaction->ops.sync   = owner_sync;
   test_owner->pool.broken = poisoned;
   owner_polls             = 0;
   owner_complete          = false;
   assert(vaapi_hevc_owner_detach(&owner_ops) == test_owner);
   if (poisoned)
   {
      term_failures = 1;
      assert(!vaapi_hevc_owner_retire(test_owner));
      assert(!owner_polls && !unloads && fd_live && !driver_live);
      native_calls = calls;
      assert(vaapi_hevc_owner_retire(test_owner) && calls == native_calls);
   }
   else
   {
      assert(!vaapi_hevc_owner_retire(test_owner) && owner_polls == 1);
      assert(!term_calls && driver_live && fd_live);
      assert(!vaapi_hevc_owner_retire(test_owner) && owner_polls == 2);
      owner_complete = true;
      assert(vaapi_hevc_owner_retire(test_owner) && owner_polls == 3);
   }
   assert(!fd_live && !display_live && loads == unloads);
}

typedef struct codec_case
{
   const char *name;
   const uint8_t *hvcc;
   size_t hvcc_size;
   const uint8_t *data;
   size_t size;
   unsigned depth, full;
   uint32_t crc[3];
} codec_case_t;

typedef struct sequence_packet
{
   int64_t dts;
   const uint8_t *data;
   size_t size;
   uint32_t crc[3];
   unsigned display_index, slice_type;
} sequence_packet_t;

typedef struct sequence_case
{
   codec_case_t picture;
   const sequence_packet_t *packets;
   unsigned reordered;
} sequence_case_t;

#include "../videotoolbox_hevc/sequence_data.h"
static rh265_access_unit_t owner_au;
static rh265_hw_state_t owner_state;
static rh265_hw_picture_t owner_picture;
static rh265_nal_t owner_nals[8];
static unsigned owner_created, owner_dropped;

static VAStatus owner_create_buffer(VADisplay display, VAContextID context,
      VABufferType type, unsigned size, unsigned count, void *data,
      VABufferID *id)
{
   assert(display == test_owner->runtime.display && context == 22 && data);
   assert(owner_created < 5 && size && count);
   if (type == VASliceParameterBufferType)
      assert(count == owner_au.num_slices
             && size
                      == (mode == VA_DEC_SLICE_MODE_NORMAL
                                  ? sizeof(VASliceParameterBufferHEVC)
                                  : sizeof(VASliceParameterBufferBase)));
   *id = 200 + owner_created++;
   return VA_STATUS_SUCCESS;
}

static VAStatus owner_destroy_buffer(VADisplay display, VABufferID id)
{
   assert(display == test_owner->runtime.display && id == 200 + owner_dropped);
   owner_dropped++;
   return VA_STATUS_SUCCESS;
}

static VAStatus owner_begin_picture(
      VADisplay display, VAContextID context, VASurfaceID surface)
{
   assert(display == test_owner->runtime.display && context == 22
          && surface == 100);
   return VA_STATUS_SUCCESS;
}

static VAStatus owner_render_picture(
      VADisplay display, VAContextID context, VABufferID *ids, int count)
{
   unsigned i;
   assert(display == test_owner->runtime.display && context == 22
          && (unsigned)count == owner_created);
   for (i = 0; i < (unsigned)count; i++)
      assert(ids[i] == 200 + i);
   return VA_STATUS_SUCCESS;
}

static VAStatus owner_end_picture(VADisplay display, VAContextID context)
{
   assert(display == test_owner->runtime.display && context == 22);
   return VA_STATUS_SUCCESS;
}

static rh265_video *owner_source_setup(unsigned bits)
{
   const codec_case_t *fixture = &sequence_cases[bits == 8 ? 0 : 1].picture;
   rh265_video *source;
   owner_reset(bits);
   source = rh265_video_open();
   assert(source
          && !rh265_video_set_extradata(
                source, fixture->hvcc, fixture->hvcc_size));
   assert(!rh265_video_get_access_unit(
         source, fixture->data, fixture->size, 4, owner_nals, 8, &owner_au));
   assert(!rh265_video_get_params(
         source, (unsigned)owner_au.first.pps_id, &owner_params));
   return source;
}

static bool owner_source_open(rh265_video *source)
{
   return vaapi_hevc_owner_open_source_with_io(&test_owner, source,
         "/render-test", 226, 128, &owner_params, &owner_color, 0, 4, 4096,
         &owner_io, &owner_exports, &owner_ops);
}

static void owner_source_case(unsigned bits, unsigned mask, bool claimed)
{
   video_hw_frame_t frame;
   vaapi_frame_view_t view;
   vaapi_hevc_export_t taken;
   rh265_video *source = owner_source_setup(bits);
   mode                = mask == VA_DEC_SLICE_MODE_BASE ? VA_DEC_SLICE_MODE_BASE
                                                        : VA_DEC_SLICE_MODE_NORMAL;
   slice_mask          = mask;
   exports.submit.create  = owner_create_buffer;
   exports.submit.destroy = owner_destroy_buffer;
   exports.submit.begin   = owner_begin_picture;
   exports.submit.render  = owner_render_picture;
   exports.submit.end     = owner_end_picture;
   exports.submit.sync    = owner_sync;
   owner_created = owner_dropped = owner_polls = 0;
   owner_complete                              = false;
   assert(owner_source_open(source));
   assert(test_owner->runtime.session.slice_mode == mode);
   assert((test_owner->pool.source == source)
          == (mode == VA_DEC_SLICE_MODE_NORMAL));
   assert(rh265_hw_state_init(&owner_state, 4));
   assert(!rh265_hw_prepare(
         &owner_state, &owner_params, &owner_au, &owner_picture));
   assert(!owner_ops.decode(owner_ops.decoder, &owner_params, &owner_au,
         owner_nals, &owner_picture));
   assert(rh265_hw_commit(&owner_state, &owner_picture));
   assert(owner_ops.output(owner_ops.decoder, owner_picture.slot, &frame) == 1);
   owner_complete = true;
   assert(!owner_ops.output(owner_ops.decoder, owner_picture.slot, &frame));
   assert(owner_created == owner_dropped && owner_fds);
   memset(&taken, 0, sizeof(taken));
   if (claimed)
   {
      assert(vaapi_frame_claim(
            frame.owner, frame.done, frame.done_value, frame.image, &view));
      assert(vaapi_frame_take(
            frame.owner, frame.done, frame.done_value, &taken));
   }
   assert(vaapi_hevc_owner_detach(&owner_ops) == test_owner
          && !test_owner->pool.source);
   rh265_video_close(source);
   assert(!vaapi_hevc_owner_retire(test_owner) && !term_calls && owner_fds);
   video_hw_frame_abandon(&frame);
   if (claimed)
   {
      assert(!vaapi_hevc_owner_retire(test_owner));
      assert(vaapi_frame_finish(
            frame.owner, frame.done, frame.done_value, &taken));
   }
   term_failures = 1;
   assert(!vaapi_hevc_owner_retire(test_owner) && !owner_fds && !unloads);
   assert(vaapi_hevc_owner_retire(test_owner) && loads == unloads && !fd_live);
}

static void owner_source_failures(unsigned bits)
{
   unsigned i;
   for (i = 1; i <= 4; i++)
   {
      rh265_video *source = owner_source_setup(bits);
      owner_alloc_fail    = i;
      assert(!owner_source_open(source) && !owner_ops.decoder && !loads);
      assert((test_owner != NULL) == (i > 1));
      rh265_video_close(source);
      assert(vaapi_hevc_owner_retire(test_owner));
   }
   {
      rh265_video *source = owner_source_setup(bits);
      mode                = VA_DEC_SLICE_MODE_NORMAL;
      fault               = CONTEXT;
      assert(!owner_source_open(source) && test_owner && !owner_ops.decoder);
      rh265_video_close(source);
      term_failures = 1;
      assert(!vaapi_hevc_owner_retire(test_owner) && !unloads && fd_live);
      assert(vaapi_hevc_owner_retire(test_owner) && loads == unloads
             && !fd_live);
   }
}

int main(void)
{
   unsigned bits, i;
   static const unsigned masks[4] = {
         VA_DEC_SLICE_MODE_NORMAL | VA_DEC_SLICE_MODE_BASE,
         VA_DEC_SLICE_MODE_NORMAL, VA_DEC_SLICE_MODE_BASE,
         VA_ATTRIB_NOT_SUPPORTED};
   owner_test_runtime = &owner_dummy;
   assert(!runtime_unit_main());
   for (bits = 8; bits <= 10; bits += 2)
   {
      owner_failures(bits);
      owner_lifetime(bits, false);
      owner_lifetime(bits, true);
      owner_transaction(bits, false);
      owner_transaction(bits, true);
      owner_source_failures(bits);
      for (i = 0; i < 4; i++)
      {
         owner_source_case(bits, masks[i], false);
         owner_source_case(bits, masks[i], true);
      }
   }
   puts("PASS: cold VA runtime/pool/decoder ownership, partial-open cleanup, held opaque exports and termination-only retry");
   return 0;
}
