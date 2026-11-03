/* The real vfs_implementation_smb.c over the built-in client: the path
 * a core takes when it opens smb://server/share/file. Same arguments
 * as smb_test; expects rsmb_test.bin from that run to exist. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_timers.h>
#include <net/net_compat.h>
#include <vfs/vfs.h>
#include <vfs/vfs_implementation.h>
#include "../../../libretro-common/vfs/vfs_implementation_smb.h"

static const char *failure_name(enum smb_failure f)
{
   static const char *const names[] = { "unknown", "connected", "resolve", "connect", "login", "share" };
   return (unsigned)f < sizeof(names) / sizeof(*names) ? names[f] : "?";
}

/* With no share configured, the stat of smb://server/@share fails and
 * the share's code is @want. With SMB_WAIT_OK=<seconds> the stat is
 * then retried until it works (the caller thaws the server): the code
 * reads connected, and a missing path leaves it so. */
static int expect_failure(struct smb_settings *cfg, const char *share,
      const char *want)
{
   char url[512];
   char missing[600];
   int tries;
   cfg->share   = "";
   cfg->timeout = 2;
   if (!smb_init_cfg(cfg))
      return 1;
   snprintf(url, sizeof(url), "smb://%s/%s", cfg->server_address, share);
   if (retro_vfs_stat_smb(url, NULL) != 0)
   {
      fprintf(stderr, "FAIL: %s was reached\n", url);
      return 1;
   }
   if (strcmp(failure_name(smb_url_failure(url)), want) != 0)
   {
      fprintf(stderr, "FAIL: %s: code %s, want %s\n", url,
            failure_name(smb_url_failure(url)), want);
      return 1;
   }
   printf("ok: %s: %s\n", url, want);
   fflush(stdout);
   if (!getenv("SMB_WAIT_OK"))
      return 0;
   for (tries = atoi(getenv("SMB_WAIT_OK")) * 5; tries > 0; tries--)
   {
      if (retro_vfs_stat_smb(url, NULL) != 0)
         break;
      retro_sleep(200);
   }
   snprintf(missing, sizeof(missing), "%s/rsmb_missing.bin", url);
   if (!tries || smb_url_failure(url) != SMB_FAILURE_CONNECTED
         || retro_vfs_stat_smb(missing, NULL) != 0
         || smb_url_failure(missing) != SMB_FAILURE_CONNECTED)
   {
      fprintf(stderr, "FAIL: %s: %s after the server came back, want connected\n",
            url, tries ? failure_name(smb_url_failure(url)) : "never reached");
      return 1;
   }
   printf("ok: %s: connected once the server answered, and after a missing path\n", url);
   return 0;
}

static const char *check_name(enum smb_url_result r)
{
   static const char *const names[] = { "ok", "no server", "host differs", "share differs" };
   return (unsigned)r < sizeof(names) / sizeof(*names) ? names[r] : "?";
}

/* smb_url_check(@url) with @server and @share configured gives @want
 * and @detail. */
