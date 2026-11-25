/* The link harness's OpenGL consoles: one surfaceless EGL context and one
 * framebuffer for each. No GL header and no GL library: a console's
 * context may be OpenGL or OpenGL ES, and every entry point comes from
 * eglGetProcAddress. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <compat/strl.h>

#include "link_gl.h"

#define LINK_GL_DEVICES 16
#define LINK_GL_ERRORS  16
#define LINK_GL_PACK    4

#define GL_PACK_ROW_LENGTH              0x0D02
#define GL_PACK_SKIP_ROWS               0x0D03
#define GL_PACK_SKIP_PIXELS             0x0D04
#define GL_PACK_ALIGNMENT               0x0D05
#define GL_TEXTURE_2D                   0x0DE1
#define GL_UNSIGNED_BYTE                0x1401
#define GL_RGBA                         0x1908
#define GL_RENDERER                     0x1F01
#define GL_VERSION                      0x1F02
#define GL_RGBA8                        0x8058
#define GL_TEXTURE_BINDING_2D           0x8069
#define GL_DEPTH_COMPONENT16            0x81A5
#define GL_PIXEL_PACK_BUFFER            0x88EB
#define GL_PIXEL_UNPACK_BUFFER          0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING    0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING  0x88EF
#define GL_DEPTH24_STENCIL8             0x88F0
#define GL_FRAMEBUFFER_BINDING          0x8CA6
#define GL_RENDERBUFFER_BINDING         0x8CA7
#define GL_READ_FRAMEBUFFER             0x8CA8
#define GL_READ_FRAMEBUFFER_BINDING     0x8CAA
#define GL_FRAMEBUFFER_COMPLETE         0x8CD5
#define GL_COLOR_ATTACHMENT0            0x8CE0
#define GL_DEPTH_ATTACHMENT             0x8D00
#define GL_STENCIL_ATTACHMENT           0x8D20
#define GL_FRAMEBUFFER                  0x8D40
#define GL_RENDERBUFFER                 0x8D41

typedef void (*link_gl_entry_t)(void);
typedef const unsigned char *(*link_gl_get_string_t)(unsigned name);
typedef void (*link_gl_get_integer_t)(unsigned name, int *value);
typedef void (*link_gl_gen_t)(int count, unsigned *names);
typedef void (*link_gl_bind_t)(unsigned target, unsigned name);
typedef void (*link_gl_attach_texture_t)(unsigned target, unsigned attachment,
      unsigned texture_target, unsigned texture, int level);
typedef void (*link_gl_attach_renderbuffer_t)(unsigned target, unsigned attachment,
      unsigned renderbuffer_target, unsigned renderbuffer);
typedef unsigned (*link_gl_status_t)(unsigned target);
typedef void (*link_gl_tex_image_t)(unsigned target, int level, int internal_format,
      int width, int height, int border, unsigned format, unsigned type, const void *pixels);
typedef void (*link_gl_storage_t)(unsigned target, unsigned format, int width, int height);
typedef void (*link_gl_read_pixels_t)(int x, int y, int width, int height, unsigned format,
      unsigned type, void *pixels);
typedef void (*link_gl_pixel_store_t)(unsigned name, int value);
typedef unsigned (*link_gl_get_error_t)(void);

struct link_gl
{
   link_gl_get_string_t get_string;
   link_gl_get_integer_t get_integer;
   link_gl_gen_t gen_framebuffers, gen_textures, gen_renderbuffers;
   link_gl_bind_t bind_framebuffer, bind_texture, bind_renderbuffer, bind_buffer;
   link_gl_attach_texture_t attach_texture;
   link_gl_attach_renderbuffer_t attach_renderbuffer;
   link_gl_status_t status;
   link_gl_tex_image_t tex_image;
   link_gl_storage_t storage;
   link_gl_read_pixels_t read_pixels;
   link_gl_pixel_store_t pixel_store;
   link_gl_get_error_t get_error;
   EGLContext context;
   EGLenum api;
   const char *version, *renderer;
   unsigned index;
   unsigned framebuffer, texture, renderbuffer;
   unsigned width, height;          /* the framebuffer's */
   unsigned depth_format;           /* 0: no depth */
   unsigned packs;                  /* of link_gl_read's names, those it has */
   bool gl3;                        /* OpenGL or OpenGL ES 3.0 or later: a binding
                                       of its own for reading */
   bool pixel_buffers;              /* it has them */
   bool bottom_left;                /* the framebuffer's first row is the picture's last */
   bool unread;                     /* said once: a frame that could not be read */
};

