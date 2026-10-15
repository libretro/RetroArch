/* The load content animation's icon.
 *
 * A start is resolved on the caller's thread and applied by the
 * widgets' owner. The icon is looked up under the directory the
 * widgets set at init, by the core's database name and then the
 * 'retroarch' fallback; the owner then loads it. Each case starts
 * the animation from a thread other than the owner's and checks
 * that the owner loads the icon the directory holds.
 *
 * The widget's source is included, for its state. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0700)
#define RMDIR(p) rmdir(p)
#endif

#include <rthreads/rthreads.h>

#include "../../../gfx/widgets/gfx_widget_load_content_animation.c"

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } } while (0)

/* --- the frontend this widget reads --- */
static dispgfx_widget_t   test_dispwidget;
static core_info_t        test_core;
static char               test_core_path[] = "/cores/pcsx2_libretro.so";
static char               test_core_name[] = "LRPS2";
static char               test_core_id[]   = "pcsx2_libretro";
static const char        *test_content     = "/roms/Soulcalibur III (USA).chd";
static char               submitted[PATH_MAX_LENGTH];
static int                stills;
static gfx_surface_t     *still_handle     = (gfx_surface_t*)&stills;

dispgfx_widget_t *dispwidget_get_ptr(void) { return &test_dispwidget; }

const char *path_get(enum rarch_path_type type)
{
   if (type == RARCH_PATH_CONTENT)
      return test_content;
   if (type == RARCH_PATH_CORE)
      return test_core_path;
   return NULL;
}

bool core_info_find(const char *core_path, core_info_t **info)
{
   (void)core_path;
   *info = &test_core;
   return true;
}

bool path_is_valid(const char *path)
{
   struct stat st;
   return path && stat(path, &st) == 0;
}

playlist_t *playlist_get_cached(void) { return NULL; }
char *playlist_get_conf_path(playlist_t *p) { (void)p; return NULL; }
void playlist_get_index(playlist_t *p, size_t i,
      const struct playlist_entry **e) { (void)p; (void)i; *e = NULL; }
void playlist_get_index_by_path(playlist_t *p, const char *s,
      const struct playlist_entry **e) { (void)p; (void)s; *e = NULL; }
bool playlist_index_is_valid(playlist_t *p, size_t i, const char *a,
      const char *b) { (void)p; (void)i; (void)a; (void)b; return false; }
struct menu_state *menu_state_get_ptr(void)
{
   static struct menu_state st;
   return &st;
}

gfx_surface_t *gfx_surface_new_still(enum texture_filter_type f)
{ (void)f; stills++; return still_handle; }
bool gfx_surface_submit_file(gfx_surface_t *s, const char *path, bool rgba)
{
   (void)s; (void)rgba;
   strlcpy(submitted, path, sizeof(submitted));
   return true;
}
void gfx_surface_free(gfx_surface_t *s) { (void)s; }
bool gfx_surface_wants_rgba(void) { return true; }
enum texture_filter_type gfx_display_texture_filter(void)
{ return (enum texture_filter_type)0; }

bool gfx_animation_push_widget(gfx_animation_ctx_entry_t *e) { (void)e; return true; }
bool gfx_animation_kill_widget_by_tag(uintptr_t *t) { (void)t; return true; }
void gfx_animation_timer_start_widget(float *t, gfx_timer_ctx_entry_t *e)
{ (void)t; (void)e; }
int font_driver_get_message_width(void *f, const char *m, size_t l, float s)
{ (void)f; (void)m; (void)s; return (int)l * 8; }
void gfx_display_blend_begin(gfx_display_ctx_driver_t *d, void *u) { (void)d; (void)u; }
void gfx_display_blend_end(gfx_display_ctx_driver_t *d, void *u) { (void)d; (void)u; }
void gfx_display_draw_quad(gfx_display_t *p, void *d, unsigned vd, int x,
      int y, unsigned dims, unsigned rd, float *c, uintptr_t *t)
{ (void)p; (void)d; (void)vd; (void)x; (void)y; (void)dims; (void)rd; (void)c; (void)t; }
void gfx_widgets_draw_icon(void *u, void *d, unsigned vd, unsigned id,
      uintptr_t t, float x, float y, float r, float c, float s, float *col)
{ (void)u; (void)d; (void)vd; (void)id; (void)t; (void)x; (void)y; (void)r; (void)c; (void)s; (void)col; }
void gfx_widgets_draw_text(gfx_widget_font_data_t *f, const char *t,
      float x, float y, unsigned dims, uint32_t c,
      enum text_alignment a, bool o)
{ (void)f; (void)t; (void)x; (void)y; (void)dims; (void)c; (void)a; (void)o; }
void gfx_widgets_flush_text(unsigned vd, gfx_widget_font_data_t *f)
{ (void)vd; (void)f; }

