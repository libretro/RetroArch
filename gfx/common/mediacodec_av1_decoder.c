/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "mediacodec_av1_decoder.h"
#include "android_image_ndk.h"
#include "mediacodec_color.h"
#include <stdlib.h>
#include <string.h>

struct mediacodec_av1_decoder
{
   uint64_t serial, tag;
   mediacodec_session_t *session;
   android_image_queue_t *images;
   uint8_t *config_obus;
   size_t config_size;
   mediacodec_av1_import_t importer;
   android_image_view_t image;
   rav1_sequence_t sequence;
   rav1_grain_t grain;
   video_color_metadata_t color;
   mediacodec_output_format_t format, display_format;
   unsigned slots, capacity, output_slot, output_width, output_height;
   bool failed, closed, eos, output_eos, rendered, pending_image, sequence_sent;
};

static bool mediacodec_av1_discard(android_image_view_t *image)
{
   if (!image->ticket)
      return true;
   if (!android_image_queue_return(image->ticket, image->generation, -1))
      return false;
   memset(image, 0, sizeof(*image));
   return true;
}

static int mediacodec_av1_service(mediacodec_av1_decoder_t *d)
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
      {
         d->rendered = true;
         if (!d->display_format.serial)
            d->display_format = d->format;
      }
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
      if (!format || !format->width || !format->height
            || format->width > d->sequence.max_width
            || format->height > d->sequence.max_height
            || format->left >= format->right || format->top >= format->bottom
            || format->right > format->width || format->bottom > format->height)
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
      mediacodec_av1_discard(&image);
      goto fail;
   }
   if (tag != d->tag)
   {
      if (!mediacodec_av1_discard(&image))
         goto fail;
      return 0;
   }
   if (d->image.ticket || !d->format.serial)
   {
      mediacodec_av1_discard(&image);
      goto fail;
   }
   if (!d->display_format.serial)
      d->display_format = d->format;
   d->image = image;
   return 0;
fail:
   d->failed = true;
   return -1;
}

static int mediacodec_av1_decode(void *opaque, const uint8_t *packet,
      size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture)
{
   mediacodec_av1_decoder_t *d = (mediacodec_av1_decoder_t *)opaque;
   bool show;
   int status;
   if (!d || !packet || !size || !picture || d->closed || d->eos
         || size > d->capacity || picture->slot < 0
         || (unsigned)picture->slot >= d->slots
         || d->serial >= ((~(uint64_t)0) >> 1) / 1000)
      return -1;
   if (!sequence || memcmp(sequence, &d->sequence, sizeof(*sequence))
         || !picture->setup.size.upscaled_width || !picture->setup.size.height
         || picture->setup.size.upscaled_width > sequence->max_width
         || picture->setup.size.height > sequence->max_height)
      return -1;
   show = picture->header.show_frame || picture->header.show_existing;
   if (mediacodec_av1_service(d) < 0)
      return -1;
   if (!d->sequence_sent)
   {
      /* Frame-only submissions still need an in-band sequence. */
      status = mediacodec_session_submit(
            d->session, d->config_obus, d->config_size, 0, false);
      if (status < 0)
      {
         d->failed = true;
         return -1;
      }
      if (!status)
         return 1;
      d->sequence_sent = true;
   }
   status = mediacodec_session_submit(
         d->session, packet, size, (int64_t)(d->serial + 1), false);
   if (status < 0)
   {
      d->failed = true;
      return -1;
   }
   if (!status)
      return 1;
   if (show && !mediacodec_av1_discard(&d->image))
   {
      d->failed = true;
      return -1;
   }
   d->serial++;
   if (show)
   {
      d->tag = d->serial;
      d->output_slot = (unsigned)picture->slot;
      d->rendered = false;
      memset(&d->display_format, 0, sizeof(d->display_format));
   }
   if (show)
   {
      d->output_width = picture->setup.size.upscaled_width;
      d->output_height = picture->setup.size.height;
      d->grain = picture->grain.params;
   }
   return 0;
}

static int mediacodec_av1_output(void *opaque, unsigned slot,
      const rav1_grain_t *grain, video_hw_frame_t *out)
{
   mediacodec_av1_decoder_t *d = (mediacodec_av1_decoder_t *)opaque;
   int status;
   if (!d || !out || !grain || d->closed || !d->tag || slot != d->output_slot
         || memcmp(grain, &d->grain, sizeof(*grain)))
      return -1;
   if (mediacodec_av1_service(d) < 0)
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
   if (d->image.right - d->image.left != d->output_width
         || d->image.bottom - d->image.top != d->output_height
         || d->display_format.right - d->display_format.left != d->output_width
         || d->display_format.bottom - d->display_format.top
               != d->output_height)
   {
      d->failed = true;
      return -1;
   }
   status = d->importer.export_image(d->importer.context, &d->image,
         &d->display_format, &d->sequence, &d->color, out);
   if (!status)
   {
      memset(&d->image, 0, sizeof(d->image));
      d->tag = 0;
   }
   if (status < 0)
      d->failed = true;
   return status;
}