static struct
{
   EGLDisplay display;              /* one for the process, while any console has a context */
   unsigned users;
   bool egl15;
   bool no_config;                  /* a context needs no EGLConfig */
} link_gl_now;

/* name among the space-separated names of list */
static bool link_gl_offers(const char *list, const char *name)
{
   size_t len = strlen(name);
   const char *at;
   for (at = list; at && (at = strstr(at, name)); at += len)
      if ((at == list || at[-1] == ' ') && (!at[len] || at[len] == ' '))
         return true;
   return false;
}

/* A platform's display, initialized, if it makes contexts with no surface */
static EGLDisplay link_gl_open(EGLenum platform, void *native)
{
   EGLint major = 0, minor = 0;
   const char *offers;
   EGLDisplay display = eglGetPlatformDisplay(platform, native, NULL);
   if (display == EGL_NO_DISPLAY || !eglInitialize(display, &major, &minor))
      return EGL_NO_DISPLAY;
   offers                = eglQueryString(display, EGL_EXTENSIONS);
   link_gl_now.egl15     = major > 1 || minor >= 5;
   link_gl_now.no_config = link_gl_offers(offers, "EGL_KHR_no_config_context");
   if (     link_gl_offers(offers, "EGL_KHR_surfaceless_context")
         && (link_gl_now.egl15 || link_gl_offers(offers, "EGL_KHR_create_context")))
      return display;
   eglTerminate(display);
   return EGL_NO_DISPLAY;
}

/* The surfaceless platform, else the first device that will do, those a
 * driver draws on in software tried first. Either may open the GPU's
 * device nodes, but --require-renderer checks which renderer draws. */
