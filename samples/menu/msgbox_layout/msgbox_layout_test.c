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
 *   heading     the Security Warning: a blank line under its heading
 *               and a rule in it across the text's column, in the frame
 *               that wrapped the text and the one drawn from what was
 *               kept; the firmware lane checks its heading's rule too
 *
 *   heading_wrap  the same with a heading wrapped to two lines: the rule
 *               under the second, none between them
 *
 *   heading_none  the same text in a confirm without a heading, after
 *               the Security Warning: no blank line, no rule
 *
 *   heading_fit the heading and as many lines as fit with the rule, then
 *               one more: drawn as without the rule, every line whole;
 *               the box that fit with the rule on a screen a little
 *               shorter: the same, and back on the first, the rule again;
 *               with other buttons swapped in, at the same text and
 *               size: as the box laid out afresh with them
 *
 *   heading_osk XMB and MaterialUI: the input dialog drawn while the
 *               missing firmware box is set: no rule under its label
 *
 *   caret       XMB and MaterialUI: the input dialog with 40 words typed,
 *               wrapped over several lines, and the cursor put at the
 *               start, inside a word, at the start of each line the
 *               wrap made, on the space before it, and at the end; the
 *               caret is drawn on the line of the character after the
 *               cursor, at its left edge - for a space the wrap made a
 *               line end, after the line it ends
 *
 *               The same with 40 Hebrew words, drawn right to left:
 *               the caret at the right edge of the letter after the
 *               cursor, at the left end of its line at the line's end
 *
 *   reopen      XMB and MaterialUI: the search dialog with 40 words
 *               typed, closed and opened again with nothing typed:
 *               each frame of it draws its label and none of the words
 *
 *   redraw      RGUI only: the box is drawn into RGUI's framebuffer,
 *               which goes to the video driver once when the box comes
 *               up, once for each change to it - a newer box, the lit
 *               button, its text - and once when it goes; never every
 *               frame it is up
 *
 * Usage: msgbox_layout_test ozone|xmb|materialui short|tall|firmware
 *        msgbox_layout_test ozone|xmb|materialui heading|heading_wrap
 *        msgbox_layout_test ozone|xmb|materialui heading_none|heading_fit
 *        msgbox_layout_test xmb|materialui caret|heading_osk|reopen
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
   uint32_t color;
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
   drawn[drawn_count].color = params ? params->color : 0;
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
   float color[4];
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
      if (color)
         memcpy(quads[quad_count].color, color, sizeof(quads[quad_count].color));
      else
         memset(quads[quad_count].color, 0, sizeof(quads[quad_count].color));
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

/* even: every line a line height under the one before */
static void check_lines(const frame_t *f, unsigned w, unsigned h,
      bool even, const char *lane)
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
         if (even && i > 1)
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
   check_lines(&first, w, h, true, lane_name);
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
   check_lines(&other, w + 160, h + 90, true, lane_name);
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

/* --- the input dialog's caret among Hebrew ------------------------- */

/* Hebrew typed: 40 words of three letters, no two alike, so every drawn
 * line can be found in what was typed */
static size_t type_hebrew(char *s, size_t len)
{
   size_t   _len = 0;
   unsigned i;
   s[0] = '\0';
   for (i = 0; i < TYPED_TOKENS; i++)
   {
      /* U+05D0 - U+05EA: alef to tav */
      unsigned a = i % 27, b = (i / 27 + i) % 27, c = (i * 7 + 3) % 27;
      char     word[16];
      snprintf(word, sizeof(word), "%s%c%c%c%c%c%c", i ? " " : "",
            0xD7, 0x90 + a, 0xD7, 0x90 + b, 0xD7, 0x90 + c);
      _len += strlcpy(s + _len, word, len - _len);
   }
   return _len;
}

/* Whether a drawn line is Hebrew typed: Hebrew letters and spaces only.
 * The harness records lines as the driver hands them over, in typed
 * order; the renderer draws them right to left. */
