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

#include <net/net_socket_ssl.h>
#include <net/net_socket.h>
#include <encodings/base64.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "../../deps/bearssl-0.6/inc/bearssl.h"

struct ssl_state
{
   int fd;
   br_ssl_client_context sc;
   br_x509_minimal_context xc;
   uint8_t iobuf[BR_SSL_BUFSIZE_BIDI];
};

/* TODO/FIXME - static global variables */
static br_x509_trust_anchor TAs[500] = {};
static size_t TAs_NUM = 0;

static uint8_t* current_vdn;
static size_t current_vdn_size;

static uint8_t* blobdup(const void * src, size_t len)
{
   uint8_t * ret = malloc(len);
   memcpy(ret, src, len);
   return ret;
}
static void vdn_append(void* dest_ctx, const void * src, size_t len)
{
   current_vdn = realloc(current_vdn, current_vdn_size + len);
   memcpy(current_vdn+current_vdn_size, src, len);
   current_vdn_size += len;
}

static bool append_cert_x509(void* x509, size_t len)
{
   br_x509_pkey* pk;
   br_x509_decoder_context dc;
   br_x509_trust_anchor* ta = &TAs[TAs_NUM];

   current_vdn              = NULL;
   current_vdn_size         = 0;

   br_x509_decoder_init(&dc, vdn_append, NULL);
   br_x509_decoder_push(&dc, x509, len);
   pk                       = br_x509_decoder_get_pkey(&dc);
   if (!pk || !br_x509_decoder_isCA(&dc))
      return false;

   ta->dn.len               = current_vdn_size;
   ta->dn.data              = current_vdn;
   ta->flags                = BR_X509_TA_CA;

   switch (pk->key_type)
   {
      case BR_KEYTYPE_RSA:
         ta->pkey.key_type     = BR_KEYTYPE_RSA;
         ta->pkey.key.rsa.nlen = pk->key.rsa.nlen;
         ta->pkey.key.rsa.n    = blobdup(pk->key.rsa.n, pk->key.rsa.nlen);
         ta->pkey.key.rsa.elen = pk->key.rsa.elen;
         ta->pkey.key.rsa.e    = blobdup(pk->key.rsa.e, pk->key.rsa.elen);
         break;
      case BR_KEYTYPE_EC:
         ta->pkey.key_type     = BR_KEYTYPE_EC;
         ta->pkey.key.ec.curve = pk->key.ec.curve;
         ta->pkey.key.ec.qlen  = pk->key.ec.qlen;
         ta->pkey.key.ec.q     = blobdup(pk->key.ec.q, pk->key.ec.qlen);
         break;
      default:
         return false;
   }

   TAs_NUM++;
   return true;
}

static char* delete_linebreaks(char* in)
{
   char* iter_in;
   char* iter_out;
   while (*in == '\n')
      in++;

   iter_in = in;

   while (*iter_in != '\n' && *iter_in != '\0')
      iter_in++;
   iter_out = iter_in;
   while (*iter_in != '\0')
   {
      while (*iter_in == '\n')
         iter_in++;
      *iter_out++ = *iter_in++;
   }

   return in;
}

/* this rearranges its input, it's easier to implement
 * that way and caller doesn't need it anymore anyways */
static void append_certs_pem_x509(char * certs_pem)
{
   void * cert_bin;
   int cert_bin_len;
   char *cert     = certs_pem;
   char *cert_end = certs_pem;

   for (;;)
   {
      cert      = strstr(cert_end, "-----BEGIN CERTIFICATE-----");
      if (!cert)
         break;
      cert     += STRLEN_CONST("-----BEGIN CERTIFICATE-----");
      cert_end  = strstr(cert, "-----END CERTIFICATE-----");

      *cert_end = '\0';
      cert      = delete_linebreaks(cert);

      cert_bin  = unbase64(cert, cert_end-cert, &cert_bin_len);
      append_cert_x509(cert_bin, cert_bin_len);
      free(cert_bin);

      cert_end++; /* skip the NUL we just added */
   }
}

/* TODO: not thread safe, rthreads doesn't provide any
 * statically allocatable mutex/etc */
static void initialize(void)
{
   void* certs_pem;
   if (TAs_NUM)
      return;
   /* filestream_read_file appends a NUL */
   filestream_read_file("/etc/ssl/certs/ca-certificates.crt", &certs_pem, NULL);
   append_certs_pem_x509((char*)certs_pem);
   free(certs_pem);
}

void* ssl_socket_init(int fd, const char *domain)
{
   struct ssl_state *state = (struct ssl_state*)calloc(1, sizeof(*state));

   initialize();

   br_ssl_client_init_full(&state->sc, &state->xc, TAs, TAs_NUM);
   br_ssl_engine_set_buffer(&state->sc.eng,
         state->iobuf, sizeof(state->iobuf), true);
   br_ssl_client_reset(&state->sc, domain, false);

   state->fd = fd;
   return state;
}

