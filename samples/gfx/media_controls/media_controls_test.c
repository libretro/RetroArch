#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rthreads/rthreads.h>
#include "../../../gfx/gfx_media_controls.h"
#include "../../../gfx/gfx_media_display.h"
#include "../../../menu/menu_media_controls.h"
#include "../../../menu/menu_media_browser.h"
#include "../../../gfx/video_defines.h"
#include "../../../configuration.h"
#include "../../../tasks/task_subtitle.h"

static struct menu_state test_menu;
struct menu_state *menu_state_get_ptr(void) { return &test_menu; }

static settings_t test_settings;
settings_t *config_get_ptr(void) { return &test_settings; }

static unsigned failures;
static bool drawing;
static unsigned settings_opened, settings_hidden, settings_shown;
static bool settings_refused, settings_walk_pending;
bool menu_dirwalk_pending(void) { return settings_walk_pending; }
int generic_action_ok_displaylist_push(const char *path, const char *new_path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   (void)path; (void)new_path; (void)label; (void)type;
   (void)idx; (void)entry_idx; (void)action_type;
   settings_opened++;
   return settings_refused ? -1 : 0;
}

#define CHECK(c) do { if (!(c)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #c); failures++; } } while (0)
static gfx_media_display_owner_t desktop_owner;
static video_display_server_t desktop_server;
static unsigned desktop_begin, desktop_end, desktop_checked;
static int desktop_data, desktop_other, desktop_token;
static bool desktop_valid = true, desktop_refused;
static void *desktop_acquire(void *data, unsigned num, unsigned den)
{
   CHECK(data && num && den);
   desktop_begin++;
   return desktop_refused ? NULL : &desktop_token;
}
static void desktop_release(void *data, void *lease)
{
   CHECK(data && lease == &desktop_token);
   desktop_end++;
}
static bool desktop_current(void *data, void *lease, unsigned num, unsigned den)
{
   CHECK(data && lease == &desktop_token);
   CHECK(num && den);
   desktop_checked++;
   return desktop_valid;
}
void video_display_server_media_refresh(unsigned num, unsigned den)
{
   gfx_media_display_owner_update(&desktop_owner, &desktop_server,
         &desktop_data, num, den, false);
}
static void display_owner_lifecycle(void)
{
   gfx_media_display_owner_t owner;
   unsigned i;
   memset(&owner, 0, sizeof(owner));
   memset(&desktop_server, 0, sizeof(desktop_server));
   desktop_server.begin_media_refresh = desktop_acquire;
   desktop_server.end_media_refresh = desktop_release;
   desktop_server.media_refresh_current = desktop_current;
   desktop_begin = desktop_end = desktop_checked = 0;
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, false);
   for (i = 0; i < 100; i++)
      gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, false);
   CHECK(desktop_begin == 1 && !desktop_end && !desktop_checked);
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, true);
   CHECK(desktop_begin == 1 && desktop_checked == 1 && !desktop_end);
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 30, 1, false);
   CHECK(desktop_begin == 1 && desktop_checked == 2 && !desktop_end);
   desktop_valid = false;
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, true);
   CHECK(desktop_begin == 2 && desktop_end == 1);
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_other, 24, 1, false);
   CHECK(desktop_begin == 3 && desktop_end == 2);
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_other, 25, 1, false);
   CHECK(desktop_begin == 4 && desktop_end == 3);
   gfx_media_display_owner_reset(&owner);
   gfx_media_display_owner_reset(&owner);
   CHECK(desktop_end == 4 && !owner.lease && !owner.server);
   desktop_refused = true;
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, false);
   for (i = 0; i < 100; i++)
      gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, false);
   CHECK(desktop_begin == 5 && desktop_end == 4);
   gfx_media_display_owner_update(&owner, &desktop_server, &desktop_data, 24, 1, true);
   CHECK(desktop_begin == 6 && desktop_end == 4);
   gfx_media_display_owner_reset(&owner);
   desktop_refused = false;
   desktop_valid = true;
}

typedef struct fake_player
{
   gfx_thumbnail_playback_state_t state;
   int64_t last_seek;
   unsigned seeks, pauses, advances;
   int last_direction;
   bool available, pause_refused, seek_refused;
} fake_player_t;
static bool fake_state(void *p, int64_t now, gfx_thumbnail_playback_state_t *out)
{ CHECK(!drawing); (void)now; *out = ((fake_player_t*)p)->state; return ((fake_player_t*)p)->available; }
static bool fake_pause(void *p, bool paused, int64_t now)
{
   fake_player_t *f = (fake_player_t*)p;
   (void)now;
   f->pauses++;
   if (f->pause_refused) return false;
   f->state.paused = paused;
   return true;
}
static bool fake_seek(void *p, int64_t target, int64_t now)
{ fake_player_t *f = (fake_player_t*)p; (void)now; if (f->seek_refused) return false; f->last_seek = target; f->state.position_us = target; f->seeks++; return true; }
static bool fake_step(void *user, int direction)
{
   fake_player_t *f = (fake_player_t*)user;
   f->last_direction = direction;
   f->advances++;
   return true;
}

/* The thumbnail adapter is not used by this CPU/texture-neutral oracle. */
bool gfx_thumbnail_get_playback_state(gfx_thumbnail_t *t, retro_time_t n, gfx_thumbnail_playback_state_t *s)
{ (void)t; (void)n; (void)s; abort(); return false; }
bool gfx_thumbnail_set_paused(gfx_thumbnail_t *t, bool p, retro_time_t n)
{ (void)t; (void)p; (void)n; abort(); return false; }
bool gfx_thumbnail_seek(gfx_thumbnail_t *t, int64_t p, retro_time_t n)
{ (void)t; (void)p; (void)n; abort(); return false; }

