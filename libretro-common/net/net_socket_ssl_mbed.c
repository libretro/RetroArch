/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_socket.c).
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

#include <string.h>
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>

#if defined(HAVE_BUILTINMBEDTLS)
#include "../../deps/mbedtls/mbedtls/config.h"
#include "../../deps/mbedtls/mbedtls/certs.h"
#include "../../deps/mbedtls/mbedtls/debug.h"
#include "../../deps/mbedtls/mbedtls/platform.h"
#include "../../deps/mbedtls/mbedtls/net_sockets.h"
#include "../../deps/mbedtls/mbedtls/ssl.h"
#include "../../deps/mbedtls/mbedtls/ctr_drbg.h"
#include "../../deps/mbedtls/mbedtls/entropy.h"
#else
#include <mbedtls/version.h>
#if MBEDTLS_VERSION_MAJOR < 3
#include <mbedtls/config.h>
#include <mbedtls/certs.h>
#else
#include <mbedtls/build_info.h>
#endif
#include <mbedtls/debug.h>
#include <mbedtls/platform.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#endif

/* Not part of the mbedtls upstream source */
#include "cacert.h"

#define DEBUG_LEVEL 0

struct ssl_state
{
   mbedtls_net_context net_ctx;
   mbedtls_ssl_context ctx;
   mbedtls_entropy_context entropy;
   mbedtls_ctr_drbg_context ctr_drbg;
   mbedtls_ssl_config conf;
#if defined(MBEDTLS_X509_CRT_PARSE_C)
   mbedtls_x509_crt ca;
#endif
  const char *domain;
};

static void ssl_debug(void *ctx, int level,
      const char *file, int line,
      const char *str)
{
   fprintf((FILE*)ctx, "%s:%04d: %s", file, line, str);
   fflush((FILE*)ctx);
}

void* ssl_socket_init(int fd, const char *domain)
{
   static const char *pers = "libretro";
   struct ssl_state *state = (struct ssl_state*)calloc(1, sizeof(*state));

   state->domain           = domain;

#if defined(MBEDTLS_DEBUG_C)
   mbedtls_debug_set_threshold(DEBUG_LEVEL);
#endif

   mbedtls_net_init(&state->net_ctx);
   mbedtls_ssl_init(&state->ctx);
   mbedtls_ssl_config_init(&state->conf);
#if defined(MBEDTLS_X509_CRT_PARSE_C)
   mbedtls_x509_crt_init(&state->ca);
#endif
   mbedtls_ctr_drbg_init(&state->ctr_drbg);
   mbedtls_entropy_init(&state->entropy);

   state->net_ctx.fd = fd;

   if (mbedtls_ctr_drbg_seed(&state->ctr_drbg, mbedtls_entropy_func, &state->entropy, (const unsigned char*)pers, strlen(pers)) != 0)
      goto error;

#if defined(MBEDTLS_X509_CRT_PARSE_C)
   if (mbedtls_x509_crt_parse(&state->ca, (const unsigned char*)cacert_pem, sizeof(cacert_pem) / sizeof(cacert_pem[0])) < 0)
      goto error;
#endif

   return state;

error:
   if (state)
      free(state);
   return NULL;
}