static int check_url(struct smb_settings *cfg, const char *server,
      const char *share, const char *url, enum smb_url_result want,
      const char *detail)
{
   char got[256];
   enum smb_url_result r;
   cfg->server_address = server;
   cfg->share          = share;
   if (!smb_init_cfg(cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg\n");
      return 1;
   }
   r = smb_url_check(url, got, sizeof(got));
   if (r != want || strcmp(got, detail) != 0)
   {
      fprintf(stderr, "FAIL: %s, server \"%s\" share \"%s\": %s \"%s\", want %s \"%s\"\n",
            url, server, share, check_name(r), got, check_name(want), detail);
      return 1;
   }
   printf("ok: %s, server \"%s\" share \"%s\": %s\n", url, server, share, check_name(r));
   return 0;
}

/* The folder the menu's SMB entry opens, as
 * menu_displaylist_build_smb_root() builds it, and one the browser
 * joins below it, both pass. */
static int check_menu(struct smb_settings *cfg, const char *server,
      const char *share, const char *subdir)
{
   char root[512];
   char below[600];
   snprintf(root, sizeof(root), "smb://%s%s%s%s%s", server,
         *share ? "/" : "", share, *subdir && *subdir != '/' ? "/" : "", subdir);
   snprintf(below, sizeof(below), "%s/Nintendo - NES", root);
   return check_url(cfg, server, share, root, SMB_URL_OK, "")
         + check_url(cfg, server, share, below, SMB_URL_OK, "");
}

/* SMB_CHECK_URLS: URLs against the settings, before network_init(), with
 * no server; localhost must resolve to 127.0.0.1. */
static int check_urls(void)
{
   struct smb_settings cfg;
   char long_host[320];
   int bad = 0;
   memset(&cfg, 0, sizeof(cfg));
   if (!smb_init_cfg(&cfg))
      return 1;
   memcpy(long_host, "smb://", 6);
   memset(long_host + 6, 'a', 300);
   strcpy(long_host + 306, "/share/roms");
   /* the same text resolves nowhere: no lookup when only case differs */
   bad += check_url(&cfg, "nas.INVALID", "share", "smb://NAS.invalid/share/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://localhost/share/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "localhost", "", "smb://127.0.0.1/share", SMB_URL_OK, "");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.2/share/roms", SMB_URL_HOST_DIFFERS, "127.0.0.1");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://nosuch.invalid/share/roms", SMB_URL_HOST_DIFFERS, "127.0.0.1");
   bad += check_url(&cfg, "nosuch.invalid", "share", "smb://127.0.0.1/share/roms", SMB_URL_HOST_DIFFERS, "nosuch.invalid");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb:///share/roms", SMB_URL_HOST_DIFFERS, "127.0.0.1");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://[::1/share/roms", SMB_URL_HOST_DIFFERS, "127.0.0.1");
   bad += check_url(&cfg, "127.0.0.1", "share", long_host, SMB_URL_HOST_DIFFERS, "127.0.0.1");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://guest@127.0.0.1:445/share/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "127.0.0.1:445", "share", "smb://127.0.0.1/share/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "::1", "share", "smb://[::1]/share/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "[::1]:445", "share", "smb://::1/share/roms", SMB_URL_OK, "");
#ifdef HAVE_INET6
   bad += check_url(&cfg, "0:0:0:0:0:0:0:1", "share", "smb://[::1]/share/roms", SMB_URL_OK, "");
#endif
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.1/other/roms", SMB_URL_SHARE_DIFFERS, "share");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.1/SHARE/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "127.0.0.1", "", "smb://127.0.0.1/other/roms", SMB_URL_OK, "");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.1", SMB_URL_SHARE_DIFFERS, "share");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.1/", SMB_URL_SHARE_DIFFERS, "share");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.1//roms", SMB_URL_SHARE_DIFFERS, "share");
   bad += check_url(&cfg, "127.0.0.1", "share", "smb://127.0.0.2/other", SMB_URL_HOST_DIFFERS, "127.0.0.1");
   bad += check_url(&cfg, "", "share", "smb://127.0.0.1/share/roms", SMB_URL_NO_SERVER, "");
   bad += check_menu(&cfg, "127.0.0.1", "share", "roms");
   bad += check_menu(&cfg, "127.0.0.1", "share", "/roms/nes");
   bad += check_menu(&cfg, "127.0.0.1", "", "");
   bad += check_menu(&cfg, "NAS.invalid", "Retro", "");
   bad += check_menu(&cfg, "user@nas.invalid:1445", "share", "roms");
   bad += check_menu(&cfg, "::1", "share", "");
   bad += check_menu(&cfg, "[::1]:445", "share", "roms");
   if (bad)
      return 1;
   printf("ok: URLs checked against the settings\n");
   return 0;
}

int main(int argc, char **argv)
{
   struct smb_settings cfg;
   libretro_vfs_implementation_file stream;
   libretro_vfs_implementation_file held[2];
   smb_dir_handle *dh;
   libretro_vfs_implementation_dir *rdir;
   struct smbc_dirent *ent;
   char url[512];
   uint8_t buf[4096];
   int64_t size = 0;
   int found = 0;
   int i;

   if (getenv("SMB_CHECK_URLS"))
      return check_urls();
   if (argc < 5)
      return 2;
   network_init();
   memset(&cfg, 0, sizeof(cfg));
   cfg.server_address = argv[1];
   cfg.share          = argv[2];
   cfg.username       = argv[3];
   cfg.password       = argv[4];
   cfg.workgroup      = argc > 5 ? argv[5] : "";
   cfg.timeout        = 5;
   cfg.num_contexts   = 2;
   cfg.auth_mode      = RETRO_SMB2_SEC_NTLMSSP;
   if (!smb_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg\n");
      return 1;
   }
   /* SMB_EXPECT_FAILURE (resolve, connect, login or share) */
   if (getenv("SMB_EXPECT_FAILURE"))
      return expect_failure(&cfg, argv[2], getenv("SMB_EXPECT_FAILURE"));
   snprintf(url, sizeof(url), "smb://%s/%s/rsmb_test.bin", argv[1], argv[2]);
   if (smb_url_failure(url) != SMB_FAILURE_UNKNOWN)
   {
      fprintf(stderr, "FAIL: a code before any connect\n");
      return 1;
   }
   if (retro_vfs_stat_smb(url, &size) == 0 || size != 200000)
   {
      fprintf(stderr, "FAIL: stat (%lld)\n", (long long)size);
      return 1;
   }
   if (smb_url_failure(url) != SMB_FAILURE_CONNECTED)
   {
      fprintf(stderr, "FAIL: code %s after a connect, want connected\n",
            failure_name(smb_url_failure(url)));
      return 1;
   }
   memset(&stream, 0, sizeof(stream));
   if (!retro_vfs_file_open_smb(&stream, url, RETRO_VFS_FILE_ACCESS_READ, 0))
   {
      fprintf(stderr, "FAIL: open\n");
      return 1;
   }
   if (retro_vfs_file_seek_smb(&stream, 100, RETRO_VFS_SEEK_POSITION_START) != 0
         || retro_vfs_file_read_smb(&stream, buf, 16) != 16
         || buf[0] != (uint8_t)(100 * 31 + 7)
         || retro_vfs_file_tell_smb(&stream) != 116)
   {
      fprintf(stderr, "FAIL: seek/read/tell\n");
      return 1;
   }
   retro_vfs_file_close_smb(&stream);

   snprintf(url, sizeof(url), "smb://%s/%s", argv[1], argv[2]);
   if (!(dh = retro_vfs_opendir_smb(url, false)))
   {
      fprintf(stderr, "FAIL: opendir\n");
      return 1;
   }
   while ((ent = retro_vfs_readdir_smb(dh)))
      if (strcmp(ent->name, "rsmb_test.bin") == 0 && ent->size == 200000)
         found = 1;
   retro_vfs_closedir_smb(dh);
   if (!found)
   {
      fprintf(stderr, "FAIL: readdir\n");
      return 1;
   }
   /* both pool slots held by open files: a missing path's stat, open
    * and opendir each connect privately, and LSan sees any of those
    * connections a failure does not close */
   snprintf(url, sizeof(url), "smb://%s/%s/rsmb_test.bin", argv[1], argv[2]);
   memset(held, 0, sizeof(held));
   for (i = 0; i < 2; i++)
      if (!retro_vfs_file_open_smb(&held[i], url, RETRO_VFS_FILE_ACCESS_READ, 0))
      {
         fprintf(stderr, "FAIL: open %d of 2\n", i + 1);
         return 1;
      }
   snprintf(url, sizeof(url), "smb://%s/%s/rsmb_missing.bin", argv[1], argv[2]);
   memset(&stream, 0, sizeof(stream));
   if (     retro_vfs_stat_smb(url, NULL) != 0
         || retro_vfs_file_open_smb(&stream, url, RETRO_VFS_FILE_ACCESS_READ, 0)
         || retro_vfs_opendir_smb(url, false))
   {
      fprintf(stderr, "FAIL: a missing path was found\n");
      return 1;
   }
   if (smb_url_failure(url) != SMB_FAILURE_CONNECTED)
   {
      fprintf(stderr, "FAIL: code %s after a missing path, want connected\n",
            failure_name(smb_url_failure(url)));
      return 1;
   }
   for (i = 0; i < 2; i++)
      retro_vfs_file_close_smb(&held[i]);
   /* the share browser: with no share configured, smb://server lists
    * the shares; "share" is there, IPC$ and the hidden one are not */
   smb_shutdown();
   cfg.share = "";
   if (!smb_init_cfg(&cfg))
   {
      fprintf(stderr, "FAIL: smb_init_cfg (browse)\n");
      return 1;
   }
   snprintf(url, sizeof(url), "smb://%s", argv[1]);
   found = 0;
   if (!(dh = retro_vfs_opendir_smb(url, false)))
   {
      fprintf(stderr, "FAIL: opendir server\n");
      return 1;
   }
   while ((ent = retro_vfs_readdir_smb(dh)))
   {
      if (strcmp(ent->name, argv[2]) == 0)
         found = 1;
      if (strcmp(ent->name, "IPC$") == 0 || strcmp(ent->name, "hidden$") == 0)
         found = -1;
   }
   retro_vfs_closedir_smb(dh);
   if (found != 1)
   {
      fprintf(stderr, "FAIL: share browse (%d)\n", found);
      return 1;
   }
   /* the same listing through the dispatcher, as the menu browses it */
   found = 0;
   if (!(rdir = retro_vfs_opendir_impl(url, false)))
   {
      fprintf(stderr, "FAIL: dispatcher opendir server\n");
      return 1;
   }
   while (retro_vfs_readdir_impl(rdir))
      if (     strcmp(retro_vfs_dirent_get_name_impl(rdir), argv[2]) == 0
            && retro_vfs_dirent_is_dir_impl(rdir))
         found = 1;
   retro_vfs_closedir_impl(rdir);
   if (found != 1)
   {
      fprintf(stderr, "FAIL: dispatcher share browse\n");
      return 1;
   }
   smb_shutdown();
   printf("ok: VFS over the SMB backend\n");
   return 0;
}