bool gfx_thumbnail_set_repeat(gfx_thumbnail_t *t, bool repeat)
{ (void)t; (void)repeat; return false; }
bool gfx_thumbnail_take_completed(gfx_thumbnail_t *t)
{ (void)t; return false; }
bool gfx_thumbnail_has_file(const gfx_thumbnail_t *t, const char *path)
{ (void)t; (void)path; abort(); return false; }
void gfx_thumbnail_reset(gfx_thumbnail_t *t) { (void)t; abort(); }
void gfx_thumbnail_request_file(const char *path, gfx_thumbnail_t *t, unsigned n)
{ (void)path; (void)t; (void)n; abort(); }
bool gfx_thumbnail_set_content_image(gfx_thumbnail_path_data_t *p,
      const char *dir, const char *name)
{ (void)p; (void)dir; (void)name; abort(); return false; }
task_subtitle_catalog_t *task_subtitle_discover(const char *path)
{ (void)path; abort(); return NULL; }
int task_subtitle_catalog_status(const task_subtitle_catalog_t *catalog)
{ if (catalog) abort(); return -1; }
size_t task_subtitle_catalog_count(const task_subtitle_catalog_t *catalog)
{ if (catalog) abort(); return 0; }
const char *task_subtitle_catalog_path(const task_subtitle_catalog_t *catalog, size_t index)
{ (void)catalog; (void)index; abort(); return NULL; }
const task_subtitle_entry_t *task_subtitle_catalog_entry(const task_subtitle_catalog_t *catalog, size_t index)
{ (void)catalog; (void)index; abort(); return NULL; }
task_subtitle_request_t *task_subtitle_load_entry(const task_subtitle_entry_t *entry)
{ if (entry) abort(); return NULL; }
void task_subtitle_catalog_release(task_subtitle_catalog_t *catalog)
{ if (catalog) abort(); }
const char *msg_hash_to_str(enum msg_hash_enums msg)
{ (void)msg; return "test"; }

enum image_type_enum image_texture_get_type(const char *path)
{
   const char *ext = path ? strrchr(path, '.') : NULL;
   if (ext && !strcmp(ext, ".mp4")) return IMAGE_TYPE_MP4;
   if (ext && !strcmp(ext, ".webm")) return IMAGE_TYPE_WEBM;
   return IMAGE_TYPE_PNG;
}

static unsigned navigations;
static void navigated(void *user, bool scroll)
{
   CHECK(user == &test_menu && scroll);
   navigations++;
}
static void folder_queue(void)
{
   static menu_list_t lists;
   static menu_ctx_driver_t driver;
   file_list_t *selection[1], *stack[1];
   menu_media_queue_t *queue;
   selection[0] = (file_list_t*)calloc(1, sizeof(file_list_t));
   stack[0] = (file_list_t*)calloc(1, sizeof(file_list_t));
   CHECK(selection[0] && stack[0]);
   lists.selection_buf = selection; lists.selection_buf_size = 1;
   lists.menu_stack = stack; lists.menu_stack_size = 1;
   test_menu.entries.list = &lists;
   test_menu.driver_ctx = &driver;
   test_menu.userdata = &test_menu;
   driver.navigation_set = navigated;
   CHECK(file_list_append(stack[0], "/shows", "", FILE_TYPE_DIRECTORY, 0, 0));
   CHECK(file_list_append(selection[0], "episode10.mp4", "", FILE_TYPE_MOVIE, 0, 0));
   CHECK(file_list_append(selection[0], "episode2.mp4", "", FILE_TYPE_MOVIE, 0, 0));
   CHECK(file_list_append(selection[0], "cover.png", "", FILE_TYPE_IMAGE, 0, 0));
   CHECK(file_list_append(selection[0], "episode1.mp4", "", FILE_TYPE_MOVIE, 0, 0));
   CHECK(file_list_append(selection[0], "episode3.webm", "", FILE_TYPE_MOVIE, 0, 0));
   CHECK(file_list_append(selection[0], "directory4.mp4", "", FILE_TYPE_DIRECTORY, 0, 0));
   test_menu.selection_ptr = 1;
   queue = menu_media_queue_new(&test_menu);
   CHECK(queue);
   selection[0]->list[4].type = FILE_TYPE_DIRECTORY;
   CHECK(!menu_media_queue_step(queue, &test_menu, 1));
   CHECK(test_menu.selection_ptr == 1 && !navigations);
   selection[0]->list[4].type = FILE_TYPE_MOVIE;
   CHECK(menu_media_queue_step(queue, &test_menu, 1) && test_menu.selection_ptr == 4);
   CHECK(file_list_append(selection[0], "episode4.mp4", "", FILE_TYPE_MOVIE, 0, 0));
   CHECK(menu_media_queue_step(queue, &test_menu, 1) && test_menu.selection_ptr == 0);
   CHECK(!menu_media_queue_step(queue, &test_menu, 1));
   CHECK(menu_media_queue_step(queue, &test_menu, -1) && test_menu.selection_ptr == 4);
   CHECK(menu_media_queue_step(queue, &test_menu, -1) && test_menu.selection_ptr == 1);
   CHECK(menu_media_queue_step(queue, &test_menu, -1) && test_menu.selection_ptr == 3);
   CHECK(!menu_media_queue_step(queue, &test_menu, -1));
   CHECK(navigations == 5);
   test_menu.selection_ptr = 2;
   CHECK(!menu_media_queue_step(queue, &test_menu, 1));
   menu_media_queue_free(queue);
   CHECK(!menu_media_queue_new(&test_menu));
   file_list_free(selection[0]); file_list_free(stack[0]);
   test_menu.entries.list = NULL;
   test_menu.driver_ctx = NULL;
}

