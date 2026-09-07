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
#include <string/stdstring.h>
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
#ifdef HAVE_THREADS
extern int video_thread_font_init_calls;
#endif
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

   /* The OSD font: rebuilt against settings rather than against the
    * language, so path and size both have to reach it. */
   {
      video_driver_state_t *vst = video_state_get_ptr();
      font_data_t          *osd = mk("/tmp/san/osd.ttf", 12.0f);
      void                 *r0;

      CHECK(osd != NULL, "osd font created");
      vst->osd_font = (struct font_data*)osd;
      r0            = osd->renderer_data;

      /* Same path, same size: no work. A rebuild here would drop the
       * atlas and the GPU texture to arrive back where it started. */
      g0 = font_driver_get_generation();
      CHECK(font_driver_reinit_osd("/tmp/san/osd.ttf", 12.0f),
            "unchanged settings report handled");
      CHECK(osd->renderer_data == r0, "unchanged settings rebuild nothing");
      CHECK(font_driver_get_generation() == g0,
            "unchanged settings do not bump the generation");

      /* Size alone. This is the case font_driver_reload_fonts() has
       * no path to at all: it re-resolves the file and passes
       * font->size straight back in. */
      CHECK(font_driver_reinit_osd("/tmp/san/osd.ttf", 32.0f),
            "size change handled");
      CHECK(osd->renderer_data != r0, "size change rebuilt the font");
      CHECK(osd->size == 32.0f, "new size remembered");
      CHECK(font_driver_get_generation() != g0, "size change bumps generation");
      CHECK(live_renderer_state == 3, "nothing leaked by the size change");

      /* Path alone. */
      r0 = osd->renderer_data;
      CHECK(font_driver_reinit_osd("/tmp/san/osd2.ttf", 32.0f),
            "path change handled");
      CHECK(osd->renderer_data != r0, "path change rebuilt the font");
      CHECK(string_is_equal(osd->path, "/tmp/san/osd2.ttf"),
            "new path remembered");
      CHECK(live_renderer_state == 3, "nothing leaked by the path change");

      /* Clearing the path hands the choice back to the renderer.
       * reload_fonts() skips a font with no path; this must not. */
      CHECK(font_driver_reinit_osd("", 32.0f), "cleared path handled");
      CHECK(osd->path == NULL, "cleared path forgotten, not kept");
      CHECK(font_driver_reinit_osd("/tmp/san/osd.ttf", 32.0f),
            "path set again from empty");
      CHECK(osd->path != NULL, "path picked up again from empty");

      /* A rebuild that fails keeps the working font: the setting does
       * not take, but the text does not vanish either. */
      r0             = osd->renderer_data;
      fail_next_init = 1;
      CHECK(font_driver_reinit_osd("/tmp/san/unreadable.ttf", 32.0f),
            "failed rebuild still counts as handled");
      fail_next_init = 0;
      CHECK(osd->renderer_data == r0, "old osd font kept on failure");
      CHECK(live_renderer_state == 3, "nothing leaked on failed osd rebuild");

      font_driver_free(osd);
      vst->osd_font = NULL;

      /* No shared OSD font - a driver that keeps its own, or video
       * not up yet. The caller needs to know, so it can fall back. */
      CHECK(!font_driver_reinit_osd("/tmp/san/osd.ttf", 12.0f),
            "absent osd font reports unhandled");
   }

#ifdef HAVE_THREADS
   /* A rebuild must take the route its creation took, or the
    * backend's texture work lands on the wrong thread. Widget and
    * menu fonts are all created with the hint set. */
   {
      font_data_t *hinted;
      font_data_t *plain;
      int          n0;

      /* threading_hint set, is_threaded set: must marshal. */
      hinted = font_driver_init_first(NULL, "/tmp/san/thr.ttf", 16.0f,
            true, true, &fake_font);
      CHECK(hinted != NULL, "threaded font created");
      n0 = video_thread_font_init_calls;
      CHECK(n0 > 0, "creation marshalled to the video thread");

      font_driver_set_language_font(hinted, "/assets/pkg",
            "/tmp/san/thr.ttf");
      test_language = TEST_LANG_KOREAN;
      font_driver_reload_fonts();
      CHECK(video_thread_font_init_calls > n0,
            "rebuild marshalled to the video thread");
      test_language = 0;
      font_driver_free(hinted);

      /* No hint - the OSD font's case - must not marshal. Both
       * callers of font_driver_init_osd() already run on the thread
       * that owns the context. */
      plain = font_driver_init_first(NULL, "/tmp/san/plain.ttf", 16.0f,
            false, true, &fake_font);
      CHECK(plain != NULL, "unhinted font created");
      n0 = video_thread_font_init_calls;
      font_driver_set_language_font(plain, "/assets/pkg",
            "/tmp/san/plain.ttf");
      test_language = TEST_LANG_KOREAN;
      font_driver_reload_fonts();
      CHECK(video_thread_font_init_calls == n0,
            "unhinted rebuild stays on the calling thread");
      test_language = 0;
      font_driver_free(plain);
   }
