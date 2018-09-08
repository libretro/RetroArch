/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2017 - Brad Parker
 *  Copyright (C) 2018-2018 - M4xw
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

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <string/stdstring.h>
#include <lists/string_list.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <encodings/utf.h>
#include <file/file_path.h>
#include <retro_inline.h>
#include <string/stdstring.h>
#include <encodings/utf.h>

#include "../../retroarch.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../frontend/frontend_driver.h"

#include "menu_generic.h"

#include "../menu_driver.h"
#include "../menu_animation.h"

#include "../widgets/menu_input_dialog.h"

#include "../../configuration.h"
#include "../../gfx/drivers_font_renderer/bitmap.h"

#define FONT_WIDTH 5
#define FONT_HEIGHT 10
#define FONT_WIDTH_STRIDE (FONT_WIDTH + 1)
#define FONT_HEIGHT_STRIDE (FONT_HEIGHT + 5)
#define nxrgui_TERM_START_X(width) (width / 21)
#define nxrgui_TERM_START_Y(height) (height / 9)
#define nxrgui_TERM_WIDTH(width) (((width - nxrgui_TERM_START_X(width) - nxrgui_TERM_START_X(width)) / (FONT_WIDTH_STRIDE)))
#define nxrgui_TERM_HEIGHT(width, height) (((height - nxrgui_TERM_START_Y(height) - nxrgui_TERM_START_X(width)) / (FONT_HEIGHT_STRIDE)) - 1)

uint32_t *nx_backgroundImage = NULL;

// Extern Prototypes
bool rpng_load_image_argb(const char *path, uint32_t **data, unsigned *width, unsigned *height);
void argb_to_rgba8(uint32_t *buff, uint32_t height, uint32_t width);

// switch_gfx.c protypes, we really need a header
void gfx_slow_swizzling_blit(uint32_t *buffer, uint32_t *image, int w, int h, int tx, int ty, bool blend);

// Temporary Overlay Buffer
uint32_t *tmp_overlay = 0;

typedef struct
{
    bool bg_modified;
    bool force_redraw;
    bool mouse_show;
    unsigned last_width;
    unsigned last_height;
    unsigned frame_count;
    bool bg_thickness;
    bool border_thickness;
    float scroll_y;
    char *msgbox;

    font_data_t *font;
} nxrgui_t;

static uint16_t *nxrgui_framebuf_data = NULL;

#if defined(GEKKO) || defined(PSP)
#define HOVER_COLOR(settings) ((3 << 0) | (10 << 4) | (3 << 8) | (7 << 12))
#define NORMAL_COLOR(settings) 0x7FFF
#define TITLE_COLOR(settings) HOVER_COLOR(settings)
#else
#define HOVER_COLOR(settings) (argb32_to_rgba4444(settings->uints.menu_entry_hover_color))
#define NORMAL_COLOR(settings) (argb32_to_rgba4444(settings->uints.menu_entry_normal_color))
#define TITLE_COLOR(settings) (argb32_to_rgba4444(settings->uints.menu_title_color))

static uint16_t argb32_to_rgba4444(uint32_t col)
{
    unsigned a = ((col >> 24) & 0xff) >> 4;
    unsigned r = ((col >> 16) & 0xff) >> 4;
    unsigned g = ((col >> 8) & 0xff) >> 4;
    unsigned b = ((col & 0xff)) >> 4;
    return (r << 12) | (g << 8) | (b << 4) | a;
}
#endif

static uint16_t nxrgui_gray_filler(nxrgui_t *nxrgui, unsigned x, unsigned y)
{
    unsigned shft = (nxrgui->bg_thickness ? 1 : 0);
    unsigned col = (((x >> shft) + (y >> shft)) & 1) + 1;
#if defined(GEKKO) || defined(PSP)
    return (6 << 12) | (col << 8) | (col << 4) | (col << 0);
#elif defined(SWITCH)
    return (((31 * (54)) / 255) << 11) |
           (((63 * (54)) / 255) << 5) |
           ((31 * (54)) / 255);
#else
    return (col << 13) | (col << 9) | (col << 5) | (12 << 0);
#endif
}