/* Loading is tested separately; this drawing oracle publishes cue views. */
task_subtitle_request_t *task_subtitle_load(const char *path)
{ (void)path; abort(); return NULL; }
int task_subtitle_status(const task_subtitle_request_t *request)
{ (void)request; abort(); return -1; }
rsubtitle_srt_t *task_subtitle_take(task_subtitle_request_t *request)
{ (void)request; abort(); return NULL; }
void task_subtitle_release(task_subtitle_request_t *request)
{ if (request) abort(); }

int font_driver_get_message_width(void *font, const char *text,
      size_t length, float scale)
{
   (void)font;
   (void)text;
   return (int)(length * 8 * scale);
}

static unsigned quads, captions, seek_captions;
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
   if (strcmp(text, "-12") == 0) seek_captions |= 1;
   if (strcmp(text, "+12") == 0) seek_captions |= 2;
   CHECK(x >= 0.0f && x <= VIDEO_SCALE_W(dims));
   CHECK(y >= 0.0f && y <= VIDEO_SCALE_H(dims));
   captions++;
}

static void explicit_playback(void)
{
   gfx_media_controls_t c;
   gfx_media_target_t target;
   fake_player_t f;
   memset(&c, 0, sizeof(c));
   memset(&target, 0, sizeof(target));
   memset(&f, 0, sizeof(f));
   f.available = true;
   target.user = &f;
   target.identity = &f;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1));
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_FOCUS, 2));
   c.focus = 3;
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_PLAY, 3));
   CHECK(!f.state.paused && !f.pauses && !f.seeks);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_PAUSE, 4));
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_PAUSE, 5));
   CHECK(f.state.paused && f.pauses == 1 && !f.seeks);
   f.pause_refused = true;
   CHECK(!gfx_media_controls_action(&c, GFX_MEDIA_PLAY, 6));
   CHECK(f.state.paused && f.pauses == 2);
   f.pause_refused = false;
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_PLAY, 7));
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_PLAY, 8));
   CHECK(!f.state.paused && f.pauses == 3 && !f.seeks);
   CHECK(c.row_focus && c.focus == 3);
   f.available = false;
   CHECK(!gfx_media_controls_action(&c, GFX_MEDIA_PAUSE, 9));
   CHECK(f.pauses == 3 && !c.target.user);
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
   CHECK(quads >= 9 && captions == 8);
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

static void menu_preferences(void)
{
   menu_media_controls_t *m = (menu_media_controls_t*)calloc(1, sizeof(*m));
   gfx_media_target_t target;
   fake_player_t f;
   font_data_t font;
   CHECK(m != NULL); if (!m) return;
   memset(&target, 0, sizeof(target));
   memset(&f, 0, sizeof(f));
   memset(&font, 0, sizeof(font));
   font.size = 20.0f;
   f.available = f.state.seekable = true;
   target.user = &f; target.identity = &f;
   target.state = fake_state; target.pause = fake_pause; target.seek = fake_seek;
   test_settings.uints.media_player_seek_seconds = 12;
   test_settings.uints.media_player_controls_timeout = 0;
   menu_media_controls_update(m, &target, 640, 480, NULL, 1);
   CHECK(menu_media_controls_action(m, MENU_ACTION_RIGHT, 2));
   CHECK(f.last_seek == 12000000);
   drawing = true;
   menu_media_controls_draw(m, NULL, NULL, VIDEO_SCALE_PACK(640, 480), &font);
   drawing = false;
   CHECK(seek_captions == 3);
   test_settings.uints.media_player_seek_seconds = 2;
   menu_media_controls_update(m, &target, 640, 480, NULL, 3);
   CHECK(menu_media_controls_action(m, MENU_ACTION_LEFT, 4));
   CHECK(f.last_seek == 10000000);
   CHECK(m->controls.hide_seconds == 0);
   test_settings.uints.media_player_seek_seconds = 5;
   test_settings.uints.media_player_controls_timeout = 3;
   free(m);
}

static void playback_preferences(void)
{
   gfx_media_controls_t c;
   gfx_media_target_t target;
   fake_player_t f;
   memset(&c, 0, sizeof(c));
   memset(&target, 0, sizeof(target));
   memset(&f, 0, sizeof(f));
   f.available = f.state.seekable = true;
   f.state.position_us = 20000000;
   target.user = &f;
   target.identity = &f;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   gfx_media_controls_configure(&c, 12, 1);
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1));
   CHECK(c.seek_seconds == 12 && c.hide_seconds == 1);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_SEEK_FORWARD, 2));
   CHECK(f.last_seek == 32000000);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_SEEK_BACK, 3));
   CHECK(f.last_seek == 20000000);
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1000004));
   CHECK(!c.visible);
   gfx_media_controls_configure(&c, 12, 0);
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1000005));
   CHECK(c.visible);
   target.identity = &target;
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1000006));
   CHECK(c.seek_seconds == 12 && c.hide_seconds == 0);
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 100000000));
   CHECK(c.visible);
   gfx_media_controls_configure(&c, 0, 61);
   CHECK(c.seek_seconds == 5 && c.hide_seconds == 3);
   gfx_media_controls_configure(&c, 601, 3);
   CHECK(c.seek_seconds == 5);
}

