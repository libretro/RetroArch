/* Menu network settings harness.
 *
 * Links the shipping RetroArch objects (see build.sh) with only main()
 * replaced, brings the menu up headless, and checks that the SMB and
 * NFS client settings screens are reachable the way a user reaches
 * them: their entries sit under Settings > Network and not in the
 * Quick Menu, the label their deferred lists are pushed with resolves
 * to a real identifier (a label missing from msg_hash_lbl.h resolves
 * to "null", and the push then finds no list: the screen came up as
 * an empty "null" browser, which is the field report), and opening
 * each screen yields its rows. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <lists/file_list.h>
#include <string/stdstring.h>
#include <time/rtime.h>
#include <file/config_file.h>

#include "../../../msg_hash.h"
#include "../../../msg_hash_lbl_str.h"
#include "../../../menu/menu_defines.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_displaylist.h"
#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../frontend/frontend_driver.h"

static unsigned failures;
#define CHECK(cond, what) do { if (cond) fprintf(stderr, "ok:   %s\n", what); \
   else { fprintf(stderr, "FAIL: %s\n", what); failures++; } } while (0)

static file_list_t *selection_buf(void)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t *menu_list     = menu_st->entries.list;
   return menu_list ? MENU_LIST_GET_SELECTION(menu_list, 0) : NULL;
}

/* Build displaylist @type into the selection buffer, the way opening
 * that screen does; @label is what the stack entry carries. */
static size_t open_list(enum menu_displaylist_ctl_state type,
      const char *label, enum msg_hash_enums enum_idx)
{
   menu_displaylist_info_t info;
   struct menu_state *menu_st = menu_state_get_ptr();
   file_list_t *buf           = selection_buf();
   file_list_t *menu_stack    = MENU_LIST_GET(menu_st->entries.list, 0);
   settings_t *settings       = config_get_ptr();

   if (!buf || !menu_stack)
      return 0;
   menu_entries_clear(buf);
   menu_entries_append(menu_stack, "", label, enum_idx,
         MENU_SETTING_ACTION, 0, 0, NULL);
   menu_displaylist_info_init(&info);
   info.list          = buf;
   info.path          = strdup("");
   info.label         = strdup(label);
   info.enum_idx      = enum_idx;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = 0;
   menu_displaylist_ctl(type, &info, settings);
   menu_displaylist_process(&info);
   menu_displaylist_info_free(&info);
   return buf->size;
}

static bool list_has_label(const char *label)
{
   file_list_t *buf = selection_buf();
   size_t i;
   for (i = 0; buf && i < buf->size; i++)
      if (string_is_equal(buf->list[i].label, label))
         return true;
   return false;
}

int main(int argc, char *argv[])
{
   static char cfg_path[640];
   char fixture_dir[128], cmd[300];
   char *rarch_argv[8];
   int rarch_argc = 0;
   size_t n;

   (void)argc; (void)argv;
   snprintf(fixture_dir, sizeof(fixture_dir), "/tmp/menu_net_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;
   snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg", fixture_dir);
   {
      FILE *cfg = fopen(cfg_path, "wb");
      if (!cfg)
         return 1;
      fprintf(cfg, "video_driver = \"null\"\naudio_driver = \"null\"\n"
            "input_driver = \"null\"\ninput_joypad_driver = \"null\"\n"
            "menu_driver = \"rgui\"\nvideo_threaded = \"false\"\n"
            "smb_client_enable = \"true\"\n");
      fclose(cfg);
   }
   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }
   if (!selection_buf())
   {
      fprintf(stderr, "FAIL: the menu came up without an entry list\n");
      return 1;
   }

   /* the labels the deferred lists are pushed with must be identifiers */
#ifdef HAVE_SMBCLIENT
   CHECK(string_is_equal(msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_SMB_CLIENT_SETTINGS_LIST),
            MENU_ENUM_LABEL_DEFERRED_SMB_CLIENT_SETTINGS_LIST_STR),
         "deferred SMB settings list label resolves");
#endif
#ifdef HAVE_NFSCLIENT
   CHECK(string_is_equal(msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_NFS_CLIENT_SETTINGS_LIST),
            MENU_ENUM_LABEL_DEFERRED_NFS_CLIENT_SETTINGS_LIST_STR),
         "deferred NFS settings list label resolves");
   CHECK(string_is_equal(msg_hash_to_str(MENU_ENUM_LABEL_NFS_CLIENT_SERVER), "nfs_client_server"),
         "NFS setting labels resolve");
#endif

   /* Settings > Network carries both entries */
   n = open_list(DISPLAYLIST_NETWORK_SETTINGS_LIST,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST),
         MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST);
   CHECK(n > 0, "Settings > Network opens");
#ifdef HAVE_SMBCLIENT
   CHECK(list_has_label(MENU_ENUM_LABEL_SMB_CLIENT_SETTINGS_STR), "SMB Network Settings is under Settings > Network");
#endif
#ifdef HAVE_NFSCLIENT
   CHECK(list_has_label(MENU_ENUM_LABEL_NFS_CLIENT_SETTINGS_STR), "NFS Network Settings is under Settings > Network");
#endif

   /* each screen opens with its rows */
#ifdef HAVE_SMBCLIENT
   n = open_list(DISPLAYLIST_SMB_CLIENT_SETTINGS_LIST,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_SMB_CLIENT_SETTINGS_LIST),
         MENU_ENUM_LABEL_DEFERRED_SMB_CLIENT_SETTINGS_LIST);
   CHECK(n > 0 && list_has_label(MENU_ENUM_LABEL_SMB_CLIENT_SERVER_STR), "SMB Network Settings screen has its rows");
#endif
#ifdef HAVE_NFSCLIENT
   n = open_list(DISPLAYLIST_NFS_CLIENT_SETTINGS_LIST,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_NFS_CLIENT_SETTINGS_LIST),
         MENU_ENUM_LABEL_DEFERRED_NFS_CLIENT_SETTINGS_LIST);
   CHECK(n > 0 && list_has_label(MENU_ENUM_LABEL_NFS_CLIENT_SERVER_STR), "NFS Network Settings screen has its rows");
   CHECK(list_has_label(MENU_ENUM_LABEL_NFS_CLIENT_VERSION_STR), "NFS Version row is on it");
#endif

   /* and neither sits in the Quick Menu */
   n = open_list(DISPLAYLIST_CONTENT_SETTINGS,
         msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS),
         MENU_ENUM_LABEL_CONTENT_SETTINGS);
   CHECK(n > 0, "Quick Menu opens");
#ifdef HAVE_SMBCLIENT
   CHECK(!list_has_label(MENU_ENUM_LABEL_SMB_CLIENT_SETTINGS_STR), "SMB Network Settings is not in the Quick Menu");
#endif
#ifdef HAVE_NFSCLIENT
   CHECK(!list_has_label(MENU_ENUM_LABEL_NFS_CLIENT_SETTINGS_STR), "NFS Network Settings is not in the Quick Menu");
#endif

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }
   if (failures)
   {
      fprintf(stderr, "FAIL menu_network_settings_test: %u failures\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS menu_network_settings_test\n");
   return 0;
}