static uint16_t nxrgui_green_filler(nxrgui_t *nxrgui, unsigned x, unsigned y)
{
    unsigned shft = (nxrgui->border_thickness ? 1 : 0);
    unsigned col = (((x >> shft) + (y >> shft)) & 1) + 1;
#if defined(GEKKO) || defined(PSP)
    return (6 << 12) | (col << 8) | (col << 5) | (col << 0);
#elif defined(SWITCH)
    return (((31 * (54)) / 255) << 11) |
           (((63 * (109)) / 255) << 5) |
           ((31 * (54)) / 255);
#else
    return (col << 13) | (col << 10) | (col << 5) | (12 << 0);
#endif
}

static void nxrgui_color_rect(
    uint16_t *data,
    size_t pitch,
    unsigned fb_width, unsigned fb_height,
    unsigned x, unsigned y,
    unsigned width, unsigned height,
    uint16_t color)
{
    unsigned i, j;

    for (j = y; j < y + height; j++)
        for (i = x; i < x + width; i++)
            if (i < fb_width && j < fb_height)
                data[j * (pitch >> 1) + i] = color;
}

static void blit_line(int x, int y,
                      const char *message, uint16_t color)
{
    size_t pitch = menu_display_get_framebuffer_pitch();
    const uint8_t *font_fb = menu_display_get_font_framebuffer();

    if (font_fb)
    {
        while (!string_is_empty(message))
        {
            unsigned i, j;
            char symbol = *message++;

            for (j = 0; j < FONT_HEIGHT; j++)
            {
                for (i = 0; i < FONT_WIDTH; i++)
                {
                    uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
                    int offset = (i + j * FONT_WIDTH) >> 3;
                    bool col = (font_fb[FONT_OFFSET(symbol) + offset] & rem);

                    if (col)
                        nxrgui_framebuf_data[(y + j) * (pitch >> 1) + (x + i)] = color;
                }
            }

            x += FONT_WIDTH_STRIDE;
        }
    }
}

#if 0
static void nxrgui_copy_glyph(uint8_t *glyph, const uint8_t *buf)
{
   int x, y;

   if (!glyph)
      return;

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint32_t col    =
            ((uint32_t)buf[3 * (-y * 256 + x) + 0] << 0) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 1] << 8) |
            ((uint32_t)buf[3 * (-y * 256 + x) + 2] << 16);

         uint8_t rem     = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;

         if (col != 0xff)
            glyph[offset] |= rem;
      }
   }
}

static bool init_font(menu_handle_t *menu, const uint8_t *font_bmp_buf)
{
   unsigned i;
   bool fb_font_inited  = true;
   uint8_t        *font = (uint8_t *)calloc(1, FONT_OFFSET(256));

   if (!font)
      return false;

   menu_display_set_font_data_init(fb_font_inited);

   for (i = 0; i < 256; i++)
   {
      unsigned y = i / 16;
      unsigned x = i % 16;
      nxrgui_copy_glyph(&font[FONT_OFFSET(i)],
            font_bmp_buf + 54 + 3 * (256 * (255 - 16 * y) + 16 * x));
   }

   menu_display_set_font_framebuffer(font);

   return true;
}
#endif

static bool nxrguidisp_init_font(menu_handle_t *menu)
{
#if 0
   const uint8_t *font_bmp_buf = NULL;
#endif
    const uint8_t *font_bin_buf = bitmap_bin;

    if (!menu)
        return false;

#if 0
   if (font_bmp_buf)
      return init_font(menu, font_bmp_buf);
#endif

    menu_display_set_font_framebuffer(font_bin_buf);

    return true;
}

static void nxrgui_render_background(nxrgui_t *nxrgui)
{
    size_t pitch_in_pixels, size;
    size_t fb_pitch;
    unsigned fb_width, fb_height;
    uint16_t *src = NULL;
    uint16_t *dst = NULL;

    menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);
    pitch_in_pixels = fb_pitch >> 1;
    size = fb_pitch * 4;
    src = nxrgui_framebuf_data + pitch_in_pixels * fb_height;
    dst = nxrgui_framebuf_data;

    while (dst < src)
    {
        memset(dst, 0, size);
        dst += pitch_in_pixels * 4;
    }

    if (nxrgui_framebuf_data)
    {
        settings_t *settings = config_get_ptr();
    }
}