static void file_buttons(void)
{
   gfx_media_controls_t controls;
   gfx_media_target_t target;
   fake_player_t player;
   const gfx_media_rect_t *button;
   memset(&controls, 0, sizeof(controls));
   memset(&target, 0, sizeof(target));
   memset(&player, 0, sizeof(player));
   player.available = true;
   target.user = &player; target.identity = &player;
   target.state = fake_state; target.pause = fake_pause; target.seek = fake_seek;
   target.navigation_user = &player; target.step = fake_step; target.directions = 3;
   CHECK(gfx_media_controls_update(&controls, &target, 640, 480, 1));
   CHECK(controls.layout.button_count == 6);
   controls.row_focus = true; controls.focus = 3;
   CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_NEXT, 2));
   CHECK(controls.focus == 4);
   CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_ACTIVATE, 3));
   CHECK(player.last_direction == -1 && player.advances == 1);
   CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_NEXT, 4));
   CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_ACTIVATE, 5));
   CHECK(player.last_direction == 1 && player.advances == 2);
   target.directions = 1;
   CHECK(gfx_media_controls_update(&controls, &target, 640, 480, 6));
   CHECK(!gfx_media_controls_action(&controls, GFX_MEDIA_NEXT_FILE, 7));
   CHECK(player.advances == 2);
   target.directions = 3;
   CHECK(gfx_media_controls_update(&controls, &target, 640, 480, 8));
   button = &controls.layout.buttons[5];
   gfx_media_controls_pointer(&controls, 0, 0, false, true, 9);
   gfx_media_controls_pointer(&controls, button->x + 1, button->y + 1, true, true, 10);
   gfx_media_controls_release(&controls, button->x + 1, button->y + 1, 11);
   CHECK(player.last_direction == 1 && player.advances == 3);
   player.state.paused = true;
   player.state.duration_us = player.state.position_us = 1000000;
   CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_PLAY, 12));
   CHECK(player.seeks == 1 && player.last_seek == 0 && !player.state.paused);
}

static void settings_hide(void *user)
{
   CHECK(user == &test_menu);
   settings_hidden++;
}

static void settings_show(void *user)
{
   CHECK(user == &test_menu);
   settings_shown++;
}

static void settings_button(void)
{
   static menu_media_controls_t media;
   gfx_media_target_t target;
   gfx_media_view_t view;
   fake_player_t player;
   menu_list_t lists;
   file_list_t *stack[1];
   unsigned paused;
   memset(&media, 0, sizeof(media));
   memset(&target, 0, sizeof(target));
   memset(&player, 0, sizeof(player));
   memset(&lists, 0, sizeof(lists));
   memset(&view, 0, sizeof(view));
   stack[0] = (file_list_t*)calloc(1, sizeof(file_list_t));
   CHECK(stack[0]);
   lists.menu_stack = stack; lists.menu_stack_size = 1;
   test_menu.entries.list = &lists;
   CHECK(file_list_append(stack[0], "/shows", "", FILE_TYPE_DIRECTORY, 0, 0));
   player.available = true;
   media.settings_hide = settings_hide;
   media.settings_show = settings_show;
   media.settings_user = &test_menu;
   target.user = target.navigation_user = &player;
   target.identity = &player;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   for (paused = 0; paused < 2; paused++)
   {
      player.state.paused = paused != 0;
      player.pause_refused = true;
      menu_media_controls_update(&media, &target, 640, 480, NULL, 1);
      CHECK(media.controls.layout.button_count == 7);
      if (!paused)
      {
         media.controls.row_focus = true;
         media.controls.focus = 6;
         CHECK(gfx_media_controls_action(&media.controls, GFX_MEDIA_ACTIVATE, 2));
      }
      else
      {
         gfx_media_rect_t button = media.controls.layout.buttons[6];
         gfx_media_controls_pointer(&media.controls, 0, 0, false, true, 2);
         gfx_media_controls_pointer(&media.controls, button.x + 1, button.y + 1,
               true, true, 3);
         gfx_media_controls_release(&media.controls, button.x + 1, button.y + 1, 4);
      }
      CHECK(settings_opened == paused + 1 && settings_hidden == paused + 1);
      CHECK(player.state.paused == (paused != 0));
      CHECK(!media.controls.held && !media.controls.row_focus);
      CHECK(gfx_media_controls_consume(&media.mailbox, &view));
      CHECK(view.settings_preview && !view.visible);
      menu_media_controls_context_reset(&media, true);
      CHECK(gfx_media_controls_consume(&media.mailbox, &view));
      CHECK(view.settings_preview && !view.visible);
      CHECK(file_list_append(stack[0], "/settings", "", 0, 0, 0));
      menu_media_controls_settings_iterate(&media);
      CHECK(media.settings_open && !media.settings_returning);
      CHECK(!menu_media_controls_update(&media, &target, 640, 480, NULL, 3));
      CHECK(file_list_append(stack[0], "/nested", "", 0, 0, 0));
      menu_media_controls_settings_iterate(&media);
      CHECK(media.settings_open);
      file_list_pop(stack[0], NULL);
      menu_media_controls_settings_iterate(&media);
      CHECK(media.settings_open);
      file_list_pop(stack[0], NULL);
      settings_walk_pending = true;
      menu_media_controls_settings_iterate(&media);
      CHECK(media.settings_open && settings_shown == paused);
      settings_walk_pending = false;
      test_menu.flags |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      menu_media_controls_settings_iterate(&media);
      CHECK(media.settings_open && settings_shown == paused);
      test_menu.flags &= ~MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      menu_media_controls_settings_iterate(&media);
      CHECK(!media.settings_open && media.settings_returning);
      CHECK(settings_shown == paused + 1);
      menu_media_controls_update(&media, &target, 640, 480, NULL, 4);
      CHECK(!menu_media_controls_settings_active(&media));
      CHECK(player.state.paused == (paused != 0));
      CHECK(gfx_media_controls_consume(&media.mailbox, &view));
      CHECK(!view.settings_preview && view.visible);
   }
   settings_refused = true;
   CHECK(!gfx_media_controls_action(&media.controls, GFX_MEDIA_SETTINGS, 5));
   CHECK(settings_opened == 3 && settings_hidden == 2 && player.state.paused);
   CHECK(!menu_media_controls_settings_active(&media));
   settings_refused = false;
   menu_media_controls_cancel(&media);
   file_list_free(stack[0]);
   test_menu.entries.list = NULL;
}

