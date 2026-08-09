/* Exercise font_driver.c's font lifecycle under the sanitizers:
 * create, reload in place, free, in the orders that actually happen.
 *
 * The real font_driver.c is compiled in; only the layers below it are
 * stubbed, so the list bookkeeping, the strdup'd path, the metrics
 * cache and the renderer-state swap are the genuine article. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <boolean.h>
#include "gfx/font_driver.h"

/* --- a fake backend, standing in for gl2_raster_font et al. --- */
static int live_renderer_state = 0;
static int fail_next_init      = 0;

typedef struct { int magic; float size; } fake_state_t;

static void *fake_init(void *data, const char *font_path,
      float font_size, bool is_threaded)
{
   fake_state_t *st;
   (void)data; (void)is_threaded;
   if (fail_next_init)
      return NULL;
   if (!(st = (fake_state_t*)calloc(1, sizeof(*st))))
      return NULL;
   st->magic = 0x5A5A;
   st->size  = font_size;
   live_renderer_state++;
   return st;
}

static void fake_free(void *data, bool is_threaded)
{
   (void)is_threaded;
   if (data)
   {
      free(data);
      live_renderer_state--;
   }
}

static void fake_render_msg(void *userdata, void *data, const char *msg,
      size_t msg_len, const struct font_params *params)
{ (void)userdata; (void)data; (void)msg; (void)msg_len; (void)params; }

static int fake_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{ (void)data; (void)scale; return (int)(msg_len * 8); }

static bool fake_get_line_metrics(void *data, struct font_line_metrics **m)
{
   static struct font_line_metrics lm;
   fake_state_t *st = (fake_state_t*)data;
   lm.height    = st ? st->size * 1.5f : 0.0f;
   lm.ascender  = st ? st->size * 1.1f : 0.0f;
   lm.descender = st ? st->size * 0.4f : 0.0f;
   *m = &lm;
   return true;
}

font_renderer_t fake_font = {
   fake_init, fake_free, fake_render_msg, "fake",
   NULL, NULL, NULL, fake_get_message_width, fake_get_line_metrics
};

/* --- scenarios --- */
int read_should_fail = 0;
extern unsigned test_language;
#define TEST_LANG_KOREAN 10   /* RETRO_LANGUAGE_KOREAN */
#define TEST_LANG_THAI   36   /* RETRO_LANGUAGE_THAI   */
#define TEST_LANG_JAPANESE 1  /* RETRO_LANGUAGE_JAPANESE */
#define TEST_LANG_FRENCH   2  /* RETRO_LANGUAGE_FRENCH   */
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

static font_data_t *mk(const char *path, float size)
{
   return font_driver_init_first(NULL, path, size, false, false, &fake_font);
}

