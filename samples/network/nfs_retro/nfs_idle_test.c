/* A file kept open across a long pause: reads after the pause must
 * work however long the client stayed quiet - past the server's lease
 * (v4), or a dropped connection.
 *
 * Usage: nfs_idle_test <server> <export> <nfs port> <mount port>
 *        <version> <idle seconds> [w]
 * With w the write comes first after the pause, so it is the call
 * that finds the connection gone.
 * Exit status is the number of failures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_timers.h>
#include <net/net_compat.h>
#include <net/net_nfs3.h>

#define SIZE 65536

int main(int argc, char **argv)
{
   static uint8_t data[SIZE], back[SIZE];
   struct rnfs_ctx  *c;
   struct rnfs_file *f, *w;
   unsigned i, idle;
   int fails = 0, write_first;

   if (argc < 7)
      return 2;
   idle        = (unsigned)atoi(argv[6]);
   write_first = argc > 7 && argv[7][0] == 'w';
   network_init();
   for (i = 0; i < SIZE; i++)
      data[i] = (uint8_t)(i * 7 + (i >> 8));
   c = rnfs_new();
   rnfs_set_timeout(c, 5);
   rnfs_set_ports(c, (uint16_t)atoi(argv[3]), (uint16_t)atoi(argv[4]));
   rnfs_set_version(c, (unsigned)atoi(argv[5]));
   /* no read-ahead, as RetroArch runs by default: each read goes to
    * the server, or a read after the pause is served from the window
    * the first one fetched and tests nothing */
   rnfs_set_readahead(c, 0);
   if (rnfs_connect(c, argv[1], argv[2]) != 0)
   {
      fprintf(stderr, "FAIL: mount (%s)\n", rnfs_get_error(c));
      return 1;
   }
   f = rnfs_open(c, "idle.bin", RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC);
   if (!f || rnfs_write(c, f, data, SIZE) != SIZE)
   {
      fprintf(stderr, "FAIL: create (%s)\n", rnfs_get_error(c));
      return 1;
   }
   rnfs_close(c, f);

   /* and a file open for writing, which in v4 holds open state the
    * server may lose */
   if (!(w = rnfs_open(c, "idle_w.bin", RNFS_O_WRONLY | RNFS_O_CREAT | RNFS_O_TRUNC))
         || rnfs_write(c, w, data, 8192) != 8192)
   {
      fprintf(stderr, "FAIL: write before the pause (%s)\n", rnfs_get_error(c));
      return 1;
   }
   if (!(f = rnfs_open(c, "idle.bin", RNFS_O_RDONLY)))
   {
      fprintf(stderr, "FAIL: open (%s)\n", rnfs_get_error(c));
      return 1;
   }
   if (rnfs_read(c, f, back, 4096) != 4096 || memcmp(back, data, 4096))
   {
      fprintf(stderr, "FAIL: read before the pause (%s)\n", rnfs_get_error(c));
      fails++;
   }
   printf("idle for %u s with the file open\n", idle);
   fflush(stdout);
   for (i = 0; i < idle; i++)
      retro_sleep(1000);
   if (write_first)
   {
      if (rnfs_write(c, w, data + 8192, 8192) != 8192)
      {
         fprintf(stderr, "FAIL: write after %u s idle (%s, status %u)\n", idle,
               rnfs_get_error(c), rnfs_get_status(c));
         fails++;
      }
      else
         printf("ok:   write after %u s idle, first call after the pause\n", idle);
   }
   rnfs_seek(c, f, 32768, 0);
   if (rnfs_read(c, f, back, 4096) != 4096 || memcmp(back, data + 32768, 4096))
   {
      fprintf(stderr, "FAIL: read after %u s idle (%s, status %u)\n", idle,
            rnfs_get_error(c), rnfs_get_status(c));
      fails++;
   }
   else
      printf("ok:   read after %u s idle\n", idle);
   if (!write_first)
   {
      if (rnfs_write(c, w, data + 8192, 8192) != 8192)
      {
         fprintf(stderr, "FAIL: write after %u s idle (%s, status %u)\n", idle,
               rnfs_get_error(c), rnfs_get_status(c));
         fails++;
      }
      else
         printf("ok:   write after %u s idle\n", idle);
   }
   rnfs_close(c, w);
   if (!(w = rnfs_open(c, "idle_w.bin", RNFS_O_RDONLY))
         || rnfs_read(c, w, back, 16384) != 16384 || memcmp(back, data, 16384))
   {
      fprintf(stderr, "FAIL: what was written before and after the pause\n");
      fails++;
   }
   else
      printf("ok:   both writes landed\n");
   if (w)
      rnfs_close(c, w);
   rnfs_close(c, f);
   rnfs_unlink(c, "idle_w.bin");
   rnfs_unlink(c, "idle.bin");
   rnfs_free(c);
   return fails;
}