static int mediacodec_av1_decoder_drain(void *opaque)
{
   mediacodec_av1_decoder_t *d = (mediacodec_av1_decoder_t *)opaque;
   int status;
   if (!d || d->closed || mediacodec_av1_service(d) < 0)
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

int mediacodec_av1_decoder_finish(void *opaque, bool discard)
{
   mediacodec_av1_decoder_t *d = (mediacodec_av1_decoder_t *)opaque;
   int status;
   if (!d || d->closed)
      return -1;
   if (discard)
   {
      if (!mediacodec_av1_discard(&d->image))
         return -1;
      d->tag = 0;
   }
   status = mediacodec_av1_decoder_drain(d);
   if (status)
      return status;
   if (d->output_eos && d->tag && !d->rendered && !d->image.ticket)
   {
      d->failed = true;
      return -1;
   }
   return d->output_eos && !d->tag ? 0 : 1;
}

int mediacodec_av1_decoder_retire_fd(const mediacodec_av1_decoder_t *d)
{
   return d ? android_image_queue_retire_fd(d->images) : -1;
}
bool mediacodec_av1_decoder_retire_poll(mediacodec_av1_decoder_t *d)
{
   if (!d)
      return true;
   if (!d->closed || !mediacodec_av1_discard(&d->image))
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
   free(d->config_obus);
   free(d);
   return true;
}

void mediacodec_av1_decoder_close(rav1_hw_ops_t *ops)
{
   mediacodec_av1_decoder_t *d;
   if (!ops || ops->decode != mediacodec_av1_decode || !ops->decoder)
      return;
   d = (mediacodec_av1_decoder_t *)ops->decoder;
   memset(ops, 0, sizeof(*ops));
   d->closed = true;
   android_image_queue_close(d->images);
   d->importer.enqueue(d->importer.context, d);
}

bool mediacodec_av1_decoder_open(JNIEnv *env, const uint8_t *av1c,
      size_t av1c_size, const video_color_metadata_t *color, unsigned slots,
      unsigned max_input, const mediacodec_av1_import_t *importer,
      rav1_hw_ops_t *ops)
{
   mediacodec_av1_decoder_t *d;
   mediacodec_config_t config;
   rav1_config_t record;
   void *window = NULL;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   if (!env || !av1c || av1c_size > 1024u * 1024u || slots < 9 || slots > 32
         || !max_input || max_input > 64u * 1024u * 1024u || !importer
         || !importer->export_image || !importer->enqueue || !importer->release
         || !rav1_config_parse(&record, av1c, av1c_size))
      return false;
   d = (mediacodec_av1_decoder_t *)calloc(1, sizeof(*d));
   if (!d)
      return false;
   if (!rav1_config_sequence(&d->sequence, &record) || d->sequence.profile
         || d->sequence.monochrome || d->sequence.subsampling_x != 1
         || d->sequence.subsampling_y != 1
         || (d->sequence.bit_depth != 8 && d->sequence.bit_depth != 10)
         || !d->sequence.max_width || d->sequence.max_width > 16384
         || !d->sequence.max_height || d->sequence.max_height > 16384
         || d->sequence.operating_point_count != 1
         || d->sequence.operating_points[0].idc)
      goto fail;
   if (!mediacodec_av1_color(&d->sequence, color, &d->color))
      goto fail;
   if (record.obus_size > max_input)
      goto fail;
   d->config_obus = (uint8_t *)malloc(record.obus_size);
   if (!d->config_obus)
      goto fail;
   memcpy(d->config_obus, record.obus, record.obus_size);
   d->config_size = record.obus_size;
   d->slots = slots;
   d->capacity = max_input;
   memset(&config, 0, sizeof(config));
   if (!mediacodec_color_configure(&d->color, &config))
      goto fail;
   if ((d->color.transfer == 16 || d->color.transfer == 18)
         && d->sequence.bit_depth != 10)
      goto fail;
   config.mime = "video/av01";
   config.width = d->sequence.max_width;
   config.height = d->sequence.max_height;
   config.max_input = max_input;
   config.profile = d->sequence.bit_depth == 10 ? 2 : 1;
   if (d->color.transfer == 16)
      config.profile = 4096;
   config.csd[0] = av1c;
   config.csd_size[0] = av1c_size;
   d->images
         = android_image_ndk_new(config.width, config.height, slots, &window);
   if (!d->images)
      goto fail;
   d->session = mediacodec_session_new(env, &config, window);
   if (!d->session)
      goto fail;
   d->importer = *importer;
   ops->decoder = d;
   ops->decode = mediacodec_av1_decode;
   ops->output = mediacodec_av1_output;
   return true;
fail:
   if (d->images)
   {
      android_image_queue_close(d->images);
      android_image_queue_free(d->images);
   }
   free(d->config_obus);
   free(d);
   return false;
}
