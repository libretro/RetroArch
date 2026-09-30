/* Exercises net_smb2.c against a share: connect, list the root, write
 * a file, read it back, stat it, seek, echo. Exit status is the
 * verdict: smb_test server share user password [domain] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_smb2.h>
#include <features/features_cpu.h>

#define CHECK(x, msg) do { if (!(x)) { fprintf(stderr, "FAIL: %s (%s, status 0x%08x)\n", msg, rsmb_get_error(c), rsmb_get_status(c)); return 1; } } while (0)

int main(int argc, char **argv)
{
   struct rsmb_ctx *c;
   struct rsmb_file *f;
   struct rsmb_dir *d;
   struct rsmb_stat st;
   const struct rsmb_dirent *e;
   static uint8_t big[200000];
   static uint8_t back[200000];
   char small[64];
   unsigned i, n = 0, found = 0;

   if (argc < 5)
      return 2;
   network_init();
   c = rsmb_new();
   if (!c)
      return 2;
   rsmb_set_credentials(c, argv[3], argv[4], argc > 5 ? argv[5] : "");
   rsmb_set_timeout(c, 5);
   /* SMB_KRB_REALM (and SMB_KRB_KDC, SMB_KRB_PORT) in the environment:
    * authenticate with Kerberos; SMB_KRB_REQUIRE=1 fails the run if
    * the session did not come from a ticket */
   if (getenv("SMB_KRB_REALM"))
      rsmb_set_kerberos(c, getenv("SMB_KRB_REALM"), getenv("SMB_KRB_KDC"),
            getenv("SMB_KRB_PORT") ? (uint16_t)atoi(getenv("SMB_KRB_PORT")) : 0);
   CHECK(rsmb_connect(c, argv[1], argv[2]) == 0, "connect");
   /* the signing algorithm negotiated, when the harness says which */
   if (getenv("SMB_EXPECT_SIGN"))
      CHECK(rsmb_get_sign_alg(c) == (unsigned)atoi(getenv("SMB_EXPECT_SIGN")), "signing algorithm");
   if (getenv("SMB_KRB_REQUIRE"))
      CHECK(rsmb_used_kerberos(c), "session authenticated with Kerberos");
   CHECK(rsmb_echo(c) == 0, "echo");

   for (i = 0; i < sizeof(big); i++)
      big[i] = (uint8_t)(i * 31 + 7);
   f = rsmb_open(c, "rsmb_test.bin", RSMB_O_WRONLY | RSMB_O_CREAT | RSMB_O_TRUNC);
   CHECK(f, "create");
   CHECK(rsmb_write(c, f, big, sizeof(big)) == (int64_t)sizeof(big), "write");
   CHECK(rsmb_close(c, f) == 0, "close after write");

   CHECK(rsmb_stat(c, "rsmb_test.bin", &st) == 0, "stat");
   CHECK(st.size == sizeof(big) && !st.is_dir, "stat size");
   CHECK(rsmb_stat(c, "/", &st) == 0 && st.is_dir, "stat root");
   CHECK(rsmb_stat(c, "no_such_file", &st) != 0, "stat missing");

   f = rsmb_open(c, "/rsmb_test.bin", RSMB_O_RDONLY);
   CHECK(f, "open");
   CHECK(rsmb_read(c, f, back, sizeof(back)) == (int64_t)sizeof(back), "read");
   CHECK(memcmp(big, back, sizeof(big)) == 0, "read content");
   CHECK(rsmb_read(c, f, small, sizeof(small)) == 0, "read at eof");
   CHECK(rsmb_seek(c, f, 100, 0) == 100, "seek");
   CHECK(rsmb_read(c, f, small, 10) == 10 && memcmp(small, big + 100, 10) == 0, "read after seek");
   CHECK(rsmb_seek(c, f, -5, 2) == (int64_t)sizeof(big) - 5, "seek end");
   CHECK(rsmb_read(c, f, small, 64) == 5, "short read at end");
   /* the read-ahead window: a run of small sequential reads, seeks
    * inside and outside it, a tiny window so its edges are crossed */
   {
      size_t pos = 0, ok = 1;
      rsmb_set_readahead(c, 4096);
      CHECK(rsmb_seek(c, f, 0, 0) == 0, "seek start");
      while (pos < sizeof(big) && ok)
      {
         size_t want = 1000;
         int64_t got = rsmb_read(c, f, small, want > sizeof(small) ? sizeof(small) : want);
         if (got <= 0 || memcmp(small, big + pos, (size_t)got) != 0)
            ok = 0;
         pos += (size_t)got;
      }
      CHECK(ok && pos == sizeof(big), "small sequential reads through the window");
      CHECK(rsmb_seek(c, f, 4090, 0) == 4090 && rsmb_read(c, f, small, 20) == 20
            && memcmp(small, big + 4090, 20) == 0, "read across a window edge");
      CHECK(rsmb_seek(c, f, 150000, 0) == 150000 && rsmb_read(c, f, small, 16) == 16
            && memcmp(small, big + 150000, 16) == 0, "read after a seek out of the window");
      CHECK(rsmb_seek(c, f, 150004, 0) == 150004 && rsmb_read(c, f, small, 8) == 8
            && memcmp(small, big + 150004, 8) == 0, "read inside the window after a seek");
      CHECK(rsmb_seek(c, f, -3, 2) == (int64_t)sizeof(big) - 3 && rsmb_read(c, f, small, 64) == 3
            && memcmp(small, big + sizeof(big) - 3, 3) == 0, "short read at end through the window");
      CHECK(rsmb_read(c, f, small, 64) == 0, "eof through the window");
      rsmb_set_readahead(c, 0);
      CHECK(rsmb_seek(c, f, 7, 0) == 7 && rsmb_read(c, f, small, 9) == 9
            && memcmp(small, big + 7, 9) == 0, "read with read-ahead off");
      rsmb_set_readahead(c, 1024 * 1024);
   }
   CHECK(rsmb_close(c, f) == 0, "close");

   d = rsmb_opendir(c, "");
   CHECK(d, "opendir");
   while ((e = rsmb_readdir(c, d)))
   {
      n++;
      if (strcmp(e->name, "rsmb_test.bin") == 0 && e->st.size == sizeof(big) && !e->st.is_dir)
         found = 1;
   }
   rsmb_closedir(c, d);
   CHECK(found, "readdir sees the file");
   CHECK(!rsmb_open(c, "no_such_file", RSMB_O_RDONLY), "open missing");

   rsmb_disconnect(c);

   /* share enumeration through IPC$: the share we used must be listed,
    * IPC$ itself comes back typed as IPC */
   {
      static struct rsmb_share list[64];
      int cnt, i, seen = 0, ipc = 0;
      CHECK(rsmb_connect(c, argv[1], "IPC$") == 0, "connect IPC$");
      cnt = rsmb_enum_shares(c, list, 64);
      CHECK(cnt > 0, "enum shares");
      for (i = 0; i < cnt && i < 64; i++)
      {
         if (strcmp(list[i].name, argv[2]) == 0 && (list[i].type & 3) == 0)
            seen = 1;
         if (strcmp(list[i].name, "IPC$") == 0 && (list[i].type & 3) == 3)
            ipc = 1;
      }
      CHECK(seen && ipc, "enum shares content");
      rsmb_disconnect(c);
   }
   /* request latency on this link, so a stall like the two-write
    * NetBIOS framing once cost (40 ms per request) shows in the log */
   {
      retro_time_t t0;
      int i, echoes = 20;
      CHECK(rsmb_connect(c, argv[1], argv[2]) == 0, "reconnect for latency");
      t0 = cpu_features_get_time_usec();
      for (i = 0; i < echoes; i++)
         CHECK(rsmb_echo(c) == 0, "echo");
      printf("echo latency: %.2f ms per request\n",
            (cpu_features_get_time_usec() - t0) / 1000.0 / echoes);
      rsmb_disconnect(c);
   }
   rsmb_free(c);
   printf("ok: %s/%s (%u entries)\n", argv[1], argv[2], n);
   return 0;
}