static void nxrgui_set_message(void *data, const char *message)
{
    nxrgui_t *nxrgui = (nxrgui_t *)data;

    if (!nxrgui || !message || !*message)
        return;

    if (!string_is_empty(nxrgui->msgbox))
        free(nxrgui->msgbox);
    nxrgui->msgbox = strdup(message);
    nxrgui->force_redraw = true;
}

static void nxrgui_render_messagebox(nxrgui_t *nxrgui, const char *message)
{
    int x, y;
    uint16_t color;
    size_t i, fb_pitch;
    unsigned fb_width, fb_height;
    unsigned width, glyphs_width, height;
    struct string_list *list = NULL;
    settings_t *settings = config_get_ptr();

    (void)settings;

    if (!message || !*message)
        return;

    list = string_split(message, "\n");
    if (!list)
        return;
    if (list->elems == 0)
        goto end;

    width = 0;
    glyphs_width = 0;

    menu_display_get_fb_size(&fb_width, &fb_height,
                             &fb_pitch);

    for (i = 0; i < list->size; i++)
    {
        unsigned line_width;
        char *msg = list->elems[i].data;
        unsigned msglen = (unsigned)utf8len(msg);

        if (msglen > nxrgui_TERM_WIDTH(fb_width))
        {
            msg[nxrgui_TERM_WIDTH(fb_width) - 2] = '.';
            msg[nxrgui_TERM_WIDTH(fb_width) - 1] = '.';
            msg[nxrgui_TERM_WIDTH(fb_width) - 0] = '.';
            msg[nxrgui_TERM_WIDTH(fb_width) + 1] = '\0';
            msglen = nxrgui_TERM_WIDTH(fb_width);
        }

        line_width = msglen * FONT_WIDTH_STRIDE - 1 + 6 + 10;
        width = MAX(width, line_width);
        glyphs_width = MAX(glyphs_width, msglen);
    }

    height = (unsigned)(FONT_HEIGHT_STRIDE * list->size + 6 + 10);
    x = (fb_width - width) / 2;
    y = (fb_height - height) / 2;

    color = NORMAL_COLOR(settings);

    for (i = 0; i < list->size; i++)
    {
        const char *msg = list->elems[i].data;
        int offset_x = (int)(FONT_WIDTH_STRIDE * (glyphs_width - utf8len(msg)) / 2);
        int offset_y = (int)(FONT_HEIGHT_STRIDE * i);

        if (nxrgui_framebuf_data)
            blit_line(x + 8 + offset_x, y + 8 + offset_y, msg, color);
    }

end:
    string_list_free(list);
}

static void nxrgui_blit_cursor(void)
{
    size_t fb_pitch;
    unsigned fb_width, fb_height;
    int16_t x = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
    int16_t y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

    menu_display_get_fb_size(&fb_width, &fb_height,
                             &fb_pitch);

    if (nxrgui_framebuf_data)
    {
        nxrgui_color_rect(nxrgui_framebuf_data, fb_pitch, fb_width, fb_height, x, y - 5, 1, 11, 0xFFFF);
        nxrgui_color_rect(nxrgui_framebuf_data, fb_pitch, fb_width, fb_height, x - 5, y, 11, 1, 0xFFFF);
    }
}

static void nxrgui_frame(void *data, video_frame_info_t *video_info)
{
    nxrgui_t *nxrgui = (nxrgui_t *)data;
    settings_t *settings = config_get_ptr();

    if ((settings->bools.menu_rgui_background_filler_thickness_enable != nxrgui->bg_thickness) ||
        (settings->bools.menu_rgui_border_filler_thickness_enable != nxrgui->border_thickness))
        nxrgui->bg_modified = true;

    unsigned fbWidth;
    unsigned fbHeight;

    uint32_t *fb = (uint32_t *)gfxGetFramebuffer(&fbWidth, &fbHeight);

    nxrgui->bg_thickness = settings->bools.menu_rgui_background_filler_thickness_enable;
    nxrgui->border_thickness = settings->bools.menu_rgui_border_filler_thickness_enable;

    //menu_display_draw_text(nxrgui->font, "Hello from freetype2!", 23, 23, fbWidth, fbHeight, 0xFFFFFFFF, TEXT_ALIGN_LEFT, 1.0f, false, 0);

    nxrgui->frame_count++;
}

