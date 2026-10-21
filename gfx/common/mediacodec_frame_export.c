/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "mediacodec_frame_export.h"
#include "mediacodec_color.h"
#include <formats/image.h>
#include <stdlib.h>
#include <string.h>
struct mediacodec_frame_exporter {
 android_frame_pool_t *pool;
 unsigned pixfmt;
};
mediacodec_frame_exporter_t *mediacodec_frame_exporter_new(unsigned slots,unsigned pixfmt)
{
 mediacodec_frame_exporter_t *e;
 if(pixfmt!=IMAGE_PIXFMT_NV12 && pixfmt!=IMAGE_PIXFMT_P010)return NULL;
 e=(mediacodec_frame_exporter_t*)calloc(1,sizeof(*e));if(!e)return NULL;
 e->pool=android_frame_pool_new(slots);e->pixfmt=pixfmt;
 if(!e->pool){free(e);return NULL;}return e;
}
bool mediacodec_frame_exporter_free(mediacodec_frame_exporter_t *e)
{
 if(!e)return true;
 android_frame_pool_close(e->pool);
 if(!android_frame_pool_free(e->pool))return false;
 free(e);return true;
}
int mediacodec_hevc_frame_export(void *opaque,const android_image_view_t *image,
      const mediacodec_output_format_t *format,const rh265_video_color_t *coded,
      video_hw_frame_t *frame)
{
 mediacodec_frame_exporter_t *e=(mediacodec_frame_exporter_t*)opaque;
 android_frame_view_t view;
 video_color_metadata_t color;
 if(!e || !image || !format || !coded || !frame)return -1;
 memset(&color,0,sizeof(color));
 if(coded->description_present) {
  color.primaries=coded->primaries;color.transfer=coded->transfer;color.matrix=coded->matrix;
 }
 if(coded->video_signal_present)color.range=coded->full_range?2:1;
 memset(&view,0,sizeof(view));
 if(!mediacodec_color_resolve(format,&color,&view.color)
       || !view.color.range || !view.color.matrix || view.color.matrix==2
       || !view.color.transfer || view.color.transfer==2
       || !view.color.primaries || view.color.primaries==2)return -1;
 view.image=*image;view.format=*format;view.pixfmt=e->pixfmt;
 return android_frame_export(e->pool,&view,frame);
}