static void desktop_refresh_lifecycle(void)
{
   static menu_media_controls_t media, other;
   gfx_media_target_t target;
   fake_player_t player;
   memset(&media, 0, sizeof(media));
   memset(&other, 0, sizeof(other));
   memset(&target, 0, sizeof(target));
   memset(&player, 0, sizeof(player));
   player.available = true;
   player.state.dims = VIDEO_SCALE_PACK(320, 240);
   player.state.cadence_num = 24;
   player.state.cadence_den = 1;
   target.user = &player;
   target.identity = &player;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   desktop_begin = desktop_end = 0;
   test_settings.bools.media_player_refresh_rate = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 1);
   CHECK(desktop_begin == 1 && !desktop_end);
   player.state.seeking = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 2);
   CHECK(desktop_begin == 1 && !desktop_end);
   player.state.seeking = false;
   test_settings.bools.media_player_refresh_rate = false;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(desktop_end == 1);
   test_settings.bools.media_player_refresh_rate = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 4);
   CHECK(desktop_begin == 2);
   test_settings.uints.crt_switch_resolution = 1;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 5);
   CHECK(desktop_end == 2);
   test_settings.uints.crt_switch_resolution = 0;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 6);
   CHECK(desktop_begin == 3);
   target.identity = &target;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 7);
   CHECK(desktop_end == 3 && desktop_begin == 4);
   menu_media_controls_cancel(&other);
   CHECK(desktop_end == 3);
   menu_media_controls_cancel(&media);
   CHECK(desktop_end == 4 && !desktop_owner.lease);
   test_settings.bools.media_player_refresh_rate = false;
}

static void display_source(void)
{
   static menu_media_controls_t media, other;
   gfx_media_target_t target;
   fake_player_t player;
   unsigned dims;
   float hz;
   memset(&media, 0, sizeof(media));
   memset(&other, 0, sizeof(other));
   memset(&target, 0, sizeof(target));
   memset(&player, 0, sizeof(player));
   player.available = true;
   player.state.dims = VIDEO_SCALE_PACK(320, 240);
   player.state.cadence_num = 24000;
   player.state.cadence_den = 1001;
   target.user = &player;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   CHECK(!gfx_media_display_source(NULL, &dims, &hz) && !dims && !hz);
   menu_media_controls_update(&media, &target, 640, 480, NULL, 1);
   CHECK(!menu_media_controls_display(&dims, &hz));
   test_settings.bools.media_player_crt_switch = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 2);
   CHECK(menu_media_controls_display(&dims, &hz)
         && dims == player.state.dims && hz > 23.97f && hz < 23.98f);
   player.state.paused = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(menu_media_controls_display(&dims, &hz));
   player.state.cadence_num = 23988;
   player.state.cadence_den = 1000;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(menu_media_controls_display(&dims, &hz) && hz > 23.97f && hz < 23.98f);
   player.state.cadence_num = 24;
   player.state.cadence_den = 1;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(menu_media_controls_display(&dims, &hz) && hz == 24.0f);
   player.state.cadence_unstable = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(!menu_media_controls_display(&dims, &hz));
   player.state.cadence_unstable = false;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(menu_media_controls_display(&dims, &hz));
   player.state.seeking = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 4);
   CHECK(menu_media_controls_display(&dims, &hz)
         && dims == player.state.dims);
   CHECK(!gfx_media_display_source(&player.state, &dims, &hz));
   player.state.seeking = false;
   player.state.seek_failed = true;
   CHECK(!gfx_media_display_source(&player.state, &dims, &hz));
   player.state.seek_failed = false;
   player.state.dims = VIDEO_SCALE_PACK(640, 360);
   menu_media_controls_update(&other, &target, 640, 480, NULL, 5);
   menu_media_controls_cancel(&media);
   CHECK(menu_media_controls_display(&dims, &hz)
         && dims == player.state.dims);
   menu_media_controls_update(&other, NULL, 640, 480, NULL, 6);
   CHECK(!menu_media_controls_display(&dims, &hz));
   menu_media_controls_update(&media, &target, 640, 480, NULL, 7);
   CHECK(menu_media_controls_display(&dims, &hz));
   CHECK(!menu_media_controls_action(&media, MENU_ACTION_CANCEL, 8));
   CHECK(!menu_media_controls_display(&dims, &hz));
   player.state.dims = VIDEO_SCALE_PACK(320, 4);
   CHECK(!gfx_media_display_source(&player.state, &dims, &hz));
   player.state.dims = VIDEO_SCALE_PACK(320, 240);
   player.state.cadence_den = 0;
   CHECK(!gfx_media_display_source(&player.state, &dims, &hz));
   player.state.cadence_den = 1;
   player.state.cadence_num = 241;
   CHECK(!gfx_media_display_source(&player.state, &dims, &hz));
   test_settings.bools.media_player_crt_switch = false;
   menu_media_controls_cancel(&media);
   menu_media_controls_cancel(&other);
}

