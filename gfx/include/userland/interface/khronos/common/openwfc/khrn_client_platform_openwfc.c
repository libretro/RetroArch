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

//==============================================================================

#include "interface/khronos/common/khrn_client_platform.h"

#include "interface/khronos/include/WF/wfc.h"
#include "interface/khronos/wf/wfc_int.h"

//==============================================================================

typedef struct
{
   WFCDevice device;
   WFCContext context;
   WFCElement element;
} WFC_DATA_T;

#define OPENWFC_BOUNCE

//==============================================================================

static EGL_DISPMANX_WINDOW_T *check_default(EGLNativeWindowType win);

//==============================================================================

static bool have_default_dwin;
static EGL_DISPMANX_WINDOW_T default_dwin;

static WFC_DATA_T wfc_data;

//==============================================================================

uint32_t platform_get_handle(EGLDisplay dpy, EGLNativeWindowType win)
{
   EGL_DISPMANX_WINDOW_T *dwin = check_default(win);
   return dwin->element;  /* same handles used on host and videocore sides */
}

//------------------------------------------------------------------------------

void platform_get_dimensions(EGLDisplay dpy, EGLNativeWindowType win,
      uint32_t *width, uint32_t *height, uint32_t *swapchain_count)
{
   EGL_DISPMANX_WINDOW_T *dwin = check_default(win);

   *width =  dwin->width;
   *height = dwin->height;
   *swapchain_count = 0;
}

//------------------------------------------------------------------------------

#define NUM_OF_ELEMENTS    2

