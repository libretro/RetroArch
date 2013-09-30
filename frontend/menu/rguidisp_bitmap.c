#include "../../gfx/fonts/bitmap.h"

#define TERM_START_X 15
#define TERM_START_Y 27
#define TERM_WIDTH (((RGUI_WIDTH - TERM_START_X - 15) / (FONT_WIDTH_STRIDE)))
#define TERM_HEIGHT (((RGUI_HEIGHT - TERM_START_Y - 15) / (FONT_HEIGHT_STRIDE)) - 1)

static void rgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   for (int y = 0; y < FONT_HEIGHT; y++)
   {
      for (int x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static uint16_t gray_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = ((x + y) & 1) + 1;
#ifdef GEKKO
   return (6 << 12) | (col << 8) | (col << 4) | (col << 0);
#else
   return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
#endif
}

static uint16_t green_filler(unsigned x, unsigned y)
{
   x >>= 1;
   y >>= 1;
   unsigned col = ((x + y) & 1) + 1;
#ifdef GEKKO
   return (6 << 12) | (col << 8) | (col << 5) | (col << 0);
#else
   return (col << 13) | (col << 10) | (col << 5) | (12 << 0);
#endif
}

static void fill_rect(uint16_t *buf, unsigned pitch,
      unsigned x, unsigned y,
      unsigned width, unsigned height,
      uint16_t (*col)(unsigned x, unsigned y))
{
   for (unsigned j = y; j < y + height; j++)
      for (unsigned i = x; i < x + width; i++)
         buf[j * (pitch >> 1) + i] = col(i, j);
}

static void blit_line(rgui_handle_t *rgui,
      int x, int y, const char *message, bool green)
{
   while (*message)
   {
      for (int j = 0; j < FONT_HEIGHT; j++)
      {
         for (int i = 0; i < FONT_WIDTH; i++)
         {
            uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
            int offset = (i + j * FONT_WIDTH) >> 3;
            bool col = (rgui->font[FONT_OFFSET((unsigned char)*message) + offset] & rem);

            if (col)
            {
               rgui->frame_buf[(y + j) * (rgui->frame_buf_pitch >> 1) + (x + i)] = green ?
#ifdef GEKKO
               (3 << 0) | (10 << 4) | (3 << 8) | (7 << 12) : 0x7FFF;
#else
               (15 << 0) | (7 << 4) | (15 << 8) | (7 << 12) : 0xFFFF;
#endif
            }
         }
      }

      x += FONT_WIDTH_STRIDE;
      message++;
   }
}

static void init_font(rgui_handle_t *rgui, const uint8_t *font_bmp_buf)
{
   uint8_t *font = (uint8_t *) calloc(1, FONT_OFFSET(256));
   rgui->alloc_font = true;
   for (unsigned i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      rgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   rgui->font = font;
}

static bool rguidisp_init_font(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   const uint8_t *font_bmp_buf = NULL;
   const uint8_t *font_bin_buf = bitmap_bin;
   bool ret = true;

   if (font_bmp_buf)
      init_font(rgui, font_bmp_buf);
   else if (font_bin_buf)
      rgui->font = font_bin_buf;
   else
      ret = false;

   return ret;
}

static void render_background(rgui_handle_t *rgui)
{
   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         0, 0, RGUI_WIDTH, RGUI_HEIGHT, gray_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         5, 5, RGUI_WIDTH - 10, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         5, RGUI_HEIGHT - 10, RGUI_WIDTH - 10, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         5, 5, 5, RGUI_HEIGHT - 10, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         RGUI_WIDTH - 10, 5, 5, RGUI_HEIGHT - 10, green_filler);
}

static void render_messagebox(rgui_handle_t *rgui, const char *message)
{
   if (!message || !*message)
      return;

   char *msg = strdup(message);
   if (strlen(msg) > TERM_WIDTH)
   {
      msg[TERM_WIDTH - 2] = '.';
      msg[TERM_WIDTH - 1] = '.';
      msg[TERM_WIDTH - 0] = '.';
      msg[TERM_WIDTH + 1] = '\0';
   }

   unsigned width = strlen(msg) * FONT_WIDTH_STRIDE - 1 + 6 + 10;
   unsigned height = FONT_HEIGHT + 6 + 10;
   int x = (RGUI_WIDTH - width) / 2;
   int y = (RGUI_HEIGHT - height) / 2;
   
   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x + 5, y + 5, width - 10, height - 10, gray_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x, y, width - 5, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x + width - 5, y, 5, height - 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x + 5, y + height - 5, width - 5, 5, green_filler);

   fill_rect(rgui->frame_buf, rgui->frame_buf_pitch,
         x, y + 5, 5, height - 5, green_filler);

   blit_line(rgui, x + 8, y + 8, msg, false);
   free(msg);
}

