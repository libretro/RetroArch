/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_mdns.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture
{
   uint8_t packet[NET_MDNS_PACKET_MAX];
   size_t size;
   unsigned sent, added, changed, removed;
   bool unicast, fail_send;
   net_mdns_peer_t destination;
   net_mdns_t *mdns;
} fixture_t;

static fixture_t sender, receiver, other;
static net_mdns_service_t service;
static uint8_t saved[NET_MDNS_PACKET_MAX], query[NET_MDNS_PACKET_MAX];
static net_mdns_peer_t peer = {{192,168,1,2}, 5353, 4};

static unsigned u16(const uint8_t *p)
{ return ((unsigned)p[0] << 8) | p[1]; }
static void put16(uint8_t *p, unsigned n)
{ p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n; }

static bool send_packet(void *data, const void *packet, size_t size, const net_mdns_peer_t *to)
{
   fixture_t *f = (fixture_t*)data;
   assert(size <= sizeof(f->packet));
   if (f->fail_send) return false;
   memcpy(f->packet, packet, size); f->size = size; f->sent++;
   f->unicast = to != NULL;
   if (to) f->destination = *to;
   return true;
}

static void changed(void *data, enum net_mdns_event event, const net_mdns_service_t *found)
{
   fixture_t *f = (fixture_t*)data;
   assert(found->instance[0]);
   if (event == NET_MDNS_ADDED) f->added++;
   else if (event == NET_MDNS_CHANGED) f->changed++;
   else if (event == NET_MDNS_REMOVED) f->removed++;
   else assert(0);
}

static void init(fixture_t *f)
{
   memset(f, 0, sizeof(*f));
   f->mdns = net_mdns_new("_pyrowave._tcp.local.", 7, send_packet, changed, f);
   assert(f->mdns);
}

static void establish(fixture_t *f, uint64_t base)
{
   net_mdns_poll(f->mdns, base + 250);
   net_mdns_poll(f->mdns, base + 500);
   net_mdns_poll(f->mdns, base + 750);
   assert(net_mdns_published(f->mdns) == NET_MDNS_PROBING);
   assert(f->sent == 3 && u16(f->packet + 8) == 4);
   net_mdns_poll(f->mdns, base + 1000);
   assert(net_mdns_published(f->mdns) == NET_MDNS_ESTABLISHED);
   assert(f->sent == 4 && u16(f->packet + 6) == 5);
}

static bool feed(fixture_t *to, const void *packet, size_t size, uint64_t now)
{ return net_mdns_receive(to->mdns, packet, size, &peer, 255, now); }

static size_t name(uint8_t *out, const char *text)
{
   size_t p = 0;
   while (*text)
   {
      const char *dot = strchr(text, '.');
      size_t n = dot ? (size_t)(dot - text) : strlen(text);
      out[p++] = (uint8_t)n; memcpy(out + p, text, n); p += n;
      text += n; if (*text) text++;
   }
   out[p++] = 0; return p;
}

static size_t make_query(unsigned type, unsigned cls, const char *text)
{
   size_t p;
   memset(query, 0, 12); put16(query + 4, 1);
   p = 12 + name(query + 12, text);
   put16(query + p, type); put16(query + p + 2, cls); return p + 4;
}

static void setup_service(void)
{
   memset(&service, 0, sizeof(service));
   strcpy(service.instance, "Living.Room"); strcpy(service.hostname, "retroarch-test");
   service.port = 12345; service.has_ipv4 = service.has_ipv6 = true;
   service.ipv4[0] = 192; service.ipv4[1] = 168; service.ipv4[2] = 1; service.ipv4[3] = 10;
   service.ipv6[0] = 0xfe; service.ipv6[1] = 0x80; service.ipv6[15] = 10;
   service.txt[0] = 9; memcpy(service.txt + 1, "version=1", 9); service.txt_size = 10;
}

