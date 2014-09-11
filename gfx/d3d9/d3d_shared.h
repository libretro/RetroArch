#include "d3d_defines.h"
#include "../gfx_common.h"

/* forward declarations */
static void d3d_calculate_rect(d3d_video_t *d3d,
      unsigned width, unsigned height,
   bool keep, float desired_aspect);
static bool d3d_init_luts(d3d_video_t *d3d);
static void d3d_set_font_rect(d3d_video_t *d3d,
      const struct font_params *params);
static bool d3d_process_shader(d3d_video_t *d3d);
static bool d3d_init_multipass(d3d_video_t *d3d);
static void d3d_deinit_chain(d3d_video_t *d3d);
static bool d3d_init_chain(d3d_video_t *d3d,
      const video_info_t *video_info);

static void renderchain_free(void *data);
void d3d_deinit_shader(void *data);
bool d3d_init_shader(void *data);

void d3d_make_d3dpp(void *data, const video_info_t *info,
	D3DPRESENT_PARAMETERS *d3dpp);

#ifdef HAVE_WINDOW
#define IDI_ICON 1
#define MAX_MONITORS 9

extern LRESULT CALLBACK WindowProc(HWND hWnd, UINT message,
        WPARAM wParam, LPARAM lParam);
static RECT d3d_monitor_rect(d3d_video_t *d3d);
#endif

static void d3d_deinit_chain(d3d_video_t *d3d)
{
#ifdef _XBOX
   renderchain_free(d3d);
#else
   if (d3d->chain)
      delete (renderchain_t *)d3d->chain;
   d3d->chain = NULL;
#endif
}

static void d3d_deinitialize(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d->font_ctx && d3d->font_ctx->deinit)
      d3d->font_ctx->deinit(d3d);
   d3d->font_ctx = NULL;
   d3d_deinit_chain(d3d);
#ifdef HAVE_SHADERS
   d3d_deinit_shader(d3d);
#endif

#ifndef _XBOX
   d3d->needs_restore = false;
#endif
}

static bool d3d_init_base(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   D3DPRESENT_PARAMETERS d3dpp;
   d3d_make_d3dpp(d3d, info, &d3dpp);

   d3d->g_pD3D = D3DCREATE_CTX(D3D_SDK_VERSION);
   if (!d3d->g_pD3D)
   {
      RARCH_ERR("Failed to create D3D interface.\n");
      return false;
   }

   if (FAILED(d3d->d3d_err = d3d->g_pD3D->CreateDevice(
            d3d->cur_mon_id,
            D3DDEVTYPE_HAL,
            d3d->hWnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &d3dpp,
            &d3d->dev)))
   {
      RARCH_WARN("[D3D]: Failed to init device with hardware vertex processing (code: 0x%x). Trying to fall back to software vertex processing.\n",
                 (unsigned)d3d->d3d_err);

      if (FAILED(d3d->d3d_err = d3d->g_pD3D->CreateDevice(
                  d3d->cur_mon_id,
                  D3DDEVTYPE_HAL,
                  d3d->hWnd,
                  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                  &d3dpp,
                  &d3d->dev)))
      {
         RARCH_ERR("Failed to initialize device.\n");
         return false;
      }
   }

   return true;
}

