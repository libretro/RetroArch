/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <queues/retro_retire.h>
#if defined(RETRO_RETIRE_LOCK_FREE) && !defined(TLS_CACHE_TEST_FLAG)
static bool cache_collision;
static unsigned cache_attempts;
static int test_cache_cas(retro_atomic_ptr_t *p, void *expected, void *desired)
{
   if (cache_collision)
   {
      assert(++cache_attempts == 1);
      return 0;
   }
   return retro_atomic_cas_ptr(p, expected, desired);
}
#undef retro_atomic_cas_ptr
#define retro_atomic_cas_ptr test_cache_cas
#endif
static void test_yield(void)
{
   assert(!"incremental handshake waited for the session cache");
}
#define sthread_yield test_yield
#endif

static uint8_t input[65536], output[65536];
static size_t input_size, input_at, available, output_size, write_quota;
static unsigned reads, writes, closes;
static bool ended, allow_blocking, mode_failed, send_failed;

static bool test_set_block(int fd, bool block)
{
   (void)fd;
   assert(!block || allow_blocking);
   return !mode_failed;
}

static bool test_wait(int fd, bool *read, bool *write, int timeout)
{
   (void)fd; (void)write;
   assert(allow_blocking && timeout == SSL_SOCKET_IO_TIMEOUT_MS);
   assert(read && *read);
   *read = available > input_at;
   return *read;
}

static ssize_t test_receive(int fd, bool *error, void *data, size_t size)
{
   size_t n = available - input_at;
   (void)fd;
   reads++;
   if (n > size)
      n = size;
   memcpy(data, input + input_at, n);
   input_at += n;
   *error = ended && !n;
   return *error ? -1 : (ssize_t)n;
}

static ssize_t test_send(int fd, const void *data, size_t size, bool no_signal)
{
   size_t n = size < write_quota ? size : write_quota;
   (void)fd; (void)no_signal;
   writes++;
   if (send_failed)
      return -1;
   assert(n <= sizeof(output) - output_size);
   memcpy(output + output_size, data, n);
   output_size += n;
   write_quota -= n;
   return (ssize_t)n;
}

static int test_close(int fd)
{
   (void)fd;
   closes++;
   return 0;
}

#define socket_close test_close
#define socket_set_block test_set_block
#define socket_wait test_wait
#define socket_receive_all_nonblocking test_receive
#define socket_send_all_nonblocking test_send
#include "../../../libretro-common/net/net_socket_ssl_retro.c"

static void reset(void)
{
   input_size = input_at = available = output_size = 0;
   write_quota = sizeof(output);
   reads = writes = closes = 0;
   ended = false;
   allow_blocking = false;
   mode_failed = send_failed = false;
}

static struct ssl_state *state(bool v13)
{
   struct ssl_state *s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   s->handshake_done = 1;
   s->v13 = v13;
   s->suite = v13 ? 0x1301 : 0xc02f;
   s->rx_encrypted = s->tx_encrypted = 1;
   if (v13)
   {
      assert(!tls13_traffic_keys(s, s->s_app, s->swk, s->siv));
      assert(!tls13_traffic_keys(s, s->c_app, s->cwk, s->civ));
   }
   return s;
}

static void append(struct ssl_state *sender, unsigned type,
      const uint8_t *data, size_t size)
{
   size_t length;
   assert(sizeof(input) - input_size >= 5 + size + TLS_REC_OVERHEAD);
   assert(!tls_seal_record(sender, (uint8_t)type, data, size,
            input + input_size, &length));
   input_size += length;
}

static void partial_records(bool v13)
{
   static const uint8_t payload[] = "fragmented application data";
   struct ssl_state *sender, *receiver;
   uint8_t data[128];
   unsigned before;
   bool error;
   reset();
   sender = state(v13);
   receiver = state(v13);
   append(sender, TLS_CT_APPDATA, payload, sizeof(payload));
   for (available = 0; available < input_size; available++)
   {
      before = reads;
      assert(ssl_socket_receive_all_nonblocking(receiver, &error,
               data, sizeof(data)) == 0 && !error);
      assert(reads - before <= 2);
   }
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data,
            sizeof(data)) == sizeof(payload));
   assert(!error && !memcmp(data, payload, sizeof(payload)));
   assert(receiver->pt == data);
   ended = true;
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data,
            sizeof(data)) == -1 && error);
   ssl_socket_free(sender);
   ssl_socket_free(receiver);
}