static bool hebrew_line(const char *t)
{
   bool any = false;
   for (; *t; t++)
   {
      if ((unsigned char)*t == 0xD7 && t[1])
      {
         any = true;
         t++;
      }
      else if (*t != ' ')
         return false;
   }
   return any;
}

static void caret_rtl_lane(unsigned w, unsigned h)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   char     typed[512];
   size_t   typed_len = type_hebrew(typed, sizeof(typed));
   size_t   at[48];
   size_t   at_count  = 0;
   size_t   k;
   unsigned checked   = 0;

   box_text[0] = '\0';
   CHECK(menu_input_dialog_start_search(), "fixture: no input dialog");
   input_keyboard_line_append(&input_st->keyboard_line, typed, typed_len);
   input_st->keyboard_line.select_all = false;
   CHECK(input_driver_keyboard_line_state() & INPUT_KEYBOARD_LINE_RTL,
         "[caret rtl] typed Hebrew does not mark the line");
   menu_st->current_time_us           = 500000;

   input_st->keyboard_line.ptr = typed_len;
   one_frame(w, h);
   at[at_count++] = 0;
   at[at_count++] = 4;
   for (k = 0; k < drawn_count && at_count + 3 < sizeof(at) / sizeof(at[0]);
         k++)
   {
      const char *s0;
      if (     !hebrew_line(drawn[k].text)
            || !(s0 = strstr(typed, drawn[k].text)) || s0 == typed)
         continue;
      at[at_count++] = (size_t)(s0 - typed);
      at[at_count++] = (size_t)(s0 - typed) - 1;
      at[at_count++] = (size_t)(s0 - typed) + 2;
   }
   at[at_count++] = typed_len;
   CHECK(at_count > 4, "fixture: the typed Hebrew was not wrapped");

   for (k = 0; k < at_count; k++)
   {
      size_t   p      = at[k];
      unsigned i, carets = 0;
      const harness_quad_t *caret = NULL;
      bool     found  = false;
      float    want_x = 0.0f;

      input_st->keyboard_line.ptr = p;
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
      CHECK(carets == 1, "[caret rtl] %u caret(s) drawn for the cursor at "
            "%u", carets, (unsigned)p);
      if (carets != 1)
         continue;

      /* The drawn line holding the cursor, read right to left: before a
       * letter at its right edge, at the end of the line its left */
      for (i = 0; i < drawn_count && !found; i++)
      {
         const char *line = drawn[i].text;
         const char *s0;
         size_t      start, n, chars, col, j;
         if (!hebrew_line(line) || !(s0 = strstr(typed, line)))
            continue;
         start = (size_t)(s0 - typed);
         n     = strlen(line);
         if (p < start || p > start + n)
            continue;
         /* A cursor on the space a line was broken at sits at its end */
         if (p == start + n && p < typed_len && typed[p] != ' ')
            continue;
         for (j = 0, chars = 0, col = 0; j < n; j++)
            if (((unsigned char)line[j] & 0xC0) != 0x80)
            {
               if (j < p - start)
                  col++;
               chars++;
            }
         want_x = drawn[i].x * w
            + drawn[i].w / (float)chars * (float)(chars - col);
         found  = true;
         CHECK(     caret->x - want_x < 1.5f && want_x - caret->x < 1.5f,
               "[caret rtl] cursor at %u: caret at x %d, want %.1f "
               "(column %u of %u)", (unsigned)p, caret->x, want_x,
               (unsigned)col, (unsigned)chars);
      }
      CHECK(found, "[caret rtl] no drawn line holds byte %u", (unsigned)p);
      checked++;
   }

   /* English typed over the Hebrew: the line is unmarked again */
   input_st->keyboard_line.select_all = true;
   input_keyboard_line_append(&input_st->keyboard_line, "abc", 3);
   CHECK(!(input_driver_keyboard_line_state() & INPUT_KEYBOARD_LINE_RTL),
         "[caret rtl] English typed over Hebrew leaves the line marked");

   menu_input_dialog_end();
   printf("[caret rtl] %u cursor position(s) at %ux%u\n", checked, w, h);
}

/* --- the input dialog opened again --------------------------------- */

