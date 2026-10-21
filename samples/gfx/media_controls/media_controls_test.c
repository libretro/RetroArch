#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include "../../../gfx/gfx_media_controls.h"
#include "../../../menu/menu_media_controls.h"
#include "../../../gfx/video_defines.h"

static unsigned failures;
static bool drawing;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #c); failures++; } } while (0)
typedef struct fake_player
{
   gfx_thumbnail_playback_state_t state;
   int64_t last_seek;
   unsigned seeks, pauses;
   bool available;
} fake_player_t;
static bool fake_state(void *p, int64_t now, gfx_thumbnail_playback_state_t *out)
{ CHECK(!drawing); (void)now; *out = ((fake_player_t*)p)->state; return ((fake_player_t*)p)->available; }
static bool fake_pause(void *p, bool paused, int64_t now)
{ fake_player_t *f = (fake_player_t*)p; (void)now; f->state.paused = paused; f->pauses++; return true; }
static bool fake_seek(void *p, int64_t target, int64_t now)
{ fake_player_t *f = (fake_player_t*)p; (void)now; f->last_seek = target; f->state.position_us = target; f->seeks++; return true; }
/* The thumbnail adapter is not used by this CPU/texture-neutral oracle. */
bool gfx_thumbnail_get_playback_state(gfx_thumbnail_t *t, retro_time_t n, gfx_thumbnail_playback_state_t *s)
{ (void)t; (void)n; (void)s; abort(); return false; }
bool gfx_thumbnail_set_paused(gfx_thumbnail_t *t, bool p, retro_time_t n)
{ (void)t; (void)p; (void)n; abort(); return false; }
bool gfx_thumbnail_seek(gfx_thumbnail_t *t, int64_t p, retro_time_t n)
{ (void)t; (void)p; (void)n; abort(); return false; }

static unsigned quads, captions;
static bool dialog_open;

bool menu_input_dialog_get_display_kb(void) { return dialog_open; }

void gfx_display_draw_quad(gfx_display_t *display, void *data,
      unsigned dims, int x, int y, unsigned rect_dims,
      unsigned ref_dims, float *color, uintptr_t *texture)
{
   (void)display; (void)data; (void)ref_dims; (void)color; (void)texture;
   CHECK(x >= 0 && y >= 0);
   CHECK((unsigned)x + VIDEO_SCALE_W(rect_dims) <= VIDEO_SCALE_W(dims));
   CHECK((unsigned)y + VIDEO_SCALE_H(rect_dims) <= VIDEO_SCALE_H(dims));
   quads++;
}

void gfx_display_draw_text(const font_data_t *font, const char *text,
      float x, float y, unsigned dims, uint32_t color,
      enum text_alignment align, float scale, bool shadows,
      float offset, bool outside)
{
   (void)font; (void)color; (void)align; (void)shadows; (void)offset; (void)outside;
   CHECK(text && *text && scale > 0.0f);
   CHECK(x >= 0.0f && x <= VIDEO_SCALE_W(dims));
   CHECK(y >= 0.0f && y <= VIDEO_SCALE_H(dims));
   captions++;
}

static void failed_seek_transport(void)
{
   gfx_media_controls_t c;
   gfx_media_target_t target;
   fake_player_t f;
   memset(&c, 0, sizeof(c)); memset(&target, 0, sizeof(target));
   memset(&f, 0, sizeof(f));
   f.available = f.state.seekable = f.state.paused = true;
   f.state.position_us = 10000000;
   f.state.seek_failed = true;
   f.state.seek_target_us = 15000000;
   target.user = &f; target.identity = &f;
   target.state = fake_state; target.pause = fake_pause; target.seek = fake_seek;
   gfx_media_controls_update(&c, &target, 640, 480, 1);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_SEEK_FORWARD, 2));
   CHECK(f.last_seek == 20000000);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_SEEK_BACK, 3));
   CHECK(f.last_seek == 10000000);
}