static void nxrgui_render(void *data, bool is_idle)
{
    menu_animation_ctx_ticker_t ticker;
    unsigned x, y;
    uint16_t hover_color, normal_color;
    size_t i, end, fb_pitch, old_start;
    unsigned fb_width, fb_height;
    int bottom;
    char title[255];
    char title_buf[255];
    char title_msg[64];
    char msg[255];
    size_t entries_end = 0;
    bool msg_force = false;
    settings_t *settings = config_get_ptr();
    nxrgui_t *nxrgui = (nxrgui_t *)data;
    uint64_t frame_count = nxrgui->frame_count;

    msg[0] = title[0] = title_buf[0] = title_msg[0] = '\0';

    if (!nxrgui->force_redraw)
    {
        msg_force = menu_display_get_msg_force();

        if (menu_entries_ctl(MENU_ENTRIES_CTL_NEEDS_REFRESH, NULL) && menu_driver_is_alive() && !msg_force)
            return;

        if (is_idle || !menu_display_get_update_pending())
            return;
    }

    menu_display_get_fb_size(&fb_width, &fb_height, &fb_pitch);

    /* if the framebuffer changed size, recache the background */
    if (nxrgui->bg_modified || nxrgui->last_width != fb_width || nxrgui->last_height != fb_height)
    {
        nxrgui->last_height = fb_height;
    }

    if (nxrgui->bg_modified)
        nxrgui->bg_modified = false;

    menu_display_set_framebuffer_dirty_flag();
    menu_animation_ctl(MENU_ANIMATION_CTL_CLEAR_ACTIVE, NULL);

    nxrgui->force_redraw = false;

    if (settings->bools.menu_pointer_enable)
    {
        unsigned new_val;

        menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

        new_val = (unsigned)(menu_input_pointer_state(MENU_POINTER_Y_AXIS) / (11 - 2 + old_start));

        menu_input_ctl(MENU_INPUT_CTL_POINTER_PTR, &new_val);

        if (menu_input_ctl(MENU_INPUT_CTL_IS_POINTER_DRAGGED, NULL))
        {
            size_t start;
            int16_t delta_y = menu_input_pointer_state(MENU_POINTER_DELTA_Y_AXIS);
            nxrgui->scroll_y += delta_y;

            start = -nxrgui->scroll_y / 11 + 2;

            menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

            if (nxrgui->scroll_y > 0)
                nxrgui->scroll_y = 0;
        }
    }

    if (settings->bools.menu_mouse_enable)
    {
        unsigned new_mouse_ptr;
        int16_t mouse_y = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);

        menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

        new_mouse_ptr = (unsigned)(mouse_y / 11 - 2 + old_start);

        menu_input_ctl(MENU_INPUT_CTL_MOUSE_PTR, &new_mouse_ptr);
    }

    /* Do not scroll if all items are visible. */
    if (menu_entries_get_size() <= nxrgui_TERM_HEIGHT(fb_width, fb_height))
    {
        size_t start = 0;
        menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
    }

    bottom = (int)(menu_entries_get_size() - nxrgui_TERM_HEIGHT(fb_width, fb_height));
    menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

    if (old_start > (unsigned)bottom)
        menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &bottom);

    menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &old_start);

    entries_end = menu_entries_get_size();

    end = ((old_start + nxrgui_TERM_HEIGHT(fb_width, fb_height)) <= (entries_end)) ? old_start + nxrgui_TERM_HEIGHT(fb_width, fb_height) : entries_end;

    nxrgui_render_background(nxrgui);

    menu_entries_get_title(title, sizeof(title));

    ticker.s = title_buf;
    ticker.len = nxrgui_TERM_WIDTH(fb_width) - 10;
    ticker.idx = frame_count / nxrgui_TERM_START_X(fb_width);
    ticker.str = title;
    ticker.selected = true;

    menu_animation_ticker(&ticker);

    hover_color = HOVER_COLOR(settings);
    normal_color = NORMAL_COLOR(settings);

    if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
    {
        char back_buf[32];
        char back_msg[32];

        back_buf[0] = back_msg[0] = '\0';

        strlcpy(back_buf, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK), sizeof(back_buf));
        string_to_upper(back_buf);
        if (nxrgui_framebuf_data)
            blit_line(
                nxrgui_TERM_START_X(fb_width),
                nxrgui_TERM_START_X(fb_width),
                back_msg,
                TITLE_COLOR(settings));
    }

    string_to_upper(title_buf);

    if (nxrgui_framebuf_data)
        blit_line(
            (int)(nxrgui_TERM_START_X(fb_width) + (nxrgui_TERM_WIDTH(fb_width) - utf8len(title_buf)) * FONT_WIDTH_STRIDE / 2),
            nxrgui_TERM_START_X(fb_width),
            title_buf, TITLE_COLOR(settings));

    if (settings->bools.menu_core_enable &&
        menu_entries_get_core_title(title_msg, sizeof(title_msg)) == 0)
    {
        if (nxrgui_framebuf_data)
            blit_line(
                nxrgui_TERM_START_X(fb_width),
                (nxrgui_TERM_HEIGHT(fb_width, fb_height) * FONT_HEIGHT_STRIDE) +
                    nxrgui_TERM_START_Y(fb_height) + 2,
                title_msg, hover_color);
    }

    if (settings->bools.menu_timedate_enable)
    {
        menu_display_ctx_datetime_t datetime;
        char timedate[255];

        timedate[0] = '\0';

        datetime.s = timedate;
        datetime.len = sizeof(timedate);
        datetime.time_mode = 3;

        menu_display_timedate(&datetime);

        if (nxrgui_framebuf_data)
            blit_line(
                nxrgui_TERM_WIDTH(fb_width) * FONT_WIDTH_STRIDE - nxrgui_TERM_START_X(fb_width),
                (nxrgui_TERM_HEIGHT(fb_width, fb_height) * FONT_HEIGHT_STRIDE) +
                    nxrgui_TERM_START_Y(fb_height) + 2,
                timedate, hover_color);
    }

    x = nxrgui_TERM_START_X(fb_width);
    y = nxrgui_TERM_START_Y(fb_height);

    menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

    for (; i < end; i++, y += FONT_HEIGHT_STRIDE)
    {
        menu_entry_t entry;
        menu_animation_ctx_ticker_t ticker;
        char entry_value[255];
        char message[255];
        char entry_title_buf[255];
        char type_str_buf[255];
        char *entry_path = NULL;
        unsigned entry_spacing = 0;
        size_t entry_title_buf_utf8len = 0;
        size_t entry_title_buf_len = 0;
        bool entry_selected = menu_entry_is_currently_selected((unsigned)i);
        size_t selection = menu_navigation_get_selection();

        if (i > (selection + 100))
            continue;

        entry_value[0] = '\0';
        message[0] = '\0';
        entry_title_buf[0] = '\0';
        type_str_buf[0] = '\0';

        menu_entry_init(&entry);
        menu_entry_get(&entry, 0, (unsigned)i, NULL, true);

        entry_spacing = menu_entry_get_spacing(&entry);
        menu_entry_get_value(&entry, entry_value, sizeof(entry_value));
        entry_path = menu_entry_get_rich_label(&entry);

        ticker.s = entry_title_buf;
        ticker.len = nxrgui_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2);
        ticker.idx = frame_count / nxrgui_TERM_START_X(fb_width);
        ticker.str = entry_path;
        ticker.selected = entry_selected;

        menu_animation_ticker(&ticker);

        ticker.s = type_str_buf;
        ticker.len = entry_spacing;
        ticker.str = entry_value;

        menu_animation_ticker(&ticker);

        entry_title_buf_utf8len = utf8len(entry_title_buf);
        entry_title_buf_len = strlen(entry_title_buf);

        snprintf(message, sizeof(message), "%c %-*.*s %-*s",
                 entry_selected ? '>' : ' ',
                 (int)(nxrgui_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2) - entry_title_buf_utf8len + entry_title_buf_len),
                 (int)(nxrgui_TERM_WIDTH(fb_width) - (entry_spacing + 1 + 2) - entry_title_buf_utf8len + entry_title_buf_len),
                 entry_title_buf,
                 entry_spacing,
                 type_str_buf);

        if (nxrgui_framebuf_data)
            blit_line(x, y, message,
                      entry_selected ? hover_color : normal_color);

        menu_entry_free(&entry);
        if (!string_is_empty(entry_path))
            free(entry_path);
    }

    if (menu_input_dialog_get_display_kb())
    {
        const char *str = menu_input_dialog_get_buffer();
        const char *label = menu_input_dialog_get_label_buffer();

        snprintf(msg, sizeof(msg), "%s\n%s", label, str);
        nxrgui_render_messagebox(nxrgui, msg);
    }

    if (!string_is_empty(nxrgui->msgbox))
    {
        nxrgui_render_messagebox(nxrgui, nxrgui->msgbox);
        free(nxrgui->msgbox);
        nxrgui->msgbox = NULL;
        nxrgui->force_redraw = true;
    }

    if (nxrgui->mouse_show)
    {
        settings_t *settings = config_get_ptr();
        bool cursor_visible = settings->bools.video_fullscreen ||
                              !video_driver_has_windowed();

        if (settings->bools.menu_mouse_enable && cursor_visible)
            nxrgui_blit_cursor();
    }
}

