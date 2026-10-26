/* frontend/frontend_salamander.c with a stand-in frontend driver: the
 * core salamander launches is marked pending in its config until that
 * core's frontend clears the mark. A boot that still finds the mark on
 * the configured core passes over it for another core; with no other
 * core left, it launches nothing and drops both entries so the boot
 * after looks again. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define rmdir _rmdir
#else
#include <unistd.h>
#endif

#include <boolean.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../../../frontend/frontend_driver.h"
#include "../../../defaults.h"
#include "../../../file_path_special.h"

int salamander_main(int argc, char *argv[]);

static int  failures;
static char launched[PATH_MAX_LENGTH];
static char root[PATH_MAX_LENGTH];
static char cores[PATH_MAX_LENGTH];
static char cfg[PATH_MAX_LENGTH];

#define CHECK(cond) do { if (!(cond)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

/* Stand-in frontend driver: the core directory and config path are the
 * test's, and exitspawn records what would have been launched. */
static void stub_get_env(int *argc, char *argv[], void *args, void *params)
{
   (void)argc; (void)argv; (void)args; (void)params;
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CORE], cores,
         sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.path_config, root,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));
}

static void stub_exitspawn(char *s, size_t len, char *args)
{
   (void)len; (void)args;
   strlcpy(launched, s, sizeof(launched));
}

static frontend_ctx_driver_t stub_ctx;

frontend_ctx_driver_t *frontend_ctx_init_first(void)
{
   stub_ctx.environment_get = stub_get_env;
   stub_ctx.exitspawn       = stub_exitspawn;
   stub_ctx.ident           = "test";
   return &stub_ctx;
}

size_t frontend_driver_get_core_extension(char *s, size_t len)
{
   return strlcpy(s, "rpx", len);
}

bool frontend_driver_get_salamander_basename(char *s, size_t len)
{
   strlcpy(s, "retroarch.rpx", len);
   return true;
}

static void touch(const char *name)
{
   char path[PATH_MAX_LENGTH];
   FILE *f;
   fill_pathname_join(path, cores, name, sizeof(path));
   if ((f = fopen(path, "wb")))
      fclose(f);
}

static void unlink_core(const char *name)
{
   char path[PATH_MAX_LENGTH];
   fill_pathname_join(path, cores, name, sizeof(path));
   remove(path);
}

static void boot(void)
{
   char *argv[1];
   argv[0]     = (char*)"retroarch";
   launched[0] = '\0';
   salamander_main(1, argv);
}

static bool cfg_get(const char *key, char *out, size_t len)
{
   bool ok;
   config_file_t *c = config_file_new_from_path_to_string(cfg);
   out[0] = '\0';
   if (!c)
      return false;
   ok = config_get_path(c, key, out, len);
   config_file_free(c);
   return ok;
}

/* What config_clear_salamander_pending() does once the core is up. */
static void frontend_came_up(void)
{
   config_file_t *c = config_file_new_from_path_to_string(cfg);
   if (!c)
      return;
   config_unset(c, SALAMANDER_CONFIG_PENDING_KEY);
   config_file_write(c, cfg, false);
   config_file_free(c);
}

static const char *base(const char *path)
{
   return path_basename(path);
}

int main(void)
{
   char v[PATH_MAX_LENGTH];

   config_file_set_io_default(config_file_io_filestream());

   strlcpy(root, "salamander_pending.tmp", sizeof(root));
   fill_pathname_join(cores, root, "cores", sizeof(cores));
   fill_pathname_join(cfg, root, FILE_PATH_SALAMANDER_CONFIG, sizeof(cfg));
   path_mkdir(cores);
   remove(cfg);
   touch("retroarch.rpx");
   touch("a_libretro.rpx");
   touch("b_libretro.rpx");

   /* No config: the first core, marked pending. */
   boot();
   CHECK(string_is_equal(base(launched), "a_libretro.rpx"));
   CHECK(cfg_get("libretro_path", v, sizeof(v))
         && string_is_equal(base(v), "a_libretro.rpx"));
   CHECK(cfg_get(SALAMANDER_CONFIG_PENDING_KEY, v, sizeof(v))
         && string_is_equal(base(v), "a_libretro.rpx"));

   /* a came up: it is launched again, and marked again. */
   frontend_came_up();
   CHECK(!cfg_get(SALAMANDER_CONFIG_PENDING_KEY, v, sizeof(v)));
   boot();
   CHECK(string_is_equal(base(launched), "a_libretro.rpx"));

   /* a never came up this time: the next boot passes it over. */
   boot();
   CHECK(string_is_equal(base(launched), "b_libretro.rpx"));
   CHECK(cfg_get("libretro_path", v, sizeof(v))
         && string_is_equal(base(v), "b_libretro.rpx"));
   CHECK(cfg_get(SALAMANDER_CONFIG_PENDING_KEY, v, sizeof(v))
         && string_is_equal(base(v), "b_libretro.rpx"));

   /* b is the only core left and never came up: nothing is launched,
    * both entries go, and the boot after starts b again. */
   unlink_core("a_libretro.rpx");
   boot();
   CHECK(launched[0] == '\0');
   CHECK(!cfg_get("libretro_path", v, sizeof(v)));
   CHECK(!cfg_get(SALAMANDER_CONFIG_PENDING_KEY, v, sizeof(v)));
   boot();
   CHECK(string_is_equal(base(launched), "b_libretro.rpx"));

   unlink_core("b_libretro.rpx");
   unlink_core("retroarch.rpx");
   remove(cfg);
   rmdir(cores);
   rmdir(root);

   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("salamander_pending: ok\n");
   return 0;
}
