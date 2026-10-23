/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_av1_runtime.h"
#include <formats/image_yuv_blit.h>
static vaapi_av1_runtime_t *owner_test_runtime;
#define VAAPI_RUNTIME_TEST_MAIN runtime_unit_main
#define VAAPI_RUNTIME_TEST_CONTEXT owner_test_runtime
#include "../vaapi_hevc/runtime_test.c"
#undef runtime
#undef VAAPI_RUNTIME_TEST_MAIN
#undef VAAPI_RUNTIME_TEST_CONTEXT
static unsigned owner_allocations, owner_alloc_fail;

static void *owner_calloc(size_t count, size_t size)
{
   owner_allocations++;
   return owner_allocations == owner_alloc_fail ? NULL : calloc(count, size);
}

#define calloc owner_calloc
#include "../../../gfx/common/vaapi_av1_owner.c"
#undef calloc
static VAStatus av1_profiles(VADisplay d, VAProfile *p, int *n)
{
   check_display(d);
   assert(*n == 2);
   p[0] = fault == NO_PROFILE ? VAProfileH264High : VAProfileAV1Profile0;
   p[1] = VAProfileH264High;
   *n = fault == PROFILE_COUNT ? 257 : 2;
   return fault == PROFILE_QUERY ? VA_STATUS_ERROR_OPERATION_FAILED
                                 : VA_STATUS_SUCCESS;
}
static VAStatus av1_entrypoints(
      VADisplay d, VAProfile p, VAEntrypoint *entries, int *n)
{
   check_display(d);
   assert(p == VAProfileAV1Profile0);
   assert(*n == 2);
   entries[0] = fault == NO_ENTRY ? VAEntrypointEncSlice : VAEntrypointVLD;
   entries[1] = VAEntrypointEncSlice;
   *n = 2;
   return fault == ENTRY_QUERY ? VA_STATUS_ERROR_OPERATION_FAILED
                               : VA_STATUS_SUCCESS;
}
static VAStatus av1_attributes(VADisplay d, VAProfile p, VAEntrypoint entry,
      VAConfigAttrib *attrs, int n)
{
   check_display(d);
   assert(p == VAProfileAV1Profile0);
   assert(entry == VAEntrypointVLD && n == 3);
   assert(attrs[0].type == VAConfigAttribRTFormat);
   assert(attrs[1].type == VAConfigAttribMaxPictureWidth);
   assert(attrs[2].type == VAConfigAttribMaxPictureHeight);
   attrs[0].value = rt_mask;
   attrs[1].value = max_width;
   attrs[2].value = 2160;
   return fault == ATTRIBUTES ? VA_STATUS_ERROR_OPERATION_FAILED
                              : VA_STATUS_SUCCESS;
}
static VAStatus av1_config(VADisplay d, VAProfile p, VAEntrypoint entry,
      VAConfigAttrib *attrs, int n, VAConfigID *out)
{
   check_display(d);
   assert(p == VAProfileAV1Profile0);
   assert(entry == VAEntrypointVLD && n == 1);
   assert(attrs[0].type == VAConfigAttribRTFormat
         && attrs[0].value
               == (depth == 10 ? VA_RT_FORMAT_YUV420_10 : VA_RT_FORMAT_YUV420));
   assert(!live_config);
   configs++;
   if (fault == CONFIG_EMPTY_SUCCESS)
      return VA_STATUS_SUCCESS;
   live_config = 1;
   *out = 11;
   return fault == CONFIG ? VA_STATUS_ERROR_OPERATION_FAILED
                          : VA_STATUS_SUCCESS;
}

