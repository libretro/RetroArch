/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_socket.h>
#include <net/net_socket_ssl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t input[65536], output[65536];
static size_t input_size, input_at, available, output_size, write_quota;
static unsigned reads, writes;
static bool ended, allow_blocking, mode_failed;

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
   assert(n <= sizeof(output) - output_size);
   memcpy(output + output_size, data, n);
   output_size += n;
   write_quota -= n;
   return (ssize_t)n;
}

#define socket_set_block test_set_block
#define socket_wait test_wait
#define socket_receive_all_nonblocking test_receive
#define socket_send_all_nonblocking test_send
#include "../../../libretro-common/net/net_socket_ssl_retro.c"

static void reset(void)
{
   input_size = input_at = available = output_size = 0;
   write_quota = sizeof(output);
   reads = writes = 0;
   ended = false;
   allow_blocking = false;
   mode_failed = false;
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

int main(void)
{
   partial_records(false);
   partial_records(true);
   fragmented_update();
   buffered_tickets();
   mixed_and_failed_records();
   socket_failures();
   puts("PASS partial TLS records, direct decrypt and backpressured KeyUpdate");
   return 0;
}