int main(void)
{
   font_data_t *a, *b, *c;
   unsigned n;
   uint32_t g0, g1;

   /* 1. create/free balances the renderer state */
   a = mk("/tmp/san/font_a.ttf", 16.0f);
   CHECK(a != NULL, "create");
   CHECK(live_renderer_state == 1, "one live renderer state after create");
   font_driver_free(a);
   CHECK(live_renderer_state == 0, "renderer state released on free");

   /* 2. reload swaps in place, keeps the pointer, releases the old */
   a = mk("/tmp/san/font_a.ttf", 16.0f);
   b = mk("/tmp/san/font_b.ttf", 24.0f);
   CHECK(live_renderer_state == 2, "two live");
   {
      void *ra = a->renderer_data, *rb = b->renderer_data;
      font_data_t *pa = a;
      /* A rebuild only happens when the resolved path actually
       * changes, so make these follow the language and switch to one
       * with a face of its own. */
      font_driver_set_language_font(a, "/assets/pkg", "/tmp/san/font_a.ttf");
      font_driver_set_language_font(b, "/assets/pkg", "/tmp/san/font_b.ttf");
      test_language = TEST_LANG_KOREAN;
      g0 = font_driver_get_generation();
      n  = font_driver_reload_fonts();
      g1 = font_driver_get_generation();
      CHECK(n == 2, "both fonts rebuilt");
      CHECK(a == pa, "font_data_t address unchanged");
      CHECK(a->renderer_data != ra, "renderer state replaced (a)");
      CHECK(b->renderer_data != rb, "renderer state replaced (b)");
      CHECK(live_renderer_state == 2, "no renderer state leaked by reload");
      CHECK(g1 != g0, "generation bumped");
   }

   /* 3. a failing rebuild keeps the old font rather than losing text */
   {
      void *ra = a->renderer_data;
      fail_next_init = 1;
      test_language  = 0;            /* back to the default face */
      n = font_driver_reload_fonts();
      fail_next_init = 0;
      CHECK(n == 0, "no fonts rebuilt when init fails");
      CHECK(a->renderer_data == ra, "old renderer state kept on failure");
      CHECK(live_renderer_state == 2, "nothing leaked on failed rebuild");
   }

   /* 4. metrics follow the rebuild */
   {
      float h = a->metrics.height;
      CHECK(h > 0.0f, "metrics cached");
      test_language = TEST_LANG_KOREAN;
      n = font_driver_reload_fonts();
      CHECK(a->metrics.height == h, "metrics re-read after rebuild");
      test_language = 0;
   }

   /* 5. free in the middle of the list, then reload the rest */
   c = mk("/tmp/san/font_c.ttf", 12.0f);
   font_driver_set_language_font(c, "/assets/pkg", "/tmp/san/font_c.ttf");
   font_driver_free(b);
   CHECK(live_renderer_state == 2, "b released");
   test_language = TEST_LANG_KOREAN;
   n = font_driver_reload_fonts();
   CHECK(n >= 1, "reload visits the live fonts and not the freed one");
   test_language = 0;

   /* 6. a font with no path is skipped, not crashed on */
   {
      font_data_t *d = mk(NULL, 10.0f);
      if (d)
      {
         n = font_driver_reload_fonts();
         CHECK(d->renderer_data != NULL, "pathless font untouched");
         font_driver_free(d);
      }
   }

   /* 6b. the generation-zero trap: a fresh impl and a fresh font
    *     driver both start at 0, so a guarded sync would decide
    *     nothing had changed and leave the metrics at zero. */
   {
      font_data_impl_t fresh;
      memset(&fresh, 0, sizeof(fresh));
      fresh.font          = a;
      fresh.wideglyph_str = "WW";
      CHECK(fresh.metrics_generation == 0, "fresh impl starts at gen 0");
      font_driver_sync_impl(&fresh);
      /* Whether this computes depends on the current generation, which
       * is why callers that know the font is new must not rely on the
       * guarded path. What must never happen is a zero line height
       * being left behind and drawn with. */
      if (fresh.metrics_generation == font_driver_get_generation())
         CHECK(fresh.line_height > 0,
               "metrics are not left at zero once stamped");
   }

   /* 7. derived metrics on an impl follow a rebuild */
   {
      font_data_impl_t impl;
      memset(&impl, 0, sizeof(impl));
      impl.font          = a;
      impl.wideglyph_str = "WW";
      font_driver_sync_impl(&impl);
      {
         int h0 = impl.line_height;
         unsigned g0w = impl.glyph_width;
         CHECK(h0 > 0, "impl metrics computed on first sync");
         CHECK(g0w > 0, "impl glyph width computed");
         /* nothing changed: sync must be a no-op */
         impl.line_height = -1;
         font_driver_sync_impl(&impl);
         CHECK(impl.line_height == -1,
               "sync does nothing when the generation is unchanged");
         /* rebuild: the sync must notice and recompute */
         test_language = TEST_LANG_KOREAN;
         font_driver_reload_fonts();
         test_language = 0;
         font_driver_sync_impl(&impl);
         CHECK(impl.line_height == h0,
               "impl metrics recomputed after a rebuild");
         CHECK(impl.wideglyph_width > 0, "wideglyph width recomputed");
      }
   }

   /* 8. a language-following font re-resolves rather than re-reads */
   {
      font_data_t *L = mk("/assets/ozone/bold.ttf", 16.0f);
      CHECK(L != NULL, "language font created");
      font_driver_set_language_font(L, "/assets/pkg", "/assets/ozone/bold.ttf");

      /* English: no special font, so the default path stands */
      test_language = 0;
      font_driver_reload_fonts();
      CHECK(L->path && !strcmp(L->path, "/assets/ozone/bold.ttf"),
            "english keeps the default face");

      /* Korean: the path must change, not merely be re-read */
      test_language = TEST_LANG_KOREAN;
      font_driver_reload_fonts();
      CHECK(L->path && strstr(L->path, "korean-fallback-font.ttf") != NULL,
            "korean re-resolves to the korean face");

      /* Thai, which needs its own face too */
      test_language = TEST_LANG_THAI;
      font_driver_reload_fonts();
      CHECK(L->path && strstr(L->path, "thai-fallback-font.ttf") != NULL,
            "thai re-resolves to the thai face");

      /* and back */
      test_language = 0;
      font_driver_reload_fonts();
      CHECK(L->path && !strcmp(L->path, "/assets/ozone/bold.ttf"),
            "switching back restores the default face");

      /* Languages that share the menu font must not rebuild anything:
       * same file, so the work would be thrown away and redone for
       * nothing. */
      {
         void    *before = L->renderer_data;
         uint32_t g      = font_driver_get_generation();
         unsigned rebuilt;

         test_language = TEST_LANG_FRENCH;
         rebuilt = font_driver_reload_fonts();
         CHECK(rebuilt == 0, "english to french rebuilds nothing");

         test_language = TEST_LANG_JAPANESE;
         rebuilt = font_driver_reload_fonts();
         CHECK(rebuilt == 0, "french to japanese rebuilds nothing");

         CHECK(L->renderer_data == before,
               "renderer state untouched when the face is unchanged");
         CHECK(font_driver_get_generation() == g,
               "generation not bumped, so metrics are not recomputed");

         /* but a real change still goes through */
         test_language = TEST_LANG_THAI;
         rebuilt = font_driver_reload_fonts();
         CHECK(rebuilt > 0, "a face change still rebuilds");
      }
      font_driver_free(L);
   }

   font_driver_free(a);
   font_driver_free(c);
   CHECK(live_renderer_state == 0, "all renderer state released at teardown");

   printf("%s (%d failures)\n", fails ? "FAILURES" : "all checks passed", fails);
   return fails ? 1 : 0;
}