static void nxrgui_framebuffer_free(void)
{
    if (nxrgui_framebuf_data)
        free(nxrgui_framebuf_data);
    nxrgui_framebuf_data = NULL;
}

static bool nxrgui_load_menu_bg(const char *path, uint32_t *width, uint32_t *height)
{
    // Default
    rpng_load_image_argb(path, &nx_backgroundImage, width, height);
    if (nx_backgroundImage)
    {
        // Convert
        argb_to_rgba8(nx_backgroundImage, *height, *width);
        return true;
    }
    else
    {
        return false;
    }
}

static void *nxrgui_init(void **userdata, bool video_is_threaded)
{
    size_t fb_pitch, start;
    unsigned fb_width, fb_height, new_font_height;
    nxrgui_t *nxrgui = NULL;
    bool ret = false;
    settings_t *settings = config_get_ptr();
    menu_handle_t *menu = (menu_handle_t *)calloc(1, sizeof(*menu));

    if (!menu)
        return NULL;

    nxrgui = (nxrgui_t *)calloc(1, sizeof(nxrgui_t));

    if (!nxrgui)
        goto error;

    *userdata = nxrgui;

    if (!menu_display_init_first_driver(video_is_threaded))
        goto error;

    /* 4 extra lines to cache  the checked background */
    nxrgui_framebuf_data = (uint16_t *)calloc(400 * (240 + 4), sizeof(uint16_t));

    if (!nxrgui_framebuf_data)
        goto error;

    fb_width = 320;
    fb_height = 240;

    fb_pitch = fb_width * sizeof(uint16_t);
    new_font_height = FONT_HEIGHT_STRIDE * 2;

    menu_display_set_width(fb_width);
    menu_display_set_height(fb_height);
    menu_display_set_header_height(new_font_height);
    menu_display_set_framebuffer_pitch(fb_pitch);

    start = 0;
    menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);

    ret = nxrguidisp_init_font(menu);

    if (!ret)
        goto error;

    nxrgui->bg_thickness = settings->bools.menu_rgui_background_filler_thickness_enable;
    nxrgui->border_thickness = settings->bools.menu_rgui_border_filler_thickness_enable;
    nxrgui->bg_modified = true;

    nxrgui->last_width = fb_width;
    nxrgui->last_height = fb_height;

    // Load PNG Data
    printf("[UI] Loading..\n");

    uint32_t width, height;
    width = height = 0;

    // Core Info
    rarch_system_info_t *sys_info = runloop_get_system_info();
    const char *core_name = NULL;
    bool menuFound = false;
    if (sys_info)
    {
        core_name = sys_info->info.library_name;

        char *full_core_menu_path = (char *)malloc(PATH_MAX);
        snprintf(full_core_menu_path, PATH_MAX, "/retroarch/nxrgui/menu/%s.png", core_name);

        // Default
        if (nxrgui_load_menu_bg((const char *)full_core_menu_path, &width, &height))
        {
            printf("[NxRGUI] Menu loaded\n");
            menuFound = true;
        }

        free(full_core_menu_path);
    }

    if (!menuFound) // Fallback
    {
        // Default
        if (nxrgui_load_menu_bg("/retroarch/nxrgui/menu/RetroArch.png", &width, &height))
        {
            menuFound = true;
            printf("[NxRGUI] Menu loaded\n");
        }
        else
        {
            // Black bg Fallback
            uint32_t buffSize = 1280 * 720 * sizeof(uint32_t);
            nx_backgroundImage = malloc(buffSize);
            memset(nx_backgroundImage, 0, buffSize);
        }
    }

    // Temp overlay hack
    // TODO: KILL IT WITH FIRE
    // At least i try to give it some options, since loading the cfg's doesnt work with threading
    if (sys_info)
    {
        printf("[Overlay] Load Overlay for Core: \"%s\"\n", core_name);
        char *full_overlaypath = (char *)malloc(PATH_MAX);
        snprintf(full_overlaypath, PATH_MAX, "/retroarch/overlays/%s.png", core_name);
        rpng_load_image_argb(full_overlaypath, &tmp_overlay, &width, &height);
        if (tmp_overlay)
        {
            // Convert
            argb_to_rgba8(tmp_overlay, height, width);
            printf("[Overlay] Overlay loaded\n");
        }
        else
        {
            printf("[Overlay] Overlay failed to load\n");
            tmp_overlay = NULL;
        }
        free(full_overlaypath);
    }
    else
    {
        tmp_overlay = NULL;
    }

    return menu;

