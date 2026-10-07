/* net_http over the built-in TLS client: GET https://localhost:port/path
 * as RetroArch's HTTP task does, through net_http_update() to the end,
 * the server's certificate checked against ca.pem, and the body's
 * SHA-256 compared with the one given.
 *   tls_http port path sha256-hex ca.pem
 * Exit 0 when the body arrives whole and right. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_http.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>
#include <lrc_hash.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain, const char *info)
{
   fprintf(stderr, "verify %s for %s: %s\n", mode_required ? "FAILED" : "failed", domain, info);
}
void ssl_socket_log_verify_disabled(const char *domain) { (void)domain; }

int main(int argc, char **argv)
{
   struct http_connection_t *conn;
   struct http_t *h;
   char     url[256];
   char     hex[65];
   char    *pem     = NULL;
   int64_t  pem_len = 0;
   size_t   pos = 0, tot = 0, len = 0;
   uint8_t *body;
   long     updates = 0;
   int      ret = 1;

   if (argc < 5)
      return 2;
   network_init();
   if (!filestream_read_file(argv[4], (void**)&pem, &pem_len))
      return 2;
   ssl_socket_retro_set_trust_pem(pem, (size_t)pem_len);
   ssl_socket_set_verify_mode(0);   /* required */

   snprintf(url, sizeof(url), "https://localhost:%s/%s", argv[1], argv[2]);
   if (!(conn = net_http_connection_new(url, "GET", NULL)))
      goto done;
   net_http_connection_iterate(conn);
   if (!net_http_connection_done(conn) || !(h = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      goto done;
   }
   net_http_connection_free(conn);

   while (!net_http_update(h, &pos, &tot))
      if (++updates > 50000000L)
         break;
   /* A transfer that never ends is a failure even with every byte
    * received: RetroArch's task would wait on it forever. */
   if (updates > 50000000L)
      fprintf(stderr, "transfer never completed (%u octets received)\n", (unsigned)pos);
   else if (net_http_error(h) || net_http_status(h) != 200)
      fprintf(stderr, "status %d, error %d\n", net_http_status(h), net_http_error(h) ? 1 : 0);
   else if (!(body = net_http_data(h, &len, false)))
      fprintf(stderr, "no body\n");
   else
   {
      sha256_hash(hex, body, len);
      if (strcmp(hex, argv[3]) == 0)
      {
         printf("ok: %u octets, SHA-256 matches\n", (unsigned)len);
         ret = 0;
      }
      else
         fprintf(stderr, "body of %u octets does not match: %s\n", (unsigned)len, hex);
      free(body);   /* net_http_data() hands the buffer over */
   }
   net_http_delete(h);
done:
   free(pem);
   return ret;
}