static bool d3d_initialize(void *data, const video_info_t *info)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   bool ret = true;
   if (!d3d->g_pD3D)
      ret = d3d_init_base(d3d, info);
   else if (d3d->needs_restore)
   {
      D3DPRESENT_PARAMETERS d3dpp;
      d3d_make_d3dpp(d3d, info, &d3dpp);
      if (d3d->dev->Reset(&d3dpp) != D3D_OK)
      {
         // Try to recreate the device completely ..
#ifndef _XBOX
         HRESULT res = d3d->dev->TestCooperativeLevel();
         const char *err;
         switch (res)
         {
            case D3DERR_DEVICELOST:
               err = "DEVICELOST";
               break;

            case D3DERR_DEVICENOTRESET:
               err = "DEVICENOTRESET";
               break;

            case D3DERR_DRIVERINTERNALERROR:
               err = "DRIVERINTERNALERROR";
               break;

            default:
               err = "Unknown";
         }
         RARCH_WARN(
               "[D3D]: Attempting to recover from dead state (%s).\n", err);
#else
         RARCH_WARN("[D3D]: Attempting to recover from dead state.\n");
#endif
         d3d_deinitialize(d3d); 
         d3d->g_pD3D->Release();
         d3d->g_pD3D = NULL;
         ret = d3d_init_base(d3d, info);
         if (ret)
            RARCH_LOG("[D3D]: Recovered from dead state.\n");
         else
            return ret;
      }
   }

   if (!ret)
      return ret;

   d3d_calculate_rect(d3d, d3d->screen_width, d3d->screen_height,
         info->force_aspect, g_extern.system.aspect_ratio);

#ifdef HAVE_SHADERS
   if (!d3d_init_shader(d3d))
   {
      RARCH_ERR("Failed to initialize shader subsystem.\n");
      return false;
   }
#endif

   if (!d3d_init_chain(d3d, info))
   {
      RARCH_ERR("Failed to initialize render chain.\n");
      return false;
   }

#if defined(_XBOX360)
   strlcpy(g_settings.video.font_path, "game:\\media\\Arial_12.xpr",
         sizeof(g_settings.video.font_path));
#endif
   d3d->font_ctx = d3d_font_init_first(d3d, g_settings.video.font_path, 0);
   if (!d3d->font_ctx)
   {
      RARCH_ERR("Failed to initialize font.\n");
      return false;
   }

   return true;
}

static void d3d_set_viewport(d3d_video_t *d3d, int x, int y,
      unsigned width, unsigned height)
{
   D3DVIEWPORT viewport;

   /* D3D doesn't support negative X/Y viewports ... */
   if (x < 0)
      x = 0;
   if (y < 0)
      y = 0;

   viewport.X = x;
   viewport.Y = y;
   viewport.Width = width;
   viewport.Height = height;
   viewport.MinZ = 0.0f;
   viewport.MaxZ = 1.0f;

   d3d->final_viewport = viewport;

#ifndef _XBOX
   d3d_set_font_rect(d3d, NULL);
#endif
}
bool d3d_restore(d3d_video_t *d3d)
{
   d3d_deinitialize(d3d);
   d3d->needs_restore = !d3d_initialize(d3d, &d3d->video_info);

   if (d3d->needs_restore)
      RARCH_ERR("[D3D]: Restore error.\n");

   return !d3d->needs_restore;
}

static void d3d_calculate_rect(d3d_video_t *d3d,
      unsigned width, unsigned height,
   bool keep, float desired_aspect)
{
   if (g_settings.video.scale_integer)
   {
      struct rarch_viewport vp = {0};
      gfx_scale_integer(&vp, width, height, desired_aspect, keep);
      d3d_set_viewport(d3d, vp.x, vp.y, vp.width, vp.height);
   }
   else if (!keep)
      d3d_set_viewport(d3d, 0, 0, width, height);
   else
   {
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const rarch_viewport_t &custom = 
            g_extern.console.screen.viewports.custom_vp;
         d3d_set_viewport(d3d, custom.x, custom.y, 
               custom.width, custom.height);
      }
      else
      {
         float device_aspect = static_cast<float>(width) / 
            static_cast<float>(height);

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
            d3d_set_viewport(d3d, 0, 0, width, height);
         else if (device_aspect > desired_aspect)
         {
            float delta = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, int(roundf(width * (0.5f - delta))),
                  0, unsigned(roundf(2.0f * width * delta)), height);
         }
         else
         {
            float delta = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            d3d_set_viewport(d3d, 0, int(roundf(height * (0.5f - delta))),
                  width, unsigned(roundf(2.0f * height * delta)));
         }
      }
   }
}

static void d3d_set_nonblock_state(void *data, bool state)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->video_info.vsync = !state;

   if (d3d->ctx_driver && d3d->ctx_driver->swap_interval)
      d3d->ctx_driver->swap_interval(d3d, state ? 0 : 1);
}

