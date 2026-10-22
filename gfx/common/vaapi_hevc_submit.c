/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_submit.h"
#include <string.h>
bool vaapi_hevc_submit_start(vaapi_hevc_submit_t *t,const vaapi_hevc_session_t *s,
 const vaapi_hevc_submit_ops_t *ops,const VAPictureParameterBufferHEVC *picture,
 const VAIQMatrixBufferHEVC *matrix,const void *slices,unsigned count,
 const uint8_t *data,unsigned size)
{
 vaapi_hevc_submit_ops_t native;
 unsigned i,j,element;
 bool found=false;
 if(!t || t->active || !s || !s->ready || s->closing || !ops || !ops->create
       || !ops->destroy || !ops->begin || !ops->render || !ops->end || !ops->sync
       || !s->display || s->context==VA_INVALID_ID || !s->slots || s->slots>32
       || !picture || !slices || !count || count>256 || !data || !size
       || (s->slice_mode!=VA_DEC_SLICE_MODE_BASE && s->slice_mode!=VA_DEC_SLICE_MODE_NORMAL))return false;
 for(i=0;i<s->slots;i++)if(s->surfaces[i]==picture->CurrPic.picture_id)found=true;
 if(!found || picture->CurrPic.picture_id==VA_INVALID_SURFACE)return false;
 if(picture->pic_width_in_luma_samples!=s->width
       || picture->pic_height_in_luma_samples!=s->height
       || picture->bit_depth_luma_minus8+8u!=s->depth
       || picture->bit_depth_chroma_minus8+8u!=s->depth
       || !!matrix!=!!picture->pic_fields.bits.scaling_list_enabled_flag)return false;
 element=s->slice_mode==VA_DEC_SLICE_MODE_BASE?sizeof(VASliceParameterBufferBase):sizeof(VASliceParameterBufferHEVC);
 for(i=0;i<count;i++)
 {
  VASliceParameterBufferBase base;
  memcpy(&base,(const uint8_t*)slices+i*element,sizeof(base));
  if(!base.slice_data_size || base.slice_data_offset>size
        || base.slice_data_size>size-base.slice_data_offset
        || base.slice_data_flag!=VA_SLICE_DATA_FLAG_ALL)return false;
 }
 native=*ops;memset(t,0,sizeof(*t));t->ops=native;t->display=s->display;
 t->surface=picture->CurrPic.picture_id;t->active=true;
 for(i=0;i<4;i++)t->buffers[i]=VA_INVALID_ID;
#define VAAPI_HEVC_BUFFER(type,bytes,elements,pointer) do { \
 t->error=t->ops.create(t->display,s->context,type,bytes,elements,(void*)(pointer),&t->buffers[t->count]); \
 t->count++; \
 for(j=0;j+1<t->count;j++)if(t->buffers[j]==t->buffers[t->count-1]) { \
 t->buffers[t->count-1]=VA_INVALID_ID;t->error=VA_STATUS_ERROR_OPERATION_FAILED;return false; } \
 if(t->error!=VA_STATUS_SUCCESS)return false; \
 if(t->buffers[t->count-1]==VA_INVALID_ID){t->error=VA_STATUS_ERROR_OPERATION_FAILED;return false;} } while(0)
 VAAPI_HEVC_BUFFER(VAPictureParameterBufferType,sizeof(*picture),1,picture);
 if(matrix) { VAAPI_HEVC_BUFFER(VAIQMatrixBufferType,sizeof(*matrix),1,matrix); }
 VAAPI_HEVC_BUFFER(VASliceParameterBufferType,element,count,slices);
 VAAPI_HEVC_BUFFER(VASliceDataBufferType,size,1,data);
#undef VAAPI_HEVC_BUFFER
 t->poisoned=true;
 t->error=t->ops.begin(t->display,s->context,t->surface);
 if(t->error!=VA_STATUS_SUCCESS)return false;
 t->error=t->ops.render(t->display,s->context,t->buffers,(int)t->count);
 if(t->error!=VA_STATUS_SUCCESS)return false;
 t->error=t->ops.end(t->display,s->context);
 if(t->error!=VA_STATUS_SUCCESS)return false;
 t->submitted=true;t->poisoned=false;return true;
}
int vaapi_hevc_submit_poll(vaapi_hevc_submit_t *t)
{
 if(!t || !t->active || !t->submitted || t->poisoned)return -1;
 if(t->complete)return t->completion==VA_STATUS_SUCCESS?0:-1;
 t->error=t->ops.sync(t->display,t->surface,0);
 if(t->error==VA_STATUS_ERROR_TIMEDOUT)return 1;
 if(t->error==VA_STATUS_SUCCESS || t->error==VA_STATUS_ERROR_DECODING_ERROR)
 {t->complete=true;t->completion=t->error;return t->completion==VA_STATUS_SUCCESS?0:-1;}
 t->poisoned=true;return -1;
}
bool vaapi_hevc_submit_retire(vaapi_hevc_submit_t *t)
{
 unsigned i;
 bool failed=false;
 if(!t || !t->active)return true;
 if(t->poisoned || (t->submitted && !t->complete))return false;
 for(i=0;i<t->count;i++)if(t->buffers[i]!=VA_INVALID_ID)
 {
  VAStatus status=t->ops.destroy(t->display,t->buffers[i]);
  if(status==VA_STATUS_SUCCESS)t->buffers[i]=VA_INVALID_ID;
  else {t->error=status;failed=true;}
 }
 if(failed)return false;
 memset(t,0,sizeof(*t));return true;
}
bool vaapi_hevc_submit_terminated(vaapi_hevc_submit_t *t,VADisplay display)
{
 if(!t || !t->active || !display || t->display!=display)return false;
 memset(t,0,sizeof(*t));return true;
}