/* Whether a drawn line holds any of the typed words, wherever it starts */
static bool has_typed_token(const char *t)
{
   for (; *t; t++)
      if (     t[0] == 't'
            && t[1] >= '0' && t[1] <= '9'
            && t[2] >= '0' && t[2] <= '9')
         return true;
   return false;
}

/* The search dialog with 40 words typed, closed, and opened again with
 * nothing typed: every frame of the second draws its label and none of
 * the words the first held - the text the driver keeps is the second's
 * whole, not the first's with the second written over its start */
static void reopen_lane(unsigned w, unsigned h)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   const char *label = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH);
   char typed[256];
   size_t typed_len = type_tokens(typed, sizeof(typed));
   unsigned frame;

   box_text[0] = '\0';
   CHECK(menu_input_dialog_start_search(), "fixture: no input dialog");
   input_keyboard_line_append(&input_st->keyboard_line, typed, typed_len);
   input_st->keyboard_line.select_all = false;
   input_st->keyboard_line.ptr        = typed_len;
   menu_st->current_time_us           = 500000;
   /* The first frame wraps the text, the second draws what was kept */
   one_frame(w, h);
   one_frame(w, h);
   menu_input_dialog_end();

   CHECK(menu_input_dialog_start_search(), "fixture: no input dialog");
   for (frame = 1; frame <= 4; frame++)
   {
      unsigned i, words = 0;
      bool labelled     = false;
      menu_st->current_time_us = 500000;
      one_frame(w, h);
      for (i = 0; i < drawn_count; i++)
      {
         const char *t = drawn[i].text;
         if (string_is_equal(t, label))
            labelled = true;
         else if (has_typed_token(t))
            words++;
      }
      CHECK(labelled, "[reopen] frame %u: the label \"%s\" not drawn",
            frame, label);
      CHECK(!words, "[reopen] frame %u: %u line(s) of the text typed "
            "into the dialog before drawn", frame, words);
   }

   menu_input_dialog_end();
   printf("[reopen] the input dialog opened again draws only its own "
         "text at %ux%u\n", w, h);
}

/* --- the confirm box's heading ------------------------------------- */

/* The Security Warning's text: its heading, then the body */
#define WARN_HEADING "Security Warning"
#define WARN_TEXT WARN_HEADING "\nThis core is not signed.\n" \
      "It was built by someone else.\n\nA core can reach your files."

static const unsigned warn_two[] = {
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CANCEL,
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK
};

/* The buttons of a core load's warning, which can stack */
static const unsigned warn_three[] = {
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_CANCEL,
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_LOAD_ONCE,
   MENU_ENUM_LABEL_VALUE_CORE_WARNING_ALWAYS_TRUST
};

/* The Security Warning's buttons, the first lit */
static void warning_buttons(const unsigned *label, unsigned count)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned i;
   for (i = 0; i < count; i++)
      menu_st->dialog_st.button_label[i] = label[i];
   menu_st->dialog_st.button_count     = (uint8_t)count;
   menu_st->dialog_st.button_highlight = 0;
   menu_st->dialog_st.heading          = true;
   menu_st->dialog_st.button_epoch++;
}

/* What the Security Warning raises: named buttons, the first lit */
static void warning_state(void)
{
   menu_dialog_confirm_clear(menu_state_get_ptr());
   warning_buttons(warn_two, 2);
}

static bool within(float a, float b, float tolerance)
{
   return a - b <= tolerance && b - a <= tolerance;
}

/* The box's lines, from the first drawn that starts with first: the
 * driver draws them one after another at one x. Their baselines go in
 * b, px down the screen; empty lines are left out. */
static unsigned box_lines(const char *first, unsigned h,
      const drawn_t **line, float *b, unsigned max)
{
   size_t first_len = strlen(first);
   unsigned n       = 0;
   unsigned i, j;
   for (i = 0; i < drawn_count; i++)
      if (!strncmp(drawn[i].text, first, first_len))
         break;
   for (j = i; j < drawn_count && n < max; j++)
   {
      if (drawn[j].x != drawn[i].x)
         break;
      if (!*drawn[j].text)
         continue;
      line[n] = &drawn[j];
      b[n++]  = (1.0f - drawn[j].y) * h;
   }
   return n;
}

