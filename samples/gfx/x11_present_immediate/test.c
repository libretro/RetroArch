/* shipping.inc, cut by run.py: x11_window_uncomposed(), the X
 * contexts' present_immediate and the Vulkan driver's, as built off
 * Windows. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>

/* the Vulkan driver's answer has a Windows half; this is the other */
#undef _WIN32
#define HAVE_EGL 1

typedef struct { int screen; } Display;
typedef unsigned long Atom;
typedef unsigned long Window;
#define None 0UL
#define False 0
#define DefaultScreen(dpy) ((dpy)->screen)

static Display *g_x11_dpy;
static char     asked_atom[32];
static unsigned asks;
static Window   owner;

static Atom XInternAtom(Display *dpy, const char *name, int only)
{
   assert(dpy == g_x11_dpy && !only);
   strcpy(asked_atom, name);
   return 77;
}
static Window XGetSelectionOwner(Display *dpy, Atom atom)
{
   assert(dpy == g_x11_dpy && atom == 77);
   asks++;
   return owner;
}

typedef struct { int interval; bool uncomposed; } gfx_ctx_x_data_t;
typedef struct { struct { int interval; } egl; bool uncomposed; }
   xegl_ctx_data_t;
typedef struct { bool uncomposed; } gfx_ctx_x_vk_data_t;

#define VK_PRESENT_MODE_IMMEDIATE_KHR 0
#define VK_PRESENT_MODE_FIFO_KHR 2
typedef struct { int swapchain_present_mode; } vulkan_context_t;
typedef struct { bool (*present_immediate)(void *data); } gfx_ctx_driver_t;
typedef struct
{
   vulkan_context_t *context;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
} vk_t;

#include "shipping.inc"

int main(void)
{
   Display dpy;
   gfx_ctx_x_data_t glx;
   xegl_ctx_data_t egl;
   gfx_ctx_x_vk_data_t xvk;
   gfx_ctx_driver_t driver;
   vulkan_context_t context;
   vk_t vk;

   /* asked to bypass: nothing to ask the server */
   dpy.screen = 0;
   g_x11_dpy  = &dpy;
   owner      = 5;
   assert(x11_window_uncomposed(true) && asks == 0);
   /* a compositing manager owns its screen's selection */
   assert(!x11_window_uncomposed(false) && asks == 1);
   assert(!strcmp(asked_atom, "_NET_WM_CM_S0"));
   owner      = None;
   assert(x11_window_uncomposed(false) && asks == 2);
   dpy.screen = 3;
   assert(x11_window_uncomposed(false) && !strcmp(asked_atom, "_NET_WM_CM_S3"));
   dpy.screen = 12;
   assert(x11_window_uncomposed(false) && !strcmp(asked_atom, "_NET_WM_CM_S12"));
   g_x11_dpy  = NULL;
   assert(!x11_window_uncomposed(false));
   assert(x11_window_uncomposed(true));

   /* GLX and EGL: no swap interval, and no compositor */
   assert(!gfx_ctx_x_present_immediate(NULL));
   glx.interval = 0; glx.uncomposed = true;
   assert(gfx_ctx_x_present_immediate(&glx));
   glx.uncomposed = false;
   assert(!gfx_ctx_x_present_immediate(&glx));
   glx.interval = 1; glx.uncomposed = true;
   assert(!gfx_ctx_x_present_immediate(&glx));
   assert(!gfx_ctx_xegl_present_immediate(NULL));
   egl.egl.interval = 0; egl.uncomposed = true;
   assert(gfx_ctx_xegl_present_immediate(&egl));
   egl.uncomposed = false;
   assert(!gfx_ctx_xegl_present_immediate(&egl));
   egl.egl.interval = 1; egl.uncomposed = true;
   assert(!gfx_ctx_xegl_present_immediate(&egl));

   /* Vulkan: the swapchain's mode, and the context's word where it
    * has one */
   assert(!vulkan_present_immediate(NULL));
   context.swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
   driver.present_immediate       = gfx_ctx_x_vk_present_immediate;
   xvk.uncomposed                 = false;
   vk.context                     = &context;
   vk.ctx_data                    = &xvk;
   vk.ctx_driver                  = &driver;
   assert(!vulkan_present_immediate(&vk));
   xvk.uncomposed                 = true;
   assert(vulkan_present_immediate(&vk));
   context.swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
   assert(!vulkan_present_immediate(&vk));
   context.swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
   assert(!gfx_ctx_x_vk_present_immediate(NULL));
   /* a context with nothing to say leaves it to the mode */
   driver.present_immediate       = NULL;
   xvk.uncomposed                 = false;
   assert(vulkan_present_immediate(&vk));
   vk.ctx_driver                  = NULL;
   assert(vulkan_present_immediate(&vk));
   puts("PASS X11 immediate present: swap interval, swapchain mode and the compositor");
   return 0;
}
