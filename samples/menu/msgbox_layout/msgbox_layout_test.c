/* Message box layout.
 *
 * Links the shipping RetroArch objects with only main() replaced, the
 * way menu_icon_ownership does, and draws a confirm box in a menu
 * driver against the null video driver. The display driver and its
 * font backend are the harness's: a glyph is half the font's size wide
 * (a wide one its full size), and every piece of text drawn is
 * recorded with where it went and how wide it is.
 *
 * The box's text is made of numbered words, w001 w002 ..., which no
 * other text in the menu has, so its lines can be picked out of a
 * frame. Each lane checks that:
 *
 *  - the box's lines hold its words in order, none lost or doubled,
 *    and Cancel and OK are drawn;
 *  - every line lies on the screen, the lines a line height apart and
 *    above the buttons, and the buttons on the screen;
 *  - the next frame draws exactly the same text in the same places:
 *    the driver keeps the wrapped text between frames, and a frame
 *    drawn from what it kept must not differ from one that wrapped;
 *  - a frame at another size draws the box as one laid out afresh at
 *    that size, and a frame back at the first size the first frame.
 *
 *   short       a box that fits the screen
 *   tall        as much text as a box holds, on a small screen: where
 *               the box is taller than the screen (Ozone and MaterialUI
 *               here) the text stops above the buttons, cut short
 *               with "..."
 *
 *   firmware    the missing firmware dialog: one underline under the
 *               file's path, not its number, as wide as the path, in
 *               the frames drawn from what was measured before too
 *
 *   caret       XMB and MaterialUI: the input dialog with 40 words typed,
 *               wrapped over several lines, and the cursor put at the
 *               start, inside a word, at the start of each line the
 *               wrap made, on the space before it, and at the end; the
 *               caret is drawn on the line of the character after the
 *               cursor, at its left edge - for a space the wrap made a
 *               line end, after the line it ends
 *
 *   redraw      RGUI only: the box is drawn into RGUI's framebuffer,
 *               which goes to the video driver once when the box comes
 *               up, once for each change to it - a newer box, the lit
 *               button, its text - and once when it goes; never every
 *               frame it is up
 *
 * Usage: msgbox_layout_test ozone|xmb|materialui short|tall|firmware
 *        msgbox_layout_test xmb|materialui caret
 *        msgbox_layout_test rgui redraw
 *
 * Requires a completed non-Qt build, same as the other menu harnesses:
 *
 *   ./configure --disable-qt && make
 *   samples/menu/msgbox_layout/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../retroarch_types.h"
#include "../../../command.h"
#include "../../../msg_hash.h"
#include "../../../menu/menu_driver.h"
#include "../../../gfx/gfx_display.h"
#include "../../../gfx/font_driver.h"
#include "../../../gfx/video_driver.h"
#include "../../../input/input_driver.h"
#include "../../../frontend/frontend.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* --- the recording font -------------------------------------------- */

#define DRAWN_MAX 512

typedef struct
{
   char text[256];
   float x;
   float y;
   float w; /* pixels */
} drawn_t;

static drawn_t drawn[DRAWN_MAX];
static unsigned drawn_count;

/* A glyph is half the font's size wide, a wide one its full size */
typedef struct
{
   float size;
} harness_face_t;

static harness_face_t faces[64];
static unsigned faces_count;

static float glyph_w(const harness_face_t *face, const char *s, size_t len)
{
   const char *end = s + len;
   float w         = 0.0f;
   while (s < end && *s)
   {
      w += ((unsigned char)*s >= 0xE0) ? face->size : face->size * 0.5f;
      s  = utf8skip(s, 1);
   }
   return w;
}

static void *font_init(void *data, const char *path, float size,
      bool threaded)
{
   harness_face_t *face;
   (void)data;
   (void)path;
   (void)threaded;
   if (faces_count >= sizeof(faces) / sizeof(faces[0]))
      return NULL;
   face       = &faces[faces_count++];
   face->size = size;
   return face;
}

static void font_free(void *data, bool threaded)
{
   (void)data;
   (void)threaded;
}

/* The menu's text reaches the video driver's set_osd_msg, which hands
 * it to the font: both record it */
