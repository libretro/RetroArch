/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "mediacodec_hevc_decoder.h"
#include "mediacodec_hevc_packet.h"
#include "android_image_ndk.h"
#include <stdlib.h>
#include <string.h>
struct mediacodec_hevc_decoder {
 mediacodec_session_t *session;
 android_image_queue_t *images;
 mediacodec_hevc_import_t importer;
 rh265_video_params_t params;
 rh265_video_color_t color;
 mediacodec_output_format_t format;
 android_image_view_t image[32];
 uint64_t tag[32],last_tag;
 uint8_t *packet;
 unsigned slots,capacity;
 int failed,eos,closed;
};
static bool discard(android_image_view_t *v)
{
 if(!v->ticket)return true;
 if(!android_image_queue_return(v->ticket,v->generation,-1))return false;
 memset(v,0,sizeof(*v));return true;
}
/* Exactly one codec output action and one reader acquire per invocation. */
static int service(mediacodec_hevc_decoder_t *d)
{
 android_image_view_t image;
 const mediacodec_output_format_t *format;
 int64_t pts;
 uint64_t tag;
 unsigned i;
 int status;
 if(d->failed)return -1;
 android_image_queue_poll(d->images);
 status=mediacodec_session_pump(d->session,&pts);
 if(status<0)goto fail;
 if(status==2) {
  format=mediacodec_session_format(d->session);
  if(!format || format->width!=(unsigned)d->params.width
        || format->height!=(unsigned)d->params.height
        || format->left!=(unsigned)d->params.crop_left
        || format->top!=(unsigned)d->params.crop_top
        || format->right!=(unsigned)(d->params.width-d->params.crop_right)
        || format->bottom!=(unsigned)(d->params.height-d->params.crop_bottom))goto fail;
  d->format=*format;
 }
 status=android_image_queue_acquire(d->images,&image);
 if(status<0)goto fail;
 if(!status)return 0;
 tag=(uint64_t)image.timestamp_ns/1000;
 if(!tag || image.timestamp_ns%1000 || tag>d->last_tag) {
  discard(&image);goto fail;
 }
 for(i=0;i<d->slots;i++)if(d->tag[i]==tag)break;
 /* A discarded parser output may arrive after its reference slot was reused. */
 if(i==d->slots) {
  if(!discard(&image))goto fail;
  return 0;
 }
 if(d->image[i].ticket || !d->format.serial) {
  discard(&image);goto fail;
 }
 d->image[i]=image;return 0;
fail:
 d->failed=1;return -1;
}
static int decode(void *opaque,const rh265_video_params_t *params,
      const rh265_access_unit_t *au,const rh265_nal_t *slices,
      const rh265_hw_picture_t *picture)
{
 mediacodec_hevc_decoder_t *d=(mediacodec_hevc_decoder_t*)opaque;
 size_t bytes;
 int status;
 if(!d || !params || !au || !slices || !picture || d->closed || d->eos
       || picture->slot>=d->slots || !picture->serial
       || picture->serial>((~(uint64_t)0)>>1)/1000
       || memcmp(params,&d->params,sizeof(*params)))return -1;
 if(service(d)<0)return -1;
 if(mediacodec_hevc_packet(au,slices,au->num_slices,d->packet,d->capacity,&bytes)!=1)
  return -1;
 status=mediacodec_session_submit(d->session,d->packet,bytes,(int64_t)picture->serial,false);
 if(status<0){d->failed=1;return -1;}
 if(!status)return 1;
 if(!discard(&d->image[picture->slot])){d->failed=1;return -1;}
 d->tag[picture->slot]=picture->serial;d->last_tag=picture->serial;
 return 0;
}
static int output(void *opaque,unsigned slot,video_hw_frame_t *out)
{
 mediacodec_hevc_decoder_t *d=(mediacodec_hevc_decoder_t*)opaque;
 int status;
 if(!d || !out || slot>=d->slots || d->closed)return -1;
 if(service(d)<0)return -1;
 if(!d->image[slot].ticket)return 1;
 status=d->importer.export_image(d->importer.context,&d->image[slot],&d->format,&d->color,out);
 if(!status)memset(&d->image[slot],0,sizeof(d->image[slot]));
 if(status<0)d->failed=1;
 return status;
}
static int drain(void *opaque)
{
 mediacodec_hevc_decoder_t *d=(mediacodec_hevc_decoder_t*)opaque;
 int status;
 if(!d || d->closed || service(d)<0)return -1;
 if(d->eos)return 0;
 status=mediacodec_session_submit(d->session,NULL,0,0,true);
 if(status<0){d->failed=1;return -1;}
 if(!status)return 1;
 d->eos=1;return 0;
}
bool mediacodec_hevc_decoder_retire_poll(mediacodec_hevc_decoder_t *d)
{
 unsigned i;
 if(!d)return true;
 if(!d->closed)return false;
 for(i=0;i<d->slots;i++)if(!discard(&d->image[i]))return false;
 android_image_queue_poll(d->images);
 if(!android_image_queue_idle(d->images))return false;
 mediacodec_session_free(d->session);d->session=NULL;
 if(!android_image_queue_free(d->images))return false;
 d->importer.release(d->importer.context);
 free(d->packet);free(d);return true;
}
void mediacodec_hevc_decoder_close(rh265_hw_ops_t *ops)
{
 mediacodec_hevc_decoder_t *d;
 if(!ops || !(d=(mediacodec_hevc_decoder_t*)ops->decoder))return;
 memset(ops,0,sizeof(*ops));d->closed=1;
 android_image_queue_close(d->images);
 d->importer.enqueue(d->importer.context,d);
}
bool mediacodec_hevc_decoder_open(JNIEnv *env,const rh265_video *source,
      const rh265_video_params_t *params,unsigned slots,unsigned max_input,
      const mediacodec_hevc_import_t *importer,rh265_hw_ops_t *ops)
{
 mediacodec_hevc_decoder_t *d;
 mediacodec_config_t config;
 rh265_nal_t sets[3];
 uint8_t *csd;
 size_t bytes;
 void *window=NULL;
 if(!ops)return false;
 memset(ops,0,sizeof(*ops));
 if(!env || !source || !params || !slots || slots>32 || !max_input
       || max_input>64u*1024u*1024u || params->width<=0 || params->height<=0
       || params->chroma_format_idc!=1 || (params->bit_depth!=8 && params->bit_depth!=10)
       || params->pps_id<0 || !importer || !importer->export_image
       || !importer->enqueue || !importer->release)return false;
 if(rh265_video_get_parameter_sets(source,(unsigned)params->pps_id,sets)
       || mediacodec_hevc_csd(sets,NULL,0,&bytes)!=0 || !bytes)return false;
 csd=(uint8_t*)malloc(bytes);if(!csd)return false;
 if(mediacodec_hevc_csd(sets,csd,bytes,&bytes)!=1){free(csd);return false;}
 d=(mediacodec_hevc_decoder_t*)calloc(1,sizeof(*d));
 if(!d){free(csd);return false;}
 d->params=*params;d->slots=slots;d->capacity=max_input;
 d->packet=(uint8_t*)malloc(max_input);
 if(!d->packet || rh265_video_get_color(source,(unsigned)params->pps_id,&d->color))goto fail;
 d->images=android_image_ndk_new((unsigned)params->width,(unsigned)params->height,slots,&window);
 if(!d->images)goto fail;
 memset(&config,0,sizeof(config));config.mime="video/hevc";
 config.width=(unsigned)params->width;config.height=(unsigned)params->height;
 config.max_input=max_input;config.profile=params->bit_depth==10?2:1;
 config.csd[0]=csd;config.csd_size[0]=bytes;
 d->session=mediacodec_session_new(env,&config,window);
 if(!d->session)goto fail;
 d->importer=*importer;free(csd);
 ops->decoder=d;ops->decode=decode;ops->output=output;ops->drain=drain;
 return true;
fail:
 if(d->images){android_image_queue_close(d->images);android_image_queue_free(d->images);}
 free(d->packet);free(d);free(csd);return false;
}
