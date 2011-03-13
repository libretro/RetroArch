/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "driver.h"
#include "general.h"
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

typedef struct xv
{
   Display *display;
   GC gc;
   Window window;
   Colormap colormap;
   XShmSegmentInfo shminfo;

   int port;
   int depth;
   int visualid;

   XvImage *image;
   uint32_t fourcc;

   unsigned width;
   unsigned height;
   bool keep_aspect;

   uint8_t *ytable;
   uint8_t *utable;
   uint8_t *vtable;
} xv_t;

static void init_yuv_tables(xv_t *xv)
{
   xv->ytable = malloc(0x10000);
   xv->utable = malloc(0x10000);
   xv->vtable = malloc(0x10000);

   for (unsigned i = 0; i < 0x10000; i++) 
   {
      // Extract RGB555 color data from i
      uint8_t r = (i >> 10) & 31, g = (i >> 5) & 31, b = (i) & 31;
      r = (r << 3) | (r >> 2);  //R5->R8
      g = (g << 3) | (g >> 2);  //G5->G8
      b = (b << 3) | (b >> 2);  //B5->B8

      int y = (int)( +((double)r * 0.257) + ((double)g * 0.504) + ((double)b * 0.098) +  16.0 );
      int u = (int)( -((double)r * 0.148) - ((double)g * 0.291) + ((double)b * 0.439) + 128.0 );
      int v = (int)( +((double)r * 0.439) - ((double)g * 0.368) - ((double)b * 0.071) + 128.0 );

      xv->ytable[i] = y < 0 ? 0 : y > 255 ? 255 : y;
      xv->utable[i] = u < 0 ? 0 : u > 255 ? 255 : u;
      xv->vtable[i] = v < 0 ? 0 : v > 255 ? 255 : v;
   }
}

static void render_yuy2(xv_t *xv, const uint16_t *input, unsigned width, unsigned height, unsigned pitch) 
{
   uint16_t *output = (uint16_t*)xv->image->data;

   for (unsigned y = 0; y < height; y++) 
   {
      for (unsigned x = 0; x < width >> 1; x++) 
      {
         uint16_t p0 = *input++;
         uint16_t p1 = *input++;

         uint8_t u = (xv->utable[p0] + xv->utable[p1]) >> 1;
         uint8_t v = (xv->vtable[p0] + xv->vtable[p1]) >> 1;

         *output++ = (u << 8) | xv->ytable[p0];
         *output++ = (v << 8) | xv->ytable[p1];
      }

      input  += (pitch >> 1) - width;
      output += xv->width - width;
   }
}