static void record_text(const harness_face_t *face, const char *msg,
      size_t msg_len, const struct font_params *params)
{
   if (drawn_count >= DRAWN_MAX)
      return;
   if (msg_len >= sizeof(drawn[0].text))
      msg_len = sizeof(drawn[0].text) - 1;
   memcpy(drawn[drawn_count].text, msg, msg_len);
   drawn[drawn_count].text[msg_len] = '\0';
   drawn[drawn_count].x = params ? params->x : 0.0f;
   drawn[drawn_count].y = params ? params->y : 0.0f;
   drawn[drawn_count].w = face ? glyph_w(face, msg, msg_len)
         * (params ? params->scale : 1.0f) : 0.0f;
   drawn_count++;
}

static void font_render_msg(void *userdata, void *data, const char *msg,
      size_t msg_len, const struct font_params *params)
{
   (void)userdata;
   record_text((const harness_face_t*)data, msg, msg_len, params);
}

static void harness_set_osd_msg(void *data, const char *msg,
      size_t msg_len, const struct font_params *params, void *font)
{
   const font_data_t *f = (const font_data_t*)font;
   (void)data;
   record_text(f ? (const harness_face_t*)f->renderer_data : NULL,
         msg, msg_len, params);
}

static int font_message_width(void *data, const char *msg, size_t len,
      float scale)
{
   return (int)(glyph_w((const harness_face_t*)data, msg, len) * scale);
}

static const struct font_glyph *font_get_glyph(void *data, uint32_t code)
{
   static struct font_glyph narrow, wide;
   (void)data;
   const harness_face_t *face = (const harness_face_t*)data;
   narrow.advance_x = (int)(face->size * 0.5f);
   narrow.dims      = VIDEO_SCALE_PACK(narrow.advance_x, face->size);
   wide.advance_x   = (int)face->size;
   wide.dims        = VIDEO_SCALE_PACK(wide.advance_x, face->size);
   return code >= 0x2E80 ? &wide : &narrow;
}

static bool font_line_metrics(void *data, struct font_line_metrics **m)
{
   static struct font_line_metrics metrics;
   const harness_face_t *face = (const harness_face_t*)data;
   metrics.height    = face->size * 1.2f;
   metrics.ascender  = face->size * 0.9f;
   metrics.descender = face->size * 0.3f;
   *m = &metrics;
   return true;
}

static const font_renderer_t harness_font = {
   font_init,
   font_free,
   font_render_msg,
   "harness",
   font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   font_message_width,
   font_line_metrics
};

/* --- the video driver ---------------------------------------------- */

static uintptr_t next_handle = 0x1000;

static uintptr_t harness_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   (void)video_data;
   (void)data;
   (void)threaded;
   (void)filter_type;
   return next_handle += 0x10;
}

static void harness_unload_texture(void *data, bool threaded, uintptr_t id)
{
   (void)data;
   (void)threaded;
   (void)id;
}

/* RGUI hands the video driver its framebuffer here: one call, one
 * upload */
static unsigned uploads;

static void harness_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned dims, float alpha)
{
   (void)data;
   (void)frame;
   (void)rgb32;
   (void)dims;
   (void)alpha;
   uploads++;
}

static const video_poke_interface_t harness_poke = {
   NULL, /* get_flags */
   harness_load_texture,
   harness_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   harness_set_texture_frame,
   NULL, /* set_texture_enable */
   harness_set_osd_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_menu_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_expand_gamut */
   NULL, /* set_hdr_scanlines */
   NULL, /* set_hdr_subpixel_layout */
   NULL, /* supports_texture_format */
   NULL, /* load_texture_compressed */
   NULL, /* present_last */
   NULL  /* get_last_present_time */
};

static void harness_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &harness_poke;
}

static void harness_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_dims)
{
   (void)draw;
   (void)data;
   (void)video_dims;
}

static gfx_display_ctx_driver_t harness_dispctx = {
   harness_draw,
   NULL, /* draw_pipeline */
   NULL, /* blend_begin */
   NULL, /* blend_end */
   NULL, /* get_default_mvp */
   NULL, /* get_default_vertices */
   NULL, /* get_default_tex_coords */
   &harness_font,
   GFX_VIDEO_DRIVER_GENERIC,
   "harness",
   false, /* handles_transform */
   true,  /* handles_vertex_strip */
   NULL, /* scissor_begin */
   NULL, /* scissor_end */
   NULL  /* mesh_draw */
};