static void test_lifecycle(void)
{
   const net_mdns_service_t *found;
   unsigned sends;
   init(&sender); init(&receiver); setup_service();
   assert(net_mdns_publish(sender.mdns, &service, 0));
   establish(&sender, 0);
   net_mdns_browse(receiver.mdns, true, 1000);
   assert(feed(&receiver, sender.packet, sender.size, 1000));
   assert(receiver.added == 1 && net_mdns_service_count(receiver.mdns) == 1);
   found = net_mdns_service_at(receiver.mdns, 0); assert(found);
   assert(!strcmp(found->instance, "Living.Room"));
   assert(!strcmp(found->hostname, "retroarch-test.local."));
   assert(found->port == 12345 && found->has_ipv4 && found->has_ipv6);
   assert(!memcmp(found->ipv4, service.ipv4, 4) && !memcmp(found->ipv6, service.ipv6, 16));
   assert(found->txt_size == service.txt_size && !memcmp(found->txt, service.txt, service.txt_size));
   assert(feed(&receiver, sender.packet, sender.size, 1100));
   assert(receiver.added == 1 && !receiver.changed);
   net_mdns_poll(receiver.mdns, 1100);
   assert(u16(receiver.packet + 6) == 1); /* Known PTR suppresses its additionals. */
   sends = sender.sent;
   assert(feed(&sender, receiver.packet, receiver.size, 1100));
   net_mdns_poll(sender.mdns, 1300); assert(sender.sent == sends);
   net_mdns_unpublish(sender.mdns);
   assert(feed(&receiver, sender.packet, sender.size, 1400));
   net_mdns_poll(receiver.mdns, 2399); assert(net_mdns_service_count(receiver.mdns) == 1);
   net_mdns_poll(receiver.mdns, 2400); assert(!net_mdns_service_count(receiver.mdns));
   assert(receiver.removed == 1);
   net_mdns_free(receiver.mdns); net_mdns_free(sender.mdns);
   puts("PASS: probe, announcement, dual-stack resolution, known answers, goodbye grace");
}

static void test_unicast(void)
{
   size_t n, p;
   net_mdns_peer_t client = {{192,168,1,3}, 45000, 4};
   init(&sender); setup_service(); assert(net_mdns_publish(sender.mdns, &service, 0));
   establish(&sender, 0);
   n = make_query(12, 1, "_pyrowave._tcp.local."); put16(query, 0x1234);
   assert(net_mdns_receive(sender.mdns, query, n, &client, 255, 1100));
   net_mdns_poll(sender.mdns, 1100);
   assert(sender.unicast && sender.destination.port == 45000);
   assert(u16(sender.packet) == 0x1234 && u16(sender.packet + 4) == 1);
   p = n;
   while (sender.packet[p]) p += 1 + sender.packet[p];
   p++;
   assert(u16(sender.packet + p + 2) == 1); /* No cache-flush bit on legacy replies. */
   assert(sender.packet[p + 4] == 0 && sender.packet[p + 7] == 10);
   n = make_query(12, 0x8001, "_pyrowave._tcp.local.");
   assert(feed(&sender, query, n, 1200)); net_mdns_poll(sender.mdns, 1400);
   assert(sender.unicast && sender.destination.port == 5353 && !u16(sender.packet));
   n = make_query(12, 1, "_services._dns-sd._udp.local.");
   assert(feed(&sender, query, n, 1500)); net_mdns_poll(sender.mdns, 1700);
   assert(!sender.unicast && u16(sender.packet + 6) == 1);
   net_mdns_free(sender.mdns);
   puts("PASS: legacy transaction IDs, question echo, TTL cap, QU and service enumeration");
}

static void test_expiry_and_validation(void)
{
   size_t n, i;
   uint32_t random = 3;
   init(&sender); init(&receiver); setup_service();
   assert(net_mdns_publish(sender.mdns, &service, 0)); establish(&sender, 0);
   n = sender.size; memcpy(saved, sender.packet, n);
   net_mdns_browse(receiver.mdns, true, 1000);
   for (i = 0; i < n; i++) assert(!feed(&receiver, saved, i, 1000));
   assert(!receiver.added);
   assert(!net_mdns_receive(receiver.mdns, saved, n, &peer, 254, 1000));
   saved[1] = 1; assert(!feed(&receiver, saved, n, 1000)); saved[1] = 0;
   /* A valid prefix followed by a malformed record is atomic. */
   put16(saved + 6, 6); saved[n] = 0xc0; saved[n + 1] = (uint8_t)n;
   assert(!feed(&receiver, saved, n + 2, 1000)); assert(!receiver.added);
   put16(saved + 6, 5);
   assert(feed(&receiver, saved, n, 1000)); assert(receiver.added == 1);
   net_mdns_poll(receiver.mdns, 120999); assert(net_mdns_service_count(receiver.mdns) == 1);
   net_mdns_poll(receiver.mdns, 121000); assert(receiver.removed == 1);
   for (i = 0; i < 10000; i++)
   {
      size_t j, len;
      random = random * 1664525U + 1013904223U; len = random % 1024;
      for (j = 0; j < len; j++)
      { random = random * 1664525U + 1013904223U; saved[j] = (uint8_t)(random >> 24); }
      feed(&receiver, saved, len, 122000);
   }
   assert(!net_mdns_service_count(receiver.mdns));
   net_mdns_free(receiver.mdns); net_mdns_free(sender.mdns);
   puts("PASS: strict truncation/hop-limit validation, atomic rejection, TTL expiry, malformed corpus");
}