static void fragmented_update(void)
{
   static const uint8_t update[5] = {TLS_HS_KEY_UPDATE, 0, 0, 1, 1};
   static const uint8_t payload[] = "after update";
   static const uint8_t prior[] = "prior";
   struct ssl_state *sender, *receiver;
   uint8_t next[48], data[128];
   bool error, write;
   reset();
   sender = state(true);
   receiver = state(true);
   write_quota = 0;
   assert(ssl_socket_send_all_nonblocking(receiver, prior, sizeof(prior), true)
         == sizeof(prior));
   append(sender, TLS_CT_HANDSHAKE, update, 2);
   append(sender, TLS_CT_HANDSHAKE, update + 2, 3);
   assert(!tls13_expand_label(sender, sender->c_app, "traffic upd", NULL, 0,
            next, tls13_hlen(sender)));
   memcpy(sender->c_app, next, tls13_hlen(sender));
   assert(!tls13_traffic_keys(sender, sender->c_app, sender->cwk, sender->civ));
   sender->cseq = 0;
   append(sender, TLS_CT_APPDATA, payload, sizeof(payload));
   available = input_size;
   write_quota = 0;
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   assert(receiver->hs_len == 2);
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   assert(!ssl_socket_retro_pending(receiver, &write) && write);
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data,
            sizeof(data)) == sizeof(payload));
   assert(!error && !memcmp(data, payload, sizeof(payload)));
   assert(!ssl_socket_send_all_nonblocking(receiver, payload, sizeof(payload), true));
   assert(!ssl_socket_flush_nonblocking(receiver));
   assert(receiver->tx_len && !output_size);
   write_quota = 1;
   assert(!ssl_socket_flush_nonblocking(receiver) && output_size == 1);
   write_quota = receiver->tx_len - receiver->tx_off;
   assert(!ssl_socket_flush_nonblocking(receiver));
   assert(receiver->cork_len && !receiver->cork_off);
   assert(!ssl_socket_send_all_nonblocking(receiver, payload, sizeof(payload), true));
   write_quota = sizeof(output) - output_size;
   assert(ssl_socket_flush_nonblocking(receiver) == 1);
   assert(output_size == 5 + sizeof(prior) + 17 + 27 && receiver->cseq == 0);
   assert(ssl_socket_send_all_nonblocking(receiver, payload, sizeof(payload), true)
         == sizeof(payload));
   input_size = available = output_size;
   input_at = 0;
   memcpy(input, output, output_size);
   assert(ssl_socket_receive_all_nonblocking(sender, &error, data,
            sizeof(data)) == sizeof(prior));
   assert(!error && !memcmp(data, prior, sizeof(prior)));
   assert(!ssl_socket_receive_all_nonblocking(sender, &error, data,
            sizeof(data)) && !error);
   assert(ssl_socket_receive_all_nonblocking(sender, &error, data,
            sizeof(data)) == sizeof(payload));
   assert(!error && !memcmp(data, payload, sizeof(payload)));
   ssl_socket_free(sender);
   ssl_socket_free(receiver);
}

static void buffered_tickets(void)
{
   static const uint8_t ticket[18] = {
      TLS_HS_NEW_SESSION_TICKET, 0, 0, 14,
      0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0xaa, 0, 0};
   struct ssl_state *sender, *receiver;
   uint8_t messages[36], data[64];
   unsigned before;
   bool error, write;
   reset();
   sender = state(true);
   receiver = state(true);
   receiver->configured = 1;
   memcpy(messages, ticket, sizeof(ticket));
   memcpy(messages + sizeof(ticket), ticket, sizeof(ticket));
   append(sender, TLS_CT_HANDSHAKE, messages, sizeof(messages));
   available = input_size;
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   assert(ssl_socket_retro_pending(receiver, &write) && !write);
   assert(receiver->hs_len == sizeof(ticket));
   before = reads;
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   assert(reads == before && !receiver->hs_len);
   assert(!ssl_socket_retro_pending(receiver, &write) && !write);
   ssl_socket_free(sender);
   ssl_socket_free(receiver);
}

