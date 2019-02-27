/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VC_VCHI_DISPMANX_H
#define VC_VCHI_DISPMANX_H

#include "interface/peer/vc_vchi_dispmanx_common.h"

#define VC_NUM_HOST_RESOURCES 64
#define DISPMANX_MSGFIFO_SIZE 1024
#define DISPMANX_CLIENT_NAME MAKE_FOURCC("DISP")
#define DISPMANX_NOTIFY_NAME MAKE_FOURCC("UPDH")

//Or with command to indicate we don't need a response
#define DISPMANX_NO_REPLY_MASK (1<<31)

typedef struct {
   char     description[32];
   uint32_t width;
   uint32_t height;
   uint32_t aspect_pixwidth;
   uint32_t aspect_pixheight;
   uint32_t fieldrate_num;
   uint32_t fieldrate_denom;
   uint32_t fields_per_frame;
   uint32_t transform;
} GET_MODES_DATA_T;

typedef struct {
   int32_t  response;
   uint32_t width;
   uint32_t height;
   uint32_t transform;
   uint32_t input_format;
} GET_INFO_DATA_T;

//Attributes changes flag mask
#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)

#endif