/* --- the producer, on a thread of its own --- */
static bool started;

static void producer(void *unused)
{
   (void)unused;
   started = gfx_widget_start_load_content_animation();
}

static void start_and_apply(void)
{
   sthread_t *t;
   submitted[0] = '\0';
   stills       = 0;
   if ((t = sthread_create(producer, NULL)))
      sthread_join(t);
   gfx_widget_load_content_animation.iterate(&test_dispwidget,
         0, false, "", NULL, true);
}

static void touch(const char *dir, const char *file)
{
   char path[PATH_MAX_LENGTH];
   FILE *f;
   fill_pathname_join_special(path, dir, file, sizeof(path));
   if ((f = fopen(path, "wb")))
      fclose(f);
}

int main(void)
{
   const char *root = "load_content_icon_test.dir";
   char png[PATH_MAX_LENGTH];
   char want[PATH_MAX_LENGTH];
   struct string_list *dbs = string_list_new();
   union string_list_elem_attr attr;
   gfx_widget_load_content_animation_state_t *state =
         &p_w_load_content_animation_st;

   if (!dbs)
      return 1;
   fill_pathname_join_special(png, root, "png", sizeof(png));
   MKDIR(root);
   MKDIR(png);

   attr.i = 0;
   string_list_append(dbs, "Sony - PlayStation 2", attr);
   test_core.path              = test_core_path;
   test_core.display_name      = test_core_name;
   test_core.core_file_id.str  = test_core_id;
   test_core.databases_list    = dbs;

   /* The widgets' init: the directory, then the widget's reset */
   strlcpy(test_dispwidget.monochrome_png_path, png,
         sizeof(test_dispwidget.monochrome_png_path));
   gfx_widget_load_content_animation.context_reset(true, 0, false, "",
         NULL, test_dispwidget.monochrome_png_path,
         test_dispwidget.gfx_widgets_path);

   /* 1. The system's own icon */
   touch(png, "Sony - PlayStation 2.png");
   touch(png, "retroarch.png");
   start_and_apply();
   fill_pathname_join_special(want, png, "Sony - PlayStation 2.png",
         sizeof(want));
   CHECK(started, "the animation starts");
   CHECK(state->has_icon, "the system's icon is found");
   CHECK(stills == 1 && string_is_equal(submitted, want),
         "the owner loads the system's icon");
   gfx_widget_load_content_animation_reset();

   /* 2. No icon for the system: the 'retroarch' one */
   fill_pathname_join_special(want, png, "Sony - PlayStation 2.png",
         sizeof(want));
   remove(want);
   start_and_apply();
   fill_pathname_join_special(want, png, "retroarch.png", sizeof(want));
   CHECK(state->has_icon, "the fallback icon is found");
   CHECK(stills == 1 && string_is_equal(submitted, want),
         "the owner loads the fallback icon");
   gfx_widget_load_content_animation_reset();

   /* 3. No icons at all: none is loaded */
   remove(want);
   start_and_apply();
   CHECK(started && !state->has_icon && stills == 0,
         "with no icon the animation still starts, without one");
   gfx_widget_load_content_animation_reset();

   gfx_widget_load_content_animation.free();
   string_list_free(dbs);
   RMDIR(png);
   RMDIR(root);

   if (fails)
   {
      printf("%d failure(s)\n", fails);
      return 1;
   }
   printf("load_content_icon_test: all checks passed\n");
   return 0;
}