static void menu_transport(void)
{
   menu_media_controls_t *m = (menu_media_controls_t*)calloc(1, sizeof(*m));
   fake_player_t f;
   gfx_media_target_t target;
   menu_input_pointer_t pointer;
   font_data_t font;
   unsigned before, old_quads;
   int x, y;
   CHECK(m != NULL); if (!m) return;
   memset(&f, 0, sizeof(f)); memset(&target, 0, sizeof(target));
   memset(&pointer, 0, sizeof(pointer)); memset(&font, 0, sizeof(font));
   font.size = 20.0f;
   f.available = f.state.seekable = true;
   f.state.duration_us = 100000000;
   f.state.position_us = 10000000;
   target.user = &f; target.identity = &f;
   target.state = fake_state; target.pause = fake_pause; target.seek = fake_seek;
   pointer.type = MENU_POINTER_DISABLED;
   menu_media_controls_update(m, &target, 640, 480, &pointer, 1000);
   CHECK(menu_media_controls_action(m, MENU_ACTION_OK, 1001));
   CHECK(f.state.paused);
   CHECK(menu_media_controls_action(m, MENU_ACTION_DOWN, 1002));
   menu_media_controls_update(m, &target, 640, 480, &pointer, 1003);
   CHECK(m->controls.row_focus);
   before = f.seeks;
   CHECK(menu_media_controls_action(m, MENU_ACTION_RIGHT, 1004));
   CHECK(f.seeks == before && m->controls.focus == 1);
   CHECK(menu_media_controls_action(m, MENU_ACTION_OK, 1005));
   CHECK(f.last_seek == 5000000);
   CHECK(menu_media_controls_action(m, MENU_ACTION_UP, 1006));
   CHECK(!m->controls.row_focus);
   CHECK(menu_media_controls_action(m, MENU_ACTION_INFO, 1007)); /* no browser fall-through */
   dialog_open = true;
   before = f.pauses;
   CHECK(!menu_media_controls_action(m, MENU_ACTION_OK, 1008));
   CHECK(f.pauses == before);
   dialog_open = false;
   pointer.type = MENU_POINTER_MOUSE;
   pointer.pos = VIDEO_POS_PACK(0, 0);
   menu_media_controls_update(m, &target, 640, 480, &pointer, 1009);
   x = m->controls.layout.timeline.x + m->controls.layout.timeline.w / 2;
   y = m->controls.layout.timeline.y + 1;
   pointer.pos = VIDEO_POS_PACK(x, y);
   pointer.flags = MENU_INP_PTR_FLG_PRESSED;
   before = f.seeks;
   menu_media_controls_update(m, &target, 640, 480, &pointer, 1010);
   CHECK(m->controls.dragging && f.seeks == before);
   CHECK(menu_media_controls_release(m, (unsigned)x, (unsigned)y, 1011));
   CHECK(f.seeks == before + 1 && f.last_seek == 50000000);
   pointer.flags = 0;
   menu_media_controls_update(m, &target, 640, 480, &pointer, 1012);
   quads = captions = 0;
   drawing = true;
   menu_media_controls_draw(m, NULL, NULL, VIDEO_SCALE_PACK(640,480), &font);
   drawing = false;
   CHECK(quads >= 6 && captions == 5);
   old_quads = quads;
   menu_media_controls_cancel(m);
   CHECK(!menu_media_controls_release(m, (unsigned)x, (unsigned)y, 1013));
   drawing = true;
   menu_media_controls_draw(m, NULL, NULL, VIDEO_SCALE_PACK(640,480), &font);
   drawing = false;
   CHECK(quads == old_quads);
   pointer.flags = MENU_INP_PTR_FLG_PRESSED;
   menu_media_controls_update(m, &target, 640, 480, &pointer, 1014);
   before = f.seeks;
   CHECK(menu_media_controls_release(m, (unsigned)x, (unsigned)y, 1015));
   CHECK(f.seeks == before); /* reset/entry held release is consumed */
   CHECK(!menu_media_controls_action(m, MENU_ACTION_CANCEL, 1016));
   free(m);
}

