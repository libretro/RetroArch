/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "mediacodec_color.h"
#include <string.h>
static bool merge(unsigned *coded,unsigned native)
{
 if(!native)return true;
 if(*coded && *coded!=2 && *coded!=native)return false;
 *coded=native;return true;
}
bool mediacodec_color_resolve(const mediacodec_output_format_t *native,
      const video_color_metadata_t *coded,video_color_metadata_t *out)
{
 video_color_metadata_t color;
 unsigned primaries=0,matrix=0,transfer=0,range=0;
 if(!out)return false;
 memset(&color,0,sizeof(color));if(coded)color=*coded;
 memset(out,0,sizeof(*out));
 if(!native || color.range>2)return false;
 switch(native->color_standard) {
  case 0:break;
  case 1:primaries=matrix=1;break;
  case 2:primaries=5;matrix=5;break;
  case 4:primaries=matrix=6;break;
  case 6:primaries=matrix=9;break;
  default:return false;
 }
 if((native->color_standard==2 || native->color_standard==4)
       && (color.matrix==5 || color.matrix==6))matrix=color.matrix;
 switch(native->color_transfer) {
  case 0:break;
  case 1:transfer=8;break;
  case 3:
   /* Android combines SDR-video transfer variants. Preserve the exact coded
    * curve when it belongs to that family instead of rewriting it as 170M. */
   if(color.transfer==1 || color.transfer==6 || color.transfer==14 || color.transfer==15)
    transfer=color.transfer;
   else transfer=6;
   break;
  case 6:transfer=16;break;
  case 7:transfer=18;break;
  default:return false;
 }
 switch(native->color_range) {
  case 0:break;
  case 1:range=2;break;
  case 2:range=1;break;
  default:return false;
 }
 if(!merge(&color.primaries,primaries) || !merge(&color.matrix,matrix)
       || !merge(&color.transfer,transfer))return false;
 /* Range uses 2 for full, not the CICP unspecified code point. */
 if(range && color.range && range!=color.range)return false;
 if(range)color.range=range;
 *out=color;return true;
}