static void mixed_and_failed_records(void)
{
   static const uint8_t payload[] = "complete after switching API";
   struct ssl_state *sender, *receiver;
   uint8_t data[64];
   size_t i;
   bool error;
   reset();
   sender = state(false);
   receiver = state(false);
   append(sender, TLS_CT_APPDATA, payload, sizeof(payload));
   available = 3;
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   assert(receiver->rx_len == 3);
   available = input_size;
   allow_blocking = true;
   assert(ssl_socket_receive_all_blocking(receiver, data, sizeof(payload)) == 1);
   assert(!memcmp(data, payload, sizeof(payload)) && receiver->sseq == 1);
   ssl_socket_free(sender);
   ssl_socket_free(receiver);

   reset();
   sender = state(true);
   receiver = state(true);
   append(sender, TLS_CT_APPDATA, payload, sizeof(payload));
   input[input_size - 1] ^= 1;
   available = input_size;
   memset(data, 0xcc, sizeof(data));
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data,
            sizeof(data)) == -1 && error);
   for (i = 0; i < sizeof(data); i++)
      assert(data[i] == 0xcc);
   ssl_socket_free(sender);
   ssl_socket_free(receiver);
}

static void socket_failures(void)
{
   struct ssl_state *receiver;
   uint8_t data[32];
   bool error, write = true;
   reset();
   assert(!ssl_socket_retro_pending(NULL, &write) && !write);
   assert(!ssl_socket_retro_pending(NULL, NULL));
   receiver = state(true);
   mode_failed = true;
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data,
            sizeof(data)) == -1 && error && !reads);
   assert(receiver->last_err == TLS_ERR_SOCKET);
   assert(ssl_socket_send_all_nonblocking(receiver, "x", 1, true) == -1 && !writes);
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, 0) && !error);
   ssl_socket_free(receiver);

   reset();
   receiver = state(true);
   input[0] = TLS_CT_APPDATA; input[1] = input[2] = 3;
   input[3] = input[4] = 0xff;
   available = input_size = 5;
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data,
            sizeof(data)) == -1 && error && reads == 1);
   assert(receiver->last_err == TLS_ERR_RECORD);
   ssl_socket_free(receiver);

   reset();
   receiver = state(true);
   available = input_size = 2;
   assert(!ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) && !error);
   ended = true;
   assert(ssl_socket_receive_all_nonblocking(receiver, &error, data, sizeof(data)) == -1 && error);
   ssl_socket_free(receiver);
}

static struct ssl_state *handshake_hello(const char *host)
{
   struct ssl_state *s = (struct ssl_state*)ssl_socket_init(-1, host);
   unsigned step;
   bool read, write;
   assert(s);
   for (step = 0; step < 2000; step++)
   {
      unsigned before = writes;
      write_quota = 7;
      assert(!ssl_socket_handshake_step(s, &read, &write));
      assert(writes - before <= 1);
      if (read)
         break;
   }
   assert(step < 2000 && !write);
   assert(output_size > 9 && output[0] == TLS_CT_HANDSHAKE);
   assert(output[5] == TLS_HS_CLIENT_HELLO);
   assert(tls_get16(output + 3) == output_size - 5);
   assert(s->connect.phase == TLS_CONNECT_HELLO);
   return s;
}

static void handshake_busy_cache(void)
{
#ifdef HAVE_THREADS
   struct tls_session_put put;
#ifndef TLS_CACHE_LOCK_FREE
   struct ssl_state *s;
#endif
   reset();
   memset(&put, 0, sizeof(put));
#ifdef TLS_CACHE_LOCK_FREE
   cache_attempts = 0;
   cache_collision = true;
   tls_cache_put("busy.test", &put, true);
   assert(cache_attempts == 1);
   cache_collision = false;
#else
   retro_atomic_store_release_int(&tls_cache_busy, 1);
   tls_cache_put("busy.test", &put, true);
   s = handshake_hello("busy.test");
   assert(!s->cached_suite);
   retro_atomic_store_release_int(&tls_cache_busy, 0);
   ssl_socket_free(s);
#endif
#endif
}