/* --- the quads ----------------------------------------------------- */

/* build.sh links with --wrap=gfx_display_draw_quad: the caret is a
 * quad 2 px wide, and only the menu drivers draw quads that way */
#define QUADS_MAX 512

typedef struct
{
   int x;
   int y;
   unsigned w;
   unsigned h;
} harness_quad_t;

static harness_quad_t quads[QUADS_MAX];
static unsigned quad_count;

void __real_gfx_display_draw_quad(gfx_display_t *p_disp, void *data,
      unsigned video_dims, int x, int y, unsigned dims, unsigned ref_dims,
      float *color, uintptr_t *texture);

void __wrap_gfx_display_draw_quad(gfx_display_t *p_disp, void *data,
      unsigned video_dims, int x, int y, unsigned dims, unsigned ref_dims,
      float *color, uintptr_t *texture)
{
   if (quad_count < QUADS_MAX)
   {
      quads[quad_count].x = x;
      quads[quad_count].y = y;
      quads[quad_count].w = VIDEO_SCALE_W(dims);
      quads[quad_count].h = VIDEO_SCALE_H(dims);
      quad_count++;
   }
   __real_gfx_display_draw_quad(p_disp, data, video_dims, x, y, dims,
         ref_dims, color, texture);
}

/* --- the fixture --------------------------------------------------- */

static char fixture_dir[512];
static char cfg_path[640];

static bool write_text(const char *path, const char *text)
{
   RFILE *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   int64_t n;
   if (!f)
      return false;
   n = filestream_write(f, text, (int64_t)strlen(text));
   filestream_close(f);
   return n == (int64_t)strlen(text);
}

static bool fixture_make(const char *ident)
{
   char cfg[1024];
   const char *tmp = getenv("TMPDIR");
   snprintf(fixture_dir, sizeof(fixture_dir), "%s/msgbox_layout_%s_%u",
         tmp ? tmp : "/tmp", ident,
         (unsigned)(cpu_features_get_time_usec() & 0xffffff));
   if (!path_mkdir(fixture_dir))
      return false;
   fill_pathname_join_special(cfg_path, fixture_dir, "harness.cfg",
         sizeof(cfg_path));
   snprintf(cfg, sizeof(cfg),
         "video_driver = \"null\"\n"
         "audio_driver = \"null\"\n"
         "input_driver = \"null\"\n"
         "input_joypad_driver = \"null\"\n"
         "video_threaded = \"false\"\n"
         "threaded_data_runloop_enable = \"false\"\n"
         "menu_ignore_missing_assets = \"true\"\n"
         "menu_timedate_enable = \"false\"\n"
         "menu_driver = \"%s\"\n"
         "assets_directory = \"%s\"\n", ident, fixture_dir);
   return write_text(cfg_path, cfg);
}

static void fixture_remove(void)
{
   char cmd[600];
   snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }
}

/* --- frames -------------------------------------------------------- */

static char box_text[1024];

static void one_frame(unsigned w, unsigned h)
{
   /* A second a frame: a fade begun in one is over by the next */
   static retro_time_t clock = 0;
   video_frame_info_t vinfo;
   unsigned i;
   clock += 1000000;
   gfx_animation_update(clock, false, 1.0f, VIDEO_SCALE_PACK(w, h));
   video_driver_set_output_dims(VIDEO_SCALE_PACK(w, h));
   video_driver_build_info(&vinfo);
   vinfo.dims             = VIDEO_SCALE_PACK(w, h);
   vinfo.menu.msgbox      = *box_text ? box_text : NULL;
   quad_count             = 0;
   disp_get_ptr()->dispctx = &harness_dispctx;
   drawn_count            = 0;
   menu_driver_frame(true, &vinfo);
   for (i = 0; i < 2000; i++)
      task_queue_check();
   if (getenv("MSGBOX_LAYOUT_DEBUG"))
   {
      fprintf(stderr, "frame %ux%u: %u text draw(s)\n", w, h, drawn_count);
      for (i = 0; i < drawn_count && i < 40; i++)
         fprintf(stderr, "  \"%s\" %.3f,%.3f\n", drawn[i].text,
               drawn[i].x, drawn[i].y);
   }
}