static void queued_display_source(void)
{
   static menu_media_controls_t media;
   menu_list_t lists;
   file_list_t *selection[1], *stack[1];
   gfx_media_target_t target;
   fake_player_t players[2];
   unsigned dims;
   float hz;
   memset(&media, 0, sizeof(media));
   memset(&lists, 0, sizeof(lists));
   memset(&target, 0, sizeof(target));
   memset(players, 0, sizeof(players));
   selection[0] = (file_list_t*)calloc(1, sizeof(file_list_t));
   stack[0] = (file_list_t*)calloc(1, sizeof(file_list_t));
   CHECK(selection[0] && stack[0]);
   lists.selection_buf = selection; lists.selection_buf_size = 1;
   lists.menu_stack = stack; lists.menu_stack_size = 1;
   test_menu.entries.list = &lists;
   test_menu.selection_ptr = 0;
   CHECK(file_list_append(stack[0], "/shows", "", FILE_TYPE_DIRECTORY, 0, 0));
   CHECK(file_list_append(selection[0], "episode1.mp4", "", FILE_TYPE_MOVIE, 0, 0));
   CHECK(file_list_append(selection[0], "episode2.mp4", "", FILE_TYPE_MOVIE, 0, 0));
   media.queue = menu_media_queue_new(&test_menu);
   CHECK(media.queue);
   players[0].available = true;
   players[0].state.dims = VIDEO_SCALE_PACK(320, 240);
   players[0].state.cadence_num = 25;
   players[0].state.cadence_den = 1;
   target.user = &players[0];
   target.identity = &players[0];
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   test_settings.bools.media_player_crt_switch = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 1);
   CHECK(menu_media_controls_display(&dims, &hz) && hz == 25.0f);
   CHECK(menu_media_queue_step(media.queue, &test_menu, 1));
   menu_media_controls_update(&media, NULL, 640, 480, NULL, 2);
   CHECK(menu_media_controls_display(&dims, &hz) && hz == 25.0f);
   players[1].available = true;
   players[1].state.dims = VIDEO_SCALE_PACK(640, 360);
   target.user = &players[1];
   target.identity = &players[1];
   menu_media_controls_update(&media, &target, 640, 480, NULL, 3);
   CHECK(menu_media_controls_display(&dims, &hz) && hz == 25.0f
         && dims == players[0].state.dims);
   players[1].state.cadence_num = 25;
   players[1].state.cadence_den = 1;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 4);
   CHECK(menu_media_controls_display(&dims, &hz) && hz == 25.0f
         && dims == players[1].state.dims);
   players[1].state.cadence_unstable = true;
   menu_media_controls_update(&media, &target, 640, 480, NULL, 5);
   CHECK(!menu_media_controls_display(&dims, &hz));
   menu_media_controls_cancel(&media);
   file_list_free(selection[0]); file_list_free(stack[0]);
   test_menu.entries.list = NULL;
   test_settings.bools.media_player_crt_switch = false;
}

#ifdef HAVE_THREADS
static void display_reader(void *user)
{
   mailbox_test_t *t = (mailbox_test_t*)user;
   unsigned dims;
   float hz;
   do
   {
      if (menu_media_controls_display(&dims, &hz))
      {
         if (VIDEO_SCALE_H(dims) != 240
               || (float)VIDEO_SCALE_W(dims) != hz)
            retro_atomic_store_release_int(&t->bad, 1);
         retro_atomic_fetch_add_int(&t->seen, 1);
      }
   } while (!retro_atomic_load_acquire_int(&t->done));
}

static void display_races(void)
{
   mailbox_test_t *t = (mailbox_test_t*)calloc(1, sizeof(*t));
   static menu_media_controls_t media;
   gfx_media_target_t target;
   fake_player_t player;
   sthread_t *thread;
   unsigned i;
   CHECK(t != NULL);
   if (!t) return;
   memset(&media, 0, sizeof(media));
   memset(&target, 0, sizeof(target));
   memset(&player, 0, sizeof(player));
   player.available = true;
   player.state.cadence_den = 1;
   target.user = &player;
   target.identity = &player;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   test_settings.bools.media_player_crt_switch = true;
   thread = sthread_create(display_reader, t);
   CHECK(thread != NULL);
   if (thread)
   {
      for (i = 1; i <= 20000; i++)
      {
         unsigned rate = 24 + (i & 1);
         player.state.cadence_num = rate;
         player.state.dims = VIDEO_SCALE_PACK(rate, 240);
         menu_media_controls_update(&media, &target, 640, 480, NULL, i);
      }
      retro_atomic_store_release_int(&t->done, 1);
      sthread_join(thread);
      CHECK(!retro_atomic_load_acquire_int(&t->bad)
            && retro_atomic_load_acquire_int(&t->seen));
   }
   test_settings.bools.media_player_crt_switch = false;
   menu_media_controls_cancel(&media);
   free(t);
}
#endif