static void handshake_cached_keys(void)
{
   uint8_t master[48], changed[48], sid[32];
   struct tls_session_put put;
   struct ssl_state *s;
   size_t length;
   bool read, write;
   reset();
   memset(master, 0xa5, sizeof(master));
   memset(changed, 0x5a, sizeof(changed));
   memset(sid, 0x31, sizeof(sid));
   memset(&put, 0, sizeof(put));
   put.what = TLS_PUT_MASTER;
   put.master = master;
   put.sid = sid;
   put.sid_len = sizeof(sid);
   put.suite = 0xc02f;
   tls_cache_put("snapshot.test", &put, false);
   s = handshake_hello("snapshot.test");
   put.master = changed;
   memset(sid, 0x32, sizeof(sid));
   tls_cache_put("snapshot.test", &put, false);
   length = s->offered_sid_len;
   input[0] = TLS_CT_HANDSHAKE;
   input[1] = input[2] = 3;
   tls_put16(input + 3, (unsigned)(42 + length));
   input[5] = TLS_HS_SERVER_HELLO;
   input[6] = input[7] = 0;
   input[8] = (uint8_t)(38 + length);
   input[9] = input[10] = 3;
   memset(input + 11, 0x11, 32);
   input[43] = (uint8_t)length;
   memcpy(input + 44, s->offered_sid, length);
   input[44 + length] = 0xc0;
   input[45 + length] = 0x2f;
   input[46 + length] = 0;
   available = input_size = 47 + length;
   assert(!ssl_socket_handshake_step(s, &read, &write));
   assert(s->resumed && !memcmp(s->master, master, sizeof(master)));
   ssl_socket_free(s);
}

static void handshake_fragments(void)
{
   struct ssl_state *s;
   size_t i;
   unsigned before;
   bool read, write;
   reset();
   s = handshake_hello("localhost");
   input[0] = TLS_CT_HANDSHAKE;
   input[1] = input[2] = 3;
   tls_put16(input + 3, 42);
   input[5] = TLS_HS_SERVER_HELLO;
   input[6] = input[7] = 0;
   input[8] = 38;
   input[9] = input[10] = 3;
   memset(input + 11, 0x11, 32);
   input[43] = 0;
   input[44] = 0xc0;
   input[45] = 0x2f;
   input[46] = 0;
   input_size = 47;
   for (i = 1; i <= input_size; i++)
   {
      available = i;
      before = reads;
      assert(!ssl_socket_handshake_step(s, &read, &write));
      assert(reads - before <= 2 && !write);
   }
   assert(s->connect.phase == TLS_CONNECT_CERT12);
   assert(s->hs_off == 42 && s->suite == 0xc02f);
   assert(!ssl_socket_handshake_step(s, &read, &write) && read && !write);
   ended = true;
   assert(ssl_socket_handshake_step(s, &read, &write) == -1);
   before = reads;
   assert(ssl_socket_handshake_step(s, &read, &write) == -1);
   assert(reads == before && !read && !write);
   ssl_socket_free(s);
}

static void handshake_backpressure(void)
{
   struct ssl_state *s;
   unsigned step, before;
   bool read, write;
   uint8_t queued[TLS_CORK_MAX];
   size_t length;
   reset();
   s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   assert(!ssl_socket_handshake_step(s, &read, &write));
   assert(s->connect_message && !reads && !writes);
   write_quota = 0;
   assert(!ssl_socket_handshake_step(s, &read, &write));
   assert(!s->connect_message && s->cork_len && !writes);
   length = s->cork_len;
   memcpy(queued, s->cork, length);
   for (step = 0; step < 10; step++)
   {
      before = writes;
      assert(!ssl_socket_handshake_step(s, &read, &write) && write && !read);
      assert(writes == before + 1 && !output_size);
      assert(s->cork_len == length && !memcmp(s->cork, queued, length));
   }
   for (step = 0; step < 2000; step++)
   {
      write_quota = 7;
      assert(!ssl_socket_handshake_step(s, &read, &write));
      if (read)
         break;
   }
   assert(step < 2000 && output_size == length);
   assert(!memcmp(output, queued, length));
   ssl_socket_free(s);

   reset();
   s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   assert(!ssl_socket_handshake_step(s, &read, &write));
   assert(!ssl_socket_handshake_step(s, &read, &write));
   send_failed = true;
   assert(ssl_socket_handshake_step(s, &read, &write) == -1);
   before = writes;
   assert(ssl_socket_handshake_step(s, &read, &write) == -1);
   assert(writes == before);
   ssl_socket_free(s);

   for (step = 0; step < 12; step++)
   {
      unsigned tick;
      reset();
      write_quota = 0;
      s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
      assert(s);
      for (tick = 0; tick < step; tick++)
         assert(!ssl_socket_handshake_step(s, &read, &write));
      ssl_socket_free(s);
   }
}