/* The rule: a quad 1 px high strictly between two baselines, from the
 * text's left edge; the menu behind the box draws others */
static unsigned rules_between(float y0, float y1, float left,
      const harness_quad_t **rule)
{
   unsigned n = 0;
   unsigned i;
   for (i = 0; i < quad_count; i++)
      if (     quads[i].h == 1
            && (float)quads[i].y > y0
            && (float)quads[i].y < y1
            && within((float)quads[i].x, left, 1.5f))
      {
         if (rule)
            *rule = &quads[i];
         n++;
      }
   return n;
}

/* A frame's text draws and quads, all of them */
typedef struct
{
   drawn_t text[DRAWN_MAX];
   harness_quad_t quad[QUADS_MAX];
   unsigned texts;
   unsigned quads;
} shot_t;

static void shot_take(shot_t *s)
{
   s->texts = drawn_count;
   s->quads = quad_count;
   memcpy(s->text, drawn, drawn_count * sizeof(*drawn));
   memcpy(s->quad, quads, quad_count * sizeof(*quads));
}

static bool shot_same(const shot_t *a, const shot_t *b)
{
   unsigned i;
   if (a->texts != b->texts || a->quads != b->quads)
      return false;
   for (i = 0; i < a->texts; i++)
      if (     strcmp(a->text[i].text, b->text[i].text)
            || a->text[i].x != b->text[i].x
            || a->text[i].y != b->text[i].y
            || a->text[i].w != b->text[i].w)
         return false;
   return !memcmp(a->quad, b->quad, a->quads * sizeof(*a->quad));
}

/* The box comes up with other text, so the next frame wraps text
 * afresh rather than drawing from what the driver kept */
static void heading_text(const char *text, unsigned w, unsigned h)
{
   strlcpy(box_text, "w001", sizeof(box_text));
   one_frame(w, h);
   strlcpy(box_text, text, sizeof(box_text));
}

/* The driver's line height: two body lines of the Security Warning
 * with no blank line between them */
static float heading_line_height(unsigned w, unsigned h)
{
   const drawn_t *line[8];
   float b[8];
   unsigned n;
   warning_state();
   heading_text(WARN_TEXT, w, h);
   one_frame(w, h);
   n = box_lines(WARN_HEADING, h, line, b, 8);
   CHECK(n == 4, "fixture: %u line(s) of the Security Warning drawn, "
         "want 4", n);
   return n == 4 ? b[2] - b[1] : 0.0f;
}

/* The Security Warning: a blank line under its heading, with a rule in
 * it across the text's column, in the frame that wrapped the text and
 * the one drawn from what was kept */
static void heading_lane(unsigned w, unsigned h)
{
   static shot_t shot[2];
   static frame_t f;
   unsigned frame;

   warning_state();
   heading_text(WARN_TEXT, w, h);
   for (frame = 1; frame <= 2; frame++)
   {
      const harness_quad_t *rule = NULL;
      const drawn_t *line[8];
      float b[8];
      float lh, left;
      unsigned i, n, rules;

      one_frame(w, h);
      shot_take(&shot[frame - 1]);
      n = box_lines(WARN_HEADING, h, line, b, 8);
      CHECK(n == 4, "[heading] frame %u: %u line(s) drawn, want 4",
            frame, n);
      if (n != 4)
         continue;
      /* As the short lane checks its lines, but not for even spacing,
       * which the blank row under the heading breaks */
      take_frame(&f);
      for (i = 0; i < n; i++)
         f.line[i] = *line[i];
      f.count = n;
      check_lines(&f, w, h, false, "heading");
      lh = b[2] - b[1];
      CHECK(within(b[1] - b[0], 2.0f * lh, 0.5f),
            "[heading] frame %u: heading and body %.1f apart, want 2 lines "
            "of %.1f", frame, b[1] - b[0], lh);
      left  = line[0]->x * w;
      rules = rules_between(b[0], b[1], left, &rule);
      CHECK(rules == 1, "[heading] frame %u: %u rule(s) under the heading, "
            "want 1", frame, rules);
      if (!rule)
         continue;
      /* The column the box centres, which its buttons may make wider
       * than the text */
      CHECK(within((float)rule->w, (float)w - 2.0f * left, 1.5f),
            "[heading] frame %u: rule %u wide, want the text's column, "
            "%.1f", frame, rule->w, (float)w - 2.0f * left);
   }
   CHECK(shot_same(&shot[0], &shot[1]),
         "[heading] the frame drawn from what was kept differs");
   printf("[heading] a rule under the heading at %ux%u\n", w, h);
}