static EGLDisplay link_gl_display(void)
{
   const char *client = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
   EGLDisplay display = EGL_NO_DISPLAY;
   if (link_gl_offers(client, "EGL_MESA_platform_surfaceless"))
      display = link_gl_open(EGL_PLATFORM_SURFACELESS_MESA, NULL);
   if (display == EGL_NO_DISPLAY && link_gl_offers(client, "EGL_EXT_platform_device"))
   {
      PFNEGLQUERYDEVICESEXTPROC query
         = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
      PFNEGLQUERYDEVICESTRINGEXTPROC string
         = (PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT");
      EGLDeviceEXT devices[LINK_GL_DEVICES];
      EGLint count = 0, i;
      int others;
      if (!query || !query(LINK_GL_DEVICES, devices, &count))
         count = 0;
      for (others = 0; others < 2; others++)
         for (i = 0; i < count && display == EGL_NO_DISPLAY; i++)
         {
            bool software = string && link_gl_offers(string(devices[i], EGL_EXTENSIONS),
                  "EGL_MESA_device_software");
            if (software == !others)
               display = link_gl_open(EGL_PLATFORM_DEVICE_EXT, devices[i]);
         }
   }
   return display;
}

/* The display goes with its last context, and EGL's state of this thread
 * with the display */
static void link_gl_release(void)
{
   if (link_gl_now.users || link_gl_now.display == EGL_NO_DISPLAY)
      return;
   eglTerminate(link_gl_now.display);
   eglReleaseThread();
   link_gl_now.display = EGL_NO_DISPLAY;
}

link_gl_t *link_gl_new(const struct retro_hw_render_callback *hw, unsigned index)
{
   EGLint attributes[9], wanted[5];
   EGLConfig config  = EGL_NO_CONFIG_KHR;
   EGLint count      = 0;
   EGLenum api       = EGL_OPENGL_ES_API;
   const char *kind  = "ES";
   unsigned major    = hw->version_major;
   unsigned minor    = hw->version_minor;
   unsigned n        = 0;
   link_gl_t *g;
   switch (hw->context_type)
   {
      case RETRO_HW_CONTEXT_OPENGL:       /* the driver's compatibility context */
         api  = EGL_OPENGL_API;
         kind = "compatibility";
         break;
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         api  = EGL_OPENGL_API;
         kind = "core";
         break;
      case RETRO_HW_CONTEXT_OPENGLES2:
         major = 2;
         minor = 0;
         break;
      case RETRO_HW_CONTEXT_OPENGLES3:
         major = 3;
         minor = 0;
         break;
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         break;
      default:
         fprintf(stderr, "link_harness: console %u: render=gl does not serve context type %u\n",
               index, (unsigned)hw->context_type);
         return NULL;
   }
   if (hw->context_type != RETRO_HW_CONTEXT_OPENGL)
   {
      attributes[n++] = EGL_CONTEXT_MAJOR_VERSION;
      attributes[n++] = (EGLint)major;
      attributes[n++] = EGL_CONTEXT_MINOR_VERSION;
      attributes[n++] = (EGLint)minor;
   }
   if (hw->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
   {
      attributes[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
      attributes[n++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
   }
   if (link_gl_now.display == EGL_NO_DISPLAY
         && (link_gl_now.display = link_gl_display()) == EGL_NO_DISPLAY)
   {
      fprintf(stderr, "link_harness: console %u: no EGL display"
            " (surfaceless or device platform)\n", index);
      return NULL;
   }
   if (hw->debug_context)
   {
      attributes[n++] = link_gl_now.egl15 ? EGL_CONTEXT_OPENGL_DEBUG : EGL_CONTEXT_FLAGS_KHR;
      attributes[n++] = link_gl_now.egl15 ? EGL_TRUE : EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
   }
   attributes[n] = EGL_NONE;
   wanted[0]     = EGL_SURFACE_TYPE;
   wanted[1]     = 0;
   wanted[2]     = EGL_RENDERABLE_TYPE;
   wanted[3]     = api == EGL_OPENGL_API ? EGL_OPENGL_BIT
      : major >= 3 ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT;
   wanted[4]     = EGL_NONE;
   if (!(g = (link_gl_t*)calloc(1, sizeof(*g))))
   {
      fprintf(stderr, "link_harness: console %u: out of memory for its context\n", index);
      link_gl_release();
      return NULL;
   }
   /* The API is bound before each context is made or made current:
    * consoles may mix OpenGL and OpenGL ES */
   if (     eglBindAPI(api)
         && (     link_gl_now.no_config
               || (eglChooseConfig(link_gl_now.display, wanted, &config, 1, &count) && count)))
      g->context = eglCreateContext(link_gl_now.display, config, EGL_NO_CONTEXT, attributes);
   if (g->context == EGL_NO_CONTEXT)
   {
      fprintf(stderr, "link_harness: console %u: no OpenGL %s %u.%u context (EGL error 0x%x)\n",
            index, kind, major, minor, (unsigned)eglGetError());
      free(g);
      link_gl_release();
      return NULL;
   }
   g->api         = api;
   g->index       = index;
   g->bottom_left = hw->bottom_left_origin;
   if (hw->depth)
      g->depth_format = hw->stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT16;
   link_gl_now.users++;
   return g;
}

/* An entry point; missing: the first name that the context lacks */
static link_gl_entry_t link_gl_entry(const char *name, const char **missing)
{
   link_gl_entry_t entry = (link_gl_entry_t)eglGetProcAddress(name);
   if (!entry && !*missing)
      *missing = name;
   return entry;
}

static bool link_gl_entries(link_gl_t *g)
{
   const char *missing = NULL;
   g->get_string  = (link_gl_get_string_t)link_gl_entry("glGetString", &missing);
   g->get_integer = (link_gl_get_integer_t)link_gl_entry("glGetIntegerv", &missing);
   g->gen_framebuffers  = (link_gl_gen_t)link_gl_entry("glGenFramebuffers", &missing);
   g->gen_textures      = (link_gl_gen_t)link_gl_entry("glGenTextures", &missing);
   g->gen_renderbuffers = (link_gl_gen_t)link_gl_entry("glGenRenderbuffers", &missing);
   g->bind_framebuffer  = (link_gl_bind_t)link_gl_entry("glBindFramebuffer", &missing);
   g->bind_texture      = (link_gl_bind_t)link_gl_entry("glBindTexture", &missing);
   g->bind_renderbuffer = (link_gl_bind_t)link_gl_entry("glBindRenderbuffer", &missing);
   g->bind_buffer       = (link_gl_bind_t)link_gl_entry("glBindBuffer", &missing);
   g->attach_texture
      = (link_gl_attach_texture_t)link_gl_entry("glFramebufferTexture2D", &missing);
   g->attach_renderbuffer
      = (link_gl_attach_renderbuffer_t)link_gl_entry("glFramebufferRenderbuffer", &missing);
   g->status      = (link_gl_status_t)link_gl_entry("glCheckFramebufferStatus", &missing);
   g->tex_image   = (link_gl_tex_image_t)link_gl_entry("glTexImage2D", &missing);
   g->storage     = (link_gl_storage_t)link_gl_entry("glRenderbufferStorage", &missing);
   g->read_pixels = (link_gl_read_pixels_t)link_gl_entry("glReadPixels", &missing);
   g->pixel_store = (link_gl_pixel_store_t)link_gl_entry("glPixelStorei", &missing);
   g->get_error   = (link_gl_get_error_t)link_gl_entry("glGetError", &missing);
   if (missing)
      fprintf(stderr, "link_harness: console %u: its context has no %s\n", g->index, missing);
   return !missing;
}

/* Binds a framebuffer where pixels are read from; returns the one that was */
static unsigned link_gl_bind_read(link_gl_t *g, unsigned framebuffer)
{
   int was = 0;
   g->get_integer(g->gl3 ? GL_READ_FRAMEBUFFER_BINDING : GL_FRAMEBUFFER_BINDING, &was);
   g->bind_framebuffer(g->gl3 ? GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER, framebuffer);
   return (unsigned)was;
}

/* Drops the errors the core left, so that one found after is the harness's.
 * A driver holds no more of them than it has kinds. */
static void link_gl_drop_errors(link_gl_t *g)
{
   unsigned i;
   for (i = 0; i < LINK_GL_ERRORS && g->get_error(); i++)
      ;
}

/* Whether the context has a name of its state */
static bool link_gl_knows(link_gl_t *g, unsigned name)
{
   int value = 0;
   g->get_integer(name, &value);
   return !g->get_error();
}

/* The framebuffer's attachments at a size, the bindings left as they were:
 * a core may be in a frame. false: the driver refused the size or the
 * framebuffer is not complete, the reason on stderr, and the size it had
 * is the one it is known by. */
static bool link_gl_resize(link_gl_t *g, unsigned width, unsigned height)
{
   int texture = 0, renderbuffer = 0, unpack = 0;
   unsigned framebuffer, status, error;
   link_gl_drop_errors(g);
   g->get_integer(GL_TEXTURE_BINDING_2D, &texture);
   g->bind_texture(GL_TEXTURE_2D, g->texture);
   if (g->pixel_buffers)
   {
      /* A buffer bound here would be read as the texture's pixels */
      g->get_integer(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
      g->bind_buffer(GL_PIXEL_UNPACK_BUFFER, 0);
   }
   g->tex_image(GL_TEXTURE_2D, 0, g->gl3 ? GL_RGBA8 : GL_RGBA, (int)width, (int)height, 0,
         GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   if (g->pixel_buffers)
      g->bind_buffer(GL_PIXEL_UNPACK_BUFFER, (unsigned)unpack);
   g->bind_texture(GL_TEXTURE_2D, (unsigned)texture);
   if (g->renderbuffer)
   {
      g->get_integer(GL_RENDERBUFFER_BINDING, &renderbuffer);
      g->bind_renderbuffer(GL_RENDERBUFFER, g->renderbuffer);
      g->storage(GL_RENDERBUFFER, g->depth_format, (int)width, (int)height);
      g->bind_renderbuffer(GL_RENDERBUFFER, (unsigned)renderbuffer);
   }
   if ((error = g->get_error()))
   {
      fprintf(stderr, "link_harness: console %u: its framebuffer cannot be %ux%u"
            " (GL error 0x%x)\n", g->index, width, height, error);
      return false;
   }
   framebuffer = link_gl_bind_read(g, g->framebuffer);
   status      = g->status(g->gl3 ? GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER);
   link_gl_bind_read(g, framebuffer);
   if (status != GL_FRAMEBUFFER_COMPLETE)
   {
      fprintf(stderr, "link_harness: console %u: its framebuffer of %ux%u is not complete"
            " (status 0x%x)\n", g->index, width, height, status);
      return false;
   }
   g->width  = width;
   g->height = height;
   return true;
}

bool link_gl_start(link_gl_t *g, unsigned max_width, unsigned max_height)
{
   if (     !eglBindAPI(g->api)
         || !eglMakeCurrent(link_gl_now.display, EGL_NO_SURFACE, EGL_NO_SURFACE, g->context))
   {
      fprintf(stderr, "link_harness: console %u: its context cannot be made current"
            " (EGL error 0x%x)\n", g->index, (unsigned)eglGetError());
      return false;
   }
   if (!link_gl_entries(g))
      return false;
   g->version  = (const char*)g->get_string(GL_VERSION);
   g->renderer = (const char*)g->get_string(GL_RENDERER);
   if (!g->version || !g->renderer)
   {
      fprintf(stderr, "link_harness: console %u: its context names no version or renderer\n",
            g->index);
      return false;
   }
   /* "4.5 (Core Profile) Mesa ...", "OpenGL ES 3.2 Mesa ..." */
   g->gl3 = atoi(g->version + strcspn(g->version, "0123456789")) >= 3;
   /* OpenGL ES 2 has pixel buffers, and pixel-pack state past an alignment,
    * only by extensions, which a driver may have and not name: the context
    * is asked, here where no error is the core's */
   g->pixel_buffers = link_gl_knows(g, GL_PIXEL_PACK_BUFFER_BINDING);
   g->packs         = link_gl_knows(g, GL_PACK_ROW_LENGTH) ? LINK_GL_PACK : 1;
   /* There is no default framebuffer: the console's stays bound, and a
    * texture and a renderbuffer are bound once to make them */
   g->gen_framebuffers(1, &g->framebuffer);
   g->bind_framebuffer(GL_FRAMEBUFFER, g->framebuffer);
   g->gen_textures(1, &g->texture);
   g->bind_texture(GL_TEXTURE_2D, g->texture);
   g->attach_texture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g->texture, 0);
   g->bind_texture(GL_TEXTURE_2D, 0);
   if (g->depth_format)
   {
      g->gen_renderbuffers(1, &g->renderbuffer);
      g->bind_renderbuffer(GL_RENDERBUFFER, g->renderbuffer);
      g->attach_renderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
            g->renderbuffer);
      if (g->depth_format == GL_DEPTH24_STENCIL8)
         g->attach_renderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
               g->renderbuffer);
      g->bind_renderbuffer(GL_RENDERBUFFER, 0);
   }
   return link_gl_resize(g, max_width, max_height);
}

bool link_gl_grow(link_gl_t *g, unsigned max_width, unsigned max_height)
{
   if (max_width <= g->width && max_height <= g->height)
      return true;
   return link_gl_resize(g, max_width > g->width ? max_width : g->width,
         max_height > g->height ? max_height : g->height);
}

void link_gl_size(const link_gl_t *g, unsigned *width, unsigned *height)
{
   *width  = g->width;
   *height = g->height;
}

uintptr_t link_gl_framebuffer(const link_gl_t *g)
{
   return g->framebuffer;
}

retro_proc_address_t RETRO_CALLCONV link_gl_proc(const char *sym)
{
   return (retro_proc_address_t)eglGetProcAddress(sym);
}

/* A pixel as glReadPixels left it, its bytes R, G, B and A in that order */
static uint32_t link_gl_pixel(const uint32_t *rgba)
{
   const unsigned char *byte = (const unsigned char*)rgba;
   return ((uint32_t)byte[0] << 16) | ((uint32_t)byte[1] << 8) | byte[2];
}

bool link_gl_read(link_gl_t *g, uint32_t *argb, unsigned width, unsigned height)
{
   /* What decides where glReadPixels puts a row of RGBA bytes, the
    * alignment first: every context has that */
   static const unsigned names[LINK_GL_PACK] = {
      GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS
   };
   static const int defaults[LINK_GL_PACK] = { 4, 0, 0, 0 };
   int was[LINK_GL_PACK]                   = { 4, 0, 0, 0 };
   size_t count         = (size_t)width * height;
   unsigned framebuffer = link_gl_bind_read(g, g->framebuffer);
   int pack             = 0;
   unsigned error, i, x, y;
   link_gl_drop_errors(g);
   for (i = 0; i < g->packs; i++)
   {
      g->get_integer(names[i], &was[i]);
      g->pixel_store(names[i], defaults[i]);
   }
   if (g->pixel_buffers)
   {
      /* A buffer bound here would be written in place of argb */
      g->get_integer(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
      g->bind_buffer(GL_PIXEL_PACK_BUFFER, 0);
   }
   g->read_pixels(0, 0, (int)width, (int)height, GL_RGBA, GL_UNSIGNED_BYTE, argb);
   error = g->get_error();
   if (g->pixel_buffers)
      g->bind_buffer(GL_PIXEL_PACK_BUFFER, (unsigned)pack);
   for (i = 0; i < g->packs; i++)
      g->pixel_store(names[i], was[i]);
   link_gl_bind_read(g, framebuffer);
   if (error)
   {
      if (!g->unread)
         fprintf(stderr, "link_harness: console %u: its framebuffer cannot be read"
               " (GL error 0x%x)\n", g->index, error);
      g->unread = true;
      return false;
   }
   if (!g->bottom_left)
   {
      size_t at;
      for (at = 0; at < count; at++)
         argb[at] = link_gl_pixel(&argb[at]);
      return true;
   }
   /* Rows change places end for end; a middle row changes with itself */
   for (y = 0; y < (height + 1) / 2; y++)
   {
      uint32_t *upper = argb + (size_t)y * width;
      uint32_t *lower = argb + (size_t)(height - 1 - y) * width;
      for (x = 0; x < width; x++)
      {
         uint32_t pixel = link_gl_pixel(&upper[x]);
         upper[x]       = link_gl_pixel(&lower[x]);
         lower[x]       = pixel;
      }
   }
   return true;
}

void link_gl_describe(const link_gl_t *g, char *out, size_t len)
{
   strlcpy(out, "gl ", len);
   strlcat(out, g->version, len);
   strlcat(out, " | ", len);
   strlcat(out, g->renderer, len);
   strlcat(out, "\n", len);
}

const char *link_gl_renderer(const link_gl_t *g)
{
   return g->renderer;
}

void link_gl_free(link_gl_t *g)
{
   if (!g)
      return;
   if (eglBindAPI(g->api) && eglGetCurrentContext() == g->context)
      eglMakeCurrent(link_gl_now.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(link_gl_now.display, g->context);
   free(g);
   link_gl_now.users--;
   link_gl_release();
}