static void test_probe_tiebreak(void)
{
   init(&sender); init(&other); setup_service();
   assert(net_mdns_publish(sender.mdns, &service, 0));
   service.ipv4[3] = 11;
   assert(net_mdns_publish(other.mdns, &service, 0));
   net_mdns_poll(sender.mdns, 250); net_mdns_poll(other.mdns, 250);
   assert(feed(&sender, other.packet, other.size, 250));
   assert(feed(&other, sender.packet, sender.size, 250));
   assert(net_mdns_published(sender.mdns) == NET_MDNS_CONFLICT);
   assert(net_mdns_published(other.mdns) == NET_MDNS_PROBING);
   net_mdns_free(sender.mdns); net_mdns_free(other.mdns);
   puts("PASS: simultaneous probe RRset tie-break");
}

/* Independent packet rearranger: the fixture's announcements deliberately
 * use uncompressed records; compressed input is constructed separately. */
static size_t subset(unsigned mask, bool reverse, size_t packet_size)
{
   size_t starts[5], lengths[5], p = 12, out = 12, i;
   unsigned count = 0;
   for (i = 0; i < 5; i++)
   {
      size_t n;
      starts[i] = p;
      while (p < packet_size && saved[p]) p += 1 + saved[p];
      assert(p + 11 <= packet_size); p++;
      n = u16(saved + p + 8); p += 10 + n; assert(p <= packet_size);
      lengths[i] = p - starts[i];
   }
   assert(p == packet_size);
   memset(query, 0, 12); query[2] = 0x84;
   for (i = 0; i < 5; i++)
   {
      size_t index = reverse ? 4 - i : i;
      if (!(mask & (1U << index))) continue;
      memcpy(query + out, saved + starts[index], lengths[index]); out += lengths[index]; count++;
   }
   put16(query + 6, count); return out;
}

static void test_split_and_compression(void)
{
   size_t n, q, packet_size;
   init(&sender); init(&receiver); setup_service();
   assert(net_mdns_publish(sender.mdns, &service, 0)); establish(&sender, 0);
   memcpy(saved, sender.packet, sender.size); packet_size = sender.size;
   net_mdns_browse(receiver.mdns, true, 1000);
   /* PTR owner and target suffix both point back into the question. */
   n = make_query(12, 1, "_pyrowave._tcp.local."); query[2] = 0x84; put16(query + 6, 1);
   query[n++] = 0xc0; query[n++] = 12; put16(query + n, 12); put16(query + n + 2, 1);
   query[n + 4] = query[n + 5] = query[n + 6] = 0; query[n + 7] = 120;
   put16(query + n + 8, 14); n += 10;
   query[n++] = 11; memcpy(query + n, "Living.Room", 11); n += 11;
   query[n++] = 0xc0; query[n++] = 12;
   assert(feed(&receiver, query, n, 1000)); assert(!receiver.added);
   net_mdns_poll(receiver.mdns, 1000); assert(u16(receiver.packet + 4) == 3);
   /* A forward/self pointer is invalid and must not change the cached PTR. */
   query[n - 1] = (uint8_t)(n - 2); query[n - 2] = (uint8_t)(0xc0 | ((n - 2) >> 8));
   assert(!feed(&receiver, query, n, 1000)); assert(!receiver.added);
   q = subset(6, false, packet_size); assert(feed(&receiver, query, q, 1001));
   assert(!receiver.added); net_mdns_poll(receiver.mdns, 2000);
   assert(u16(receiver.packet + 4) == 2); /* PTR plus unresolved host ANY. */
   q = subset(24, true, packet_size); assert(feed(&receiver, query, q, 2001));
   assert(receiver.added == 1 && net_mdns_service_at(receiver.mdns, 0)->has_ipv6);
   net_mdns_browse(receiver.mdns, false, 2002); assert(receiver.removed == 1);
   net_mdns_browse(receiver.mdns, true, 2003);
   q = subset(31, true, packet_size); assert(feed(&receiver, query, q, 2003));
   assert(receiver.added == 2); /* Additional addresses can precede SRV/PTR. */
   net_mdns_free(receiver.mdns); net_mdns_free(sender.mdns);
   puts("PASS: compressed DNS names, pointer rejection, split resolution, reversed additionals");
}