static void display_rates(void)
{
   static const float rates[] = {24.0f, 48.0f, 50.0f, 59.94f,
      60.0f, 71.928f, 72.0f, 119.88f, 120.0f};
   video_display_config_t modes[9];
   unsigned i, dims = VIDEO_SCALE_PACK(1920, 1080);
   float rate;
   memset(modes, 0, sizeof(modes));
   for (i = 0; i < 9; i++)
   {
      modes[i].dims = dims;
      modes[i].bpp = 32;
      modes[i].refreshrate_float = rates[i];
   }
   rate = gfx_media_display_rate(modes, 9, dims, 32, 24000, 1001, 60.0f);
   CHECK(rate > 71.927f && rate < 71.929f);
   CHECK(gfx_media_display_rate(modes, 9, dims, 32, 24, 1, 60.0f) == 48.0f);
   CHECK(gfx_media_display_rate(modes, 9, dims, 32, 25, 1, 60.0f) == 50.0f);
   CHECK(gfx_media_display_rate(modes, 9, dims, 32, 30, 1, 60.0f) == 60.0f);
   rate = gfx_media_display_rate(modes, 9, dims, 32, 24000, 1001, 119.88f);
   CHECK(rate > 119.879f && rate < 119.881f);
   CHECK(!gfx_media_display_rate(modes + 4, 1, dims, 32, 24, 1, 60.0f));
   CHECK(!gfx_media_display_rate(modes, 9, dims, 16, 30, 1, 60.0f));
   CHECK(!gfx_media_display_rate(modes, 9, VIDEO_SCALE_PACK(1280, 720),
            32, 30, 1, 60.0f));
   CHECK(!gfx_media_display_rate(modes, 9, dims, 32, 30, 0, 60.0f));
   CHECK(!gfx_media_display_rate(NULL, 9, dims, 32, 30, 1, 60.0f));
   modes[4].interlaced = true;
   CHECK(!gfx_media_display_rate(modes + 4, 1, dims, 32, 30, 1, 60.0f));
   modes[4].interlaced = false;
   modes[4].dblscan = true;
   CHECK(!gfx_media_display_rate(modes + 4, 1, dims, 32, 30, 1, 60.0f));
   modes[4].dblscan = false;
   modes[4].refreshrate_float = 0.0f;
   modes[4].refreshrate = 60;
   CHECK(gfx_media_display_rate(modes + 4, 1, dims, 32, 30, 1, 60.0f) == 60.0f);
}

static bool toolbar_settings(void *user)
{
   unsigned *opened = (unsigned*)user;
   (*opened)++;
   return true;
}

static void combined_subtitle_toolbar(void)
{
   static gfx_media_mailbox_t mailbox;
   gfx_media_view_t view;
   gfx_media_controls_t controls;
   gfx_media_target_t target;
   fake_player_t player;
   unsigned variant;
   for (variant = 0; variant < 3; variant++)
   {
      unsigned opened = 0;
      unsigned base = variant == 2 ? 7 : variant == 1 ? 6 : 4;
      const gfx_media_rect_t *button;
      memset(&controls, 0, sizeof(controls));
      memset(&target, 0, sizeof(target));
      memset(&player, 0, sizeof(player));
      memset(&mailbox, 0, sizeof(mailbox));
      player.available = true;
      target.user = target.navigation_user = &player;
      target.identity = &player;
      target.state = fake_state;
      target.pause = fake_pause;
      target.seek = fake_seek;
      target.subtitle_tracks = true;
      if (variant)
      {
         target.step = fake_step;
         target.directions = 3;
      }
      if (variant == 2)
      {
         target.settings = toolbar_settings;
         target.settings_user = &opened;
      }
      CHECK(gfx_media_controls_update(&controls, &target, 640, 480, 1));
      CHECK(controls.layout.button_count == base + 1);
      controls.row_focus = true;
      controls.focus = base;
      CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_ACTIVATE, 2));
      CHECK(controls.subtitle_requested && !opened && !player.advances);
      controls.subtitle_requested = false;
      if (variant)
      {
         controls.focus = 4;
         CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_ACTIVATE, 3));
         CHECK(player.advances == 1 && player.last_direction == -1);
      }
      if (variant == 2)
      {
         controls.focus = 6;
         CHECK(gfx_media_controls_action(&controls, GFX_MEDIA_ACTIVATE, 4));
         CHECK(opened == 1);
      }
      gfx_media_controls_publish(&controls, &mailbox);
      CHECK(gfx_media_controls_consume(&mailbox, &view));
      CHECK(view.subtitle_tracks && view.layout.button_count == base + 1);
      button = &controls.layout.buttons[base];
      gfx_media_controls_pointer(&controls, 0, 0, false, true, 5);
      gfx_media_controls_pointer(&controls, button->x + 1, button->y + 1, true, true, 6);
      gfx_media_controls_release(&controls, button->x + 1, button->y + 1, 7);
      CHECK(controls.subtitle_requested);
      CHECK(opened == (variant == 2 ? 1u : 0u));
      CHECK(player.advances == (variant ? 1u : 0u));
      controls.subtitle_requested = false;
      if (!variant)
      {
         CHECK(!gfx_media_controls_action(&controls, GFX_MEDIA_PREVIOUS_FILE, 8));
         CHECK(!gfx_media_controls_action(&controls, GFX_MEDIA_NEXT_FILE, 8));
         CHECK(!controls.subtitle_requested);
      }
      if (variant < 2)
      {
         CHECK(!gfx_media_controls_action(&controls, GFX_MEDIA_SETTINGS, 8));
         CHECK(!controls.subtitle_requested);
      }
      target.subtitle_tracks = false;
      controls.focus = base;
      CHECK(gfx_media_controls_update(&controls, &target, 640, 480, 8));
      CHECK(controls.layout.button_count == base && !controls.focus);
   }
}