error:
    nxrgui_framebuffer_free();
    if (menu)
        free(menu);
    return NULL;
}

static void nxrgui_free(void *data)
{
    const uint8_t *font_fb;
    bool fb_font_inited = false;

    fb_font_inited = menu_display_get_font_data_init();
    font_fb = menu_display_get_font_framebuffer();

    if (fb_font_inited)
        free((void *)font_fb);

    fb_font_inited = false;
    menu_display_set_font_data_init(fb_font_inited);

    if (nx_backgroundImage)
    {
        free(nx_backgroundImage);
        nx_backgroundImage = NULL;
    }

    // Temp Overlay hack
    if (tmp_overlay)
    {
        free(tmp_overlay);
        tmp_overlay = NULL;
    }
}

static void nxrgui_set_texture(void)
{
    size_t fb_pitch;
    unsigned fb_width, fb_height;

    if (!menu_display_get_framebuffer_dirty_flag())
        return;

    menu_display_get_fb_size(&fb_width, &fb_height,
                             &fb_pitch);

    menu_display_unset_framebuffer_dirty_flag();

    video_driver_set_texture_frame(nxrgui_framebuf_data,
                                   false, fb_width, fb_height, 1.0f);
}

static void nxrgui_navigation_clear(void *data, bool pending_push)
{
    size_t start;
    nxrgui_t *nxrgui = (nxrgui_t *)data;
    if (!nxrgui)
        return;

    start = 0;
    menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
    nxrgui->scroll_y = 0;
}