static void handshake_output(void)
{
   static uint8_t body[TLS_REC_MAX + 37];
   static const uint8_t ccs = 1;
   struct ssl_state *s;
   unsigned step, before, count;
   size_t at, payload;
   bool read, write;
   reset();
   s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   s->connect_nonblock = true;
   tls_cork(s);
   memset(body, 0x5a, sizeof(body));
   assert(!tls_send_handshake(s, TLS_HS_CERTIFICATE, body, sizeof(body)));
   for (step = 0; step < 10000; step++)
   {
      int result;
      write_quota = 7;
      before = writes;
      result = tls_connect_output(s);
      assert(result >= 0 && writes - before <= 1);
      if (result == 1)
         break;
   }
   assert(step < 10000 && !s->connect_message && s->cork_len);
   assert(!tls_send_record(s, TLS_CT_CCS, &ccs, 1));
   assert(!tls_uncork(s));
   for (step = 0; step < 10000; step++)
   {
      write_quota = 7;
      if (tls_connect_output(s) == 1)
         break;
   }
   assert(step < 10000);
   count = 0;
   payload = 0;
   for (at = 0; at < output_size;)
   {
      size_t length = tls_get16(output + at + 3);
      assert(at + 5 + length <= output_size && length <= TLS_REC_MAX);
      if (output[at] == TLS_CT_HANDSHAKE)
      {
         size_t offset = 0;
         if (!payload)
         {
            assert(output[at + 5] == TLS_HS_CERTIFICATE);
            assert(tls_get24(output + at + 6) == sizeof(body));
            offset = 4;
         }
         assert(!memcmp(output + at + 5 + offset, body + payload,
                  length - offset));
         payload += length - offset;
      }
      else
         assert(output[at] == TLS_CT_CCS && length == 1 && output[at + 5] == 1);
      at += 5 + length;
      count++;
   }
   assert(payload == sizeof(body) && count == 3);
   ssl_socket_free(s);

   reset();
   s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   s->connect_nonblock = true;
   tls_cork(s);
   assert(!tls_send_handshake(s, TLS_HS_CERTIFICATE, body, 935));
   assert(!tls_connect_output(s) && s->cork_len == 944);
   assert(!tls_send_record(s, TLS_CT_CCS, &ccs, 1) && s->tx_len == 6);
   assert(!tls_uncork(s));
   for (step = 0; step < 1000; step++)
   {
      write_quota = 7;
      if (tls_connect_output(s) == 1)
         break;
   }
   assert(step < 1000 && output_size == 950);
   assert(output[0] == TLS_CT_HANDSHAKE && output[944] == TLS_CT_CCS);
   ssl_socket_free(s);
   read = write = true;
   assert(ssl_socket_handshake_step(NULL, &read, &write) == -1 && !read && !write);
}

static void nonblocking_close(void)
{
   struct ssl_state *s;
   unsigned before;
   bool read, write;
   reset();
   s = state(true);
   s->fd = 7;
   write_quota = 0;
   ssl_socket_close(s);
   assert(closes == 1 && writes == 1 && s->fd == -1);
   ssl_socket_close(s);
   assert(closes == 1 && writes == 1);
   ssl_socket_free(s);

   reset();
   s = state(true);
   s->fd = 7;
   write_quota = 0;
   assert(ssl_socket_send_all_nonblocking(s, "data", 4, true) == 4);
   before = writes;
   ssl_socket_close(s);
   assert(closes == 1 && writes == before);
   ssl_socket_free(s);

   reset();
   s = (struct ssl_state*)ssl_socket_init(-1, "localhost");
   assert(s);
   s->fd = 7;
   assert(!ssl_socket_handshake_step(s, &read, &write));
   before = writes;
   ssl_socket_close(s);
   assert(closes == 1 && writes == before);
   ssl_socket_free(s);
}

int main(void)
{
   handshake_busy_cache();
   nonblocking_close();
   handshake_cached_keys();
   handshake_fragments();
   handshake_backpressure();
   handshake_output();
   partial_records(false);
   partial_records(true);
   fragmented_update();
   buffered_tickets();
   mixed_and_failed_records();
   socket_failures();
   puts("PASS incremental TLS handshake, partial records and backpressured KeyUpdate");
   return 0;
}