static void transport(void)
{
   fake_player_t f;
   gfx_media_controls_t c;
   gfx_media_target_t target;
   unsigned before;
   int x, y;
   memset(&f, 0, sizeof(f)); memset(&target, 0, sizeof(target));
   gfx_media_controls_reset(&c);
   f.available = f.state.seekable = true;
   f.state.duration_us = 100000000;
   f.state.position_us = 10000000;
   target.user = &f;
   target.identity = &f;
   target.state = fake_state; target.pause = fake_pause; target.seek = fake_seek;
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1000));
   CHECK(c.visible && c.layout.timeline.w > 0);
   gfx_media_controls_pointer(&c, c.layout.timeline.x + 2,
         c.layout.timeline.y + 1, true, true, 1000);
   gfx_media_controls_release(&c, c.layout.timeline.x + 2,
         c.layout.timeline.y + 1, 1000);
   CHECK(f.seeks == 0); /* held opener cannot seek */
   gfx_media_controls_pointer(&c, 0, 0, false, true, 1000);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_ACTIVATE, 1001));
   CHECK(f.state.paused && f.pauses == 1);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_NEXT, 1002));
   CHECK(f.last_seek == 15000000);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_NEXT, 1003));
   CHECK(f.last_seek == 20000000); /* refresh state between repeats */
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_FOCUS, 1004));
   gfx_media_controls_pointer(&c, 0, 0, false, false, 1004);
   CHECK(c.row_focus); /* gamepad focus survives a disabled mouse */
   before = f.seeks;
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_NEXT, 1005));
   CHECK(c.focus == 1 && f.seeks == before);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_ACTIVATE, 1006));
   CHECK(f.last_seek == 15000000);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_STOP, 1007));
   CHECK(f.state.paused && f.last_seek == 0);
   gfx_media_controls_action(&c, GFX_MEDIA_UNFOCUS, 1008);
   gfx_media_controls_action(&c, GFX_MEDIA_ACTIVATE, 1009);
   CHECK(!f.state.paused);
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 4000010));
   CHECK(!c.visible);
   gfx_media_controls_pointer(&c, 1, 1, false, true, 4000011);
   CHECK(c.visible);
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 4000012));
   x = c.layout.timeline.x + c.layout.timeline.w / 2;
   y = c.layout.timeline.y + 1;
   before = f.seeks;
   gfx_media_controls_pointer(&c, x, y, true, true, 4000013);
   CHECK(c.dragging && f.seeks == before);
   gfx_media_controls_pointer(&c, x + 30, y, true, true, 4000014);
   CHECK(f.seeks == before); /* drag is local until release */
   gfx_media_controls_release(&c, x, y, 4000015);
   CHECK(f.seeks == before + 1 && f.last_seek == 50000000);
   gfx_media_controls_pointer(&c, x, y, true, true, 4000016);
   gfx_media_controls_cancel(&c);
   before = f.seeks;
   gfx_media_controls_release(&c, x, y, 4000017);
   CHECK(f.seeks == before && !c.dragging);
   gfx_media_controls_pointer(&c, x, y, false, true, 4000018);
   gfx_media_controls_pointer(&c, x, y, true, true, 4000019);
   gfx_media_controls_release(&c, -1000, y, 4000020);
   CHECK(f.last_seek == 0); /* captured drag clamps outside the bar */
   before = f.pauses;
   gfx_media_controls_pointer(&c, 1, 1, true, true, 4000021);
   gfx_media_controls_release(&c, c.layout.buttons[0].x + 1,
         c.layout.buttons[0].y + 1, 4000022);
   CHECK(f.pauses == before); /* a picture drag is not a button click */
   before = f.seeks;
   gfx_media_controls_pointer(&c, 1, 1, true, true, 4000022);
   gfx_media_controls_release(&c, x, y, 4000022);
   CHECK(f.seeks == before); /* picture drag cannot acquire the timeline */
   f.available = false;
   CHECK(!gfx_media_controls_update(&c, &target, 640, 480, 4000023));
   CHECK(!c.target.user && !c.dragging);
   f.available = true;
   target.identity = &c;
   CHECK(gfx_media_controls_update(&c, &target, 320, 240, 4000024));
   CHECK(c.focus == 0 && c.visible); /* framebuffer-sized backend */
   CHECK(!gfx_media_controls_update(&c, &target, 1, 1, 4000025));
   CHECK(!c.target.user);
}

#ifdef HAVE_THREADS
typedef struct mailbox_test
{
   gfx_media_mailbox_t mailbox;
   retro_atomic_int_t done, bad, seen;
} mailbox_test_t;
static void mailbox_reader(void *user)
{
   mailbox_test_t *t = (mailbox_test_t*)user;
   gfx_media_view_t v;
   do
   {
      if (gfx_media_controls_consume(&t->mailbox, &v))
      {
         if (v.playback.duration_us != v.playback.position_us + 12345
               || v.drag_us != v.playback.position_us + 17
               || v.layout.panel.x != (int)v.playback.position_us)
            retro_atomic_store_release_int(&t->bad, 1);
         retro_atomic_fetch_add_int(&t->seen, 1);
      }
   } while (!retro_atomic_load_acquire_int(&t->done));
}
static void mailbox_races(void)
{
   mailbox_test_t *t = (mailbox_test_t*)calloc(1, sizeof(*t));
   gfx_media_controls_t c;
   sthread_t *thread;
   unsigned i;
   CHECK(t != NULL); if (!t) return;
   gfx_media_controls_reset(&c);
   c.target.user = &c;
   thread = sthread_create(mailbox_reader, t);
   CHECK(thread != NULL); if (!thread) { free(t); return; }
   for (i = 1; i <= 50000; i++)
   {
      c.playback.position_us = i;
      c.playback.duration_us = i + 12345;
      c.drag_us = i + 17;
      c.layout.panel.x = (int)i;
      gfx_media_controls_publish(&c, &t->mailbox);
   }
   retro_atomic_store_release_int(&t->done, 1);
   sthread_join(thread);
   CHECK(!retro_atomic_load_acquire_int(&t->bad));
   CHECK(retro_atomic_load_acquire_int(&t->seen) > 0);
   free(t);
}
#endif

int main(void)
{
   transport();
   failed_seek_transport();
   menu_transport();
#ifdef HAVE_THREADS
   mailbox_races();
#endif
   printf("media controls: %u failure(s)\n", failures);
   return failures ? 1 : 0;
}
