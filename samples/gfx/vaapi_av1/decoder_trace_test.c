/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include "../../../gfx/common/vaapi_av1_decoder.h"
static vaapi_av1_session_t td_session;
static vaapi_av1_pool_t td_pool;
static rav1_hw_ops_t td_adapter;
static unsigned td_created, td_destroyed, td_tiles;
static VAStatus td_create(VADisplay d, VAContextID c, VABufferType type,
      unsigned bytes, unsigned count, void *data, VABufferID *id)
{
   assert(d == td_session.display && c == td_session.context && data);
   if (type == VAPictureParameterBufferType)
   {
      const VADecPictureParameterBufferAV1 *p
            = (const VADecPictureParameterBufferAV1 *)data;
      assert(count == 1 && bytes == sizeof(*p));
      assert(p->current_frame >= 100 && p->current_frame < 132);
      assert(p->current_display_picture >= 100
            && p->current_display_picture < 132);
   }
   else if (type == VASliceParameterBufferType)
      assert(bytes == sizeof(VASliceParameterBufferAV1) && count == td_tiles);
   else
      assert(type == VASliceDataBufferType && count == 1 && bytes);
   *id = ++td_created;
   return VA_STATUS_SUCCESS;
}
static VAStatus td_destroy(VADisplay d, VABufferID id)
{
   assert(d == td_session.display && id && id <= td_created);
   td_destroyed++;
   return VA_STATUS_SUCCESS;
}
static VAStatus td_begin(VADisplay d, VAContextID c, VASurfaceID surface)
{
   assert(d == td_session.display && c == td_session.context);
   assert(surface >= 100 && surface < 132);
   return VA_STATUS_SUCCESS;
}
static VAStatus td_render(
      VADisplay d, VAContextID c, VABufferID *ids, int count)
{
   assert(d == td_session.display && c == td_session.context && ids
         && count == 3);
   return VA_STATUS_SUCCESS;
}
static VAStatus td_end(VADisplay d, VAContextID c)
{
   assert(d == td_session.display && c == td_session.context);
   return VA_STATUS_SUCCESS;
}
static VAStatus td_sync(VADisplay d, VASurfaceID surface, uint64_t timeout)
{
   assert(d == td_session.display && surface >= 100 && surface < 132
         && !timeout);
   return VA_STATUS_SUCCESS;
}
static VAStatus td_export(
      VADisplay d, VASurfaceID s, uint32_t type, uint32_t flags, void *out)
{
   (void)d;
   (void)s;
   (void)type;
   (void)flags;
   (void)out;
   assert(0);
   return VA_STATUS_ERROR_OPERATION_FAILED;
}
static bool td_cloexec(int fd)
{
   (void)fd;
   assert(0);
   return false;
}
static void td_close(int fd)
{
   (void)fd;
   assert(0);
}
static bool td_identity(int fd, uint64_t *device, uint64_t *inode)
{
   (void)fd;
   (void)device;
   (void)inode;
   assert(0);
   return false;
}
static bool td_decode(const uint8_t *packet, size_t size,
      const rav1_sequence_t *s, const rav1_hw_picture_t *p)
{
   static const vaapi_av1_submit_ops_t native
         = {td_create, td_destroy, td_begin, td_render, td_end, td_sync};
   static const vaapi_av1_export_ops_t exports
         = {td_export, td_cloexec, td_close, td_identity};
   unsigned i;
   if (!td_adapter.decoder)
   {
      td_session.display = (VADisplay)&td_session;
      td_session.context = 1;
      td_session.ready = true;
      td_session.slots = 32;
      td_session.depth = s->bit_depth;
      td_session.width = s->max_width;
      td_session.height = s->max_height;
      td_session.profile = VAProfileAV1Profile0;
      for (i = 0; i < 32; i++)
         td_session.surfaces[i] = 100 + i;
      assert(vaapi_av1_pool_init(&td_pool, &td_session, &native));
      assert(vaapi_av1_decoder_open(&td_pool, s, NULL, &exports, &td_adapter));
   }
   td_tiles = p->tiles.layout.columns * p->tiles.layout.rows;
   assert(!td_adapter.decode(td_adapter.decoder, packet, size, s, p));
   assert(vaapi_av1_decoder_poll((vaapi_av1_decoder_t *)td_adapter.decoder)
         == 1);
   assert(td_created == td_destroyed);
   return true;
}
static bool (*volatile td_decode_call)(const uint8_t *, size_t,
      const rav1_sequence_t *, const rav1_hw_picture_t *) = td_decode;
#define VAAPI_AV1_TRACE_DECODE td_decode_call
#define VAAPI_AV1_TRACE_MAIN td_inspect
#include "tiles_trace_test.c"
#undef VAAPI_AV1_TRACE_MAIN
int main(int argc, char **argv)
{
   vaapi_av1_decoder_t *detached;
   int result = td_inspect(argc, argv);
   if (td_adapter.decoder)
   {
      detached = vaapi_av1_decoder_detach(&td_adapter);
      assert(detached && vaapi_av1_decoder_retire(detached));
      assert(vaapi_av1_pool_close(&td_pool));
   }
   return result;
}