int ssl_socket_connect(void *state_data,
      void *data, bool timeout_enable, bool nonblock)
{
   int ret, flags;
   struct ssl_state *state = (struct ssl_state*)state_data;

   if (timeout_enable)
   {
      if (!socket_connect_with_timeout(state->net_ctx.fd, data, 5000))
         return -1;
      /* socket_connect_with_timeout makes the socket non-blocking. */
      if (!socket_set_block(state->net_ctx.fd, true))
         return -1;
   }
   else
   {
      if (socket_connect(state->net_ctx.fd, data))
         return -1;
   }

   if (mbedtls_ssl_config_defaults(&state->conf,
               MBEDTLS_SSL_IS_CLIENT,
               MBEDTLS_SSL_TRANSPORT_STREAM,
               MBEDTLS_SSL_PRESET_DEFAULT) != 0)
      return -1;

   mbedtls_ssl_conf_authmode(&state->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
   mbedtls_ssl_conf_ca_chain(&state->conf, &state->ca, NULL);
   mbedtls_ssl_conf_rng(&state->conf, mbedtls_ctr_drbg_random, &state->ctr_drbg);
   mbedtls_ssl_conf_dbg(&state->conf, ssl_debug, stderr);

   if (mbedtls_ssl_setup(&state->ctx, &state->conf) != 0)
      return -1;

#if defined(MBEDTLS_X509_CRT_PARSE_C)
   if (mbedtls_ssl_set_hostname(&state->ctx, state->domain) != 0)
      return -1;
#endif

   mbedtls_ssl_set_bio(&state->ctx, &state->net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

   while ((ret = mbedtls_ssl_handshake(&state->ctx)) != 0)
   {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
         return -1;
   }

   if ((flags = mbedtls_ssl_get_verify_result(&state->ctx)) != 0)
   {
      char vrfy_buf[512];
      mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
   }

   return state->net_ctx.fd;
}

ssize_t ssl_socket_receive_all_nonblocking(void *state_data,
      bool *error, void *data_, size_t len)
{
   ssize_t         ret;
   struct ssl_state *state = (struct ssl_state*)state_data;
   const uint8_t     *data = (const uint8_t*)data_;
   /* mbedtls_ssl_read wants non-const data but it only reads it, so this cast is safe */

   mbedtls_net_set_nonblock(&state->net_ctx);

   ret = mbedtls_ssl_read(&state->ctx, (unsigned char*)data, len);

   if (ret > 0)
      return ret;

   if (ret == 0)
   {
      /* Socket closed */
      *error = true;
      return -1;
   }

   if (isagain((int)ret) || ret == MBEDTLS_ERR_SSL_WANT_READ)
      return 0;

   *error = true;
   return -1;
}

int ssl_socket_receive_all_blocking(void *state_data,
      void *data_, size_t len)
{
   struct ssl_state *state = (struct ssl_state*)state_data;
   const uint8_t     *data = (const uint8_t*)data_;

   mbedtls_net_set_block(&state->net_ctx);

   for (;;)
   {
      /* mbedtls_ssl_read wants non-const data but it only reads it,
       * so this cast is safe */
      int ret = mbedtls_ssl_read(&state->ctx, (unsigned char*)data, len);

      if (     ret == MBEDTLS_ERR_SSL_WANT_READ
            || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
         continue;

      if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
         break;

      if (ret == 0)
         break; /* normal EOF */

      if (ret < 0)
         return -1;
   }

   return 1;
}

int ssl_socket_send_all_blocking(void *state_data,
      const void *data_, size_t len, bool no_signal)
{
   int ret;
   struct ssl_state *state = (struct ssl_state*)state_data;
   const     uint8_t *data = (const uint8_t*)data_;

   mbedtls_net_set_block(&state->net_ctx);

   while (len)
   {
      ret = mbedtls_ssl_write(&state->ctx, data, len);

      if (!ret)
         continue;

      if (ret < 0)
      {
         if (  ret != MBEDTLS_ERR_SSL_WANT_READ &&
              ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            return false;
      }
      else
      {
          data += ret;
          len  -= ret;
      }
   }

   return true;
}

ssize_t ssl_socket_send_all_nonblocking(void *state_data,
      const void *data_, size_t len, bool no_signal)
{
   int ret;
   ssize_t __len = len;
   struct ssl_state *state = (struct ssl_state*)state_data;
   const uint8_t     *data = (const uint8_t*)data_;
   mbedtls_net_set_nonblock(&state->net_ctx);
   ret = mbedtls_ssl_write(&state->ctx, data, len);
   if (ret <= 0)
      return -1;
   return __len;
}

void ssl_socket_close(void *state_data)
{
   struct ssl_state *state = (struct ssl_state*)state_data;

   mbedtls_ssl_close_notify(&state->ctx);

   socket_close(state->net_ctx.fd);
}

void ssl_socket_free(void *state_data)
{
   struct ssl_state *state = (struct ssl_state*)state_data;

   if (!state)
      return;

   mbedtls_ssl_free(&state->ctx);
   mbedtls_ssl_config_free(&state->conf);
   mbedtls_ctr_drbg_free(&state->ctr_drbg);
   mbedtls_entropy_free(&state->entropy);
#if defined(MBEDTLS_X509_CRT_PARSE_C)
   mbedtls_x509_crt_free(&state->ca);
#endif

   free(state);
}