/* A box line: one or more numbered words, maybe cut short with "..." */
static bool is_box_line(const char *s)
{
   return s[0] == 'w' && s[1] >= '0' && s[1] <= '9';
}

typedef struct
{
   drawn_t line[DRAWN_MAX];
   unsigned count;
   float button_y[2]; /* Cancel's and OK's, 0..1 up the screen */
   bool cancel;
   bool ok;
} frame_t;

static void take_frame(frame_t *f)
{
   const char *cancel = msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CANCEL);
   const char *ok     = msg_hash_to_str(
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK);
   unsigned i;
   memset(f, 0, sizeof(*f));
   for (i = 0; i < drawn_count; i++)
   {
      if (is_box_line(drawn[i].text))
         f->line[f->count++] = drawn[i];
      else if (string_is_equal(drawn[i].text, cancel))
      {
         f->cancel      = true;
         f->button_y[0] = drawn[i].y;
      }
      else if (string_is_equal(drawn[i].text, ok))
      {
         f->ok          = true;
         f->button_y[1] = drawn[i].y;
      }
   }
}

static bool same_frame(const frame_t *a, const frame_t *b)
{
   unsigned i;
   if (a->count != b->count || a->cancel != b->cancel || a->ok != b->ok)
      return false;
   for (i = 0; i < a->count; i++)
      if (     strcmp(a->line[i].text, b->line[i].text)
            || a->line[i].x != b->line[i].x
            || a->line[i].y != b->line[i].y)
         return false;
   return true;
}

/* The lines hold words 1..n in order; true if the text was cut */
static bool check_words(const frame_t *f, unsigned words, const char *lane)
{
   unsigned next = 1;
   unsigned i;
   bool cut      = false;
   for (i = 0; i < f->count; i++)
   {
      const char *p = f->line[i].text;
      while (*p)
      {
         unsigned n;
         if (*p == ' ')
         {
            p++;
            continue;
         }
         /* The last line cut short, maybe inside a word */
         {
            const char *q = p;
            if (*q == 'w')
               q++;
            while (*q >= '0' && *q <= '9')
               q++;
            if (!strcmp(q, "..."))
            {
               cut = true;
               break;
            }
         }
         if (*p != 'w' || sscanf(p + 1, "%3u", &n) != 1)
         {
            CHECK(0, "[%s] line %u holds \"%s\"", lane, i, f->line[i].text);
            return cut;
         }
         CHECK(n == next, "[%s] word %u where %u was due", lane, n, next);
         next = n + 1;
         p   += 4;
      }
   }
   if (!cut)
      CHECK(next == words + 1 || f->count == 0,
            "[%s] %u of %u words drawn", lane, next - 1, words);
   return cut || next != words + 1;
}

static void check_lines(const frame_t *f, unsigned w, unsigned h,
      const char *lane)
{
   unsigned i;
   CHECK(f->count > 0, "[%s] no line of the box drawn", lane);
   CHECK(f->cancel && f->ok, "[%s] Cancel or OK not drawn", lane);
   for (i = 0; i < 2; i++)
      CHECK(f->button_y[i] > 0.0f && f->button_y[i] < 1.0f,
            "[%s] %s is off the screen", lane, i ? "OK" : "Cancel");
   /* The text stops above the buttons */
   if (f->count && f->cancel)
      CHECK(f->line[f->count - 1].y > f->button_y[0],
            "[%s] the last line is not above the buttons", lane);
   for (i = 0; i < f->count; i++)
   {
      /* Positions as the driver handed them: 0..1 of the screen */
      float x = f->line[i].x * w;
      float y = (1.0f - f->line[i].y) * h;
      CHECK(   x >= 0.0f && x + f->line[i].w <= (float)w + 0.5f
            && y >= 0.0f && y <= (float)h,
            "[%s] line %u at %.1f,%.1f, %.1f wide, is off a %ux%u screen",
            lane, i, x, y, f->line[i].w, w, h);
      if (i)
      {
         float gap = (f->line[i - 1].y - f->line[i].y) * h;
         CHECK(gap > 0.0f,
               "[%s] line %u is not below line %u", lane, i, i - 1);
         if (i > 1)
         {
            float prev = (f->line[i - 2].y - f->line[i - 1].y) * h;
            CHECK(gap - prev < 0.5f && prev - gap < 0.5f,
                  "[%s] lines %u apart, then %u", lane,
                  (unsigned)prev, (unsigned)gap);
         }
      }
   }
}