static vaapi_av1_runtime_t owner_dummy;
static vaapi_av1_owner_t *test_owner;
static rav1_hw_ops_t owner_ops;
static rav1_sequence_t owner_params;
static video_color_metadata_t owner_color;
static rav1_hw_picture_t owner_picture;
static const uint8_t owner_packet[] = {0x32, 4, 0, 1, 2, 3};
static unsigned owner_buffers;
static VAStatus owner_create(VADisplay d, VAContextID c, VABufferType type,
      unsigned bytes, unsigned count, void *data, VABufferID *id)
{
   assert(d == test_owner->runtime.display
         && c == test_owner->runtime.session.context);
   assert(data && bytes && count);
   if (type == VAPictureParameterBufferType)
   {
      const VADecPictureParameterBufferAV1 *p
            = (const VADecPictureParameterBufferAV1 *)data;
      assert(bytes == sizeof(*p) && p->current_frame == 100
            && p->current_display_picture == 101);
   }
   else if (type == VASliceParameterBufferType)
      assert(bytes == sizeof(VASliceParameterBufferAV1) && count == 1);
   else
      assert(type == VASliceDataBufferType && bytes == sizeof(owner_packet)
            && count == 1);
   *id = 500 + owner_buffers++;
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_destroy(VADisplay d, VABufferID id)
{
   assert(d == test_owner->runtime.display && id >= 500 && id < 503);
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_begin(VADisplay d, VAContextID c, VASurfaceID surface)
{
   assert(d == test_owner->runtime.display
         && c == test_owner->runtime.session.context && surface == 100);
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_render(
      VADisplay d, VAContextID c, VABufferID *ids, int count)
{
   assert(d == test_owner->runtime.display
         && c == test_owner->runtime.session.context && ids && count == 3);
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_end(VADisplay d, VAContextID c)
{
   assert(d == test_owner->runtime.display
         && c == test_owner->runtime.session.context);
   return VA_STATUS_SUCCESS;
}
static VAStatus owner_sync(VADisplay d, VASurfaceID surface, uint64_t timeout)
{
   assert(d == test_owner->runtime.display && (surface == 100 || surface == 101)
         && !timeout);
   return VA_STATUS_SUCCESS;
}
static const vaapi_av1_submit_ops_t owner_native = {owner_create, owner_destroy,
      owner_begin, owner_render, owner_end, owner_sync};
static unsigned owner_fds, owner_fd_closes;

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
   unsigned pitch = 64 * (depth == 10 ? 2 : 1);
   assert(display == test_owner->runtime.display && surface == 101);
   assert(memory == VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2);
   assert(flags
         == (VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS));
   assert(!owner_fds && test_owner->pool.exports == 2);
   d->fourcc = depth == 10 ? VA_FOURCC_P010 : VA_FOURCC_NV12;
   d->width = d->height = 64;
   d->num_objects = 1;
   d->objects[0].fd = 42;
   d->objects[0].size = pitch * 64 * 3 / 2;
   d->objects[0].drm_format_modifier = 0;
   d->num_layers = 2;
   d->layers[0].drm_format
         = depth == 10 ? UINT32_C(0x20363152) : UINT32_C(0x20203852);
   d->layers[1].drm_format
         = depth == 10 ? UINT32_C(0x32335247) : UINT32_C(0x38385247);
   d->layers[0].num_planes = d->layers[1].num_planes = 1;
   d->layers[0].object_index[0] = d->layers[1].object_index[0] = 0;
   d->layers[0].offset[0] = 0;
   d->layers[1].offset[0] = pitch * 64;
   d->layers[0].pitch[0] = d->layers[1].pitch[0] = pitch;
   owner_fds = 1;
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
   *inode = 2;
   return true;
}

static void owner_fd_close(int fd)
{
   assert(fd == 42 && owner_fds && !term_calls && !unloads && driver_live);
   owner_fds = 0;
   owner_fd_closes++;
}

static const vaapi_av1_export_ops_t owner_exports
      = {owner_export, owner_seal, owner_fd_close, owner_identity};

static function_t owner_symbol(dylib_t lib, const char *name)
{
   if (!strcmp(name, "vaExportSurfaceHandle"))
      return NULL;
   return symbol(lib, name);
}

static const vaapi_av1_runtime_io_t owner_io
      = {owner_node_open, node_close, load, owner_symbol, unload};

static void owner_reset(unsigned bits)
{
   owner_test_runtime = &owner_dummy;
   runtime_reset();
   depth = bits;
   expected_slots = 18;
   exports.submit = owner_native;
   exports.session.ops.profiles = av1_profiles;
   exports.session.ops.entrypoints = av1_entrypoints;
   exports.session.ops.attributes = av1_attributes;
   exports.session.ops.create_config = av1_config;
   memset(&owner_ops, 0, sizeof(owner_ops));
   memset(&owner_params, 0, sizeof(owner_params));
   memset(&owner_color, 0, sizeof(owner_color));
   owner_params.max_width = owner_params.max_height = 64;
   owner_params.bit_depth = bits;
   owner_params.operating_point_count = 1;
   owner_params.subsampling_x = owner_params.subsampling_y = 1;
   owner_params.color_primaries = owner_params.matrix_coefficients = 1;
   owner_params.transfer_characteristics = 13;
   owner_params.color_range = 1;
   owner_params.film_grain_params_present = 1;
   memset(&owner_picture, 0, sizeof(owner_picture));
   owner_picture.header.frame_is_intra = 1;
   owner_picture.header.primary_ref_frame = 7;
   owner_picture.setup.size.width = owner_picture.setup.size.upscaled_width
         = 64;
   owner_picture.setup.size.height = 64;
   owner_picture.setup.size.superres_denom = 8;
   owner_picture.tiles.layout.columns = owner_picture.tiles.layout.rows = 1;
   owner_picture.tiles.mi_col_starts[1] = owner_picture.tiles.mi_row_starts[1]
         = 16;
   owner_picture.grain.params.apply = 1;
   owner_picture.grain.params.scaling_shift = 8;
   owner_picture.grain.params.ar_coeff_shift = 6;
   {
      unsigned i;
      for (i = 0; i < 8; i++)
         owner_picture.reference_slots[i] = -1;
   }
   assert(rav1_tile_group_init(&owner_picture.tile_group, owner_packet + 2,
         sizeof(owner_packet) - 2, &owner_picture.tiles.layout, true));
   owner_buffers = 0;
   owner_color.primaries = owner_color.matrix = 1;
   owner_color.transfer = 13;
   owner_color.range = 2;
   test_owner = NULL;
   owner_allocations = owner_alloc_fail = 0;
   owner_fds = owner_fd_closes = 0;
}
static bool owner_open(void)
{
   return vaapi_av1_owner_open_with_io(&test_owner, "/render-test", 226, 128,
         &owner_params, &owner_color, 18, &owner_io, &owner_exports,
         &owner_ops);
}
static void owner_failures(unsigned bits)
{
   unsigned i;
   int f;
   for (i = 1; i <= 22; i++)
   {
      owner_reset(bits);
      missing_symbol = i;
      assert(!owner_open() && test_owner && !owner_ops.decoder);
      assert(vaapi_av1_owner_retire(test_owner));
      assert(loads == unloads && !fd_live && !driver_live && !display_live);
   }
   for (i = 1; i <= 3; i++)
   {
      owner_reset(bits);
      failed_load = i;
      assert(!owner_open() && test_owner && !owner_ops.decoder);
      assert(
            vaapi_av1_owner_retire(test_owner) && !fd_live && loads == unloads);
   }
   for (f = NONE; f < FAULT_END; f++)
   {
      owner_reset(bits);
      fault = (enum fault)f;
      assert(owner_open() == (f == NONE));
      if (f == NONE)
         assert(vaapi_av1_owner_detach(&owner_ops) == test_owner);
      assert(vaapi_av1_owner_retire(test_owner));
      assert(!fd_live && loads == unloads);
   }
   owner_reset(bits);
   owner_alloc_fail = 1;
   assert(!owner_open() && !test_owner && !loads);
   owner_reset(bits);
   assert(!vaapi_av1_owner_open_with_io(&test_owner, "/render-test", 226, 128,
         &owner_params, &owner_color, 18, &owner_io, NULL, &owner_ops));
   assert(test_owner && !owner_ops.decoder && loads == 3);
   assert(vaapi_av1_owner_retire(test_owner) && !fd_live);
   owner_reset(bits);
   owner_params.max_width = 65537;
   assert(!owner_open() && !test_owner && !loads);
   owner_reset(bits);
   owner_color.transfer = 16;
   assert(!owner_open() && !test_owner && !loads);
   owner_reset(bits);
   owner_params.chroma_sample_position = 3;
   assert(!owner_open() && !test_owner && !loads);
   owner_reset(bits);
   assert(!vaapi_av1_owner_open_with_io(&test_owner, "/render-test", 226, 128,
         &owner_params, &owner_color, 17, &owner_io, &owner_exports,
         &owner_ops));
   assert(!test_owner && !loads);
   owner_reset(bits);
   owner_params.film_grain_params_present = 0;
   expected_slots = 9;
   assert(vaapi_av1_owner_open_with_io(&test_owner, "/render-test", 226, 128,
         &owner_params, &owner_color, 9, &owner_io, &owner_exports,
         &owner_ops));
   assert(vaapi_av1_owner_detach(&owner_ops) == test_owner);
   assert(vaapi_av1_owner_retire(test_owner) && !fd_live && loads == unloads);
}
static void owner_lifetime(unsigned bits, bool claimed)
{
   video_hw_frame_t frame;
   vaapi_frame_view_t view;
   vaapi_av1_export_t taken;
   unsigned native_calls;
   owner_reset(bits);
   owner_params.chroma_sample_position = claimed ? 1 : 2;
   assert(owner_open() && owner_ops.decoder == test_owner);
   assert(!vaapi_av1_owner_retire(test_owner));
   assert(!owner_ops.decode(owner_ops.decoder, owner_packet,
         sizeof(owner_packet), &owner_params, &owner_picture));
   assert(!owner_ops.output(
         owner_ops.decoder, 0, &owner_picture.grain.params, &frame));
   assert(owner_fds && frame.width == 64 && frame.height == 64);
   assert(frame.yuv == (IMAGE_YUV_FLAG_FULL_RANGE | IMAGE_YUV_FLAG_BT709));
   memset(&taken, 0, sizeof(taken));
   if (claimed)
   {
      assert(vaapi_frame_claim(
            frame.owner, frame.done, frame.done_value, frame.image, &view));
      assert(vaapi_frame_take(
            frame.owner, frame.done, frame.done_value, &taken));
   }
   assert(vaapi_av1_owner_detach(&owner_ops) == test_owner);
   assert(!owner_ops.decoder && !owner_ops.decode && !owner_ops.output);
   assert(!vaapi_av1_owner_retire(test_owner));
   assert(!term_calls && !unloads && driver_live && fd_live && owner_fds);
   video_hw_frame_abandon(&frame);
   if (claimed)
   {
      assert(!vaapi_av1_owner_retire(test_owner));
      assert(vaapi_frame_finish(
            frame.owner, frame.done, frame.done_value, &taken));
   }
   term_failures = 2;
   assert(!vaapi_av1_owner_retire(test_owner));
   assert(!owner_fds && owner_fd_closes == 1 && !unloads && fd_live
         && !driver_live);
   native_calls = calls;
   assert(!vaapi_av1_owner_retire(test_owner) && calls == native_calls);
   assert(vaapi_av1_owner_retire(test_owner) && calls == native_calls);
   assert(term_calls == 3 && !fd_live && !display_live && loads == unloads);
}
int main(void)
{
   unsigned bits;
   for (bits = 8; bits <= 10; bits += 2)
   {
      owner_failures(bits);
      owner_lifetime(bits, false);
      owner_lifetime(bits, true);
   }
   puts("PASS: AV1 VA runtime/pool/decoder ownership, partial startup, "
        "retained exports and termination-only retry; mock native calls");
   return 0;
}