/* A heading that wraps: the rule under its last line */
static void heading_wrap_lane(unsigned w, unsigned h)
{
   const drawn_t *line[16];
   float b[16];
   float lh = heading_line_height(w, h);
   unsigned repeats;
   unsigned n = 0;

   for (repeats = 2; repeats < 40; repeats++)
   {
      size_t _len = 0;
      unsigned i, heading = 0;
      box_text[0] = '\0';
      for (i = 0; i < repeats; i++)
         _len += strlcpy(box_text + _len, i ? " " WARN_HEADING : WARN_HEADING,
               sizeof(box_text) - _len);
      strlcpy(box_text + _len, &WARN_TEXT[sizeof(WARN_HEADING) - 1],
            sizeof(box_text) - _len);
      one_frame(w, h);
      n = box_lines(WARN_HEADING, h, line, b, 16);
      while (heading < n && strcmp(line[heading]->text,
               "This core is not signed."))
         heading++;
      if (heading >= 2)
      {
         CHECK(heading == 2, "fixture: the heading wrapped to %u lines, "
               "want 2", heading);
         break;
      }
   }
   CHECK(repeats < 40 && n == 5, "fixture: the heading did not wrap to "
         "two lines");
   if (repeats < 40 && n == 5)
   {
      float left     = line[0]->x * w;
      unsigned rules = rules_between(b[1], b[2], left, NULL);
      CHECK(within(b[1] - b[0], lh, 0.5f), "[heading_wrap] the heading's "
            "lines %.1f apart, want %.1f", b[1] - b[0], lh);
      CHECK(within(b[2] - b[1], 2.0f * lh, 0.5f), "[heading_wrap] heading "
            "and body %.1f apart, want 2 lines of %.1f", b[2] - b[1], lh);
      CHECK(!rules_between(b[0], b[1], left, NULL),
            "[heading_wrap] a rule between the heading's lines");
      CHECK(rules == 1, "[heading_wrap] %u rule(s) under the heading, "
            "want 1", rules);
   }
   printf("[heading_wrap] heading of %u repeats on two lines at %ux%u\n",
         repeats, w, h);
}

/* The same text in a confirm without a heading, after the Security
 * Warning: drawn as it is, not from the warning's wrap */
static void heading_none_lane(unsigned w, unsigned h)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned frame;

   heading_line_height(w, h);
   menu_dialog_confirm_set(menu_st, MSG_UNKNOWN, CMD_EVENT_QUIT);
   for (frame = 1; frame <= 2; frame++)
   {
      const drawn_t *line[8];
      float b[8];
      unsigned n;
      one_frame(w, h);
      n = box_lines(WARN_HEADING, h, line, b, 8);
      CHECK(n == 4, "[heading_none] frame %u: %u line(s) drawn, want 4",
            frame, n);
      if (n != 4)
         continue;
      CHECK(within(b[1] - b[0], b[2] - b[1], 0.5f), "[heading_none] frame "
            "%u: first lines %.1f apart, want %.1f", frame, b[1] - b[0],
            b[2] - b[1]);
      CHECK(!rules_between(b[0], b[1], line[0]->x * w, NULL),
            "[heading_none] frame %u: a rule under the first line", frame);
   }
   printf("[heading_none] no rule without a heading at %ux%u\n", w, h);
}

