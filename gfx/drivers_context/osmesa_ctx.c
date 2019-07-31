/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Higor Euripedes
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include <compat/strl.h>

#include <GL/osmesa.h>

#include "../../configuration.h"
#include "../../verbosity.h"
#include "../common/gl_common.h"

#if (OSMESA_MAJOR_VERSION * 1000 + OSMESA_MINOR_VERSION) >= 11002
#define HAVE_OSMESA_CREATE_CONTEXT_ATTRIBS 1
#endif

#if (OSMESA_MAJOR_VERSION * 1000 + OSMESA_MINOR_VERSION) >= 3005
#define HAVE_OSMESA_CREATE_CONTEXT_EXT 1
#endif

static bool           g_osmesa_profile = OSMESA_COMPAT_PROFILE;
static int            g_osmesa_major   = 2;
static int            g_osmesa_minor   = 1;
static int            g_osmesa_format  = OSMESA_RGBA;
static int            g_osmesa_bpp     = 4;
static const char    *g_osmesa_fifo    = "/tmp/osmesa-retroarch.sock";

static enum gfx_ctx_api osmesa_api     = GFX_CTX_NONE;

typedef struct gfx_osmesa_ctx_data
{
   uint8_t *screen;
   int  width;
   int  height;
   int  pixsize;

   OSMesaContext ctx;
   int socket;
   int client;
} gfx_ctx_osmesa_data_t;

static void osmesa_fifo_open(gfx_ctx_osmesa_data_t *osmesa)
{
   struct sockaddr_un saun, fsaun;

   osmesa->socket = socket(AF_UNIX, SOCK_STREAM, 0);
   osmesa->client = -1;

   if (osmesa->socket < 0)
   {
      perror("[osmesa] socket()");
      return;
   }

   saun.sun_family = AF_UNIX;

   strlcpy(saun.sun_path, g_osmesa_fifo, sizeof(saun.sun_path));

   unlink(g_osmesa_fifo);

   if (bind(osmesa->socket, &saun, sizeof(saun.sun_family) + sizeof(saun.sun_path)) < 0)
   {
      perror("[osmesa] bind()");
      close(osmesa->socket);
      return;
   }

   if (listen(osmesa->socket, 1) < 0)
   {
      perror("[osmesa] listen()");
      close(osmesa->socket);
      return;
   }

   fprintf(stderr, "[osmesa] Frame size is %ix%ix%i\n", osmesa->width, osmesa->height, osmesa->pixsize);
   fprintf(stderr, "[osmesa] Please connect to unix:%s\n", g_osmesa_fifo);
}

static void osmesa_fifo_accept(gfx_ctx_osmesa_data_t *osmesa)
{
   int res;
   struct pollfd fds;
   fds.fd = osmesa->socket;
   fds.events = POLLIN;

   if (osmesa->client >= 0)
      return;

   res = poll(&fds, 1, 0);

   if (res < 0)
      perror("[osmesa] poll() error");
   else if (res > 0)
   {
      osmesa->client = accept(osmesa->socket, NULL, NULL);
      fprintf(stderr, "[osmesa] Client %i connected.\n", osmesa->client);
   }
}

static void osmesa_fifo_write(gfx_ctx_osmesa_data_t *osmesa)
{
   int i;
   size_t len = osmesa->width * osmesa->pixsize;

   if (osmesa->client < 0)
      return;

   for (i = osmesa->height -1; i >= 0; --i)
   {
      int res = send(osmesa->client, osmesa->screen + i * len, len, MSG_NOSIGNAL);

      if (res < 0)
      {
         fprintf(stderr, "[osmesa] Lost connection to %i: %s\n", osmesa->client, strerror(errno));
         close(osmesa->client);
         osmesa->client = -1;
         break;
      }
   }
}

static void *osmesa_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
#ifdef HAVE_OSMESA_CREATE_CONTEXT_ATTRIBS
   const int attribs[] = {
      OSMESA_FORMAT, g_osmesa_format,
      OSMESA_DEPTH_BITS, 0,
      OSMESA_STENCIL_BITS, 0,
      OSMESA_ACCUM_BITS, 0,
      OSMESA_PROFILE, g_osmesa_profile,
      OSMESA_CONTEXT_MAJOR_VERSION, g_osmesa_major,
      OSMESA_CONTEXT_MINOR_VERSION, g_osmesa_minor,
      0, 0
   };
#endif
   gfx_ctx_osmesa_data_t *osmesa = (gfx_ctx_osmesa_data_t*)
      calloc(1, sizeof(gfx_ctx_osmesa_data_t));

   if (!osmesa)
      goto error;

#ifdef HAVE_OSMESA_CREATE_CONTEXT_ATTRIBS
   osmesa->ctx = OSMesaCreateContextAttribs(attribs, NULL);
#endif

#ifdef HAVE_OSMESA_CREATE_CONTEXT_EXT
   if (!osmesa->ctx)
      osmesa->ctx = OSMesaCreateContextExt(g_osmesa_format, 0, 0, 0, NULL);
#endif

   if (!osmesa->ctx)
   {
#if defined(HAVE_OSMESA_CREATE_CONTEXT_ATTRIBS) || defined(HAVE_OSMESA_CREATE_CONTEXT_EXT)
      RARCH_WARN("[osmesa]: Falling back to standard context creation.\n");
#endif
      osmesa->ctx = OSMesaCreateContext(g_osmesa_format, NULL);
   }

   if (!osmesa->ctx)
      goto error;

   osmesa->pixsize = g_osmesa_bpp;

   return osmesa;