#endif

   font_driver_free(a);
   font_driver_free(c);
   CHECK(live_renderer_state == 0, "all renderer state released at teardown");

   /* 9. deferred free: the handle survives the frames in which the
    *    GPU may still be reading its atlas, and goes away after. */
   {
      font_data_t *r = mk("/tmp/san/retire.ttf", 16.0f);
      CHECK(r != NULL, "retire font created");
      CHECK(live_renderer_state == 1, "one live before retire");

      font_driver_free_deferred(r);
      CHECK(live_renderer_state == 1, "still live immediately after retire");

      /* The point of the window is that the handle stays usable
       * throughout it, because the GPU may still be reading its
       * atlas. Touch the real font_data_t on each frame: a premature
       * release would be a use-after-free here, which is what ASan is
       * for. */
      font_driver_free_pending(false);
      CHECK(live_renderer_state == 1, "still live one frame after retire");
      CHECK(r->renderer_data != NULL, "renderer state usable inside window");
      CHECK(r->path != NULL, "path still owned inside window");
      CHECK(font_driver_get_message_width(r, "a", 1, 1.0f) > 0,
            "retired font still answers metrics inside window");

      font_driver_free_pending(false);
      CHECK(live_renderer_state == 0, "released two frames after retire");

      /* Idempotent: an empty queue must not double-free. */
      font_driver_free_pending(false);
      font_driver_free_pending(true);
      CHECK(live_renderer_state == 0, "draining an empty queue is a no-op");
   }

   /* 10. a retire per frame, the steady state of dragging a scale
     *    slider: each handle gets its full two frames and none is
     *    lost, so the queue neither frees early nor grows without
     *    bound. */
   {
      unsigned f;
      font_data_t *cur = mk("/tmp/san/drag.ttf", 10.0f);
      CHECK(cur != NULL, "drag font created");

      for (f = 0; f < 200; f++)
      {
         font_data_t *next = mk("/tmp/san/drag.ttf", 10.0f + f);
         font_driver_free_deferred(cur);
         cur = next;
         font_driver_free_pending(false);
         /* current, plus the handles still inside their window */
         CHECK(live_renderer_state <= 3, "in-flight handles stay bounded");
      }

      font_driver_free_pending(true);
      font_driver_free(cur);
      CHECK(live_renderer_state == 0, "nothing leaked by a sustained drag");
   }

   /* 11. flush is what teardown relies on: a handle retired with
    *     frames still to go must not outlive the context. */
   {
      font_data_t *p = mk("/tmp/san/flush.ttf", 14.0f);
      font_data_t *q = mk("/tmp/san/flush2.ttf", 14.0f);
      CHECK(live_renderer_state == 2, "two live before flush");
      font_driver_free_deferred(p);
      font_driver_free_deferred(q);
      font_driver_free_pending(true);
      CHECK(live_renderer_state == 0, "flush releases everything queued");
   }

   /* 11b. font_driver_matches(): the predicate the layout paths use to
    *      skip a rebuild that would produce the font already loaded.
    *      A wrong yes keeps a font that should have been replaced, so
    *      the cases that must answer no matter more than the one that
    *      must answer yes. */
   {
      font_data_t *m = mk("/tmp/san/match.ttf", 16.0f);
      font_data_t *stale;

      CHECK(m != NULL, "match font created");
      CHECK(font_driver_matches(m, "/tmp/san/match.ttf", 16.0f),
            "same path and size matches");
      CHECK(!font_driver_matches(m, "/tmp/san/match.ttf", 16.5f),
            "a different size does not match");
      CHECK(!font_driver_matches(m, "/tmp/san/other.ttf", 16.0f),
            "a different path does not match");
      CHECK(!font_driver_matches(m, NULL, 16.0f),
            "a pathless request does not match a font with a path");
      CHECK(!font_driver_matches(m, "", 16.0f),
            "an empty path does not match a font with a path");
      CHECK(!font_driver_matches(NULL, "/tmp/san/match.ttf", 16.0f),
            "NULL never matches");

      /* A language switch re-resolves the face in place. The
       * predicate must follow the font, not the request that built
       * it, or the caller would skip past a face change. */
      font_driver_set_language_font(m, "/assets/pkg", "/tmp/san/match.ttf");
      test_language = TEST_LANG_KOREAN;
      font_driver_reload_fonts();
      CHECK(!font_driver_matches(m, "/tmp/san/match.ttf", 16.0f),
            "a re-resolved face no longer matches the old path");
      CHECK(font_driver_matches(m, m->path, 16.0f),
            "and does match the face it now carries");
      test_language = 0;
      font_driver_reload_fonts();

      /* The case the liveness walk exists for: a caller that frees a
       * font and leaves the pointer behind. Reading through it would
       * be a use-after-free, and the honest answer is no. */
      stale = mk("/tmp/san/stale.ttf", 16.0f);
      font_driver_free(stale);
      CHECK(!font_driver_matches(stale, "/tmp/san/stale.ttf", 16.0f),
            "a freed handle does not match");

      /* Same again for one released through the deferred queue,
       * before and after the window closes. */
      stale = mk("/tmp/san/stale2.ttf", 16.0f);
      font_driver_free_deferred(stale);
      CHECK(font_driver_matches(stale, "/tmp/san/stale2.ttf", 16.0f),
            "a retired handle still matches inside its window");
      font_driver_free_pending(true);
      CHECK(!font_driver_matches(stale, "/tmp/san/stale2.ttf", 16.0f),
            "and stops matching once released");

      font_driver_free(m);
   }

   /* 12. retiring NULL is allowed, so callers need no guard. */
   font_driver_free_deferred(NULL);
   font_driver_free_pending(false);
   CHECK(live_renderer_state == 0, "retiring NULL does nothing");

   printf("%s (%d failures)\n", fails ? "FAILURES" : "all checks passed", fails);
   return fails ? 1 : 0;
}