/* Whether the frame drew the heading's rule and its n one-word lines,
 * none cut */
static void heading_fit_drawn(unsigned n, unsigned w, unsigned h,
      bool *rule, bool *whole, float *b)
{
   const drawn_t *line[200];
   unsigned count = box_lines(WARN_HEADING, h, line, b, 200);
   unsigned i;
   *whole = count == n + 1 && !strcmp(line[0]->text, WARN_HEADING);
   for (i = 1; *whole && i < count; i++)
   {
      char word[8];
      snprintf(word, sizeof(word), "w%03u", i);
      *whole = !strcmp(line[i]->text, word);
   }
   *rule  = count > 1
         && rules_between(b[0], b[1], line[0]->x * w, NULL) > 0;
}

/* The heading and n one-word lines: whether the box drew the rule and
 * every line, none cut */
static void heading_fit_frame(unsigned n, unsigned w, unsigned h,
      bool *rule, bool *whole, float *b)
{
   unsigned i;
   size_t _len = strlcpy(box_text, WARN_HEADING, sizeof(box_text));
   for (i = 1; i <= n; i++)
   {
      char word[8];
      snprintf(word, sizeof(word), "\nw%03u", i);
      _len += strlcpy(box_text + _len, word, sizeof(box_text) - _len);
   }
   one_frame(w, h);
   one_frame(w, h);
   heading_fit_drawn(n, w, h, rule, whole, b);
}

/* As heading_fit_frame(), after a frame with other text, so the box is
 * laid out afresh */
static void heading_fit_afresh(unsigned n, unsigned w, unsigned h,
      bool *rule, bool *whole, float *b)
{
   strlcpy(box_text, "w001", sizeof(box_text));
   one_frame(w, h);
   heading_fit_frame(n, w, h, rule, whole, b);
}

/* The heading and n lines with other buttons swapped in, the text and
 * size kept, which a driver may draw from the wrap it kept: laid out as
 * afresh with them. Checked at a width where three buttons, stacked,
 * leave no room for the rule and two do; XMB, whose kept wrap is keyed
 * on the buttons, may have none. */
static void heading_swap(unsigned n, unsigned w, unsigned h)
{
   float b[200], kept_b[200];
   bool rule = false, whole = false;
   unsigned w2, i, j;

   for (w2 = w; w2 > w / 4; w2 -= 8)
   {
      warning_buttons(warn_three, 3);
      heading_fit_afresh(n, w2, h, &rule, &whole, b);
      if (rule)
         continue;
      warning_buttons(warn_two, 2);
      heading_fit_afresh(n, w2, h, &rule, &whole, b);
      if (rule && whole)
         break;
   }
   if (w2 <= w / 4)
   {
      bool xmb = string_is_equal(menu_state_get_ptr()->driver_ctx->ident,
            "xmb");
      CHECK(xmb, "[heading_fit] three buttons take the rule's room at no "
            "width from %u to %u", w, w / 4);
      if (xmb)
         printf("[heading_fit] three buttons take the rule's room at no "
               "width from %u to %u: no swap checked\n", w, w / 4);
      return;
   }

   /* Three buttons for the two, then two for the three */
   for (i = 3; i >= 2; i--)
   {
      bool kept_rule, kept_whole, same;
      warning_buttons(i == 3 ? warn_three : warn_two, i);
      /* The first frame with them, drawn from the wrap kept: a box cut
       * short wraps again on the next */
      one_frame(w2, h);
      heading_fit_drawn(n, w2, h, &kept_rule, &kept_whole, kept_b);
      heading_fit_afresh(n, w2, h, &rule, &whole, b);
      CHECK(kept_rule == (i == 2), "[heading_fit] %u line(s) at %ux%u, "
            "%u buttons swapped in: %s", n, w2, h, i,
            kept_rule ? "a rule" : "no rule");
      same = kept_rule == rule && kept_whole == whole;
      for (j = 0; same && whole && j <= n; j++)
         same = within(kept_b[j], b[j], 0.01f);
      CHECK(same, "[heading_fit] %u line(s) at %ux%u, %u buttons swapped "
            "in: the box differs from one laid out afresh", n, w2, h, i);
   }
   printf("[heading_fit] %u line(s) at %ux%u: a rule with two buttons, "
         "none with three, swapped either way\n", n, w2, h);
}

