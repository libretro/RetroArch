/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_session_vulkan_hevc.h"
#include "record_gpu_session_vulkan_backend.h"
#include "record_gpu_worker_vulkan_hevc.h"
#include <stdlib.h>

static bool record_vulkan_hevc_drain(void *encoder)
{
   return vulkan_encode_hevc_drain((vulkan_encode_hevc_t*)encoder);
}

static bool record_vulkan_hevc_free(void *encoder)
{
   return vulkan_encode_hevc_free((vulkan_encode_hevc_t*)encoder);
}

record_gpu_session_vulkan_t *record_gpu_session_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *config,
      const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   static const record_gpu_session_vulkan_backend_t backend = {
      record_vulkan_hevc_drain, record_vulkan_hevc_free
   };
   record_gpu_session_vulkan_t *s;
   vulkan_encode_hevc_t *encoder = NULL;
   record_gpu_worker_vulkan_t *worker = NULL;
   record_gpu_output_vulkan_config_t mux;
   void *headers;
   size_t size = 0;
   if (!config || !ops || (unsigned)config->session.video.color > VULKAN_ENCODE_HEVC_HLG)
      return NULL;
   s = record_gpu_session_vulkan_allocate(config->slots, output, path);
   if (!s)
      return NULL;
   mux = *output;
   headers = malloc(65536);
   if (headers)
   {
      encoder = vulkan_encode_hevc_open(config, ops);
      if (encoder && vulkan_encode_hevc_headers(encoder, headers, 65536, &size))
      {
         static const unsigned colors[4][3] = {
            {2, 2, 2}, {1, 1, 1}, {9, 16, 9}, {9, 18, 9}
         };
         unsigned color = (unsigned)config->session.video.color;
         worker = record_gpu_worker_vulkan_new_hevc(
               record_gpu_session_vulkan_queue(s), encoder);
         mux.video.headers = headers;
         mux.video.headers_size = size;
         mux.video.codec = RECORD_MP4_HEVC;
         mux.video.bit_depth = config->session.video.bit_depth;
         mux.video.width = config->session.video.width;
         mux.video.height = config->session.video.height;
         mux.video.timescale = config->session.video.fps_num;
         mux.video.color_primaries = colors[color][0];
         mux.video.transfer_characteristics = colors[color][1];
         mux.video.matrix_coefficients = colors[color][2];
         mux.video.full_range = config->session.video.full_range;
      }
   }
   record_gpu_session_vulkan_start(s, encoder, &backend,
         encoder ? vulkan_encode_hevc_pool(encoder) : NULL,
         worker, &mux, path);
   free(headers);
   return s;
}
