/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <streams/file_stream.h>

static bool checked_nonblock(int fd, bool block)
{
   assert(!block);
   return socket_set_block(fd, block);
}

static bool forbidden_wait(int fd, bool *read, bool *write, int timeout)
{
   (void)fd; (void)read; (void)write; (void)timeout;
   assert(!"TLS handshake/close entered a socket wait");
   return false;
}

#if defined(HAVE_RETROSSL) || defined(HAVE_BEARSSL)
static bool forbidden_send(int fd, const void *data, size_t length, bool no_signal)
{
   (void)fd; (void)data; (void)length; (void)no_signal;
   assert(!"TLS handshake/close entered a blocking send");
   return false;
}
#define socket_send_all_blocking forbidden_send
#endif
#define HAVE_GRIFFIN
#define socket_set_block checked_nonblock
#define socket_wait forbidden_wait
#if defined(HAVE_RETROSSL)
#include "../../../libretro-common/net/net_socket_ssl_retro.c"
#define TEST_BACKEND "retro"
#elif defined(HAVE_BEARSSL)
#include "../../../libretro-common/net/net_socket_ssl_bear.c"
#define TEST_BACKEND "bear"
#else
#include "../../../libretro-common/net/net_socket_ssl_mbed.c"
#define TEST_BACKEND "mbed"
#endif
#undef socket_set_block
#undef socket_wait
#undef socket_send_all_blocking
#undef HAVE_GRIFFIN
#include "tls_handshake_driver.h"

static unsigned verify_failures;
void ssl_socket_log_verify_fail(int required, const char *domain, const char *info)
{
   (void)domain; (void)info;
   assert(required);
   verify_failures++;
}
void ssl_socket_log_verify_disabled(const char *domain)
{
   (void)domain;
   assert(!"verification must remain required");
}

static bool wait_for(int fd, bool write, retro_time_t deadline)
{
   bool read = !write;
   retro_time_t left = deadline - cpu_features_get_time_usec();
   return left > 0 && socket_wait(fd, &read, &write, (int)((left + 999) / 1000));
}

int main(int argc, char **argv)
{
   static const char request[] = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
   struct addrinfo *address = NULL;
   struct ssl_state *ssl;
   char *pem = NULL;
   char response[128];
   int64_t pem_length = 0;
   int fd, result;
   size_t sent = 0, received = 0;
   retro_time_t deadline;
   bool error = false;
   bool reject = argc > 3;
   if (argc < 3)
      return 2;
   setvbuf(stdout, NULL, _IONBF, 0);
   network_init();
   assert(filestream_read_file(argv[2], (void**)&pem, &pem_length));
   assert(pem && pem_length > 0);
#if defined(HAVE_RETROSSL)
   ssl_socket_retro_set_trust_pem(pem, (size_t)pem_length);
#elif defined(HAVE_BEARSSL)
   append_certs_pem_x509(pem);
   assert(TAs_NUM);
#endif
   ssl_socket_set_verify_mode(0);
   fd = socket_init((void**)&address, (uint16_t)atoi(argv[1]), "localhost",
         SOCKET_TYPE_STREAM, AF_INET);
   assert(fd >= 0 && address);
   ssl = (struct ssl_state*)ssl_socket_init(fd, reject ? "wrong.invalid" : "localhost");
   assert(ssl);
#if !defined(HAVE_RETROSSL) && !defined(HAVE_BEARSSL)
   assert(!mbedtls_x509_crt_parse(&ssl->ca, (unsigned char*)pem, (size_t)pem_length + 1));
#endif
   result = tls_test_connect(ssl, fd, address, true);
   freeaddrinfo(address);
   if (reject)
   {
      assert(result == -1 && verify_failures == 1);
      ssl_socket_close(ssl);
      ssl_socket_free(ssl);
      free(pem);
      printf("PASS %s: incremental handshake rejects the wrong host\n", TEST_BACKEND);
      return 0;
   }
   assert(result == 1 && !verify_failures);
   assert(ssl_socket_handshake_step(ssl, NULL, NULL) == 1);
   deadline = cpu_features_get_time_usec() + 5000000;
   while (sent < sizeof(request) - 1)
   {
      ssize_t n = ssl_socket_send_all_nonblocking(ssl, request + sent,
            sizeof(request) - 1 - sent, true);
      assert(n >= 0);
      sent += (size_t)n;
      if (!n)
         assert(wait_for(fd, true, deadline));
   }
   while ((result = ssl_socket_flush_nonblocking(ssl)) == 0)
      assert(wait_for(fd, true, deadline));
   assert(result == 1);
   while (received + 1 < sizeof(response))
   {
      ssize_t n = ssl_socket_receive_all_nonblocking(ssl, &error,
            response + received, 1);
      assert(n >= 0 && !error);
      if (!n)
      {
         bool write = false;
#if defined(HAVE_RETROSSL)
         if (ssl_socket_retro_pending(ssl, &write))
            continue;
#endif
         assert(wait_for(fd, write, deadline));
         continue;
      }
      received += (size_t)n;
      if (response[received - 1] == '\n')
         break;
   }
   response[received] = '\0';
   assert(!strncmp(response, "HTTP/1.", 7) && strstr(response, " 200 "));
   ssl_socket_close(ssl);
   ssl_socket_close(ssl);
   assert(ssl_socket_handshake_step(ssl, NULL, NULL) == -1);
   ssl_socket_free(ssl);
   free(pem);
   printf("PASS %s: incremental handshake, application records and nonblocking close\n", TEST_BACKEND);
   return 0;
}