static void nxrgui_navigation_set(void *data, bool scroll)
{
    size_t start, fb_pitch;
    unsigned fb_width, fb_height;
    bool do_set_start = false;
    size_t end = menu_entries_get_size();
    size_t selection = menu_navigation_get_selection();

    if (!scroll)
        return;

    menu_display_get_fb_size(&fb_width, &fb_height,
                             &fb_pitch);

    if (selection < nxrgui_TERM_HEIGHT(fb_width, fb_height) / 2)
    {
        start = 0;
        do_set_start = true;
    }
    else if (selection >= (nxrgui_TERM_HEIGHT(fb_width, fb_height) / 2) && selection < (end - nxrgui_TERM_HEIGHT(fb_width, fb_height) / 2))
    {
        start = selection - nxrgui_TERM_HEIGHT(fb_width, fb_height) / 2;
        do_set_start = true;
    }
    else if (selection >= (end - nxrgui_TERM_HEIGHT(fb_width, fb_height) / 2))
    {
        start = end - nxrgui_TERM_HEIGHT(fb_width, fb_height);
        do_set_start = true;
    }

    if (do_set_start)
        menu_entries_ctl(MENU_ENTRIES_CTL_SET_START, &start);
}

static void nxrgui_navigation_set_last(void *data)
{
    nxrgui_navigation_set(data, true);
}