/* expect_cut: 1 must be cut, 0 must not, -1 either, as the driver's
 * font size for the screen has it */
static void set_text(unsigned words)
{
   size_t _len = 0;
   unsigned i;
   box_text[0] = '\0';
   for (i = 1; i <= words; i++)
   {
      char word[8];
      snprintf(word, sizeof(word), i == 1 ? "w%03u" : " w%03u", i);
      _len += strlcpy(box_text + _len, word, sizeof(box_text) - _len);
   }
}

/* The box at w x h as a driver lays it out afresh: after a frame with
 * other text, so nothing it kept can stand in for the layout */
static void fresh_frame(frame_t *f, unsigned words, unsigned w,
      unsigned h)
{
   set_text(words - 1);
   one_frame(w, h);
   set_text(words);
   one_frame(w, h);
   take_frame(f);
}

static void lane(const char *lane_name, unsigned words, unsigned w,
      unsigned h, int expect_cut)
{
   static frame_t first, again, other, other_fresh, back;
   bool cut;

   set_text(words);

   /* The box comes up, fading in where the driver fades it */
   one_frame(w, h);
   one_frame(w, h);
   take_frame(&first);
   check_lines(&first, w, h, lane_name);
   cut = check_words(&first, words, lane_name);
   if (expect_cut >= 0)
      CHECK(cut == (expect_cut != 0), "[%s] the text was %scut, want %scut",
            lane_name, cut ? "" : "not ", expect_cut ? "" : "not ");

   one_frame(w, h);
   take_frame(&again);
   CHECK(same_frame(&first, &again),
         "[%s] the next frame drew the box differently", lane_name);

   one_frame(w + 160, h + 90);
   take_frame(&other);
   check_lines(&other, w + 160, h + 90, lane_name);
   fresh_frame(&other_fresh, words, w + 160, h + 90);
   CHECK(same_frame(&other, &other_fresh),
         "[%s] at %ux%u the box is laid out as for the screen before",
         lane_name, w + 160, h + 90);

   one_frame(w, h);
   take_frame(&back);
   CHECK(same_frame(&first, &back),
         "[%s] back at %ux%u the box differs from the first frame",
         lane_name, w, h);

   printf("[%s] %u line(s) at %ux%u%s\n", lane_name, first.count, w, h,
         cut ? ", cut short" : "");
}

/* --- the input dialog's caret -------------------------------------- */

#define TYPED_TOKENS 40

/* The text typed: t01 t02 ..., which no other text in the menu has */
static size_t type_tokens(char *s, size_t len)
{
   size_t _len = 0;
   unsigned i;
   s[0] = '\0';
   for (i = 1; i <= TYPED_TOKENS; i++)
   {
      char word[8];
      snprintf(word, sizeof(word), i == 1 ? "t%02u" : " t%02u", i);
      _len += strlcpy(s + _len, word, len - _len);
   }
   return _len;
}

/* Where the caret is due for a cursor before byte p of the typed text:
 * the drawn line holding that byte, and how far into it */
static bool caret_due(const char *typed, size_t p, unsigned *line,
      size_t *col)
{
   unsigned i;
   const drawn_t *last = NULL;
   unsigned last_i     = 0;
   for (i = 0; i < drawn_count; i++)
   {
      const char *t = drawn[i].text;
      const char *at;
      size_t      n;
      if (t[0] != 't' || t[1] < '0' || t[1] > '9')
         continue;
      last   = &drawn[i];
      last_i = i;
      /* The line's text is the typed text from its first token on */
      at = strstr(typed, t);
      n  = strlen(t);
      if (at && p >= (size_t)(at - typed) && p < (size_t)(at - typed) + n)
      {
         *line = i;
         *col  = p - (size_t)(at - typed);
         return true;
      }
      /* On the space the wrap made the end of this line: after its last
       * character */
      if (at && p == (size_t)(at - typed) + n && typed[p] == ' ')
      {
         *line = i;
         *col  = n;
         return true;
      }
   }
   /* At the end, or on the space a line was broken at: after the last
    * character of the line before it */
   if (last && p == strlen(typed))
   {
      *line = last_i;
      *col  = strlen(last->text);
      return true;
   }
   return false;
}

