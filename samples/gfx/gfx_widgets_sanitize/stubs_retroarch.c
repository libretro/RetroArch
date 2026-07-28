/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_retroarch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_retroarch.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Everything gfx/gfx_widgets.c reaches for outside itself.
 *
 * The font metrics are the load-bearing ones.  gfx_widgets.c sizes
 * every message box from font_driver_get_message_width(), and the
 * width it gets back decides how many glyphs it will try to fit and
 * therefore how much it writes.  A stub returning zero would make the
 * layout arithmetic trivially satisfiable and hide exactly the
 * overflows this is looking for, so these return a plausible
 * proportional-ish width instead: it does not have to match any real
 * font, only to grow with the string and not be zero.
 *
 * The ten gfx_widget_t instances are the individual widgets, defined
 * under gfx/widgets.  Linking those would drag in the whole frontend;
 * they are dummies here with every hook NULL, which gfx_widgets.c
 * already handles because a build without HAVE_CHEEVOS or HAVE_NETWORKING
 * omits several of them for real.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>

#include "../../../gfx/gfx_widgets.h"
#include "../../../gfx/gfx_display.h"
#include "../../../gfx/gfx_animation.h"
#include "../../../retroarch.h"
#include "../../../msg_hash.h"

/* --- singletons --- */
static char s_video_state[1 << 16];
video_driver_state_t *video_state_get_ptr(void)
{ return (video_driver_state_t*)s_video_state; }

const char *msg_hash_to_str(enum msg_hash_enums msg)
{ (void)msg; return "stub message"; }

unsigned *msg_hash_get_uint(enum msg_hash_action type)
{ static unsigned v; (void)type; return &v; }

/* --- font metrics --- */
int font_driver_get_message_width(void *font_data, const char *msg,
      size_t len, float scale)
{
   (void)font_data;
   if (!msg)
      return 0;
   if (len == 0)
      len = strlen(msg);
   /* Deliberately non-zero and proportional: a zero here would make
    * every "does this fit" test in gfx_widgets.c succeed and the
    * layout paths would never be stressed. */
   return (int)(len * 10 * (scale > 0.0f ? scale : 1.0f));
}

int font_driver_get_line_height(font_data_t *font, float scale)
{ (void)font; return (int)(20 * (scale > 0.0f ? scale : 1.0f)); }
int font_driver_get_line_ascender(font_data_t *font, float scale)
{ (void)font; return (int)(15 * (scale > 0.0f ? scale : 1.0f)); }
int font_driver_get_line_descender(font_data_t *font, float scale)
{ (void)font; return (int)(5 * (scale > 0.0f ? scale : 1.0f)); }
int font_driver_get_line_centre_offset(font_data_t *font, float scale)
{ (void)font; return (int)(8 * (scale > 0.0f ? scale : 1.0f)); }

void font_driver_bind_block(void *font_data, void *block)
{ (void)font_data; (void)block; }
void font_driver_free(font_data_t *font) { (void)font; }

/* --- display: signatures copied from gfx/gfx_display.h --- */
void gfx_display_draw_quad(gfx_display_t *p_disp, void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned w, unsigned h,
      unsigned width, unsigned height, float *color, uintptr_t *texture)
{ (void)p_disp; (void)data; (void)video_width; (void)video_height;
  (void)x; (void)y; (void)w; (void)h; (void)width; (void)height;
  (void)color; (void)texture; }

void gfx_display_draw_text(const font_data_t *font, const char *text,
      float x, float y, int width, int height, uint32_t color,
      enum text_alignment text_align, float scale,
      bool shadows_enable, float shadow_offset, bool draw_outside)
{ (void)font; (void)text; (void)x; (void)y; (void)width; (void)height;
  (void)color; (void)text_align; (void)scale; (void)shadows_enable;
  (void)shadow_offset; (void)draw_outside; }

void gfx_display_rotate_z(gfx_display_t *p_disp, math_matrix_4x4 *matrix,
      float cosine, float sine, void *data)
{ (void)p_disp; (void)matrix; (void)cosine; (void)sine; (void)data; }

void gfx_display_scissor_begin(gfx_display_t *p_disp, void *userdata,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{ (void)p_disp; (void)userdata; (void)video_width; (void)video_height;
  (void)x; (void)y; (void)width; (void)height; }

enum texture_filter_type gfx_display_texture_filter(void)
{ return TEXTURE_FILTER_LINEAR; }

/* gfx_widgets_init() bails immediately on a false here, so the whole
 * sweep would report a clean pass having initialised nothing.  The
 * harness asserts pushes > 0 to catch that, and this returns true so
 * it does not have to. */
bool gfx_display_init_first_driver(gfx_display_t *p_disp,
      bool video_is_threaded)
{ (void)p_disp; (void)video_is_threaded; return true; }

float gfx_display_get_dpi_scale(gfx_display_t *p_disp, void *settings_data,
      unsigned width, unsigned height, bool fullscreen, bool is_widget)
{ (void)p_disp; (void)settings_data; (void)width; (void)height;
  (void)fullscreen; (void)is_widget; return 1.0f; }

bool gfx_display_reset_textures_list_buffer(uintptr_t *item,
      enum texture_filter_type filter_type, void *buffer,
      unsigned buffer_len, enum image_type_enum image_type,
      unsigned *width, unsigned *height)
{
   (void)item; (void)filter_type; (void)buffer; (void)buffer_len;
   (void)image_type;
   if (width)
      *width = 0;
   if (height)
      *height = 0;
   return false;
}

bool gfx_display_load_icon(const char *fullpath, bool supports_rgba,
      uintptr_t *target_texture, uint64_t generation,
      uint64_t *generation_ptr)
{ (void)fullpath; (void)supports_rgba; (void)target_texture;
  (void)generation; (void)generation_ptr; return false; }

font_data_t *gfx_display_font_file(gfx_display_t *p_disp, char *fontpath,
      float font_size, bool is_threaded)
{ (void)p_disp; (void)fontpath; (void)font_size; (void)is_threaded;
  return NULL; }

/* --- animation --- */
bool gfx_animation_push(gfx_animation_ctx_entry_t *entry)
{ (void)entry; return true; }
bool gfx_animation_kill_by_tag(uintptr_t *tag) { (void)tag; return true; }
void gfx_animation_timer_start(float *timer,
      gfx_timer_ctx_entry_t *timer_entry)
{ (void)timer_entry; if (timer) *timer = 0.0f; }

/* --- video driver --- */
uint32_t video_driver_get_disp_flags(void) { return 0; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
void video_driver_monitor_reset(void) { }
bool video_driver_texture_unload(uintptr_t *id) { (void)id; return true; }
void video_coord_array_free(video_coord_array_t *ca) { (void)ca; }

/* --- the individual widgets --- */
#define STUB_WIDGET(name) const gfx_widget_t name = { 0 }

STUB_WIDGET(gfx_widget_screenshot);
STUB_WIDGET(gfx_widget_volume);
STUB_WIDGET(gfx_widget_generic_message);
STUB_WIDGET(gfx_widget_libretro_message);
STUB_WIDGET(gfx_widget_progress_message);
STUB_WIDGET(gfx_widget_load_content_animation);
STUB_WIDGET(gfx_widget_achievement_popup);
STUB_WIDGET(gfx_widget_leaderboard_display);
STUB_WIDGET(gfx_widget_netplay_chat);
STUB_WIDGET(gfx_widget_netplay_ping);
