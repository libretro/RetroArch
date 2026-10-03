/* KeyUpdate handling against openssl s_server's interactive console:
 * the server is driven by a script on its stdin (lines are sent to the
 * client; 'K' sends a KeyUpdate with update_requested, 'k' one
 * without). The client reads a line, sends a line, reads the next line
 * after each update, and the server's stdout shows what it decrypted
 * from us under the new keys.
 *   tls_keyupdate host port ca.pem */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>
void ssl_socket_log_verify_fail(int m, const char *d, const char *i) { fprintf(stderr, "verify: %s %s\n", d, i); (void)m; }
void ssl_socket_log_verify_disabled(const char *d) { (void)d; }

static int read_line(void *ssl, char *out, size_t cap)
{
   size_t n = 0;
   while (n + 1 < cap)
   {
      if (ssl_socket_receive_all_blocking(ssl, out + n, 1) != 1)
         return -1;
      if (out[n] == '\n')
         break;
      n++;
   }
   out[n] = '\0';
   return 0;
}

int main(int argc, char **argv)
{
   struct addrinfo *addr = NULL;
   int fd; void *ssl; char line[128], *pem = NULL; int64_t pem_len = 0;
   const char *expect[] = { "hello", "after-K", "after-k" };
   const char *reply[]  = { "ack-1\n", "ack-after-K\n", "ack-after-k\n" };
   int i;
   if (argc < 4)
      return 2;
   network_init();
   if (filestream_read_file(argv[3], (void**)&pem, &pem_len))
      ssl_socket_retro_set_trust_pem(pem, (size_t)pem_len);
   ssl_socket_set_verify_mode(0);
   fd = socket_init((void**)&addr, atoi(argv[2]), argv[1], SOCKET_TYPE_STREAM, AF_INET);
   /* ssl_socket_connect() makes the TCP connection itself */
   if (fd < 0 || !addr) return 1;
   ssl = ssl_socket_init(fd, argv[1]);
   if (ssl_socket_connect(ssl, addr, true, true) < 0) { fprintf(stderr, "connect failed (%d)\n", ssl_socket_last_error(ssl)); return 1; }
   for (i = 0; i < 3; i++)
   {
      if (read_line(ssl, line, sizeof(line)) != 0 || strcmp(line, expect[i]) != 0)
      {
         fprintf(stderr, "step %d: got \"%s\", expected \"%s\"\n", i, line, expect[i]);
         return 1;
      }
      if (ssl_socket_send_all_blocking(ssl, reply[i], strlen(reply[i]), true) < 0)
         return 1;
   }
   printf("ok: three exchanges across a requested and an unrequested KeyUpdate\n");
   ssl_socket_close(ssl); ssl_socket_free(ssl); freeaddrinfo_retro(addr); free(pem); return 0;
}