error:
   if (osmesa)
      free(osmesa);
   RARCH_WARN("[omesa]: Failed to initialize the context driver.\n");
   return NULL;
}

static void osmesa_ctx_destroy(void *data)
{
   gfx_ctx_osmesa_data_t *osmesa = (gfx_ctx_osmesa_data_t*)data;

   if (!osmesa)
      return;

   if (osmesa->socket)
      close(osmesa->socket);

   unlink("/tmp/retroarch-osmesa.fifo");

   free(osmesa->screen);
   OSMesaDestroyContext(osmesa->ctx);

   free(osmesa);
}

static enum gfx_ctx_api osmesa_ctx_get_api(void *data)
{
   return osmesa_api;
}

static bool osmesa_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor)
{
   if (api != GFX_CTX_OPENGL_API)
      return false;

   osmesa_api       = api;
   g_osmesa_profile = OSMESA_COMPAT_PROFILE;

   if (major)
   {
      g_osmesa_major = major;
      g_osmesa_minor = minor;
   }
   else
   {
      g_osmesa_major = 2;
      g_osmesa_minor = 1;
   }

   return true;
}

static void osmesa_ctx_swap_interval(void *data, int interval)
{
   (void)data;
   (void)interval;
}

static bool osmesa_ctx_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_osmesa_data_t *osmesa = (gfx_ctx_osmesa_data_t*)data;
   uint8_t               *screen = osmesa->screen;
   bool             size_changed = (width * height) != (osmesa->width * osmesa->height);

   if (!osmesa->screen || size_changed)
      screen = (uint8_t*)calloc(1, (width * height) * osmesa->pixsize);

   if (!screen)
      return false;

   if (!OSMesaMakeCurrent(osmesa->ctx, screen, GL_UNSIGNED_BYTE, width, height))
   {
      if (screen != osmesa->screen)
         free(screen);

      return false;
   }

   osmesa->width  = width;
   osmesa->height = height;

   if (osmesa->screen && osmesa->screen != screen)
      free(osmesa->screen);

   osmesa->screen = screen;

   if (!osmesa->socket)
   {
#if 0
      unlink(g_osmesa_fifo);
      if (mkfifo(g_osmesa_fifo, 0666) == 0)
      {
         RARCH_WARN("[osmesa]: Please connect the sink to the fifo...\n");
         RARCH_WARN("[osmesa]: Picture size is %ux%u\n", width, height);
         osmesa->socket = open(g_osmesa_fifo, O_WRONLY);

         if (osmesa->socket)
            RARCH_WARN("[osmesa]: Initialized fifo at %s\n", g_osmesa_fifo);
      }

      if (!osmesa->socket || osmesa->socket < 0)
      {
         unlink(g_osmesa_fifo);
         RARCH_WARN("[osmesa]: Failed to initialize fifo: %s\n", strerror(errno));
      }
#endif
      osmesa_fifo_open(osmesa);
   }

   return true;
}

static void osmesa_ctx_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_osmesa_data_t *osmesa = (gfx_ctx_osmesa_data_t*)data;

   if (!osmesa)
      return;

   *width  = osmesa->width;
   *height = osmesa->height;
}

static void osmesa_ctx_check_window(void *data, bool *quit,
      bool *resize,unsigned *width,
      unsigned *height, bool is_shutdown)
{
   gfx_ctx_osmesa_data_t *osmesa = (gfx_ctx_osmesa_data_t*)data;

   *width              = osmesa->width;
   *height             = osmesa->height;
   *resize             = false;
   *quit               = false;
}

static bool osmesa_ctx_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool osmesa_ctx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void osmesa_ctx_swap_buffers(void *data, void *data2)
{
   gfx_ctx_osmesa_data_t *osmesa = (gfx_ctx_osmesa_data_t*)data;
   osmesa_fifo_accept(osmesa);
   osmesa_fifo_write(osmesa);

#if 0
   write(osmesa->socket, osmesa->screen, osmesa->width * osmesa->height * osmesa->pixsize);
#endif
}

static void osmesa_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static gfx_ctx_proc_t osmesa_ctx_get_proc_address(const char *name)
{
   return (gfx_ctx_proc_t)OSMesaGetProcAddress(name);
}

static void osmesa_ctx_show_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static uint32_t osmesa_ctx_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void osmesa_ctx_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_osmesa =
{
   osmesa_ctx_init,
   osmesa_ctx_destroy,
   osmesa_ctx_get_api,
   osmesa_ctx_bind_api,
   osmesa_ctx_swap_interval,
   osmesa_ctx_set_video_mode,
   osmesa_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL, /* translate_aspect */
   NULL, /* update_title */
   osmesa_ctx_check_window,
   NULL, /* set_resize */
   osmesa_ctx_has_focus,
   osmesa_ctx_suppress_screensaver,
   true, /* has_windowed */
   osmesa_ctx_swap_buffers,
   osmesa_ctx_input_driver,
   osmesa_ctx_get_proc_address,
   NULL,
   NULL,
   osmesa_ctx_show_mouse,
   "osmesa",
   osmesa_ctx_get_flags,
   osmesa_ctx_set_flags,
   NULL, /* bind_hw_render */
   NULL,
   NULL
};