static void test_cache_changes(void)
{
   init(&sender); init(&receiver); init(&other); setup_service();
   assert(net_mdns_publish(sender.mdns, &service, 0)); establish(&sender, 0);
   net_mdns_browse(receiver.mdns, true, 1000);
   assert(feed(&receiver, sender.packet, sender.size, 1000));
   service.ipv4[3] = service.ipv6[15] = 20; service.port = 12346;
   assert(net_mdns_publish(other.mdns, &service, 2000)); establish(&other, 2000);
   assert(feed(&receiver, other.packet, other.size, 3000));
   assert(receiver.changed == 1 && receiver.added == 1);
   assert(net_mdns_service_at(receiver.mdns, 0)->ipv4[3] == 20);
   assert(net_mdns_service_at(receiver.mdns, 0)->port == 12346);
   net_mdns_poll(receiver.mdns, 4001);
   assert(!receiver.removed && receiver.changed == 1);
   net_mdns_poll(receiver.mdns, 98000);
   net_mdns_poll(receiver.mdns, 99000);
   assert(u16(receiver.packet + 4) >= 4); /* Refresh SRV/TXT/address before expiry. */
   assert(!u16(receiver.packet + 6)); /* Do not suppress our own refresh. */
   assert(net_mdns_timeout(receiver.mdns, 99000) != 0);
   net_mdns_browse(receiver.mdns, false, 99001);
   assert(receiver.removed == 1 && !net_mdns_service_count(receiver.mdns));
   net_mdns_free(receiver.mdns); net_mdns_free(sender.mdns); net_mdns_free(other.mdns);
   puts("PASS: cache-flush replacement, stable snapshots, proactive refresh, browse cancellation");
}

static void test_backpressure_and_defense(void)
{
   unsigned count;
   init(&sender); init(&other); setup_service();
   sender.fail_send = true;
   assert(net_mdns_publish(sender.mdns, &service, 0));
   net_mdns_poll(sender.mdns, 250);
   assert(net_mdns_timeout(sender.mdns, 250) == 250 && !sender.sent);
   sender.fail_send = false; establish(&sender, 250);
   assert(net_mdns_publish(other.mdns, &service, 2000)); establish(&other, 2000);
   net_mdns_unpublish(other.mdns); count = sender.sent;
   assert(feed(&sender, other.packet, other.size, 3100)); net_mdns_poll(sender.mdns, 3100);
   assert(sender.sent > count && net_mdns_published(sender.mdns) == NET_MDNS_ESTABLISHED);
   net_mdns_free(other.mdns); init(&other);
   service.ipv4[3] = 11; assert(net_mdns_publish(other.mdns, &service, 4000));
   establish(&other, 4000); count = sender.sent;
   assert(feed(&sender, other.packet, other.size, 5000));
   assert(net_mdns_published(sender.mdns) == NET_MDNS_ESTABLISHED);
   net_mdns_poll(sender.mdns, 5000); assert(sender.sent > count);
   assert(feed(&sender, other.packet, other.size, 5100));
   assert(net_mdns_published(sender.mdns) == NET_MDNS_CONFLICT);
   net_mdns_free(sender.mdns); net_mdns_free(other.mdns);
   puts("PASS: send backpressure, cooperating goodbye rescue, name defense and conflict withdrawal");
}

int main(void)
{
   assert(!net_mdns_new("http.local.", 1, send_packet, NULL, NULL));
   test_lifecycle(); test_unicast(); test_expiry_and_validation(); test_probe_tiebreak();
   test_split_and_compression(); test_cache_changes(); test_backpressure_and_defense();
   puts("mDNS/DNS-SD engine tests passed");
   return 0;
}