void *platform_wfc_bounce_thread(void *param)
// Thread function for making previously-created source bounce around the screen.
{
   WFC_BOUNCE_DATA_T *bounce_data = (WFC_BOUNCE_DATA_T *) param;

   uint32_t i;
   int32_t xstep[NUM_OF_ELEMENTS], ystep[NUM_OF_ELEMENTS];
   WFCint dest_rect[NUM_OF_ELEMENTS][WFC_RECT_SIZE];
   WFCint src_rect[WFC_RECT_SIZE];
   WFCElement element_local[NUM_OF_ELEMENTS];

   uint32_t num_of_elements = NUM_OF_ELEMENTS;
   WFCElement *element = element_local;

   // Get context (i.e. screen) dimensions.
   int32_t ctx_width;
   int32_t ctx_height;
   int32_t dest_width, dest_height;

   int32_t x, y;

   bool use_local_elements = (bounce_data->num_of_elements == 0);
   if(!use_local_elements)
   {
      vcos_assert(bounce_data->num_of_elements <= NUM_OF_ELEMENTS);
      vcos_assert(bounce_data->element != NULL);
      num_of_elements = bounce_data->num_of_elements;
      element = bounce_data->element;
   } // if

   // Initialise values
   ctx_width = wfcGetContextAttribi(bounce_data->device,
      bounce_data->context, WFC_CONTEXT_TARGET_WIDTH);
   ctx_height = wfcGetContextAttribi(bounce_data->device,
      bounce_data->context, WFC_CONTEXT_TARGET_HEIGHT);

   // Change background colour
   wfcSetContextAttribi(bounce_data->device,
      bounce_data->context, WFC_CONTEXT_BG_COLOR, 0x0000FFFF);

   float scale = 0.4;
   if(num_of_elements == 1)
      {scale = 0.75;}

   dest_width = bounce_data->dest_width * scale;
   dest_height = bounce_data->dest_height * scale;

   // Define source rectangle
   src_rect[WFC_RECT_X] = bounce_data->src_x;
   src_rect[WFC_RECT_Y] = bounce_data->src_y;
   src_rect[WFC_RECT_WIDTH] = bounce_data->src_width;
   src_rect[WFC_RECT_HEIGHT] = bounce_data->src_height;

   for(i = 0; i < num_of_elements; i++)
   {
      if(use_local_elements)
      {
         // Create and set up element
         element[i] = wfcCreateElement(bounce_data->device,
            bounce_data->context, NO_ATTRIBUTES);
         vcos_assert(element[i] != WFC_INVALID_HANDLE);

         wfcInsertElement(bounce_data->device, element[i], WFC_INVALID_HANDLE);
         if(vcos_verify(wfcGetError(bounce_data->device) == WFC_ERROR_NONE)) {};
      } // if
      else
      {
         // Element created in calling app, so use that
         element[i] = bounce_data->element[i];
      } // else

      wfcSetElementAttribiv(bounce_data->device, element[i],
         WFC_ELEMENT_SOURCE_RECTANGLE, WFC_RECT_SIZE, src_rect);

      // Attach existing source
      wfcSetElementAttribi(bounce_data->device, element[i],
         WFC_ELEMENT_SOURCE, bounce_data->source);

      if(num_of_elements > 1)
      {
         wfcSetElementAttribi(bounce_data->device, element[i],
            WFC_ELEMENT_TRANSPARENCY_TYPES, WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA);
         wfcSetElementAttribf(bounce_data->device, element[i],
            WFC_ELEMENT_GLOBAL_ALPHA, 0.75);
      } // if

      // Define initial destination rectangle
      dest_rect[i][WFC_RECT_X] = i * 100;
      dest_rect[i][WFC_RECT_Y] = i * 10;
      dest_rect[i][WFC_RECT_WIDTH] = dest_width;
      dest_rect[i][WFC_RECT_HEIGHT] = dest_height;
      wfcSetElementAttribiv(bounce_data->device, element[i],
         WFC_ELEMENT_DESTINATION_RECTANGLE, WFC_RECT_SIZE, dest_rect[i]);

      xstep[i] = (i + 1) * 2;
      ystep[i] = (i + 1) * 2;
   } // for

   while(!bounce_data->stop_bouncing)
   {
      for(i = 0; i < num_of_elements; i++)
      {
         // Compute new x and y values.
         x = dest_rect[i][WFC_RECT_X];
         y = dest_rect[i][WFC_RECT_Y];

         x += xstep[i];
         if(x + dest_width >= ctx_width)
            {x = ctx_width - dest_width - 1; xstep[i] *= -1;}
         else if(x < 0)
            {x = 0; xstep[i] *= -1;}

         y += ystep[i];
         if(y + dest_height >= ctx_height)
            {y = ctx_height - dest_height - 1; ystep[i] *= -1;}
         else if(y < 0)
            {y = 0; ystep[i] *= -1;}

         dest_rect[i][WFC_RECT_X] = x;
         dest_rect[i][WFC_RECT_Y] = y;

         // Set updated destination rectangle
         wfcSetElementAttribiv(bounce_data->device, element[i],
            WFC_ELEMENT_DESTINATION_RECTANGLE, WFC_RECT_SIZE, dest_rect[i]);
      } // for

      wfcCommit(bounce_data->device, bounce_data->context, WFC_TRUE);
      vcos_sleep(30);
   } // while

   // Remove elements
   if(use_local_elements)
   {
      for(i = 0; i < num_of_elements; i++)
      {
         wfcDestroyElement(bounce_data->device, element[i]);
      } // for
   } // if

   // Change background colour
   wfcSetContextAttribi(bounce_data->device,
      bounce_data->context, WFC_CONTEXT_BG_COLOR, 0xFF0000FF);

   wfcCommit(bounce_data->device, bounce_data->context, WFC_TRUE);

   return NULL;
} // platform_bounce_thread

//==============================================================================