/* A box that fits only without the blank line: drawn without it, so no
 * rule, every line whole and the heading a line height above the body.
 * So is the box that fit with it, on a screen a little shorter, which a
 * driver may draw from the wrap it kept; back on the first, the rule
 * again. */
static void heading_fit_lane(unsigned w, unsigned h)
{
   float b[200];
   float lh = heading_line_height(w, h);
   bool rule = false, whole = false;
   unsigned n, h2;

   for (n = 1; n < 150; n++)
   {
      heading_fit_frame(n, w, h, &rule, &whole, b);
      if (!rule || !whole)
         break;
   }
   CHECK(n > 1, "[heading_fit] no rule, even with one line under the "
         "heading");
   CHECK(n < 150, "fixture: 150 lines fit the box");
   CHECK(!rule, "[heading_fit] %u line(s): a rule where the box fits only "
         "without it", n);
   CHECK(whole, "[heading_fit] %u line(s): not every line drawn whole", n);
   if (whole)
      CHECK(within(b[1] - b[0], lh, 0.5f), "[heading_fit] %u line(s): heading "
            "and body %.1f apart, want %.1f", n, b[1] - b[0], lh);
   if (n < 2 || n >= 150)
      return;

   for (h2 = h; h2 > h / 2; h2--)
   {
      heading_fit_frame(n - 1, w, h2, &rule, &whole, b);
      if (!rule)
         break;
   }
   CHECK(h2 > h / 2, "fixture: %u line(s) with a rule down to %ux%u",
         n - 1, w, h2);
   CHECK(h2 < h, "[heading_fit] %u line(s) at %ux%u: no rule", n - 1, w, h);
   CHECK(whole, "[heading_fit] %u line(s) at %ux%u, too short for the "
         "rule: not every line drawn whole", n - 1, w, h2);
   heading_fit_frame(n - 1, w, h, &rule, &whole, b);
   CHECK(rule && whole, "[heading_fit] %u line(s) back at %ux%u: %s",
         n - 1, w, h, rule ? "not every line drawn whole" : "no rule");
   printf("[heading_fit] a rule with up to %u line(s) at %ux%u, none at "
         "%ux%u\n", n - 1, w, h, w, h2);
   heading_swap(n - 1, w, h);
}

/* The input dialog drawn while the missing firmware box is set: its
 * label is no heading, so no rule under it */