static void* xv_init(video_info_t *video, const input_driver_t **input, void **input_data)
{
   xv_t *xv = calloc(1, sizeof(*xv));
   if (!xv)
      return NULL;

   xv->display = XOpenDisplay(NULL);

   if (!XShmQueryExtension(xv->display))
   {
      SSNES_ERR("XVideo: XShm extension not found.\n");
      goto error;
   }

   xv->keep_aspect = video->force_aspect;

   // Find an appropriate Xv port.
   xv->port = -1;
   XvAdaptorInfo *adaptor_info;
   unsigned adaptor_count = 0;
   XvQueryAdaptors(xv->display, DefaultRootWindow(xv->display), &adaptor_count, &adaptor_info);
   for (unsigned i = 0; i < adaptor_count; i++) 
   {
      // Find adaptor that supports both input (memory->drawable) and image (drawable->screen) masks.
      if (adaptor_info[i].num_formats < 1) continue;
      if (!(adaptor_info[i].type & XvInputMask)) continue;
      if (!(adaptor_info[i].type & XvImageMask)) continue;

      xv->port     = adaptor_info[i].base_id;
      xv->depth    = adaptor_info[i].formats->depth;
      xv->visualid = adaptor_info[i].formats->visual_id;
      break;
   }
   XvFreeAdaptorInfo(adaptor_info);

   if (xv->port < 0) 
   {
      SSNES_ERR("XVideo: Failed to find valid XvPort.\n");
      goto error;
   }

   XVisualInfo visualtemplate;
   visualtemplate.visualid = xv->visualid;
   visualtemplate.screen   = DefaultScreen(xv->display);
   visualtemplate.depth    = xv->depth;
   visualtemplate.visual   = 0;
   int visualmatches       = 0;
   XVisualInfo *visualinfo = XGetVisualInfo(xv->display, VisualIDMask | VisualScreenMask | VisualDepthMask, &visualtemplate, &visualmatches);
   if (visualmatches < 1 || !visualinfo->visual) 
   {
      if (visualinfo) XFree(visualinfo);
      SSNES_ERR("XVideo: Unable to find Xv-compatible visual.\n");
      goto error;
   }

   xv->colormap = XCreateColormap(xv->display, DefaultRootWindow(xv->display), visualinfo->visual, AllocNone);
   XSetWindowAttributes attributes;
   attributes.colormap = xv->colormap;
   attributes.border_pixel = 0;
   attributes.event_mask = StructureNotifyMask;

   xv->window = XCreateWindow(xv->display, DefaultRootWindow(xv->display),
         0, 0, video->width, video->height,
         0, xv->depth, InputOutput, visualinfo->visual,
         CWColormap | CWBorderPixel | CWEventMask, &attributes);

   XFree(visualinfo);
   XSetWindowBackground(xv->display, xv->window, 0);
   XMapWindow(xv->display, xv->window);

   xv->gc = XCreateGC(xv->display, xv->window, 0, 0);

   // Set colorkey to auto paint, so that Xv video output is always visible
   Atom atom = XInternAtom(xv->display, "XV_AUTOPAINT_COLORKEY", true);
   if(atom != None) XvSetPortAttribute(xv->display, xv->port, atom, 1);

   int format_count;
   XvImageFormatValues *format = XvListImageFormats(xv->display, xv->port, &format_count);

   bool has_format = false;
   for (int i = 0; i < format_count; i++) 
   {
      if (format[i].type == XvYUV && format[i].bits_per_pixel == 16 && format[i].format == XvPacked) 
      {
         if (format[i].component_order[0] == 'Y' && format[i].component_order[1] == 'U'
               && format[i].component_order[2] == 'Y' && format[i].component_order[3] == 'V') 
         {
            has_format = true;
            xv->fourcc = format[i].id;
            break;
         }
      }
   }

   free(format);
   if (!has_format) 
   {
      SSNES_ERR("XVideo: unable to find a supported image format.\n");
      goto error;
   }

   xv->width = 256;
   xv->height = 256;

   xv->image = XvShmCreateImage(xv->display, xv->port, xv->fourcc, NULL, xv->width, xv->height, &xv->shminfo);
   if (!xv->image) 
   {
      SSNES_ERR("XVideo: XShmCreateImage failed.\n");
      goto error;
   }
   xv->width = xv->image->width;
   xv->height = xv->image->height;

   xv->shminfo.shmid = shmget(IPC_PRIVATE, xv->image->data_size, IPC_CREAT | 0777);
   xv->shminfo.shmaddr = xv->image->data = shmat(xv->shminfo.shmid, NULL, 0);
   xv->shminfo.readOnly = false;
   if (!XShmAttach(xv->display, &xv->shminfo)) 
   {
      SSNES_ERR("XVideo: XShmAttach failed.\n");
      goto error;
   }
   XSync(xv->display, False);
   memset(xv->image->data, 128, xv->image->data_size);

   void *xinput = input_x.init();
   if (xinput)
   {
      *input = &input_x;
      *input_data = xinput;
   }
   else
      *input = NULL;

   init_yuv_tables(xv);
   return xv;

error:
   free(xv);
   return NULL;
}

