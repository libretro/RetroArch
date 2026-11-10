/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (tls_config.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* One connection under ssl_socket_retro_configure(), then a GET:
 *
 *   tls_config host port [-t ca.der] [-c cert.der]... [-k key.der]
 *              [-s 0035,c02f] [-n] [-H] [-g path sha256hex]
 *
 * -t trusts that certificate instead of the bundle, -c / -k present a
 * client chain and its RSA key (PKCS#1 or PKCS#8), -s offers those
 * suites, -n / -H skip the verification / the hostname check, and -g
 * fetches path and checks the body's SHA-256. Prints the version and
 * suite, and a record refused mid-transfer; exits 0 on success, 1 on a
 * failed connection or check. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include "tls_handshake_driver.h"
#include <crypto/pk.h>
#include <crypto/x509.h>
#include <lrc_hash.h>

void ssl_socket_log_verify_fail(int mode_required, const char *domain, const char *info)
{
   fprintf(stderr, "verify %s for %s: %s\n", mode_required ? "FAILED (required)" : "failed (optional)", domain, info);
}
void ssl_socket_log_verify_disabled(const char *domain)
{
   fprintf(stderr, "verify disabled for %s\n", domain);
}

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE    *f = fopen(path, "rb");
   long     n;
   uint8_t *b;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc((size_t)n + 1);
   if (!b || fread(b, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(b);
      return NULL;
   }
   fclose(f);
   *len = (size_t)n;
   return b;
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
   const char    *host, *path = NULL, *want = NULL;
   uint16_t       port;
   struct addrinfo *addr = NULL;
   struct sha256_state h;
   char           req[512];
   static char    buf[65536];
   size_t         got = 0, body = 0;
   int            i, in_body = 0;
   void          *ssl;
   int            fd;
   int            rc  = 1;
   bool           err = false;

   if (argc < 3)
      return 2;
   host = argv[1];
   port = (uint16_t)atoi(argv[2]);
   memset(&cfg, 0, sizeof(cfg));
   for (i = 3; i < argc; i++)
   {
      size_t len;
      if (!strcmp(argv[i], "-t") && i + 1 < argc)
      {
         if (!(trust[0] = slurp(argv[++i], &trust_len[0])))
            return 2;
         cfg.trust       = trust;
         cfg.trust_lens  = trust_len;
         cfg.trust_count = 1;
      }
      else if (!strcmp(argv[i], "-c") && i + 1 < argc && cfg.chain_count < 4)
      {
         if (!(chain[cfg.chain_count] = slurp(argv[++i], &chain_len[cfg.chain_count])))
            return 2;
         cfg.chain      = chain;
         cfg.chain_lens = chain_len;
         cfg.chain_count++;
      }
      else if (!strcmp(argv[i], "-k") && i + 1 < argc)
      {
         if (!(key_der = slurp(argv[++i], &len))
               || x509_rsa_private_key_parse(&key, key_der, len) != 0)
            return 2;
         cfg.key = &key;
      }
      else if (!strcmp(argv[i], "-s") && i + 1 < argc)
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
      else if (!strcmp(argv[i], "-g") && i + 2 < argc)
      {
         path = argv[++i];
         want = argv[++i];
      }
      else
         return 2;
   }

   network_init();
   fd = socket_init((void**)&addr, port, host, SOCKET_TYPE_STREAM, AF_INET);
   if (fd < 0 || !addr || !(ssl = ssl_socket_init(fd, host)))
      return 2;
   if (ssl_socket_retro_configure(ssl, &cfg) != 0)
   {
      fprintf(stderr, "configuration refused\n");
      goto done;
   }
   if (tls_test_connect(ssl, fd, addr, getenv("TLS_TEST_HANDSHAKE_STEP") != NULL) < 0)
   {
      fprintf(stderr, "TLS connect failed (%d)\n", ssl_socket_last_error(ssl));
      goto done;
   }
   printf("%s suite %04x\n", ssl_socket_retro_version(ssl) == 0x0304 ? "TLS 1.3" : "TLS 1.2",
         ssl_socket_retro_suite(ssl));
   snprintf(req, sizeof(req), "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n", path ? path : "", host);
   if (ssl_socket_send_all_blocking(ssl, req, strlen(req), true) < 0)
      goto done;

   /* the whole response, to the server's close; the body after the
    * blank line goes through SHA-256 */
   sha256_stream_init(&h, 0);
   for (;;)
   {
      ssize_t n = ssl_socket_receive_all_nonblocking(ssl, &err, buf, sizeof(buf));
      size_t  off = 0;
      if (n < 0 || err)
      {
         if (ssl_socket_last_error(ssl))
            fprintf(stderr, "receive error %d\n", ssl_socket_last_error(ssl));
         break;
      }
      if (n == 0)
         continue;
      if (!in_body)
      {
         char *e;
         buf[(size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1] = '\0';
         if (!got && strncmp(buf, "HTTP/1.", 7) != 0)
            goto done;
         if ((e = strstr(buf, "\r\n\r\n")))
         {
            in_body = 1;
            off     = (size_t)(e + 4 - buf);
         }
         else
            off = (size_t)n;
      }
      sha256_stream_update(&h, (const uint8_t*)buf + off, (size_t)n - off);
      body += (size_t)n - off;
      got  += (size_t)n;
   }
   if (!got)
      goto done;
   rc = 0;
   if (want)
   {
      uint8_t d[32];
      char    hex[65];
      sha256_stream_final(&h, d);
      for (i = 0; i < 32; i++)
         sprintf(hex + 2 * i, "%02x", d[i]);
      if (strcmp(hex, want) != 0)
      {
         fprintf(stderr, "body: %u octets, sha256 %s, want %s\n", (unsigned)body, hex, want);
         rc = 1;
      }
   }

done:
   ssl_socket_close(ssl);
   ssl_socket_free(ssl);
   freeaddrinfo_retro(addr);
   if (cfg.trust_count)
      free((void*)trust[0]);
   for (i = 0; i < (int)cfg.chain_count; i++)
      free((void*)chain[i]);
   free(key_der);
   return rc;
}