static void caret_lane(unsigned w, unsigned h)
{
   size_t at[32];
   size_t at_count = 0;
   input_driver_state_t *input_st = input_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   char typed[256];
   size_t typed_len = type_tokens(typed, sizeof(typed));
   size_t k;
   unsigned checked = 0;

   box_text[0] = '\0';
   CHECK(menu_input_dialog_start_search(), "fixture: no input dialog");
   input_keyboard_line_append(&input_st->keyboard_line, typed, typed_len);
   input_st->keyboard_line.select_all = false;
   /* The caret blinks on the menu's clock: on for this half second */
   menu_st->current_time_us           = 500000;

   /* Where the wrap broke the typed text: the cursor goes at the start
    * of each line, on the space before it, and inside a word of it */
   input_st->keyboard_line.ptr = typed_len;
   one_frame(w, h);
   at[at_count++] = 0;
   at[at_count++] = 2;
   for (k = 0; k < drawn_count && at_count + 3 < sizeof(at) / sizeof(at[0]);
         k++)
   {
      const char *t = drawn[k].text;
      const char *s0;
      if (t[0] != 't' || t[1] < '0' || t[1] > '9')
         continue;
      if (!(s0 = strstr(typed, t)) || s0 == typed)
         continue;
      at[at_count++] = (size_t)(s0 - typed);
      at[at_count++] = (size_t)(s0 - typed) - 1;
      at[at_count++] = (size_t)(s0 - typed) + 5;
   }
   at[at_count++] = typed_len;
   CHECK(at_count > 3, "fixture: the typed text was not wrapped");

   for (k = 0; k < at_count; k++)
   {
      size_t p = at[k] < typed_len ? at[k] : typed_len;
      unsigned i, carets = 0, line = 0;
      size_t col = 0;
      const harness_quad_t *caret = NULL;

      input_st->keyboard_line.ptr = p;
      /* Twice: the first after a frame with another cursor, the second
       * from what the driver kept */
      for (i = 0; i < 2; i++)
      {
         one_frame(w, h);
         menu_st->current_time_us = 500000;
      }

      for (i = 0; i < quad_count; i++)
         if (quads[i].w == 2)
         {
            caret = &quads[i];
            carets++;
         }
      CHECK(carets == 1, "[caret] %u caret(s) drawn for the cursor at %u",
            carets, (unsigned)p);
      if (carets != 1)
         continue;
      if (!caret_due(typed, p, &line, &col))
      {
         CHECK(0, "[caret] no line holds byte %u", (unsigned)p);
         continue;
      }
      {
         const drawn_t *d = &drawn[line];
         float glyph      = d->w / (float)strlen(d->text);
         float line_x     = d->x * w;
         float baseline   = (1.0f - d->y) * h;
         float want_x     = line_x + glyph * col;
         CHECK(caret->x - want_x < 1.5f && want_x - caret->x < 1.5f,
               "[caret] cursor at %u: caret at x %d, want %.1f (\"%s\" "
               "column %u)", (unsigned)p, caret->x, want_x, d->text,
               (unsigned)col);
         CHECK(     baseline >= (float)caret->y - 1.0f
               &&   baseline <= (float)(caret->y + (int)caret->h) + 1.5f,
               "[caret] cursor at %u: caret at y %d..%d, its line's "
               "baseline at %.1f", (unsigned)p, caret->y,
               caret->y + (int)caret->h, baseline);
      }
      checked++;
   }

   menu_input_dialog_end();
   printf("[caret] %u cursor position(s) at %ux%u\n", checked, w, h);
}

/* --- the missing firmware dialog's underlines ---------------------- */

/* The path of each missing file is underlined: a quad 1 px high under
 * the path, not under its number, as wide as the path - in every frame,
 * the ones drawn from what was measured before too */