static bool check_resize(xv_t *xv, unsigned width, unsigned height)
{
   if (xv->width != width || xv->height != height)
   {
      xv->width = width;
      xv->height = height;

      XShmDetach(xv->display, &xv->shminfo);
      shmdt(xv->shminfo.shmaddr);
      shmctl(xv->shminfo.shmid, IPC_RMID, NULL);
      XFree(xv->image);

      memset(&xv->shminfo, 0, sizeof(xv->shminfo));
      xv->image = XvShmCreateImage(xv->display, xv->port, xv->fourcc, NULL, xv->width, xv->height, &xv->shminfo);
      if (xv->image == None)
      {
         SSNES_ERR("Failed to create image.\n");
         return false;
      }

      xv->width = xv->image->width;
      xv->height = xv->image->height;

      xv->shminfo.shmid = shmget(IPC_PRIVATE, xv->image->data_size, IPC_CREAT | 0777);
      if (xv->shminfo.shmid < 0)
      {
         SSNES_ERR("Failed to init SHM.\n");
         return false;
      }

      xv->shminfo.shmaddr = xv->image->data = shmat(xv->shminfo.shmid, NULL, 0);
      xv->shminfo.readOnly = false;
      
      if (!XShmAttach(xv->display, &xv->shminfo))
      {
         SSNES_ERR("Failed to reattch XvShm image.\n");
         return false;
      }
      XSync(xv->display, False);
      memset(xv->image->data, 128, xv->image->data_size);
   }
   return true;
}

static void calc_out_rect(bool keep_aspect, unsigned *x, unsigned *y, unsigned *width, unsigned *height, unsigned vp_width, unsigned vp_height)
{
   if (!keep_aspect)
   {
      *x = 0; *y = 0; *width = vp_width; *height = vp_height;
   }
   else
   {
      float desired_aspect = g_settings.video.aspect_ratio;
      float device_aspect = (float)vp_width / vp_height;

      // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
      // assume they are actually equal.
      if ( (int)(device_aspect*1000) > (int)(desired_aspect*1000) )
      {
         float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
         *x = vp_width * (0.5 - delta); *y = 0; *width = 2.0 * vp_width * delta; *height = vp_height;
      }

      else if ( (int)(device_aspect*1000) < (int)(desired_aspect*1000) )
      {
         float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
         *x = 0; *y = vp_height * (0.5 - delta); *width = vp_width; *height = 2.0 * vp_height * delta;
      }
      else
      {
         *x = 0; *y = 0; *width = vp_width; *height = vp_height;
      }
   }
}

static bool xv_frame(void *data, const void* frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   (void)msg;
   xv_t *xv = data;

   if (!check_resize(xv, width, height))
      return false;

   XWindowAttributes target;
   XGetWindowAttributes(xv->display, xv->window, &target);
   render_yuy2(xv, frame, width, height, pitch);

   unsigned x, y, owidth, oheight;
   calc_out_rect(xv->keep_aspect, &x, &y, &owidth, &oheight, target.width, target.height);
   XvShmPutImage(xv->display, xv->port, xv->window, xv->gc, xv->image,
         0, 0, width, height,
         x, y, owidth, oheight,
         true);

   return true;
}

static bool xv_alive(void *data)
{
   return true;
}

static bool xv_focus(void *data)
{
   return true;
}

static void xv_set_nonblock_state(void *data, bool state)
{
   xv_t *xv = data;
   Atom atom = XInternAtom(xv->display, "XV_SYNC_TO_VBLANK", true);
   if (atom != None && xv->port >= 0)
      XvSetPortAttribute(xv->display, xv->port, atom, !state);
}

static void xv_free(void *data)
{
   xv_t *xv = data;
   XShmDetach(xv->display, &xv->shminfo);
   shmdt(xv->shminfo.shmaddr);
   shmctl(xv->shminfo.shmid, IPC_RMID, NULL);
   XFree(xv->image);

   if(xv->window) 
      XUnmapWindow(xv->display, xv->window);
   if(xv->colormap) 
      XFreeColormap(xv->display, xv->colormap);

   XCloseDisplay(xv->display);

   free(xv->ytable);
   free(xv->utable);
   free(xv->vtable);
   free(xv);
}

const video_driver_t video_xvideo = {
   .init = xv_init,
   .frame = xv_frame,
   .alive = xv_alive,
   .set_nonblock_state = xv_set_nonblock_state,
   .focus = xv_focus,
   .free = xv_free,
   .ident = "xvideo"
};

