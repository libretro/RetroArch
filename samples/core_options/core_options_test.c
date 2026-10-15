/* The core option manager, driven directly.
 *
 * core_option_manager.c is linked as shipped against the real
 * libretro-common config_file, string_list and nested_list.
 * string_list_append is wrapped (--wrap) so a test can make the
 * Nth append fail and check that the manager refuses the options
 * cleanly - no write outside a list, no leak - under ASan. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <libretro.h>
#include <lists/string_list.h>
#include <file/config_file.h>

#include "../../core_option_manager.h"
#include "../../msg_hash.h"
#include "../../verbosity.h"

static unsigned failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("      FAIL: %s\n", msg); failures++; } } while (0)

/* --- what core_option_manager.c needs from the frontend ------------- */

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MENU_ENUM_LABEL_VALUE_ON:
         return "ON";
      case MENU_ENUM_LABEL_VALUE_OFF:
         return "OFF";
      case MENU_ENUM_LABEL_ENABLED:
         return "enabled";
      case MENU_ENUM_LABEL_DISABLED:
         return "disabled";
      default:
         break;
   }
   return "\x01";
}

void RARCH_DBG(const char *fmt, ...) { (void)fmt; }

/* --- the failing append ------------------------------------------- */

bool __real_string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr);

/* 0: never fail; N: the Nth append from now fails */
static unsigned append_fail_at;
static unsigned append_calls;

bool __wrap_string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{
   append_calls++;
   if (append_fail_at && append_calls == append_fail_at)
      return false;
   return __real_string_list_append(list, elem, attr);
}

static void fail_append(unsigned n)
{
   append_calls   = 0;
   append_fail_at = n;
}

/* --- fixtures ------------------------------------------------------ */

#define CONF_PATH "core_options_test.cfg"

static void write_conf(const char *text)
{
   FILE *f = fopen(CONF_PATH, "wb");
   if (f)
   {
      fputs(text, f);
      fclose(f);
   }
}

static size_t find_option(core_option_manager_t *opt, const char *key)
{
   size_t i;
   for (i = 0; i < opt->size; i++)
      if (opt->opts[i].key && !strcmp(opt->opts[i].key, key))
         return i;
   return (size_t)-1;
}

static struct retro_core_option_v2_definition v2_defs[4];
static struct retro_core_options_v2 v2_opts;

static void build_v2(void)
{
   memset(v2_defs, 0, sizeof(v2_defs));

   v2_defs[0].key                = "t_res";
   v2_defs[0].desc               = "Resolution";
   v2_defs[0].values[0].value    = "320x240";
   v2_defs[0].values[1].value    = "640x480";
   v2_defs[0].values[1].label    = "Double";
   v2_defs[0].values[2].value    = "1280x960";
   v2_defs[0].default_value      = "640x480";

   v2_defs[1].key                = "t_flag";
   v2_defs[1].desc               = "Flag";
   v2_defs[1].values[0].value    = "disabled";
   v2_defs[1].values[1].value    = "enabled";
   v2_defs[1].default_value      = "disabled";

   /* Every value slot filled: the count stops at the array's end */
   {
      static char vals[RETRO_NUM_CORE_OPTION_VALUES_MAX][12];
      int i;
      v2_defs[2].key  = "t_full";
      v2_defs[2].desc = "Full";
      for (i = 0; i < RETRO_NUM_CORE_OPTION_VALUES_MAX; i++)
      {
         snprintf(vals[i], sizeof(vals[i]), "%d", i);
         v2_defs[2].values[i].value = vals[i];
      }
      v2_defs[2].default_value = "5";
   }

   v2_opts.categories  = NULL;
   v2_opts.definitions = v2_defs;
}

/* --- v2 interface -------------------------------------------------- */

static void t_v2(void)
{
   core_option_manager_t *opt;
   size_t i, idx;

   printf("   v2: values, defaults, config value and lookup\n");
   build_v2();
   write_conf("t_flag = \"enabled\"\n");
   fail_append(0);

   opt = core_option_manager_new(CONF_PATH, NULL, &v2_opts, false);
   CHECK(opt != NULL, "manager not created");
   if (!opt)
      return;
   CHECK(opt->size == 3, "option count");

   i = find_option(opt, "t_res");
   CHECK(i != (size_t)-1, "t_res missing");
   if (i != (size_t)-1)
   {
      CHECK(opt->opts[i].vals->size == 3, "t_res value count");
      CHECK(opt->opts[i].val_labels->size == 3, "t_res label count");
      CHECK(opt->opts[i].index == 1, "t_res default not applied");
      CHECK(!strcmp(core_option_manager_get_val(opt, i), "640x480"), "t_res value");
      CHECK(!strcmp(core_option_manager_get_val_label(opt, i), "Double"), "t_res label");
      CHECK(core_option_manager_get_val_idx(opt, i, "1280x960", &idx) && idx == 2,
            "t_res lookup of the last value");
      CHECK(!core_option_manager_get_val_idx(opt, i, "1280x96", &idx),
            "t_res lookup matched a value it does not have");
   }

   i = find_option(opt, "t_flag");
   CHECK(i != (size_t)-1, "t_flag missing");
   if (i != (size_t)-1)
   {
      CHECK(opt->opts[i].index == 1, "t_flag config value not applied");
      CHECK(!strcmp(core_option_manager_get_val_label(opt, i), "ON"),
            "t_flag label not sanitised");
   }

   i = find_option(opt, "t_full");
   CHECK(i != (size_t)-1, "t_full missing");
   if (i != (size_t)-1)
   {
      CHECK(opt->opts[i].vals->size == RETRO_NUM_CORE_OPTION_VALUES_MAX,
            "t_full value count");
      CHECK(opt->opts[i].index == 5, "t_full default");
      CHECK(core_option_manager_get_val_idx(opt, i, "127", &idx) && idx == 127,
            "t_full lookup of the last value");
   }

   core_option_manager_free(opt);
}