static void firmware_lane(unsigned w, unsigned h)
{
   static const char *line_want = "1. tos.img";
   struct menu_state *menu_st   = menu_state_get_ptr();
   unsigned frame;

   menu_dialog_confirm_set(menu_st, MSG_CORE_FIRMWARE_MISSING,
         CMD_EVENT_CORE_FIRMWARE_DOWNLOAD);
   strlcpy(box_text, "Required firmware is missing:\n\n1. tos.img\n"
         "[This firmware must be supplied manually.]\n"
         "tos.img (TOS 1.02 US aka Mega TOS)", sizeof(box_text));

   for (frame = 0; frame < 3; frame++)
   {
      const drawn_t *line = NULL;
      unsigned i, lines = 0;
      unsigned found    = 0;
      one_frame(w, h);
      if (!frame)
         continue; /* fading in */
      for (i = 0; i < drawn_count; i++)
         if (string_is_equal(drawn[i].text, line_want))
         {
            line = &drawn[i];
            lines++;
         }
      CHECK(lines == 1, "[firmware] \"%s\" drawn %u time(s)", line_want,
            lines);
      if (!line)
         continue;
      {
         float glyph  = line->w / (float)strlen(line_want);
         float want_x = line->x * w + glyph * 3;
         float want_w = glyph * 7;
         float baseline = (1.0f - line->y) * h;
         for (i = 0; i < quad_count; i++)
         {
            /* A line 1 px high just under the line's baseline; the
             * menus' other 1 px lines (separators) are elsewhere */
            if (     quads[i].h != 1
                  || (float)quads[i].y < baseline - 0.5f
                  || (float)quads[i].y > baseline + 2.5f)
               continue;
            found++;
            CHECK(   quads[i].x - want_x < 1.5f && want_x - quads[i].x < 1.5f
                  && (float)quads[i].w - want_w < 1.5f
                  && want_w - (float)quads[i].w < 1.5f,
                  "[firmware] frame %u: underline at x %d, %u wide; want "
                  "%.1f, %.1f wide", frame, quads[i].x, quads[i].w, want_x,
                  want_w);
         }
      }
      CHECK(found == 1, "[firmware] frame %u: %u underline(s), want 1",
            frame, found);
   }
   box_text[0] = '\0';
   menu_dialog_confirm_clear(menu_st);
   printf("[firmware] one underline under the path\n");
}

/* --- RGUI ---------------------------------------------------------- */

/* One iteration as the runloop runs it: the box's text handed over
 * while one is up, the render, the frame, and the framebuffer handed
 * to the video driver if RGUI marked it changed */
static void rgui_iteration(bool box)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   video_frame_info_t vinfo;
   if (box)
      menu_st->driver_ctx->render_messagebox(menu_st->userdata, box_text);
   video_driver_build_info(&vinfo);
   vinfo.dims = VIDEO_SCALE_PACK(640, 480);
   menu_st->driver_ctx->render(menu_st->userdata, vinfo.dims, false);
   menu_driver_frame(true, &vinfo);
   menu_st->driver_ctx->set_texture(menu_st->userdata);
}

/* How many of n iterations sent the framebuffer to the video driver */
static unsigned rgui_uploads(unsigned n, bool box)
{
   unsigned had = uploads;
   unsigned i;
   for (i = 0; i < n; i++)
      rgui_iteration(box);
   return uploads - had;
}

/* RGUI keeps the box in its framebuffer: it draws and uploads it again
 * only when something in it changed, not every frame it is up */