static bool process_inner(struct ssl_state *state, bool blocking)
{
   bool dummy;
   size_t buflen;
   ssize_t bytes;
   uint8_t *buf = br_ssl_engine_sendrec_buf(&state->sc.eng, &buflen);

   if (buflen)
   {
      if (blocking)
         bytes = (socket_send_all_blocking(state->fd, buf, buflen, true)
               ? buflen
               : -1);
      else
         bytes = socket_send_all_nonblocking(state->fd, buf, buflen, true);

      if (bytes > 0)
         br_ssl_engine_sendrec_ack(&state->sc.eng, bytes);
      if (bytes < 0)
         return false;
      /* if we did something, return immediately so we
       * don't try to read if Bear still wants to send */
      return true;
   }

   buf = br_ssl_engine_recvrec_buf(&state->sc.eng, &buflen);
   if (buflen)
   {
      /* if the socket is blocking, socket_receive_all_nonblocking blocks,
       * but only to read at least 1 byte which is exactly what we want */
      bytes = socket_receive_all_nonblocking(state->fd, &dummy, buf, buflen);
      if (bytes > 0)
         br_ssl_engine_recvrec_ack(&state->sc.eng, bytes);
      if (bytes < 0)
         return false;
   }

   return true;
}

int ssl_socket_connect(void *state_data,
      void *data, bool timeout_enable, bool nonblock)
{
   struct ssl_state *state = (struct ssl_state*)state_data;
   unsigned bearstate;

   if (timeout_enable)
   {
      if (!socket_connect_with_timeout(state->fd, data, 5000))
         return -1;
      /* socket_connect_with_timeout makes the socket non-blocking. */
      if (!socket_set_block(state->fd, true))
         return -1;
   }
   else
   {
      if (socket_connect(state->fd, data))
         return -1;
   }

   for (;;)
   {
      if (!process_inner(state, true))
         return -1;

      bearstate = br_ssl_engine_current_state(&state->sc.eng);
      if (bearstate & BR_SSL_SENDAPP)
         break; /* handshake done */
      if (bearstate & BR_SSL_CLOSED)
         return -1; /* failed */
   }

   return 1;
}

ssize_t ssl_socket_receive_all_nonblocking(void *state_data,
      bool *error, void *data_, size_t len)
{
   size_t __len;
   uint8_t *bear_data;
   struct ssl_state *state = (struct ssl_state*)state_data;
   socket_set_block(state->fd, false);
   if (!process_inner(state, false))
   {
      *error = true;
      return -1;
   }
   bear_data = br_ssl_engine_recvapp_buf(&state->sc.eng, &__len);
   if (__len > len)
      __len = len;
   memcpy(data_, bear_data, __len);
   if (__len)
      br_ssl_engine_recvapp_ack(&state->sc.eng, __len);
   return __len;
}

int ssl_socket_receive_all_blocking(void *state_data,
      void *data_, size_t len)
{
   size_t __len;
   struct ssl_state *state = (struct ssl_state*)state_data;
   uint8_t           *data = (uint8_t*)data_;
   uint8_t *         bear_data;

   socket_set_block(state->fd, true);

   for (;;)
   {
      bear_data = br_ssl_engine_recvapp_buf(&state->sc.eng, &__len);
      if (__len > len)
         __len = len;
      memcpy(data, bear_data, __len);
      if (__len)
         br_ssl_engine_recvapp_ack(&state->sc.eng, __len);
      data += __len;
      len -= __len;

      if (len)
         process_inner(state, true);
      else
         break;
   }
   return 1;
}

int ssl_socket_send_all_blocking(void *state_data,
      const void *data_, size_t len, bool no_signal)
{
   size_t __len;
   struct ssl_state *state = (struct ssl_state*)state_data;
   const     uint8_t *data = (const uint8_t*)data_;
   uint8_t *         bear_data;

   socket_set_block(state->fd, true);

   for (;;)
   {
      bear_data = br_ssl_engine_sendapp_buf(&state->sc.eng, &__len);
      if (__len > len)
         __len = len;
      memcpy(bear_data, data_, __len);
      if (__len)
         br_ssl_engine_sendapp_ack(&state->sc.eng, __len);
      data += __len;
      len  -= __len;

      if (len)
         process_inner(state, true);
      else
         break;
   }

   br_ssl_engine_flush(&state->sc.eng, false);
   process_inner(state, false);
   return 1;
}

ssize_t ssl_socket_send_all_nonblocking(void *state_data,
      const void *data_, size_t len, bool no_signal)
{
   size_t __len;
   uint8_t *bear_data;
   struct ssl_state *state = (struct ssl_state*)state_data;

   socket_set_block(state->fd, false);
   bear_data = br_ssl_engine_sendapp_buf(&state->sc.eng, &__len);
   if (__len > len)
      __len = len;
   memcpy(bear_data, data_, __len);
   if (__len)
   {
      br_ssl_engine_sendapp_ack(&state->sc.eng, __len);
      br_ssl_engine_flush(&state->sc.eng, false);
   }
   if (!process_inner(state, false))
      return -1;
   return __len;
}

void ssl_socket_close(void *state_data)
{
   struct ssl_state *state = (struct ssl_state*)state_data;

   br_ssl_engine_close(&state->sc.eng);
   process_inner(state, false); /* send close notification */
   socket_close(state->fd);     /* but immediately close socket
                                   and don't worry about recipient
                                   getting our message */
}

void ssl_socket_free(void *state_data)
{
   struct ssl_state *state = (struct ssl_state*)state_data;
   /* BearSSL does zero allocations of its own,
    * so other than this struct, there is nothing to free */
   free(state);
}
