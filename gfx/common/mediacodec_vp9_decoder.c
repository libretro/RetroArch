/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mediacodec_vp9_decoder.h"
#include "android_image_ndk.h"
#include "mediacodec_color.h"
#include <stdlib.h>
#include <string.h>

struct mediacodec_vp9_decoder
{
   uint64_t serial, tag;
   mediacodec_session_t *session;
   android_image_queue_t *images;
   mediacodec_vp9_import_t importer;
   android_image_view_t image;
   rvp9_hdr header;
   video_color_metadata_t color;
   mediacodec_output_format_t format;
   unsigned slots, capacity, output_slot;
   bool failed, closed, eos, output_eos, rendered, pending_image;
};

static bool mediacodec_vp9_discard(android_image_view_t *image)
{
   if (!image->ticket)
      return true;
   if (!android_image_queue_return(image->ticket, image->generation, -1))
      return false;
   memset(image, 0, sizeof(*image));
   return true;
}

static int mediacodec_vp9_service(mediacodec_vp9_decoder_t *d)
{
   android_image_view_t image;
   const mediacodec_output_format_t *format;
   int64_t pts;
   uint64_t tag;
   int status;
   if (d->failed)
      return -1;
   android_image_queue_poll(d->images);
   status = d->pending_image ? 0 : mediacodec_session_pump(d->session, &pts);
   if (status < 0)
      goto fail;
   if (status == 1)
   {
      d->pending_image = true;
      if (pts <= 0 || (uint64_t)pts > d->serial)
         goto fail;
      if ((uint64_t)pts == d->tag)
         d->rendered = true;
   }
   if (status == 3)
   {
      if (!d->eos)
         goto fail;
      d->output_eos = true;
   }
   if (status == 2)
   {
      format = mediacodec_session_format(d->session);
      if (!format || format->width != (unsigned)d->header.w
            || format->height != (unsigned)d->header.h || format->left
            || format->top || format->right != (unsigned)d->header.w
            || format->bottom != (unsigned)d->header.h)
         goto fail;
      d->format = *format;
   }
   status = android_image_queue_acquire(d->images, &image);
   if (status < 0)
      goto fail;
   if (!status)
      return 0;
   d->pending_image = false;
   tag = (uint64_t)image.timestamp_ns / 1000;
   if (image.timestamp_ns <= 0 || image.timestamp_ns % 1000 || tag > d->serial)
   {
      mediacodec_vp9_discard(&image);
      goto fail;
   }
   if (tag != d->tag)
   {
      if (!mediacodec_vp9_discard(&image))
         goto fail;
      return 0;
   }
   if (d->image.ticket || !d->format.serial)
   {
      mediacodec_vp9_discard(&image);
      goto fail;
   }
   d->image = image;
   return 0;
fail:
   d->failed = true;
   return -1;
}

static int mediacodec_vp9_decode(void *opaque, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture)
{
   mediacodec_vp9_decoder_t *d = (mediacodec_vp9_decoder_t *)opaque;
   const rvp9_hdr *h;
   int status;
   if (!d || !packet || !size || !picture || d->closed || d->eos
         || size > d->capacity || picture->slot < 0
         || (unsigned)picture->slot >= d->slots
         || d->serial >= ((~(uint64_t)0) >> 1) / 1000)
      return -1;
   h = &picture->header;
   if (h->w != d->header.w || h->h != d->header.h
         || h->profile != d->header.profile
         || h->color_space != d->header.color_space
         || h->color_range != d->header.color_range
         || h->bit_depth != d->header.bit_depth || h->subsampling_x != 1
         || h->subsampling_y != 1)
      return -1;
   if (mediacodec_vp9_service(d) < 0)
      return -1;
   status = mediacodec_session_submit(
         d->session, packet, size, (int64_t)(d->serial + 1), false);
   if (status < 0)
   {
      d->failed = true;
      return -1;
   }
   if (!status)
      return 1;
   if (h->show_frame && !mediacodec_vp9_discard(&d->image))
   {
      d->failed = true;
      return -1;
   }
   d->serial++;
   if (h->show_frame)
   {
      d->tag = d->serial;
      d->output_slot = (unsigned)picture->slot;
      d->rendered = false;
   }
   d->header = *h;
   return 0;
}

static int mediacodec_vp9_output(
      void *opaque, unsigned slot, video_hw_frame_t *out)
{
   mediacodec_vp9_decoder_t *d = (mediacodec_vp9_decoder_t *)opaque;
   int status;
   if (!d || !out || d->closed || !d->tag || slot != d->output_slot)
      return -1;
   if (mediacodec_vp9_service(d) < 0)
      return -1;
   if (!d->image.ticket)
   {
      if (d->output_eos && !d->rendered)
      {
         d->failed = true;
         return -1;
      }
      return 1;
   }
   status = d->importer.export_image(d->importer.context, &d->image, &d->format,
         &d->header, &d->color, out);
   if (!status)
   {
      memset(&d->image, 0, sizeof(d->image));
      d->tag = 0;
   }
   if (status < 0)
      d->failed = true;
   return status;
}