static void rgui_redraw_lane(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned n;

   video_driver_set_output_dims(VIDEO_SCALE_PACK(640, 480));
   /* Whatever the menu does as it comes up, done */
   rgui_uploads(30, false);
   n = rgui_uploads(10, false);
   CHECK(n == 0, "fixture: the menu alone uploaded %u frame(s) of 10", n);

   set_text(12);
   n = rgui_uploads(10, true);
   CHECK(n == 1, "the box coming up uploaded %u frame(s) of 10, want 1",
         n);

   /* A newer box in its place, with named buttons, the first lit:
    * what the Security Warning raises. Left and Right move the light. */
   menu_st->dialog_st.button_count     = 2;
   menu_st->dialog_st.button_label[0]  =
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CANCEL;
   menu_st->dialog_st.button_label[1]  =
         MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK;
   menu_st->dialog_st.button_highlight = 0;
   menu_st->dialog_st.button_epoch++;
   n = rgui_uploads(10, true);
   CHECK(n == 1, "a newer box uploaded %u frame(s) of 10, want 1", n);
   menu_st->dialog_st.button_highlight = 1;
   n = rgui_uploads(10, true);
   CHECK(n == 1, "the light moving to OK uploaded %u frame(s) of 10, "
         "want 1", n);

   set_text(13);
   n = rgui_uploads(10, true);
   CHECK(n == 1, "new text uploaded %u frame(s) of 10, want 1", n);

   n = rgui_uploads(10, false);
   CHECK(n == 1, "the box going uploaded %u frame(s) of 10, want 1", n);

   printf("[redraw] one upload per change to the box\n");
}

int main(int argc, char *argv[])
{
   const char *driver = argc > 1 ? argv[1] : "";
   const char *which  = argc > 2 ? argv[2] : "";
   const char *ident;
   char *rarch_argv[4];
   int rarch_argc = 0;
   struct menu_state *menu_st;

   if (string_is_equal(driver, "materialui"))
      ident = "glui";
   else if (string_is_equal(driver, "ozone") || string_is_equal(driver, "xmb"))
      ident = driver;
   else if (string_is_equal(driver, "rgui") && string_is_equal(which, "redraw"))
      ident = driver;
   else
      ident = NULL;
   if (     string_is_equal(which, "caret")
         && !string_is_equal(driver, "xmb")
         && !string_is_equal(driver, "materialui"))
      ident = NULL;
   if (!ident || (!string_is_equal(which, "short")
            && !string_is_equal(which, "tall")
            && !string_is_equal(which, "firmware")
            && !string_is_equal(which, "caret")
            && !string_is_equal(which, "redraw")))
   {
      fprintf(stderr, "usage: %s ozone|xmb|materialui short|tall|firmware\n"
            "       %s xmb|materialui caret\n"
            "       %s rgui redraw\n", argv[0], argv[0], argv[0]);
      return 2;
   }

   video_null.poke_interface = harness_get_poke_interface;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);

   if (!fixture_make(ident))
   {
      fprintf(stderr, "FAIL: could not write the fixture\n");
      return 1;
   }

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   retroarch_menu_running();

   menu_st = menu_state_get_ptr();
   CHECK(   menu_st->driver_ctx && menu_st->driver_ctx->ident
         && string_is_equal(menu_st->driver_ctx->ident, ident),
         "fixture: menu driver is %s, want %s",
         (menu_st->driver_ctx && menu_st->driver_ctx->ident)
            ? menu_st->driver_ctx->ident : "(none)", ident);

   /* A Cancel/OK confirm */
   menu_dialog_confirm_set(menu_st, MSG_UNKNOWN, CMD_EVENT_QUIT);

   if (string_is_equal(which, "redraw"))
      rgui_redraw_lane();
   else
   {
      /* The fonts made again, now with the recording backend */
      disp_get_ptr()->dispctx = &harness_dispctx;
      menu_st->driver_ctx->context_destroy(menu_st->userdata);
      menu_st->driver_ctx->context_reset(menu_st->userdata, false);
      one_frame(1280, 720);
      if (string_is_equal(which, "short"))
         lane("short", 40, 1280, 720, 0);
      else if (string_is_equal(which, "tall"))
         lane("tall", 199, 480, 272, -1);
      else if (string_is_equal(which, "firmware"))
      {
         menu_dialog_confirm_clear(menu_st);
         firmware_lane(1280, 720);
      }
      else
      {
         menu_dialog_confirm_clear(menu_st);
         caret_lane(640, 480);
         caret_lane(720, 480);
         caret_lane(800, 600);
      }
   }

   menu_st->dialog_st.button_count = 0;
   menu_dialog_confirm_clear(menu_st);
   main_exit(NULL);
   fixture_remove();

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u) [%s %s]\n", failures, driver, which);
      return 1;
   }
   printf("msgbox_layout [%s %s]: pass\n", driver, which);
   return 0;
}
