/* net_http over the built-in TLS client: GET https://localhost:port/path
 * as RetroArch's HTTP task does, through net_http_update() to the end,
 * the server's certificate checked against ca.pem, and the body's
 * SHA-256 compared with the one given.
 *   tls_http port path sha256-hex ca.pem [steps]
 * With no steps, one GET. Steps run in order:
 *   -t ca.der -c cert.der -k key.der -s suites -n -H
 *        settings for the GETs after them (net_http_connection_set_tls)
 *   -g   a GET with those settings
 *   -u   a GET without them, on the global trust (ca.pem)
 * Each step prints "get N ok" or "get N failed".
 * Exit 0 when every body arrives whole and right. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/net_compat.h>
#include <net/net_http.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>
#include <crypto/pk.h>
#include <crypto/x509.h>
#include <lrc_hash.h>
#include <features/features_cpu.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain, const char *info)
{
   fprintf(stderr, "verify %s for %s: %s\n", mode_required ? "FAILED" : "failed", domain, info);
}
void ssl_socket_log_verify_disabled(const char *domain) { (void)domain; }

static uint8_t *slurp(const char *path, size_t *len)
{
   int64_t n = 0;
   void   *b = NULL;
   if (!filestream_read_file(path, &b, &n))
      return NULL;
   *len = (size_t)n;
   return (uint8_t*)b;
}

/* One GET of url through net_http, cfg (NULL: none) for its connection.
 * Returns 0 when the body hashes to want. */
static int get(const char *url, const char *want,
      const struct ssl_socket_retro_config *cfg)
{
   struct http_connection_t *conn;
   struct http_t *h;
   char     hex[65];
   size_t   pos = 0, tot = 0, len = 0;
   uint8_t *body;
   retro_time_t deadline = cpu_features_get_time_usec() + 30000000;
   bool timed_out = false;
   int      ret = 1;

   if (!(conn = net_http_connection_new(url, "GET", NULL)))
      return 1;
   if (cfg)
      net_http_connection_set_tls(conn, cfg);
   net_http_connection_iterate(conn);
   if (!net_http_connection_done(conn) || !(h = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      return 1;
   }
   net_http_connection_free(conn);

   while (!net_http_update(h, &pos, &tot))
   {
      if (cpu_features_get_time_usec() >= deadline)
      {
         timed_out = true;
         break;
      }
      net_http_wait(h, 100);
   }
   /* A transfer that never ends is a failure even with every byte
    * received: RetroArch's task would wait on it forever. */
   if (timed_out)
      fprintf(stderr, "transfer never completed (%u octets received)\n", (unsigned)pos);
   else if (net_http_error(h) || net_http_status(h) != 200)
      fprintf(stderr, "status %d, error %d\n", net_http_status(h), net_http_error(h) ? 1 : 0);
   else if (!(body = net_http_data(h, &len, false)))
      fprintf(stderr, "no body\n");
   else
   {
      sha256_hash(hex, body, len);
      if (strcmp(hex, want) == 0)
         ret = 0;
      else
         fprintf(stderr, "body of %u octets does not match: %s\n", (unsigned)len, hex);
      free(body);   /* net_http_data() hands the buffer over */
   }
   net_http_delete(h);
   return ret;
}

int main(int argc, char **argv)
{
   struct ssl_socket_retro_config cfg;
   struct rsa_private_key key;
   uint8_t       *key_der = NULL;
   const uint8_t *trust[1];
   size_t         trust_len[1];
   const uint8_t *chain[4];
   size_t         chain_len[4];
   unsigned       suites[24];
   char           url[256];
   char          *pem     = NULL;
   int64_t        pem_len = 0;
   int            i, n = 0, bad = 0;

   if (argc < 5)
      return 2;
   network_init();
   if (!filestream_read_file(argv[4], (void**)&pem, &pem_len))
      return 2;
   ssl_socket_retro_set_trust_pem(pem, (size_t)pem_len);
   ssl_socket_set_verify_mode(0);   /* required */
   snprintf(url, sizeof(url), "https://localhost:%s/%s", argv[1], argv[2]);
   memset(&cfg, 0, sizeof(cfg));

   if (argc == 5)
      bad = get(url, argv[3], NULL);
   for (i = 5; i < argc && bad >= 0; i++)
   {
      size_t len;
      if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "-u"))
      {
         int r = get(url, argv[3], argv[i][1] == 'g' ? &cfg : NULL);
         printf("get %d %s\n", ++n, r ? "failed" : "ok");
         fflush(stdout);
         bad |= r;
      }
      else if (!strcmp(argv[i], "-t") && i + 1 < argc && !cfg.trust_count)
      {
         if (!(trust[0] = slurp(argv[++i], &trust_len[0])))
            bad = -1;
         cfg.trust       = trust;
         cfg.trust_lens  = trust_len;
         cfg.trust_count = 1;
      }
      else if (!strcmp(argv[i], "-c") && i + 1 < argc && cfg.chain_count < 4)
      {
         if (!(chain[cfg.chain_count] = slurp(argv[++i], &chain_len[cfg.chain_count])))
            bad = -1;
         cfg.chain      = chain;
         cfg.chain_lens = chain_len;
         cfg.chain_count++;
      }
      else if (!strcmp(argv[i], "-k") && i + 1 < argc && !key_der)
      {
         if (!(key_der = slurp(argv[++i], &len))
               || x509_rsa_private_key_parse(&key, key_der, len) != 0)
            bad = -1;
         cfg.key = &key;
      }
      else if (!strcmp(argv[i], "-s") && i + 1 < argc && !cfg.suite_count)
      {
         char *p = argv[++i];
         while (*p && cfg.suite_count < 24)
         {
            suites[cfg.suite_count++] = (unsigned)strtoul(p, &p, 16);
            if (*p == ',')
               p++;
         }
         cfg.suites = suites;
      }
      else if (!strcmp(argv[i], "-n"))
         cfg.flags |= SSL_RETRO_NO_VERIFY;
      else if (!strcmp(argv[i], "-H"))
         cfg.flags |= SSL_RETRO_NO_HOSTNAME;
      else
         bad = -1;
   }

   if (bad == 0 && argc == 5)
      printf("ok: SHA-256 matches\n");
   if (cfg.trust_count)
      free((void*)trust[0]);
   for (i = 0; i < (int)cfg.chain_count; i++)
      free((void*)chain[i]);
   free(key_der);
   free(pem);
   return bad < 0 ? 2 : bad;
}