static void render_text(rgui_handle_t *rgui)
{
   if (rgui->need_refresh && 
         (g_extern.lifecycle_mode_state & (1ULL << MODE_MENU))
         && !rgui->msg_force)
      return;

   size_t begin = rgui->selection_ptr >= TERM_HEIGHT / 2 ?
      rgui->selection_ptr - TERM_HEIGHT / 2 : 0;
   size_t end = rgui->selection_ptr + TERM_HEIGHT <= rgui->selection_buf->size ?
      rgui->selection_ptr + TERM_HEIGHT : rgui->selection_buf->size;
   
   // Do not scroll if all items are visible.
   if (rgui->selection_buf->size <= TERM_HEIGHT)
      begin = 0;

   if (end - begin > TERM_HEIGHT)
      end = begin + TERM_HEIGHT;

   render_background(rgui);

   char title[256];
   const char *dir = NULL;
   unsigned menu_type = 0;
   rgui_list_get_last(rgui->menu_stack, &dir, &menu_type);

   if (menu_type == RGUI_SETTINGS_CORE)
      snprintf(title, sizeof(title), "CORE SELECTION %s", dir);
   else if (menu_type == RGUI_SETTINGS_CONFIG)
      snprintf(title, sizeof(title), "CONFIG %s", dir);
   else if (menu_type == RGUI_SETTINGS_DISK_APPEND)
      snprintf(title, sizeof(title), "DISK APPEND %s", dir);
   else if (menu_type == RGUI_SETTINGS_VIDEO_OPTIONS)
      strlcpy(title, "VIDEO OPTIONS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type == RGUI_SETTINGS_SHADER_OPTIONS)
      strlcpy(title, "SHADER OPTIONS", sizeof(title));
#endif
   else if (menu_type == RGUI_SETTINGS_AUDIO_OPTIONS)
      strlcpy(title, "AUDIO OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_DISK_OPTIONS)
      strlcpy(title, "DISK OPTIONS", sizeof(title));
   else if (menu_type == RGUI_SETTINGS_CORE_OPTIONS)
      strlcpy(title, "CORE OPTIONS", sizeof(title));
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type_is_shader_browser(menu_type))
      snprintf(title, sizeof(title), "SHADER %s", dir);
#endif
   else if ((menu_type == RGUI_SETTINGS_INPUT_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_PATH_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_OPTIONS) ||
         (menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT || menu_type == RGUI_SETTINGS_CUSTOM_VIEWPORT_2) ||
         menu_type == RGUI_SETTINGS_CUSTOM_BIND ||
         menu_type == RGUI_SETTINGS)
      snprintf(title, sizeof(title), "MENU %s", dir);
   else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
      strlcpy(title, "LOAD HISTORY", sizeof(title));
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_SETTINGS_OVERLAY_PRESET)
      snprintf(title, sizeof(title), "OVERLAY %s", dir);
#endif
   else if (menu_type == RGUI_BROWSER_DIR_PATH)
      snprintf(title, sizeof(title), "BROWSER DIR %s", dir);
#ifdef HAVE_SCREENSHOTS
   else if (menu_type == RGUI_SCREENSHOT_DIR_PATH)
      snprintf(title, sizeof(title), "SCREENSHOT DIR %s", dir);
#endif
   else if (menu_type == RGUI_SHADER_DIR_PATH)
      snprintf(title, sizeof(title), "SHADER DIR %s", dir);
   else if (menu_type == RGUI_SAVESTATE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVESTATE DIR %s", dir);
#ifdef HAVE_DYNAMIC
   else if (menu_type == RGUI_LIBRETRO_DIR_PATH)
      snprintf(title, sizeof(title), "LIBRETRO DIR %s", dir);
#endif
   else if (menu_type == RGUI_CONFIG_DIR_PATH)
      snprintf(title, sizeof(title), "CONFIG DIR %s", dir);
   else if (menu_type == RGUI_SAVEFILE_DIR_PATH)
      snprintf(title, sizeof(title), "SAVEFILE DIR %s", dir);
#ifdef HAVE_OVERLAY
   else if (menu_type == RGUI_OVERLAY_DIR_PATH)
      snprintf(title, sizeof(title), "OVERLAY DIR %s", dir);
#endif
   else if (menu_type == RGUI_SYSTEM_DIR_PATH)
      snprintf(title, sizeof(title), "SYSTEM DIR %s", dir);
   else
   {
      const char *core_name = rgui->info.library_name;
      if (!core_name)
         core_name = g_extern.system.info.library_name;
      if (!core_name)
         core_name = "No Core";

      snprintf(title, sizeof(title), "GAME (%s) %s", core_name, dir);
   }

   char title_buf[256];
   menu_ticker_line(title_buf, TERM_WIDTH - 3, g_extern.frame_count / 15, title, true);
   blit_line(rgui, TERM_START_X + 15, 15, title_buf, true);

   char title_msg[64];
   const char *core_name = rgui->info.library_name;
   if (!core_name)
      core_name = g_extern.system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   const char *core_version = rgui->info.library_version;
   if (!core_version)
      core_version = g_extern.system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, sizeof(title_msg), "%s - %s %s", PACKAGE_VERSION, core_name, core_version);
   blit_line(rgui, TERM_START_X + 15, (TERM_HEIGHT * FONT_HEIGHT_STRIDE) + TERM_START_Y + 2, title_msg, true);

   unsigned x = TERM_START_X;
   unsigned y = TERM_START_Y;

   for (size_t i = begin; i < end; i++, y += FONT_HEIGHT_STRIDE)
   {
      const char *path = 0;
      unsigned type = 0;
      rgui_list_get_at_offset(rgui->selection_buf, i, &path, &type);
      char message[256];
      char type_str[256];

      unsigned w = 19;
      if (menu_type == RGUI_SETTINGS_INPUT_OPTIONS || menu_type == RGUI_SETTINGS_CUSTOM_BIND)
         w = 21;
      else if (menu_type == RGUI_SETTINGS_PATH_OPTIONS)
         w = 24;

      unsigned port = rgui->current_pad;
      
#ifdef HAVE_SHADER_MANAGER
      if (type >= RGUI_SETTINGS_SHADER_FILTER &&
            type <= RGUI_SETTINGS_SHADER_LAST)
      {
         // HACK. Work around that we're using the menu_type as dir type to propagate state correctly.
         if (menu_type_is_shader_browser(menu_type) && menu_type_is_shader_browser(type))
         {
            type = RGUI_FILE_DIRECTORY;
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            w = 5;
         }
         else if (type == RGUI_SETTINGS_SHADER_OPTIONS || type == RGUI_SETTINGS_SHADER_PRESET)
            strlcpy(type_str, "...", sizeof(type_str));
         else if (type == RGUI_SETTINGS_SHADER_FILTER)
            snprintf(type_str, sizeof(type_str), "%s",
                  g_settings.video.smooth ? "Linear" : "Nearest");
         else
            shader_manager_get_str(&rgui->shader, type_str, sizeof(type_str), type);
      }
      else
#endif
      if (menu_type == RGUI_SETTINGS_CORE ||
            menu_type == RGUI_SETTINGS_CONFIG ||
#ifdef HAVE_OVERLAY
            menu_type == RGUI_SETTINGS_OVERLAY_PRESET ||
#endif
            menu_type == RGUI_SETTINGS_DISK_APPEND ||
            menu_type_is_directory_browser(menu_type))
      {
         if (type == RGUI_FILE_PLAIN)
         {
            strlcpy(type_str, "(FILE)", sizeof(type_str));
            w = 6;
         }
         else if (type == RGUI_FILE_USE_DIRECTORY)
         {
            *type_str = '\0';
            w = 0;
         }
         else
         {
            strlcpy(type_str, "(DIR)", sizeof(type_str));
            type = RGUI_FILE_DIRECTORY;
            w = 5;
         }
      }
      else if (menu_type == RGUI_SETTINGS_OPEN_HISTORY)
      {
         *type_str = '\0';
         w = 0;
      }
      else if (type >= RGUI_SETTINGS_CORE_OPTION_START)
         strlcpy(type_str, core_option_get_val(g_extern.system.core_options, type - RGUI_SETTINGS_CORE_OPTION_START), sizeof(type_str));
      else
      {
         switch (type)
         {
            case RGUI_SETTINGS_VIDEO_ROTATION:
               strlcpy(type_str, rotation_lut[g_settings.video.rotation],
                     sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_SOFT_FILTER:
               snprintf(type_str, sizeof(type_str),
                     (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_SOFT_FILTER_ENABLE)) ? "ON" : "OFF");
               break;
            case RGUI_SETTINGS_VIDEO_FILTER:
               if (g_settings.video.smooth)
                  strlcpy(type_str, "Bilinear filtering", sizeof(type_str));
               else
                  strlcpy(type_str, "Point filtering", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_GAMMA:
               snprintf(type_str, sizeof(type_str), "%d", g_extern.console.screen.gamma_correction);
               break;
            case RGUI_SETTINGS_VIDEO_VSYNC:
               strlcpy(type_str, g_settings.video.vsync ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_HARD_SYNC:
               strlcpy(type_str, g_settings.video.hard_sync ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_BLACK_FRAME_INSERTION:
               strlcpy(type_str, g_settings.video.black_frame_insertion ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_SWAP_INTERVAL:
               snprintf(type_str, sizeof(type_str), "%u", g_settings.video.swap_interval);
               break;
            case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_X:
               snprintf(type_str, sizeof(type_str), "%.1fx", g_settings.video.xscale);
               break;
            case RGUI_SETTINGS_VIDEO_WINDOW_SCALE_Y:
               snprintf(type_str, sizeof(type_str), "%.1fx", g_settings.video.yscale);
               break;
            case RGUI_SETTINGS_VIDEO_CROP_OVERSCAN:
               strlcpy(type_str, g_settings.video.crop_overscan ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_HARD_SYNC_FRAMES:
               snprintf(type_str, sizeof(type_str), "%u", g_settings.video.hard_sync_frames);
               break;
            case RGUI_SETTINGS_VIDEO_REFRESH_RATE_AUTO:
            {
               double refresh_rate = 0.0;
               double deviation = 0.0;
               unsigned sample_points = 0;
               if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
                  snprintf(type_str, sizeof(type_str), "%.3f Hz (%.1f%% dev, %u samples)", refresh_rate, 100.0 * deviation, sample_points);
               else
                  strlcpy(type_str, "N/A", sizeof(type_str));
               break;
            }
            case RGUI_SETTINGS_VIDEO_INTEGER_SCALE:
               strlcpy(type_str, g_settings.video.scale_integer ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_VIDEO_ASPECT_RATIO:
               strlcpy(type_str, aspectratio_lut[g_settings.video.aspect_ratio_idx].name, sizeof(type_str));
               break;
#ifdef GEKKO
            case RGUI_SETTINGS_VIDEO_RESOLUTION:
               strlcpy(type_str, gx_get_video_mode(), sizeof(type_str));
               break;
#endif

            case RGUI_FILE_PLAIN:
               strlcpy(type_str, "(FILE)", sizeof(type_str));
               w = 6;
               break;
            case RGUI_FILE_DIRECTORY:
               strlcpy(type_str, "(DIR)", sizeof(type_str));
               w = 5;
               break;
            case RGUI_SETTINGS_REWIND_ENABLE:
               strlcpy(type_str, g_settings.rewind_enable ? "ON" : "OFF", sizeof(type_str));
               break;
#ifdef HAVE_SCREENSHOTS
            case RGUI_SETTINGS_GPU_SCREENSHOT:
               strlcpy(type_str, g_settings.video.gpu_screenshot ? "ON" : "OFF", sizeof(type_str));
               break;
#endif
            case RGUI_SETTINGS_REWIND_GRANULARITY:
               snprintf(type_str, sizeof(type_str), "%u", g_settings.rewind_granularity);
               break;
            case RGUI_SETTINGS_CONFIG_SAVE_ON_EXIT:
               strlcpy(type_str, g_extern.config_save_on_exit ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_SRAM_AUTOSAVE:
               strlcpy(type_str, g_settings.autosave_interval ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_SAVESTATE_SAVE:
            case RGUI_SETTINGS_SAVESTATE_LOAD:
               snprintf(type_str, sizeof(type_str), "%d", g_extern.state_slot);
               break;
            case RGUI_SETTINGS_AUDIO_MUTE:
               strlcpy(type_str, g_extern.audio_data.mute ? "ON" : "OFF", sizeof(type_str));
               break;
            case RGUI_SETTINGS_AUDIO_CONTROL_RATE_DELTA:
               snprintf(type_str, sizeof(type_str), "%.3f", g_settings.audio.rate_control_delta);
               break;
            case RGUI_SETTINGS_DEBUG_TEXT:
               snprintf(type_str, sizeof(type_str), (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW)) ? "ON" : "OFF");
               break;
            case RGUI_BROWSER_DIR_PATH:
               strlcpy(type_str, *g_settings.rgui_browser_directory ? g_settings.rgui_browser_directory : "<default>", sizeof(type_str));
               break;
#ifdef HAVE_SCREENSHOTS
            case RGUI_SCREENSHOT_DIR_PATH:
               strlcpy(type_str, *g_settings.screenshot_directory ? g_settings.screenshot_directory : "<ROM dir>", sizeof(type_str));
               break;
#endif
            case RGUI_SAVEFILE_DIR_PATH:
               strlcpy(type_str, *g_extern.savefile_dir ? g_extern.savefile_dir : "<ROM dir>", sizeof(type_str));
               break;
#ifdef HAVE_OVERLAY
            case RGUI_OVERLAY_DIR_PATH:
               strlcpy(type_str, *g_extern.overlay_dir ? g_extern.overlay_dir : "<default>", sizeof(type_str));
               break;
#endif
            case RGUI_SAVESTATE_DIR_PATH:
               strlcpy(type_str, *g_extern.savestate_dir ? g_extern.savestate_dir : "<ROM dir>", sizeof(type_str));
               break;
#ifdef HAVE_DYNAMIC
            case RGUI_LIBRETRO_DIR_PATH:
               strlcpy(type_str, *rgui->libretro_dir ? rgui->libretro_dir : "<None>", sizeof(type_str));
               break;
#endif
            case RGUI_CONFIG_DIR_PATH:
               strlcpy(type_str, *g_settings.rgui_config_directory ? g_settings.rgui_config_directory : "<default>", sizeof(type_str));
               break;
            case RGUI_SHADER_DIR_PATH:
               strlcpy(type_str, *g_settings.video.shader_dir ? g_settings.video.shader_dir : "<default>", sizeof(type_str));
               break;
            case RGUI_SYSTEM_DIR_PATH:
               strlcpy(type_str, *g_settings.system_directory ? g_settings.system_directory : "<ROM dir>", sizeof(type_str));
               break;
            case RGUI_SETTINGS_DISK_INDEX:
            {
               const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
               unsigned images = control->get_num_images();
               unsigned current = control->get_image_index();
               if (current >= images)
                  strlcpy(type_str, "No Disk", sizeof(type_str));
               else
                  snprintf(type_str, sizeof(type_str), "%u", current + 1);
               break;
            }
            case RGUI_SETTINGS_CONFIG:
               if (*g_extern.config_path)
                  fill_pathname_base(type_str, g_extern.config_path, sizeof(type_str));
               else
                  strlcpy(type_str, "<default>", sizeof(type_str));
               break;
            case RGUI_SETTINGS_OPEN_FILEBROWSER:
            case RGUI_SETTINGS_OPEN_HISTORY:
            case RGUI_SETTINGS_CORE_OPTIONS:
            case RGUI_SETTINGS_CUSTOM_VIEWPORT:
            case RGUI_SETTINGS_TOGGLE_FULLSCREEN:
            case RGUI_SETTINGS_VIDEO_OPTIONS:
            case RGUI_SETTINGS_AUDIO_OPTIONS:
            case RGUI_SETTINGS_DISK_OPTIONS:
            case RGUI_SETTINGS_SAVE_CONFIG:
#ifdef HAVE_SHADER_MANAGER
            case RGUI_SETTINGS_SHADER_OPTIONS:
            case RGUI_SETTINGS_SHADER_PRESET:
#endif
            case RGUI_SETTINGS_CORE:
            case RGUI_SETTINGS_DISK_APPEND:
            case RGUI_SETTINGS_INPUT_OPTIONS:
            case RGUI_SETTINGS_PATH_OPTIONS:
            case RGUI_SETTINGS_OPTIONS:
            case RGUI_SETTINGS_CUSTOM_BIND_ALL:
            case RGUI_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
               strlcpy(type_str, "...", sizeof(type_str));
               break;
#ifdef HAVE_OVERLAY
            case RGUI_SETTINGS_OVERLAY_PRESET:
               strlcpy(type_str, path_basename(g_settings.input.overlay), sizeof(type_str));
               break;

            case RGUI_SETTINGS_OVERLAY_OPACITY:
            {
               snprintf(type_str, sizeof(type_str), "%.2f", g_settings.input.overlay_opacity);
               break;
            }

            case RGUI_SETTINGS_OVERLAY_SCALE:
            {
               snprintf(type_str, sizeof(type_str), "%.2f", g_settings.input.overlay_scale);
               break;
            }
#endif
            case RGUI_SETTINGS_BIND_PLAYER:
            {
               snprintf(type_str, sizeof(type_str), "#%d", port + 1);
               break;
            }
            case RGUI_SETTINGS_BIND_DEVICE:
            {
               int map = g_settings.input.joypad_map[port];
               if (map >= 0 && map < MAX_PLAYERS)
               {
                  const char *device_name = g_settings.input.device_names[map];
                  if (*device_name)
                     strlcpy(type_str, device_name, sizeof(type_str));
                  else
                     snprintf(type_str, sizeof(type_str), "N/A (port #%u)", map);
               }
               else
                  strlcpy(type_str, "Disabled", sizeof(type_str));
               break;
            }
            case RGUI_SETTINGS_BIND_DEVICE_TYPE:
            {
               const char *name;
               switch (g_settings.input.libretro_device[port])
               {
                  case RETRO_DEVICE_NONE: name = "None"; break;
                  case RETRO_DEVICE_JOYPAD: name = "Joypad"; break;
                  case RETRO_DEVICE_ANALOG: name = "Joypad w/ Analog"; break;
                  case RETRO_DEVICE_JOYPAD_MULTITAP: name = "Multitap"; break;
                  case RETRO_DEVICE_MOUSE: name = "Mouse"; break;
                  case RETRO_DEVICE_LIGHTGUN_JUSTIFIER: name = "Justifier"; break;
                  case RETRO_DEVICE_LIGHTGUN_JUSTIFIERS: name = "Justifiers"; break;
                  case RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE: name = "SuperScope"; break;
                  default: name = "Unknown"; break;
               }

               strlcpy(type_str, name, sizeof(type_str));
               break;
            }
            case RGUI_SETTINGS_BIND_DPAD_EMULATION:
               switch (g_settings.input.dpad_emulation[port])
               {
                  case ANALOG_DPAD_NONE:
                     strlcpy(type_str, "None", sizeof(type_str));
                     break;
                  case ANALOG_DPAD_LSTICK:
                     strlcpy(type_str, "Left Stick", sizeof(type_str));
                     break;
                  case ANALOG_DPAD_DUALANALOG:
                     strlcpy(type_str, "Dual Analog", sizeof(type_str));
                     break;
                  case ANALOG_DPAD_RSTICK:
                     strlcpy(type_str, "Right Stick", sizeof(type_str));
                     break;
               }
               break;
            case RGUI_SETTINGS_BIND_UP:
            case RGUI_SETTINGS_BIND_DOWN:
            case RGUI_SETTINGS_BIND_LEFT:
            case RGUI_SETTINGS_BIND_RIGHT:
            case RGUI_SETTINGS_BIND_A:
            case RGUI_SETTINGS_BIND_B:
            case RGUI_SETTINGS_BIND_X:
            case RGUI_SETTINGS_BIND_Y:
            case RGUI_SETTINGS_BIND_START:
            case RGUI_SETTINGS_BIND_SELECT:
            case RGUI_SETTINGS_BIND_L:
            case RGUI_SETTINGS_BIND_R:
            case RGUI_SETTINGS_BIND_L2:
            case RGUI_SETTINGS_BIND_R2:
            case RGUI_SETTINGS_BIND_L3:
            case RGUI_SETTINGS_BIND_R3:
            case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_PLUS:
            case RGUI_SETTINGS_BIND_ANALOG_LEFT_X_MINUS:
            case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_PLUS:
            case RGUI_SETTINGS_BIND_ANALOG_LEFT_Y_MINUS:
            case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_PLUS:
            case RGUI_SETTINGS_BIND_ANALOG_RIGHT_X_MINUS:
            case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_PLUS:
            case RGUI_SETTINGS_BIND_ANALOG_RIGHT_Y_MINUS:
            case RGUI_SETTINGS_BIND_MENU_TOGGLE:
            {
               unsigned id = type - RGUI_SETTINGS_BIND_B;
               struct platform_bind key_label;
               strlcpy(key_label.desc, "Unknown", sizeof(key_label.desc));
               key_label.joykey = g_settings.input.binds[port][id].joykey;

               if (driver.input->set_keybinds)
               {
                  driver.input->set_keybinds(&key_label, 0, 0, 0, (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL));
                  strlcpy(type_str, key_label.desc, sizeof(type_str));
               }
               else
               {
                  const struct retro_keybind *bind = &g_settings.input.binds[port][type - RGUI_SETTINGS_BIND_BEGIN];
                  input_get_bind_string(type_str, bind, sizeof(type_str));
               }
               break;
            }
            default:
               type_str[0] = 0;
               w = 0;
               break;
         }
      }

      char entry_title_buf[256];
      char type_str_buf[64];
      bool selected = i == rgui->selection_ptr;

      strlcpy(entry_title_buf, path, sizeof(entry_title_buf));
      strlcpy(type_str_buf, type_str, sizeof(type_str_buf));

      if ((type == RGUI_FILE_PLAIN || type == RGUI_FILE_DIRECTORY))
         menu_ticker_line(entry_title_buf, TERM_WIDTH - (w + 1 + 2), g_extern.frame_count / 15, path, selected);
      else
         menu_ticker_line(type_str_buf, w, g_extern.frame_count / 15, type_str, selected);

      snprintf(message, sizeof(message), "%c %-*.*s %-*s",
            selected ? '>' : ' ',
            TERM_WIDTH - (w + 1 + 2), TERM_WIDTH - (w + 1 + 2),
            entry_title_buf,
            w,
            type_str_buf);

      blit_line(rgui, x, y, message, selected);
   }

#ifdef GEKKO
   const char *message_queue;

   if (rgui->msg_force)
   {
      message_queue = msg_queue_pull(g_extern.msg_queue);
      rgui->msg_force = false;
   }
   else
      message_queue = driver.current_msg;

   render_messagebox(rgui, message_queue);
#endif
}