int mediacodec_vp9_decoder_drain(void *opaque)
{
   mediacodec_vp9_decoder_t *d = (mediacodec_vp9_decoder_t *)opaque;
   int status;
   if (!d || d->closed || mediacodec_vp9_service(d) < 0)
      return -1;
   if (d->eos)
      return 0;
   status = mediacodec_session_submit(d->session, NULL, 0, 0, true);
   if (status < 0)
   {
      d->failed = true;
      return -1;
   }
   if (!status)
      return 1;
   d->eos = true;
   return 0;
}

int mediacodec_vp9_decoder_finish(void *opaque, bool discard)
{
   mediacodec_vp9_decoder_t *d = (mediacodec_vp9_decoder_t *)opaque;
   int status;
   if (!d || d->closed)
      return -1;
   if (discard)
   {
      if (!mediacodec_vp9_discard(&d->image))
         return -1;
      d->tag = 0;
   }
   status = mediacodec_vp9_decoder_drain(d);
   if (status)
      return status;
   if (d->output_eos && d->tag && !d->rendered && !d->image.ticket)
   {
      d->failed = true;
      return -1;
   }
   return d->output_eos && !d->tag ? 0 : 1;
}

int mediacodec_vp9_decoder_retire_fd(const mediacodec_vp9_decoder_t *d)
{
   return d ? android_image_queue_retire_fd(d->images) : -1;
}
bool mediacodec_vp9_decoder_retire_poll(mediacodec_vp9_decoder_t *d)
{
   if (!d)
      return true;
   if (!d->closed || !mediacodec_vp9_discard(&d->image))
      return false;
   if (d->images)
   {
      android_image_queue_poll(d->images);
      if (!android_image_queue_idle(d->images))
         return false;
      if (d->session)
      {
         mediacodec_session_free(d->session);
         d->session = NULL;
      }
      if (!android_image_queue_free(d->images))
         return false;
      d->images = NULL;
   }
   if (!d->importer.release(d->importer.context))
      return false;
   free(d);
   return true;
}

void mediacodec_vp9_decoder_close(rvp9_hw_ops_t *ops)
{
   mediacodec_vp9_decoder_t *d;
   if (!ops || ops->decode != mediacodec_vp9_decode || !ops->decoder)
      return;
   d = (mediacodec_vp9_decoder_t *)ops->decoder;
   memset(ops, 0, sizeof(*ops));
   d->closed = true;
   android_image_queue_close(d->images);
   d->importer.enqueue(d->importer.context, d);
}

bool mediacodec_vp9_decoder_open_color(JNIEnv *env, const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned slots, unsigned max_input,
      const mediacodec_vp9_import_t *importer, rvp9_hw_ops_t *ops)
{
   mediacodec_vp9_decoder_t *d;
   mediacodec_config_t config;
   void *window = NULL;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!env || !header || header->w <= 0 || header->h <= 0 || header->frame_type
         || header->show_existing_frame || header->subsampling_x != 1
         || header->subsampling_y != 1
         || (header->profile != 0 && header->profile != 2)
         || header->bit_depth != (header->profile == 2 ? 10 : 8) || slots < 10
         || slots > 32 || !max_input || max_input > 64u * 1024u * 1024u
         || !importer || !importer->export_image || !importer->enqueue
         || !importer->release)
      return false;
   d = (mediacodec_vp9_decoder_t *)calloc(1, sizeof(*d));
   if (!d)
      return false;
   d->header = *header;
   d->slots = slots;
   d->capacity = max_input;
   memset(&config, 0, sizeof(config));
   if (!mediacodec_vp9_color(header, color, &d->color)
         || !mediacodec_color_configure(&d->color, &config))
      goto fail;
   if ((d->color.transfer == 16 || d->color.transfer == 18)
         && header->bit_depth != 10)
      goto fail;
   config.mime = "video/x-vnd.on2.vp9";
   config.width = (unsigned)header->w;
   config.height = (unsigned)header->h;
   config.max_input = max_input;
   config.profile = header->profile == 2 ? 4 : 1;
   if (d->color.transfer == 16 || d->color.transfer == 18)
      config.profile = 4096;
   d->images = android_image_ndk_new(
         (unsigned)header->w, (unsigned)header->h, slots, &window);
   if (!d->images)
      goto fail;
   d->session = mediacodec_session_new(env, &config, window);
   if (!d->session)
      goto fail;
   d->importer = *importer;
   ops->decoder = d;
   ops->decode = mediacodec_vp9_decode;
   ops->output = mediacodec_vp9_output;
   return true;
fail:
   if (d->images)
   {
      android_image_queue_close(d->images);
      android_image_queue_free(d->images);
   }
   free(d);
   return false;
}

bool mediacodec_vp9_decoder_open(JNIEnv *env, const rvp9_hdr *header,
      unsigned slots, unsigned max_input,
      const mediacodec_vp9_import_t *importer, rvp9_hw_ops_t *ops)
{
   return mediacodec_vp9_decoder_open_color(
         env, header, NULL, slots, max_input, importer, ops);
}
