/* A sparse file read whole: the bytes must match the file on the
 * server, and with NFSv4.2 READ_PLUS its holes must not cross the
 * network - what the replies came to is checked against the data.
 *
 * Usage: nfs_sparse_test <server> <export> <nfs port> <file in export>
 *        <the same file, local path> <data bytes in it> [plus|fallback]
 * plus: READ_PLUS must still be in use at the end; fallback: the server
 * gave replies no READ could have, so the client must have gone back
 * to READ - and read every octet right either way.
 * Exit status is the failure count. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_nfs3.h>

int main(int argc, char **argv)
{
   struct rnfs_ctx  *c;
   struct rnfs_file *f;
   FILE     *lf;
   uint8_t  *mine, *theirs;
   long      size;
   uint64_t  rx0, rx, data;
   int64_t   got, total = 0;
   int       fails = 0;
   const char *expect = argc > 7 ? argv[7] : "";

   if (argc < 7)
      return 2;
   data = (uint64_t)strtoull(argv[6], NULL, 10);
   network_init();
   if (!(lf = fopen(argv[5], "rb")))
      return 2;
   fseek(lf, 0, SEEK_END);
   size = ftell(lf);
   fseek(lf, 0, SEEK_SET);
   theirs = (uint8_t*)malloc((size_t)size);
   mine   = (uint8_t*)malloc((size_t)size);
   if (!theirs || !mine || fread(theirs, 1, (size_t)size, lf) != (size_t)size)
      return 2;
   fclose(lf);

   c = rnfs_new();
   rnfs_set_timeout(c, 5);
   rnfs_set_ports(c, (uint16_t)atoi(argv[3]), 0);
   rnfs_set_version(c, 4);
   rnfs_set_readahead(c, 0);
   if (rnfs_connect(c, argv[1], argv[2]) != 0 || !(f = rnfs_open(c, argv[4], RNFS_O_RDONLY)))
   {
      fprintf(stderr, "FAIL: open (%s)\n", rnfs_get_error(c));
      return 1;
   }
   rx0 = rnfs_get_rx_bytes(c);
   while (total < size && (got = rnfs_read(c, f, mine + total, 1 << 20)) > 0)
      total += got;
   rx = rnfs_get_rx_bytes(c) - rx0;
   rnfs_close(c, f);

   if (total != size || memcmp(mine, theirs, (size_t)size))
   {
      fprintf(stderr, "FAIL: %lld of %ld octets read, or they differ\n",
            (long long)total, size);
      fails++;
   }
   else
      printf("ok:   %ld octets read over NFSv4.%u, all as on the server\n",
            size, rnfs_get_minor_version(c));
   printf("replies: %llu octets for %llu of data (%s)\n",
         (unsigned long long)rx, (unsigned long long)data,
         rnfs_get_read_plus(c) ? "READ_PLUS" : "READ");
   if (!strcmp(expect, "plus") && !rnfs_get_read_plus(c))
   {
      fprintf(stderr, "FAIL: READ_PLUS was given up on\n");
      fails++;
   }
   if (!strcmp(expect, "fallback") && rnfs_get_read_plus(c))
   {
      fprintf(stderr, "FAIL: a broken READ_PLUS was kept\n");
      fails++;
   }
   /* READ_PLUS: the data and a little framing, not the holes */
   if (rnfs_get_read_plus(c) && rx > data + data / 8 + 65536)
   {
      fprintf(stderr, "FAIL: holes crossed the network\n");
      fails++;
   }
   rnfs_free(c);
   free(mine);
   free(theirs);
   return fails;
}