static EGL_DISPMANX_WINDOW_T *check_default(EGLNativeWindowType win)
{
   // Window already exists, so return it.
   if(win != 0)
      {return (EGL_DISPMANX_WINDOW_T*) win;}

   // Window doesn't exist (= 0), but one has previously been created, so use
   // that.
   if(have_default_dwin)
      {return &default_dwin;}

   // Window doesn't exist, and hasn't previously been created, so make it so.
   wfc_data.device = wfcCreateDevice(WFC_DEFAULT_DEVICE_ID, NO_ATTRIBUTES);
   vcos_assert(wfc_data.device != WFC_INVALID_HANDLE);
   wfc_data.context = wfcCreateOnScreenContext(wfc_data.device, 0, NO_ATTRIBUTES);
   vcos_assert(wfc_data.context != WFC_INVALID_HANDLE);

   WFCint context_width = wfcGetContextAttribi
      (wfc_data.device, wfc_data.context, WFC_CONTEXT_TARGET_WIDTH);
   WFCint context_height = wfcGetContextAttribi
      (wfc_data.device, wfc_data.context, WFC_CONTEXT_TARGET_HEIGHT);

#ifdef OPENWFC_BOUNCE
   // Create and attach source
   default_dwin.element = 1; // Use arbitrary non-zero value for stream number.
   WFCNativeStreamType stream = (WFCNativeStreamType) default_dwin.element;
   vcos_assert(stream != WFC_INVALID_HANDLE);
   WFCSource source = wfcCreateSourceFromStream(wfc_data.device, wfc_data.context, stream, NO_ATTRIBUTES);
   vcos_assert(source != WFC_INVALID_HANDLE);

   static VCOS_THREAD_T bounce_thread_data;
   static WFC_BOUNCE_DATA_T bounce_data;

   bounce_data.device = wfc_data.device;
   bounce_data.context = wfc_data.context;
   bounce_data.source = source;
   bounce_data.src_width = context_width;
   bounce_data.src_height = context_height;
   bounce_data.dest_width = context_width;
   bounce_data.dest_height = context_height;
   bounce_data.stop_bouncing = 0;

   VCOS_STATUS_T status;
   status = vcos_thread_create(&bounce_thread_data, "bounce_thread", NULL,
      platform_wfc_bounce_thread, &bounce_data);
   vcos_assert(status == VCOS_SUCCESS);
#else
   WFCint rect_src[WFC_RECT_SIZE] = { 0, 0, 0, 0 };
   WFCint rect_dest[WFC_RECT_SIZE] = { 0, 0, 0, 0 };

   wfc_data.element = wfcCreateElement(wfc_data.device, wfc_data.context, NO_ATTRIBUTES);
   vcos_assert(wfc_data.element != WFC_INVALID_HANDLE);
   default_dwin.element = wfc_data.element;

   wfcInsertElement(wfc_data.device, wfc_data.element, WFC_INVALID_HANDLE);
   if(vcos_verify(wfcGetError(wfc_data.device) == WFC_ERROR_NONE)) {};

   /* Set element attributes */
   rect_src[WFC_RECT_X] = 0;
   rect_src[WFC_RECT_Y] = 0;
   rect_src[WFC_RECT_WIDTH] = context_width;
   rect_src[WFC_RECT_HEIGHT] = context_height;
   wfcSetElementAttribiv(wfc_data.device, wfc_data.element,
      WFC_ELEMENT_SOURCE_RECTANGLE, WFC_RECT_SIZE, rect_src);

   rect_dest[WFC_RECT_X] = 0;
   rect_dest[WFC_RECT_Y] = 0;
   rect_dest[WFC_RECT_WIDTH] = context_width;
   rect_dest[WFC_RECT_HEIGHT] = context_height;
   wfcSetElementAttribiv(wfc_data.device, wfc_data.element,
      WFC_ELEMENT_DESTINATION_RECTANGLE, WFC_RECT_SIZE, rect_dest);

   // Create and attach source
   default_dwin.element = wfc_data.element; // Stream and element handles given same value.
   WFCNativeStreamType stream = (WFCNativeStreamType) default_dwin.element;
   vcos_assert(stream != WFC_INVALID_HANDLE);
   WFCSource source = wfcCreateSourceFromStream(wfc_data.device, wfc_data.context, stream, NO_ATTRIBUTES);
   vcos_assert(source != WFC_INVALID_HANDLE);

   wfcSetElementAttribi(wfc_data.device, wfc_data.element, WFC_ELEMENT_SOURCE, source);
#endif

   // Send to display
   wfcCommit(wfc_data.device, wfc_data.context, WFC_TRUE);

   // Enable this and future commits to be enacted immediately
   wfcActivate(wfc_data.device, wfc_data.context);

   default_dwin.width = 800;
   default_dwin.height = 480;

   have_default_dwin = true;

   return &default_dwin;

} // check_default()

//==============================================================================