static void nxrgui_navigation_descend_alphabet(void *data, size_t *unused)
{
    nxrgui_navigation_set(data, true);
}

static void nxrgui_navigation_ascend_alphabet(void *data, size_t *unused)
{
    nxrgui_navigation_set(data, true);
}

static void nxrgui_populate_entries(void *data,
                                    const char *path,
                                    const char *label, unsigned k)
{
    nxrgui_navigation_set(data, true);
}

static int nxrgui_environ(enum menu_environ_cb type,
                          void *data, void *userdata)
{
    nxrgui_t *nxrgui = (nxrgui_t *)userdata;

    switch (type)
    {
    case MENU_ENVIRON_ENABLE_MOUSE_CURSOR:
        if (!nxrgui)
            return -1;
        nxrgui->mouse_show = true;
        menu_display_set_framebuffer_dirty_flag();
        break;
    case MENU_ENVIRON_DISABLE_MOUSE_CURSOR:
        if (!nxrgui)
            return -1;
        nxrgui->mouse_show = false;
        menu_display_unset_framebuffer_dirty_flag();
        break;
    case 0:
    default:
        break;
    }

    return -1;
}

static int nxrgui_pointer_tap(void *data,
                              unsigned x, unsigned y,
                              unsigned ptr, menu_file_list_cbs_t *cbs,
                              menu_entry_t *entry, unsigned action)
{
    unsigned header_height = menu_display_get_header_height();

    if (y < header_height)
    {
        size_t selection = menu_navigation_get_selection();
        return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_CANCEL);
    }
    else if (ptr <= (menu_entries_get_size() - 1))
    {
        size_t selection = menu_navigation_get_selection();

        if (ptr == selection && cbs && cbs->action_select)
            return menu_entry_action(entry, (unsigned)selection, MENU_ACTION_SELECT);

        menu_navigation_set_selection(ptr);
        menu_driver_navigation_set(false);
    }

    return 0;
}

static void nxrgui_context_reset(void *data, bool is_threaded)
{
    nxrgui_t *nxrgui = (nxrgui_t *)data;

    if (!nxrgui)
        return;

    nxrgui->font = menu_display_font(APPLICATION_SPECIAL_DIRECTORY_ASSETS_NXRGUI_FONT, 24, is_threaded);

    if (!nxrgui->font)
        printf("[NxRGUI] Unable to load font\n");
}

static void nxrgui_context_destroy(void *data)
{
    nxrgui_t *nxrgui = (nxrgui_t *)data;

    if (!nxrgui)
        return;

    if (nxrgui->font)
        menu_display_font_free(nxrgui->font);
}

menu_ctx_driver_t menu_ctx_nxrgui = {
    nxrgui_set_texture,
    nxrgui_set_message,
    generic_menu_iterate,
    nxrgui_render,
    nxrgui_frame,
    nxrgui_init,
    nxrgui_free,
    nxrgui_context_reset,
    nxrgui_context_destroy,
    nxrgui_populate_entries,
    NULL,
    nxrgui_navigation_clear,
    NULL,
    NULL,
    nxrgui_navigation_set,
    nxrgui_navigation_set_last,
    nxrgui_navigation_descend_alphabet,
    nxrgui_navigation_ascend_alphabet,
    generic_menu_init_list,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "nxrgui",
    nxrgui_environ,
    nxrgui_pointer_tap,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL};