static void subtitle_button(void)
{
   gfx_media_controls_t c;
   gfx_media_target_t target;
   fake_player_t player;
   const gfx_media_rect_t *button;
   unsigned i;
   memset(&c, 0, sizeof(c));
   memset(&target, 0, sizeof(target));
   memset(&player, 0, sizeof(player));
   player.available = true;
   target.user = &player;
   target.identity = &player;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   target.subtitle_tracks = true;
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 1));
   CHECK(c.layout.button_count == 5);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_FOCUS, 2));
   for (i = 0; i < 4; i++)
      CHECK(gfx_media_controls_action(&c, GFX_MEDIA_NEXT, 3));
   CHECK(c.focus == 4);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_ACTIVATE, 4));
   CHECK(c.subtitle_requested && !player.seeks && !player.pauses);
   c.subtitle_requested = false;
   button = &c.layout.buttons[4];
   gfx_media_controls_pointer(&c, button->x + 1, button->y + 1, false, true, 5);
   gfx_media_controls_pointer(&c, button->x + 1, button->y + 1, true, true, 6);
   gfx_media_controls_release(&c, button->x + 1, button->y + 1, 7);
   CHECK(c.subtitle_requested && !player.seeks && !player.pauses);
   target.identity = &target;
   CHECK(gfx_media_controls_update(&c, &target, 640, 480, 8));
   CHECK(!c.subtitle_requested);
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_FOCUS, 9));
   CHECK(gfx_media_controls_action(&c, GFX_MEDIA_PREVIOUS, 10));
   CHECK(c.focus == 4);
}

static void subtitles_without_transport(void)
{
   menu_media_controls_t *media = (menu_media_controls_t*)calloc(1, sizeof(*media));
   gfx_media_subtitle_view_t view;
   font_data_t font;
   unsigned before;
   CHECK(media != NULL);
   if (!media)
      return;
   memset(&view, 0, sizeof(view));
   memset(&font, 0, sizeof(font));
   font.size = 20.0f;
   strcpy(view.text, "First line\nSecond line");
   gfx_media_subtitle_publish(&media->subtitle_mailbox, &view);
   before = captions;
   drawing = true;
   menu_media_controls_draw(media, NULL, NULL, VIDEO_SCALE_PACK(640, 480), &font);
   drawing = false;
   CHECK(!media->view.visible && captions == before + 2);
   gfx_media_subtitle_publish(&media->subtitle_mailbox, NULL);
   before = captions;
   drawing = true;
   menu_media_controls_draw(media, NULL, NULL, VIDEO_SCALE_PACK(640, 480), &font);
   drawing = false;
   CHECK(captions == before);
   menu_media_controls_cancel(media);
   free(media);
}

static void context_resume(void)
{
   menu_media_controls_t *media = (menu_media_controls_t*)calloc(1, sizeof(*media));
   fake_player_t player;
   gfx_media_target_t target;
   char path[] = "movie.mp4";
   CHECK(media != NULL);
   if (!media) return;
   memset(&player, 0, sizeof(player));
   memset(&target, 0, sizeof(target));
   player.available = true;
   player.state.seekable = true;
   player.state.duration_us = 10000000;
   player.state.position_us = 3000000;
   player.state.paused = true;
   target.user = &player;
   target.identity = &player;
   target.state = fake_state;
   target.pause = fake_pause;
   target.seek = fake_seek;
   target.source_path = path;
   menu_media_controls_update(media, &target, 640, 480, NULL, 1);
   menu_media_controls_context_reset(media, true);
   CHECK(media->resume_path && media->resume.position_us == 3000000);
   menu_media_controls_context_reset(media, true);
   path[0] = 'X';
   target.source_path = "movie.mp4";
   player.available = false;
   player.state.position_us = 0;
   player.state.paused = false;
   menu_media_controls_update(media, &target, 640, 480, NULL, 2);
   CHECK(media->resume_path && !player.seeks);
   player.available = true;
   player.pause_refused = true;
   menu_media_controls_update(media, &target, 640, 480, NULL, 3);
   CHECK(media->resume_path && !player.seeks);
   player.pause_refused = false;
   player.seek_refused = true;
   menu_media_controls_update(media, &target, 640, 480, NULL, 4);
   CHECK(media->resume_path && !player.seeks);
   player.seek_refused = false;
   media->settings_open = true;
   menu_media_controls_update(media, &target, 640, 480, NULL, 5);
   CHECK(!media->resume_path && player.seeks == 1);
   CHECK(player.state.paused && player.state.position_us == 3000000);
   media->settings_open = false;
   player.state.seeking = true;
   player.state.seek_target_us = 7000000;
   menu_media_controls_context_reset(media, true);
   CHECK(media->resume_path && media->resume.position_us == 7000000);
   player.state.seeking = false;
   target.source_path = "other.mp4";
   menu_media_controls_update(media, &target, 640, 480, NULL, 6);
   CHECK(!media->resume_path && player.seeks == 1);
   menu_media_controls_context_reset(media, true);
   CHECK(media->resume_path != NULL);
   menu_media_controls_context_reset(media, false);
   CHECK(!media->resume_path);
   menu_media_controls_update(media, &target, 640, 480, NULL, 7);
   menu_media_controls_context_reset(media, true);
   CHECK(media->resume_path != NULL);
   menu_media_controls_cancel(media);
   CHECK(!media->resume_path);
   free(media);
}

int main(void)
{
   test_settings.uints.media_player_seek_seconds = 5;
   test_settings.uints.media_player_controls_timeout = 3;
   display_owner_lifecycle();
   desktop_refresh_lifecycle();
   display_rates();
   display_source();
   queued_display_source();
#ifdef HAVE_THREADS
   display_races();
#endif
   folder_queue();
   combined_subtitle_toolbar();
   subtitle_button();
   subtitles_without_transport();
   playback_preferences();
   menu_preferences();
   transport();
   explicit_playback();
   failed_seek_transport();
   menu_transport();
   file_buttons();
   settings_button();
   context_resume();
#ifdef HAVE_THREADS
   mailbox_races();
#endif
   printf("media controls: %u failure(s)\n", failures);
   return failures ? 1 : 0;
}