static bool d3d_alive(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   bool quit, resize;

   quit = false;
   resize = false;

   if (d3d->ctx_driver && d3d->ctx_driver->check_window)
      d3d->ctx_driver->check_window(d3d, &quit, &resize,
            &d3d->screen_width, &d3d->screen_height, g_extern.frame_count);

   if (quit)
      d3d->quitting = quit;
   else if (resize)
      d3d->should_resize = true;

   return !quit;
}

static bool d3d_focus(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   if (d3d && d3d->ctx_driver && d3d->ctx_driver->has_focus)
      return d3d->ctx_driver->has_focus(d3d);
   return false;
}

static void d3d_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         gfx_set_square_pixel_viewport(
               g_extern.system.av_info.geometry.base_width,
               g_extern.system.av_info.geometry.base_height);
         break;

      case ASPECT_RATIO_CORE:
         gfx_set_core_viewport();
         break;

      case ASPECT_RATIO_CONFIG:
         gfx_set_config_viewport();
         break;

      default:
         break;
   }

   g_extern.system.aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;
   d3d->video_info.force_aspect = true;
   d3d->should_resize = true;
}

static void d3d_apply_state_changes(void *data)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->should_resize = true;
}

static void d3d_set_osd_msg(void *data, const char *msg,
      const struct font_params *params)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

#ifndef _XBOX
   if (params)
      d3d_set_font_rect(d3d, params);
#endif

   if (d3d && d3d->font_ctx && d3d->font_ctx->render_msg)
      d3d->font_ctx->render_msg(d3d, msg, params);
}

/* Delay constructor due to lack of exceptions. */

static bool d3d_construct(d3d_video_t *d3d,
      const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   d3d->should_resize = false;
#ifndef _XBOX
   gfx_set_dwm();
#endif

#ifdef HAVE_MENU
   if (d3d->menu)
      free(d3d->menu);

   d3d->menu = (overlay_t*)calloc(1, sizeof(overlay_t));
   d3d->menu->tex_coords.x = 0;
   d3d->menu->tex_coords.y = 0;
   d3d->menu->tex_coords.w = 1;
   d3d->menu->tex_coords.h = 1;
   d3d->menu->vert_coords.x = 0;
   d3d->menu->vert_coords.y = 1;
   d3d->menu->vert_coords.w = 1;
   d3d->menu->vert_coords.h = -1;
#endif

#ifdef HAVE_WINDOW
   memset(&d3d->windowClass, 0, sizeof(d3d->windowClass));
   d3d->windowClass.cbSize        = sizeof(d3d->windowClass);
   d3d->windowClass.style         = CS_HREDRAW | CS_VREDRAW;
   d3d->windowClass.lpfnWndProc   = WindowProc;
   d3d->windowClass.hInstance     = NULL;
   d3d->windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
   d3d->windowClass.lpszClassName = "RetroArch";
   d3d->windowClass.hIcon = LoadIcon(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON));
   d3d->windowClass.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!info->fullscreen)
      d3d->windowClass.hbrBackground = (HBRUSH)COLOR_WINDOW;

   RegisterClassEx(&d3d->windowClass);
#endif

   unsigned full_x, full_y;
#ifdef HAVE_MONITOR
   RECT mon_rect = d3d_monitor_rect(d3d);

   bool windowed_full = g_settings.video.windowed_fullscreen;

   full_x = (windowed_full || info->width  == 0) ? 
      (mon_rect.right  - mon_rect.left) : info->width;
   full_y = (windowed_full || info->height == 0) ? 
      (mon_rect.bottom - mon_rect.top)  : info->height;
   RARCH_LOG("[D3D]: Monitor size: %dx%d.\n", 
         (int)(mon_rect.right  - mon_rect.left),
         (int)(mon_rect.bottom - mon_rect.top));
#else
   if (d3d->ctx_driver && d3d->ctx_driver->get_video_size)
      d3d->ctx_driver->get_video_size(d3d, &full_x, &full_y);