static void heading_osk_lane(unsigned w, unsigned h)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   menu_input_ctx_line_t dialog;
   char typed[256];
   size_t typed_len = type_tokens(typed, sizeof(typed));
   unsigned frame;

   menu_dialog_confirm_set(menu_st, MSG_CORE_FIRMWARE_MISSING,
         CMD_EVENT_CORE_FIRMWARE_DOWNLOAD);
   box_text[0]  = '\0';
   memset(&dialog, 0, sizeof(dialog));
   dialog.label = "Enter a name";
   CHECK(menu_input_dialog_start(&dialog), "fixture: no input dialog");
   input_keyboard_line_append(&input_st->keyboard_line, typed, typed_len);
   input_st->keyboard_line.select_all = false;
   input_st->keyboard_line.ptr        = typed_len;
   menu_st->current_time_us           = 500000;

   /* The first frame wraps the text, the second draws what was kept */
   for (frame = 1; frame <= 2; frame++)
   {
      const drawn_t *line[16];
      float b[16];
      unsigned n;
      one_frame(w, h);
      menu_st->current_time_us = 500000;
      n = box_lines(dialog.label, h, line, b, 16);
      CHECK(n >= 2, "fixture: frame %u: %u line(s) of the input dialog "
            "drawn", frame, n);
      if (n >= 2)
         CHECK(!rules_between(b[0], b[1], line[0]->x * w, NULL),
               "[heading_osk] frame %u: a rule under the label", frame);
   }

   menu_input_dialog_end();
   menu_dialog_confirm_clear(menu_st);
   printf("[heading_osk] no rule under the input dialog's label\n");
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
            /* Background separators can share the path's baseline.
             * Its underline uses the text's foreground color. */
            if (     quads[i].h != 1
                  || !within(quads[i].color[0],
                     FONT_COLOR_GET_RED(line->color) / 255.0f, 0.001f)
                  || !within(quads[i].color[1],
                     FONT_COLOR_GET_GREEN(line->color) / 255.0f, 0.001f)
                  || !within(quads[i].color[2],
                     FONT_COLOR_GET_BLUE(line->color) / 255.0f, 0.001f)
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
      /* The heading's rule in the blank line under it, which the box
       * already has: it grows by no line */
      {
         const drawn_t *head  = NULL;
         const drawn_t *badge = NULL;
         for (i = 0; i < drawn_count; i++)
            if (string_is_equal(drawn[i].text,
                     "Required firmware is missing:"))
               head  = &drawn[i];
            else if (string_is_equal(drawn[i].text,
                     "[This firmware must be supplied manually.]"))
               badge = &drawn[i];
         CHECK(head && badge, "[firmware] frame %u: the heading or the "
               "badge not drawn", frame);
         if (head && badge)
         {
            float b0       = (1.0f - head->y) * h;
            float b1       = (1.0f - line->y) * h;
            float lh       = (line->y - badge->y) * h;
            unsigned rules = rules_between(b0, b1, head->x * w, NULL);
            CHECK(within(b1 - b0, 2.0f * lh, 0.5f), "[firmware] frame %u: "
                  "heading and path %.1f apart, want 2 lines of %.1f",
                  frame, b1 - b0, lh);
            CHECK(rules == 1, "[firmware] frame %u: %u rule(s) under the "
                  "heading, want 1", frame, rules);
         }
      }
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
   if (     (string_is_equal(which, "caret")
            || string_is_equal(which, "heading_osk")
            || string_is_equal(which, "reopen"))
         && !string_is_equal(driver, "xmb")
         && !string_is_equal(driver, "materialui"))
      ident = NULL;
   if (!ident || (!string_is_equal(which, "short")
            && !string_is_equal(which, "tall")
            && !string_is_equal(which, "firmware")
            && !string_is_equal(which, "heading")
            && !string_is_equal(which, "heading_wrap")
            && !string_is_equal(which, "heading_none")
            && !string_is_equal(which, "heading_fit")
            && !string_is_equal(which, "heading_osk")
            && !string_is_equal(which, "caret")
            && !string_is_equal(which, "reopen")
            && !string_is_equal(which, "redraw")))
   {
      fprintf(stderr, "usage: %s ozone|xmb|materialui short|tall|firmware\n"
            "       %s ozone|xmb|materialui heading|heading_wrap\n"
            "       %s ozone|xmb|materialui heading_none|heading_fit\n"
            "       %s xmb|materialui caret|heading_osk|reopen\n"
            "       %s rgui redraw\n", argv[0], argv[0], argv[0], argv[0],
            argv[0]);
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
      else if (string_is_equal(which, "heading"))
         heading_lane(1280, 720);
      else if (string_is_equal(which, "heading_wrap"))
         heading_wrap_lane(1280, 720);
      else if (string_is_equal(which, "heading_none"))
         heading_none_lane(1280, 720);
      else if (string_is_equal(which, "heading_fit"))
         heading_fit_lane(1280, 720);
      else if (string_is_equal(which, "heading_osk"))
         heading_osk_lane(1280, 720);
      else if (string_is_equal(which, "reopen"))
      {
         menu_dialog_confirm_clear(menu_st);
         reopen_lane(1280, 720);
      }
      else
      {
         menu_dialog_confirm_clear(menu_st);
         caret_lane(640, 480);
         caret_lane(720, 480);
         caret_lane(800, 600);
         caret_rtl_lane(640, 480);
         caret_rtl_lane(720, 480);
         caret_rtl_lane(800, 600);
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