/* Each append a v2 parse makes, made to fail in turn: the manager
 * must come back NULL with nothing written past a list. The first
 * failure is the first value of the first option, which a write
 * through 'elems[size - 1]' would have put before the list. */
static void t_v2_append_failures(void)
{
   unsigned n, total;
   core_option_manager_t *opt;

   printf("   v2: every value/label append failing in turn\n");
   build_v2();
   write_conf("");

   fail_append(0);
   opt   = core_option_manager_new(CONF_PATH, NULL, &v2_opts, false);
   total = append_calls;
   core_option_manager_free(opt);
   CHECK(total > 0, "no appends counted");

   for (n = 1; n <= total; n++)
   {
      fail_append(n);
      opt = core_option_manager_new(CONF_PATH, NULL, &v2_opts, false);
      if (opt)
      {
         printf("      FAIL: append %u of %u failed and a manager was still made\n",
               n, total);
         failures++;
         core_option_manager_free(opt);
      }
   }
   fail_append(0);
}

/* --- legacy interface ---------------------------------------------- */

static const struct retro_variable legacy_vars[] = {
   { "t_mode",  "Mode; fast|accurate|slow" },
   { "t_nodesc", "; on|off" },
   { "t_gaps",  "Gaps; a||b|" },
   { NULL, NULL },
};

static void t_legacy(void)
{
   core_option_manager_t *opt;
   size_t i, idx;

   printf("   legacy: values, description, empty tokens, config value\n");
   write_conf("t_mode = \"slow\"\nt_gaps = \"b\"\n");
   fail_append(0);

   opt = core_option_manager_new_vars(CONF_PATH, NULL, legacy_vars);
   CHECK(opt != NULL, "manager not created");
   if (!opt)
      return;

   i = find_option(opt, "t_mode");
   CHECK(i != (size_t)-1, "t_mode missing");
   if (i != (size_t)-1)
   {
      CHECK(opt->opts[i].desc && !strcmp(opt->opts[i].desc, "Mode"), "t_mode desc");
      CHECK(opt->opts[i].vals->size == 3, "t_mode value count");
      CHECK(opt->opts[i].index == 2, "t_mode config value not applied");
      CHECK(core_option_manager_get_val_idx(opt, i, "fast", &idx) && idx == 0,
            "t_mode lookup");
   }

   i = find_option(opt, "t_nodesc");
   CHECK(i != (size_t)-1, "t_nodesc missing");
   if (i != (size_t)-1)
   {
      CHECK(opt->opts[i].desc == NULL, "t_nodesc was given a description");
      CHECK(!strcmp(core_option_manager_get_val_label(opt, i), "ON"),
            "t_nodesc label not sanitised");
   }

   i = find_option(opt, "t_gaps");
   CHECK(i != (size_t)-1, "t_gaps missing");
   if (i != (size_t)-1)
   {
      CHECK(opt->opts[i].vals->size == 2, "t_gaps kept an empty value");
      CHECK(opt->opts[i].index == 1, "t_gaps config value not applied");
   }

   core_option_manager_free(opt);
}

static void t_legacy_append_failures(void)
{
   unsigned n, total;
   core_option_manager_t *opt;

   printf("   legacy: every label append failing in turn\n");
   write_conf("");

   fail_append(0);
   opt   = core_option_manager_new_vars(CONF_PATH, NULL, legacy_vars);
   total = append_calls;
   core_option_manager_free(opt);
   CHECK(total > 0, "no appends counted");

   for (n = 1; n <= total; n++)
   {
      fail_append(n);
      opt = core_option_manager_new_vars(CONF_PATH, NULL, legacy_vars);
      if (opt)
      {
         printf("      FAIL: append %u of %u failed and a manager was still made\n",
               n, total);
         failures++;
         core_option_manager_free(opt);
      }
   }
   fail_append(0);
}

int main(void)
{
   printf("core_options\n");
   t_v2();
   t_v2_append_failures();
   t_legacy();
   t_legacy_append_failures();
   remove(CONF_PATH);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