#endif
   d3d->screen_width  = info->fullscreen ? full_x : info->width;
   d3d->screen_height = info->fullscreen ? full_y : info->height;

#ifdef HAVE_WINDOW
   unsigned win_width  = d3d->screen_width;
   unsigned win_height = d3d->screen_height;

   if (!info->fullscreen)
   {
      RECT rect   = {0};
      rect.right  = d3d->screen_width;
      rect.bottom = d3d->screen_height;
      AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
      win_width  = rect.right - rect.left;
      win_height = rect.bottom - rect.top;
   }

   char buffer[128];
   gfx_get_fps(buffer, sizeof(buffer), NULL, 0);
   std::string title = buffer;
   title += " || Direct3D";

   d3d->hWnd = CreateWindowEx(0, "RetroArch", title.c_str(),
         info->fullscreen ?
         (WS_EX_TOPMOST | WS_POPUP) : WS_OVERLAPPEDWINDOW,
         info->fullscreen ? mon_rect.left : CW_USEDEFAULT,
         info->fullscreen ? mon_rect.top  : CW_USEDEFAULT,
         win_width, win_height,
         NULL, NULL, NULL, d3d);

   driver.display_type  = RARCH_DISPLAY_WIN32;
   driver.video_display = 0;
   driver.video_window  = (uintptr_t)d3d->hWnd;
#endif

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(d3d, !info->fullscreen
#ifdef HAVE_OVERLAY
      || d3d->overlays_enabled
#endif
   );

#ifdef HAVE_WINDOW
   ShowWindow(d3d->hWnd, SW_RESTORE);
   UpdateWindow(d3d->hWnd);
   SetForegroundWindow(d3d->hWnd);
   SetFocus(d3d->hWnd);
#endif

#ifndef _XBOX
#ifdef HAVE_SHADERS
   /* This should only be done once here
    * to avoid set_shader() to be overridden
    * later. */
   enum rarch_shader_type type = 
      gfx_shader_parse_type(g_settings.video.shader_path, RARCH_SHADER_NONE);
   if (g_settings.video.shader_enable && type == RARCH_SHADER_CG)
      d3d->cg_shader = g_settings.video.shader_path;

   if (!d3d_process_shader(d3d))
      return false;
#endif
#endif

   d3d->video_info = *info;
   if (!d3d_initialize(d3d, &d3d->video_info))
      return false;

   if (input && input_data &&
      d3d->ctx_driver && d3d->ctx_driver->input_driver)
      d3d->ctx_driver->input_driver(d3d, input, input_data);

   RARCH_LOG("[D3D]: Init complete.\n");
   return true;
}

static void d3d_viewport_info(void *data, struct rarch_viewport *vp)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   vp->x           = d3d->final_viewport.X;
   vp->y           = d3d->final_viewport.Y;
   vp->width       = d3d->final_viewport.Width;
   vp->height      = d3d->final_viewport.Height;

   vp->full_width  = d3d->screen_width;
   vp->full_height = d3d->screen_height;
}

static void d3d_set_rotation(void *data, unsigned rot)
{
   d3d_video_t *d3d = (d3d_video_t*)data;
   d3d->dev_rotation = rot;
}

static void d3d_show_mouse(void *data, bool state)
{
   d3d_video_t *d3d = (d3d_video_t*)data;

   if (d3d && d3d->ctx_driver && d3d->ctx_driver->show_mouse)
      d3d->ctx_driver->show_mouse(d3d, state);
}

static const gfx_ctx_driver_t *d3d_get_context(void *data)
{
   /* Default to Direct3D9 for now.
   TODO: GL core contexts through ANGLE? */
   enum gfx_ctx_api api = GFX_CTX_DIRECT3D9_API;
   unsigned major = 9, minor = 0;
   (void)major;
   (void)minor;

#if defined(_XBOX1)
   api = GFX_CTX_DIRECT3D8_API;
   major = 8;
#endif
   return gfx_ctx_init_first(driver.video_data, api,
         major, minor, false);
}
