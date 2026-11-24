/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_mdns.h>

#ifdef HAVE_MDNS
#include <stdlib.h>
#include <string.h>

#define MDNS_A 1
#define MDNS_PTR 12
#define MDNS_TXT 16
#define MDNS_AAAA 28
#define MDNS_SRV 33
#define MDNS_ANY 255
#define MDNS_CACHE 128
#define MDNS_RECORD_LIMIT 128
#define MDNS_OUTPUT 1400
#define MDNS_TTL 120

typedef struct mdns_name
{
   uint8_t wire[NET_MDNS_NAME_MAX];
   size_t size;
} mdns_name_t;

typedef struct mdns_record
{
   mdns_name_t owner;
   uint8_t data[NET_MDNS_TXT_MAX];
   size_t size;
   uint32_t ttl;
   uint16_t type, cls;
} mdns_record_t;

typedef struct mdns_cache
{
   mdns_record_t record;
   uint64_t expires, seen;
   bool used;
} mdns_cache_t;

struct net_mdns
{
   mdns_name_t type, enumeration, instance, hostname;
   mdns_record_t input_record;
   mdns_name_t input_name;
   mdns_record_t published[5];
   mdns_record_t probe_remote[5];
   mdns_cache_t cache[MDNS_CACHE];
   net_mdns_service_t service_storage[2][NET_MDNS_SERVICE_MAX];
   net_mdns_service_t *services, *next_services;
   net_mdns_peer_t reply_peer;
   uint8_t output[MDNS_OUTPUT];
   uint8_t legacy_questions[MDNS_OUTPUT];
   size_t service_count, publish_count, legacy_size;
   unsigned legacy_count, probes, announcements, reply_mask;
   uint32_t random, browse_interval;
   uint16_t reply_id;
   uint64_t browse_due, publish_due, reply_due, defended, now;
   enum net_mdns_publish_state state;
   bool browsing, reply_pending, reply_unicast, legacy, has_defended;
   net_mdns_send_t send;
   net_mdns_event_t event;
   void *user;
};

static uint16_t mdns_u16(const uint8_t *p)
{ return (uint16_t)(((unsigned)p[0] << 8) | p[1]); }
static uint32_t mdns_u32(const uint8_t *p)
{ return ((uint32_t)mdns_u16(p) << 16) | mdns_u16(p + 2); }
static void mdns_put16(uint8_t *p, unsigned n)
{ p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n; }
static void mdns_put32(uint8_t *p, uint32_t n)
{ mdns_put16(p, n >> 16); mdns_put16(p + 2, n); }
static uint8_t mdns_lower(uint8_t c)
{ return c >= 'A' && c <= 'Z' ? (uint8_t)(c + 'a' - 'A') : c; }
static uint64_t mdns_after(uint64_t now, uint64_t delta)
{ return (uint64_t)-1 - now < delta ? (uint64_t)-1 : now + delta; }
static unsigned mdns_random(net_mdns_t *m, unsigned range)
{
   m->random ^= m->random << 13;
   m->random ^= m->random >> 17;
   m->random ^= m->random << 5;
   return m->random % range;
}

static bool mdns_equal(const mdns_name_t *a, const mdns_name_t *b)
{
   size_t i;
   if (a->size != b->size) return false;
   for (i = 0; i < a->size; i++)
      if (mdns_lower(a->wire[i]) != mdns_lower(b->wire[i])) return false;
   return true;
}

static bool mdns_label(mdns_name_t *name, const char *label)
{
   size_t n = strlen(label);
   if (!n || n > 63 || name->size + n + 2 > 255) return false;
   name->wire[name->size++] = (uint8_t)n;
   memcpy(name->wire + name->size, label, n);
   name->size += n;
   name->wire[name->size] = 0;
   return true;
}

static bool mdns_text_name(mdns_name_t *name, const char *text)
{
   const char *p = text;
   name->size = 0;
   while (*p)
   {
      const char *end = strchr(p, '.');
      size_t len = end ? (size_t)(end - p) : strlen(p);
      if (!len || len > 63 || name->size + len + 2 > 255) return false;
      name->wire[name->size++] = (uint8_t)len;
      memcpy(name->wire + name->size, p, len); name->size += len;
      p += len;
      if (*p) p++;
   }
   if (!name->size) return false;
   name->wire[name->size++] = 0;
   return true;
}

/* Backward-only pointers make cycles impossible, even across pointer chains. */
static bool mdns_read_name(const uint8_t *packet, size_t size, size_t *offset,
      mdns_name_t *name)
{
   size_t pos = *offset, end = 0, hops = 0;
   bool jumped = false;
   name->size = 0;
   for (;;)
   {
      unsigned len;
      if (pos >= size || ++hops > 128) return false;
      len = packet[pos++];
      if ((len & 0xc0) == 0xc0)
      {
         size_t target;
         if (pos >= size) return false;
         target = ((size_t)(len & 63) << 8) | packet[pos];
         if (target >= pos - 1) return false;
         if (!jumped) { end = pos + 1; jumped = true; }
         pos = target;
         continue;
      }
      if (len & 0xc0 || len > size - pos || name->size + len + 1 > 255)
         return false;
      name->wire[name->size++] = (uint8_t)len;
      if (!len) break;
      memcpy(name->wire + name->size, packet + pos, len);
      name->size += len; pos += len;
   }
   *offset = jumped ? end : pos;
   return true;
}

static bool mdns_txt_valid(const uint8_t *data, size_t size)
{
   size_t p = 0;
   while (p < size)
   {
      unsigned n = data[p++];
      if (n > size - p) return false;
      p += n;
   }
   return size != 0;
}

static bool mdns_read_record(const uint8_t *packet, size_t size, size_t *offset,
      mdns_record_t *rr)
{
   size_t p = *offset, end, bytes;
   if (!mdns_read_name(packet, size, &p, &rr->owner) || size - p < 10)
      return false;
   rr->type = mdns_u16(packet + p); rr->cls = mdns_u16(packet + p + 2);
   rr->ttl = mdns_u32(packet + p + 4); bytes = mdns_u16(packet + p + 8); p += 10;
   if (bytes > size - p) return false;
   end = p + bytes; rr->size = bytes;
   if (rr->type == MDNS_PTR || rr->type == MDNS_SRV)
   {
      mdns_name_t target;
      size_t prefix = rr->type == MDNS_SRV ? 6 : 0;
      if (bytes <= prefix) return false;
      if (prefix) memcpy(rr->data, packet + p, prefix);
      p += prefix;
      if (!mdns_read_name(packet, size, &p, &target) || p != end) return false;
      memcpy(rr->data + prefix, target.wire, target.size);
      rr->size = prefix + target.size;
   }
   else
   {
      if ((rr->type == MDNS_A && bytes != 4)
            || (rr->type == MDNS_AAAA && bytes != 16)
            || (rr->type == MDNS_TXT && !mdns_txt_valid(packet + p, bytes)))
         return false;
      if (bytes > sizeof(rr->data)) rr->type = 0;
      else memcpy(rr->data, packet + p, bytes);
   }
   *offset = end;
   return true;
}

static bool mdns_same_data(const mdns_record_t *a, const mdns_record_t *b)
{
   size_t i, prefix;
   if (a->type != b->type || a->size != b->size || !mdns_equal(&a->owner, &b->owner))
      return false;
   prefix = a->type == MDNS_SRV ? 6 : 0;
   for (i = 0; i < a->size; i++)
   {
      uint8_t av = a->data[i], bv = b->data[i];
      if ((a->type == MDNS_PTR || a->type == MDNS_SRV) && i >= prefix)
      { av = mdns_lower(av); bv = mdns_lower(bv); }
      if (av != bv) return false;
   }
   return true;
}

static bool mdns_instance_name(const net_mdns_t *m, const mdns_name_t *name)
{
   mdns_name_t suffix;
   size_t prefix, i;
   if (!name->size || !name->wire[0]) return false;
   prefix = 1 + name->wire[0];
   for (i = 1; i < prefix; i++)
      if (name->wire[i] < 32 || name->wire[i] == 127) return false;
   if (prefix >= name->size || name->size - prefix != m->type.size) return false;
   suffix.size = name->size - prefix;
   memcpy(suffix.wire, name->wire + prefix, suffix.size);
   return mdns_equal(&suffix, &m->type);
}

static bool mdns_local(const mdns_name_t *name)
{
   static const uint8_t local[] = {5, 'l','o','c','a','l',0};
   size_t p = 0, i;
   while (p < name->size)
   {
      if (name->size - p == sizeof(local))
      {
         for (i = 0; i < sizeof(local); i++)
            if (mdns_lower(name->wire[p + i]) != local[i]) break;
         if (i == sizeof(local)) return true;
      }
      if (!name->wire[p]) break;
      p += 1 + name->wire[p];
   }
   return false;
}

static void mdns_cache_record(net_mdns_t *m, const mdns_record_t *rr, uint64_t now)
{
   size_t i, slot = MDNS_CACHE;
   bool address_wanted = false;
   uint64_t expiry = mdns_after(now, (uint64_t)rr->ttl * 1000);
   if (!mdns_local(&rr->owner) || (rr->cls & 0x7fff) != 1) return;
   if (rr->type == MDNS_PTR)
   {
      mdns_name_t target;
      if (!mdns_equal(&rr->owner, &m->type)) return;
      memcpy(target.wire, rr->data, rr->size); target.size = rr->size;
      if (!mdns_instance_name(m, &target)) return;
   }
   else if (rr->type == MDNS_SRV || rr->type == MDNS_TXT)
   { if (!mdns_instance_name(m, &rr->owner)) return; }
   else if (rr->type != MDNS_A && rr->type != MDNS_AAAA) return;
   else
   {
      for (i = 0; i < MDNS_CACHE; i++)
      {
         const mdns_cache_t *entry = &m->cache[i];
         mdns_name_t hostname;
         if (!entry->used || entry->expires <= now || entry->record.type != MDNS_SRV) continue;
         hostname.size = entry->record.size - 6;
         memcpy(hostname.wire, entry->record.data + 6, hostname.size);
         if (mdns_equal(&hostname, &rr->owner)) { address_wanted = true; break; }
      }
      if (!address_wanted) return;
   }
   if (rr->ttl && rr->type != MDNS_PTR && (rr->cls & 0x8000))
      for (i = 0; i < MDNS_CACHE; i++)
      {
         mdns_cache_t *entry = &m->cache[i];
         if (entry->used && entry->expires > mdns_after(now, 1000)
               && rr->type == entry->record.type && mdns_equal(&rr->owner, &entry->record.owner)
               && !mdns_same_data(&entry->record, rr) && now >= entry->seen
               && now - entry->seen >= 1000) entry->expires = mdns_after(now, 1000);
      }
   if (rr->ttl)
   {
      uint64_t refresh = mdns_after(now, (uint64_t)rr->ttl * 800);
      if (m->browse_due > refresh) m->browse_due = refresh;
   }
   for (i = 0; i < MDNS_CACHE; i++)
   {
      mdns_cache_t *entry = &m->cache[i];
      if (!entry->used || entry->expires <= now)
      {
         if (slot == MDNS_CACHE) slot = i;
         continue;
      }
      if (mdns_same_data(&entry->record, rr))
      {
         if (!rr->ttl)
         {
            uint64_t goodbye = mdns_after(now, 1000);
            if (entry->expires > goodbye) entry->expires = goodbye;
         }
         else
         { entry->expires = expiry; entry->seen = now; entry->record.ttl = rr->ttl; }
         return;
      }
   }
   if (!rr->ttl || slot == MDNS_CACHE) return;
   m->cache[slot].record = *rr;
   m->cache[slot].expires = expiry; m->cache[slot].seen = now;
   m->cache[slot].used = true;
}

static const mdns_record_t *mdns_find(const net_mdns_t *m,
      const mdns_name_t *owner, unsigned type, uint64_t now)
{
   size_t i;
   const mdns_record_t *result = NULL;
   uint64_t seen = 0;
   for (i = 0; i < MDNS_CACHE; i++)
   {
      const mdns_cache_t *entry = &m->cache[i];
      if (entry->used && entry->expires > now && entry->record.type == type
            && mdns_equal(&entry->record.owner, owner) && (!result || entry->seen >= seen))
      { result = &entry->record; seen = entry->seen; }
   }
   return result;
}

static bool mdns_needs_refresh(const net_mdns_t *m, const mdns_name_t *owner,
      unsigned type, uint64_t now)
{
   const mdns_record_t *rr = mdns_find(m, owner, type, now);
   size_t i;
   if (!rr) return true;
   for (i = 0; i < MDNS_CACHE; i++)
      if (&m->cache[i].record == rr)
         return m->cache[i].expires - now <= (uint64_t)rr->ttl * 200;
   return true;
}

static bool mdns_hostname_text(char *text, const uint8_t *wire, size_t size)
{
   size_t p = 0, out = 0;
   while (p < size && wire[p])
   {
      unsigned len = wire[p++], i;
      if (len > size - p || out + len + 1 >= NET_MDNS_NAME_MAX) return false;
      for (i = 0; i < len; i++)
      {
         uint8_t c = wire[p++];
         if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
               || (c >= '0' && c <= '9') || c == '-')) return false;
         text[out++] = (char)c;
      }
      text[out++] = '.';
   }
   if (!out) return false;
   text[out] = '\0';
   return true;
}

static void mdns_refresh_services(net_mdns_t *m, uint64_t now)
{
   size_t i, j, count = 0;
   net_mdns_service_t *previous;
   memset(m->next_services, 0, sizeof(m->service_storage[0]));
   for (i = 0; i < MDNS_CACHE && count < NET_MDNS_SERVICE_MAX; i++)
   {
      const mdns_cache_t *ptr = &m->cache[i];
      const mdns_record_t *srv, *txt, *a, *aaaa;
      net_mdns_service_t *service = &m->next_services[count];
      mdns_name_t instance, hostname;
      unsigned len;
      if (!ptr->used || ptr->expires <= now || ptr->record.type != MDNS_PTR) continue;
      instance.size = ptr->record.size;
      memcpy(instance.wire, ptr->record.data, instance.size);
      srv = mdns_find(m, &instance, MDNS_SRV, now);
      txt = mdns_find(m, &instance, MDNS_TXT, now);
      if (!srv || !txt || !mdns_u16(srv->data + 4)) continue;
      hostname.size = srv->size - 6;
      memcpy(hostname.wire, srv->data + 6, hostname.size);
      a = mdns_find(m, &hostname, MDNS_A, now);
      aaaa = mdns_find(m, &hostname, MDNS_AAAA, now);
      if ((!a && !aaaa) || !mdns_hostname_text(service->hostname, hostname.wire, hostname.size)) continue;
      len = instance.wire[0];
      memcpy(service->instance, instance.wire + 1, len); service->instance[len] = '\0';
      for (j = 0; j < count; j++)
         if (!strcmp(m->next_services[j].instance, service->instance)) break;
      if (j != count) continue;
      service->port = mdns_u16(srv->data + 4);
      service->txt_size = txt->size; memcpy(service->txt, txt->data, txt->size);
      if (a) { memcpy(service->ipv4, a->data, 4); service->has_ipv4 = true; }
      if (aaaa) { memcpy(service->ipv6, aaaa->data, 16); service->has_ipv6 = true; }
      count++;
   }
   if (m->event)
   {
      for (i = 0; i < m->service_count; i++)
      {
         for (j = 0; j < count; j++)
            if (!strcmp(m->services[i].instance, m->next_services[j].instance)) break;
         if (j == count) m->event(m->user, NET_MDNS_REMOVED, &m->services[i]);
      }
      for (i = 0; i < count; i++)
      {
         for (j = 0; j < m->service_count; j++)
            if (!strcmp(m->services[j].instance, m->next_services[i].instance)) break;
         if (j == m->service_count) m->event(m->user, NET_MDNS_ADDED, &m->next_services[i]);
         else if (memcmp(&m->services[j], &m->next_services[i], sizeof(m->services[j])))
            m->event(m->user, NET_MDNS_CHANGED, &m->next_services[i]);
      }
   }
   previous = m->services; m->services = m->next_services; m->next_services = previous;
   m->service_count = count;
}

static bool mdns_append(uint8_t *out, size_t *pos, const void *data, size_t size)
{
   if (size > MDNS_OUTPUT - *pos) return false;
   memcpy(out + *pos, data, size); *pos += size;
   return true;
}

static bool mdns_question(uint8_t *out, size_t *pos, const mdns_name_t *name,
      unsigned type, unsigned cls)
{
   if (name->size + 4 > MDNS_OUTPUT - *pos) return false;
   mdns_append(out, pos, name->wire, name->size);
   mdns_put16(out + *pos, type); mdns_put16(out + *pos + 2, cls); *pos += 4;
   return true;
}

static bool mdns_write_record(uint8_t *out, size_t *pos, const mdns_record_t *rr,
      uint32_t ttl, bool flush)
{
   if (rr->owner.size + rr->size + 10 > MDNS_OUTPUT - *pos) return false;
   mdns_append(out, pos, rr->owner.wire, rr->owner.size);
   mdns_put16(out + *pos, rr->type); mdns_put16(out + *pos + 2, flush ? 0x8001 : 1);
   mdns_put32(out + *pos + 4, ttl); mdns_put16(out + *pos + 8, (unsigned)rr->size); *pos += 10;
   return mdns_append(out, pos, rr->data, rr->size);
}

static bool mdns_announce(net_mdns_t *m, unsigned mask, uint32_t ttl,
      const net_mdns_peer_t *peer, bool legacy)
{
   size_t pos = 12, i;
   unsigned count = 0;
   memset(m->output, 0, 12);
   mdns_put16(m->output, legacy ? m->reply_id : 0); mdns_put16(m->output + 2, 0x8400);
   if (legacy)
   {
      if (!mdns_append(m->output, &pos, m->legacy_questions, m->legacy_size)) return false;
      mdns_put16(m->output + 4, m->legacy_count);
   }
   for (i = 0; i < m->publish_count; i++)
      if (mask & (1U << i))
      {
         if (!mdns_write_record(m->output, &pos, &m->published[i],
                  legacy && ttl > 10 ? 10 : ttl, !legacy && m->published[i].type != MDNS_PTR))
         {
            if (!legacy) return false;
            mdns_put16(m->output + 2, 0x8600);
            break;
         }
         count++;
      }
   if (mask & 32U)
   {
      mdns_record_t meta;
      memset(&meta, 0, sizeof(meta));
      meta.owner = m->enumeration; meta.type = MDNS_PTR;
      meta.size = m->type.size; memcpy(meta.data, m->type.wire, meta.size);
      if (!mdns_write_record(m->output, &pos, &meta, ttl, false))
      {
         if (!legacy) return false;
         mdns_put16(m->output + 2, 0x8600);
      }
      else count++;
   }
   mdns_put16(m->output + 6, count);
   return (count || legacy) && m->send(m->user, m->output, pos, peer);
}

net_mdns_t *net_mdns_new(const char *service_type, uint32_t seed,
      net_mdns_send_t send, net_mdns_event_t event, void *user)
{
   net_mdns_t *m;
   mdns_name_t type;
   size_t offset, i;
   bool letter = false;
   if (!service_type || !send || !mdns_text_name(&type, service_type)) return NULL;
   offset = 1 + type.wire[0];
   for (i = 0; i < type.size; i++) type.wire[i] = mdns_lower(type.wire[i]);
   if (type.wire[0] < 2 || type.wire[0] > 16 || type.wire[1] != '_'
         || type.size != offset + 12 || type.wire[offset] != 4
         || (memcmp(type.wire + offset + 1, "_tcp", 4)
            && memcmp(type.wire + offset + 1, "_udp", 4))
         || !mdns_local(&type)) return NULL;
   for (i = 2; i < offset; i++)
   {
      uint8_t c = type.wire[i];
      if (c >= 'a' && c <= 'z') letter = true;
      else if (!(c >= '0' && c <= '9') && c != '-') return NULL;
      if (c == '-' && (i == 2 || i == offset - 1 || type.wire[i - 1] == '-')) return NULL;
   }
   if (!letter) return NULL;
   m = (net_mdns_t*)calloc(1, sizeof(*m));
   if (!m) return NULL;
   m->type = type; m->send = send; m->event = event; m->user = user;
   m->services = m->service_storage[0]; m->next_services = m->service_storage[1];
   m->random = seed ? seed : 1;
   mdns_text_name(&m->enumeration, "_services._dns-sd._udp.local.");
   return m;
}

void net_mdns_free(net_mdns_t *m)
{ if (m) { net_mdns_unpublish(m); free(m); } }

void net_mdns_browse(net_mdns_t *m, bool enabled, uint64_t now)
{
   if (!m) return;
   if (now < m->now) now = m->now;
   m->now = now; m->browsing = enabled; m->browse_due = now; m->browse_interval = 1000;
   if (!enabled)
   {
      memset(m->cache, 0, sizeof(m->cache));
      mdns_refresh_services(m, now);
   }
}

static void mdns_publish_record(mdns_record_t *rr, const mdns_name_t *owner,
      unsigned type, const void *data, size_t size)
{
   rr->owner = *owner; rr->type = (uint16_t)type;
   rr->cls = type == MDNS_PTR ? 1 : 0x8001; rr->ttl = MDNS_TTL;
   rr->size = size; memcpy(rr->data, data, size);
}

bool net_mdns_publish(net_mdns_t *m, const net_mdns_service_t *service, uint64_t now)
{
   mdns_name_t instance, host;
   size_t i, len;
   uint8_t srv[NET_MDNS_NAME_MAX + 6];
   uint8_t empty = 0;
   if (!m || !service || !service->port || (!service->has_ipv4 && !service->has_ipv6)
         || !memchr(service->instance, 0, sizeof(service->instance))
         || !memchr(service->hostname, 0, sizeof(service->hostname))
         || service->txt_size > NET_MDNS_TXT_MAX
         || (service->txt_size && !mdns_txt_valid(service->txt, service->txt_size))) return false;
   len = strlen(service->hostname);
   if (!len || len > 63 || service->hostname[0] == '-' || service->hostname[len - 1] == '-') return false;
   for (i = 0; i < len; i++)
   {
      uint8_t c = mdns_lower((uint8_t)service->hostname[i]);
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
   }
   instance.size = 0; host.size = 0;
   if (!mdns_label(&instance, service->instance) || instance.size + m->type.size > 255) return false;
   memcpy(instance.wire + instance.size, m->type.wire, m->type.size); instance.size += m->type.size;
   if (!mdns_instance_name(m, &instance)) return false;
   if (!mdns_label(&host, service->hostname) || !mdns_label(&host, "local")) return false;
   host.size++;
   net_mdns_unpublish(m);
   if (now < m->now) now = m->now;
   m->instance = instance; m->hostname = host; m->publish_count = 0;
   mdns_publish_record(&m->published[m->publish_count++], &m->type, MDNS_PTR, instance.wire, instance.size);
   memset(srv, 0, 6); mdns_put16(srv + 4, service->port);
   memcpy(srv + 6, host.wire, host.size);
   mdns_publish_record(&m->published[m->publish_count++], &instance, MDNS_SRV, srv, host.size + 6);
   mdns_publish_record(&m->published[m->publish_count++], &instance, MDNS_TXT,
         service->txt_size ? service->txt : &empty, service->txt_size ? service->txt_size : 1);
   if (service->has_ipv4) mdns_publish_record(&m->published[m->publish_count++], &host, MDNS_A, service->ipv4, 4);
   if (service->has_ipv6) mdns_publish_record(&m->published[m->publish_count++], &host, MDNS_AAAA, service->ipv6, 16);
   m->state = NET_MDNS_PROBING; m->probes = m->announcements = 0;
   m->has_defended = false; m->now = now; m->publish_due = mdns_after(now, mdns_random(m, 251));
   return true;
}

void net_mdns_unpublish(net_mdns_t *m)
{
   if (!m) return;
   if (m->state == NET_MDNS_ESTABLISHED)
      mdns_announce(m, (1U << m->publish_count) - 1, 0, NULL, false);
   m->state = NET_MDNS_IDLE; m->reply_pending = false;
}

enum net_mdns_publish_state net_mdns_published(const net_mdns_t *m)
{ return m ? m->state : NET_MDNS_IDLE; }

size_t net_mdns_service_count(const net_mdns_t *m)
{ return m ? m->service_count : 0; }

const net_mdns_service_t *net_mdns_service_at(const net_mdns_t *m, size_t index)
{ return m && index < m->service_count ? &m->services[index] : NULL; }

static int mdns_record_order(const void *av, const void *bv)
{
   const mdns_record_t *a = *(const mdns_record_t *const*)av;
   const mdns_record_t *b = *(const mdns_record_t *const*)bv;
   size_t i, prefix = a->type == MDNS_SRV ? 6 : 0;
   if (a->type != b->type) return a->type < b->type ? -1 : 1;
   for (i = 0; i < a->size && i < b->size; i++)
   {
      uint8_t ac = a->data[i], bc = b->data[i];
      if ((a->type == MDNS_SRV || a->type == MDNS_PTR) && i >= prefix)
      { ac = mdns_lower(ac); bc = mdns_lower(bc); }
      if (ac != bc) return ac < bc ? -1 : 1;
   }
   return a->size == b->size ? 0 : a->size < b->size ? -1 : 1;
}

/* RFC 6762 section 8.2: compare the whole proposed RRset, not just whichever
 * conflicting record happened to arrive first. Lexicographically later wins. */
static bool mdns_probe_lost(net_mdns_t *m, unsigned remote_count)
{
   const mdns_record_t *ours[5], *theirs[5];
   const mdns_name_t *owner;
   unsigned group, i, own_count, their_count;
   for (group = 0; group < 2; group++)
   {
      owner = group ? &m->hostname : &m->instance;
      own_count = their_count = 0;
      for (i = 1; i < m->publish_count; i++)
         if (mdns_equal(&m->published[i].owner, owner)) ours[own_count++] = &m->published[i];
      for (i = 0; i < remote_count; i++)
         if (mdns_equal(&m->probe_remote[i].owner, owner)) theirs[their_count++] = &m->probe_remote[i];
      if (!their_count) continue;
      qsort((void*)ours, own_count, sizeof(ours[0]), mdns_record_order);
      qsort((void*)theirs, their_count, sizeof(theirs[0]), mdns_record_order);
      for (i = 0; i < own_count && i < their_count; i++)
      {
         int order = mdns_record_order(&ours[i], &theirs[i]);
         if (order < 0) return true;
         if (order > 0) break;
      }
      if (i == own_count && i < their_count) return true;
   }
   return false;
}

static void mdns_response_conflict(net_mdns_t *m, const mdns_record_t *rr, uint64_t now)
{
   size_t i;
   if ((rr->cls & 0x7fff) != 1 || m->state == NET_MDNS_IDLE
         || m->state == NET_MDNS_CONFLICT) return;
   for (i = 0; i < m->publish_count; i++)
      if (rr->type == m->published[i].type && mdns_equal(&rr->owner, &m->published[i].owner))
      {
         if (mdns_same_data(rr, &m->published[i]))
         {
            /* A cooperating publisher may be leaving. Rescue our still-valid
             * records before its one-second goodbye expires. */
            if (m->state == NET_MDNS_ESTABLISHED && rr->ttl < MDNS_TTL / 2)
               m->publish_due = now;
            continue;
         }
         if (!i || !rr->ttl) continue;
         if (m->state == NET_MDNS_PROBING || (m->has_defended && now - m->defended < 10000))
         { m->state = NET_MDNS_CONFLICT; m->reply_pending = false; }
         else
         { m->has_defended = true; m->defended = now; m->publish_due = now; }
         break;
      }
}

typedef struct mdns_message
{
   size_t records_start;
   unsigned questions, answers, authority, total;
   bool response;
} mdns_message_t;

static bool mdns_validate_packet(net_mdns_t *m, const uint8_t *packet, size_t size,
      const net_mdns_peer_t *peer, mdns_message_t *message)
{
   uint16_t flags;
   unsigned questions, answers, authority, additional, total, i;
   size_t p;
   bool response;
   mdns_record_t *rr = &m->input_record;
   mdns_name_t *question = &m->input_name;
   flags = mdns_u16(packet + 2); response = (flags & 0x8000) != 0;
   if (flags & 0x7a0f) return false; /* No opcodes, truncation or error responses. */
   if (response && (peer->port != 5353 || mdns_u16(packet))) return false;
   questions = mdns_u16(packet + 4); answers = mdns_u16(packet + 6);
   authority = mdns_u16(packet + 8); additional = mdns_u16(packet + 10);
   total = answers + authority + additional;
   if (questions > 64 || total > MDNS_RECORD_LIMIT) return false;
   p = 12;
   for (i = 0; i < questions; i++)
   {
      if (!mdns_read_name(packet, size, &p, question) || size - p < 4) return false;
      p += 4;
   }
   message->records_start = p;
   /* Validate everything before publishing events, cache entries, or replies. */
   for (i = 0; i < total; i++)
      if (!mdns_read_record(packet, size, &p, rr)) return false;
   if (p != size) return false;
   message->questions = questions; message->answers = answers;
   message->authority = authority; message->total = total; message->response = response;
   return true;
}

static void mdns_receive_response(net_mdns_t *m, const uint8_t *packet, size_t size,
      const mdns_message_t *message, uint64_t now)
{
   size_t p;
   unsigned i;
   mdns_record_t *rr = &m->input_record;
   p = message->records_start;
   for (i = 0; i < message->total; i++)
   {
      mdns_read_record(packet, size, &p, rr);
      mdns_response_conflict(m, rr, now);
      if (m->browsing && rr->type != MDNS_A && rr->type != MDNS_AAAA)
         mdns_cache_record(m, rr, now);
   }
   if (m->browsing)
   {
      p = message->records_start;
      for (i = 0; i < message->total; i++)
      {
         mdns_read_record(packet, size, &p, rr);
         if (rr->type == MDNS_A || rr->type == MDNS_AAAA) mdns_cache_record(m, rr, now);
      }
      mdns_refresh_services(m, now);
   }
}

static bool mdns_receive_query(net_mdns_t *m, const uint8_t *packet, size_t size,
      const net_mdns_peer_t *peer, const mdns_message_t *message, uint64_t now)
{
   size_t p, q;
   unsigned i, j, requested = 0, suppressed = 0, remote_count = 0;
   bool legacy, multicast = false;
   mdns_record_t *rr = &m->input_record;
   mdns_name_t *question = &m->input_name;
   p = message->records_start;
   for (i = 0; i < message->total; i++)
   {
      mdns_read_record(packet, size, &p, rr);
      if ((rr->cls & 0x7fff) != 1) continue;
      if (i < message->answers)
      {
         for (j = 0; j < m->publish_count; j++)
            if (rr->ttl >= MDNS_TTL / 2 && mdns_same_data(rr, &m->published[j]))
               suppressed |= 1U << j;
      }
      else if (i < message->answers + message->authority && m->state == NET_MDNS_PROBING
            && rr->type && rr->type != MDNS_PTR
            && (mdns_equal(&rr->owner, &m->hostname) || mdns_equal(&rr->owner, &m->instance)))
      {
         if (remote_count == 5)
         { m->state = NET_MDNS_CONFLICT; continue; }
         memcpy(&m->probe_remote[remote_count], rr, sizeof(*rr));
         remote_count++;
      }
   }
   if (m->state == NET_MDNS_PROBING && remote_count && mdns_probe_lost(m, remote_count))
      m->state = NET_MDNS_CONFLICT;
   if (m->state != NET_MDNS_ESTABLISHED) return true;
   if (m->reply_pending && m->reply_unicast) return true;
   p = 12; q = 0; legacy = peer->port != 5353;
   for (i = 0; i < message->questions; i++)
   {
      unsigned type, cls;
      mdns_read_name(packet, size, &p, question);
      type = mdns_u16(packet + p); cls = mdns_u16(packet + p + 2); p += 4;
      if (legacy && !mdns_question(m->legacy_questions, &q, question, type, cls)) return true;
      if ((cls & 0x7fff) != 1) continue;
      if (!(cls & 0x8000)) multicast = true;
      if ((type == MDNS_PTR || type == MDNS_ANY) && mdns_equal(question, &m->enumeration))
         requested |= 32U;
      for (j = 0; j < m->publish_count; j++)
         if ((type == MDNS_ANY || type == m->published[j].type)
               && mdns_equal(question, &m->published[j].owner) && !(suppressed & (1U << j)))
         {
            requested |= 1U << j;
            if (j == 0) requested |= (1U << m->publish_count) - 1;
            if (m->published[j].type == MDNS_SRV)
               requested |= ((1U << m->publish_count) - 1) & ~7U;
         }
   }
   if (!requested) return true;
   if (!m->reply_pending)
   {
      m->reply_mask = requested; m->reply_peer = *peer; m->legacy = legacy;
      m->reply_unicast = legacy || !multicast; m->reply_id = mdns_u16(packet);
      m->legacy_size = q; m->legacy_count = message->questions;
      m->reply_due = mdns_after(now, legacy ? 0 : 20 + mdns_random(m, 101));
      m->reply_pending = true;
   }
   else if (!m->reply_unicast && !legacy) m->reply_mask |= requested;
   return true;
}

bool net_mdns_receive(net_mdns_t *m, const void *data, size_t size,
      const net_mdns_peer_t *peer, unsigned hop_limit, uint64_t now)
{
   const uint8_t *packet = (const uint8_t*)data;
   mdns_message_t message;
   if (!m || !packet || !peer || (peer->family != 4 && peer->family != 6)
         || !peer->port || hop_limit != 255 || size < 12 || size > NET_MDNS_PACKET_MAX)
      return false;
   if (!mdns_validate_packet(m, packet, size, peer, &message)) return false;
   if (now < m->now) now = m->now;
   m->now = now;
   if (message.response)
   {
      mdns_receive_response(m, packet, size, &message, now);
      return true;
   }
   return mdns_receive_query(m, packet, size, peer, &message, now);
}

static bool mdns_send_probe(net_mdns_t *m)
{
   size_t p = 12, i;
   memset(m->output, 0, 12);
   mdns_question(m->output, &p, &m->instance, MDNS_ANY, 1);
   mdns_question(m->output, &p, &m->hostname, MDNS_ANY, 1);
   mdns_put16(m->output + 4, 2);
   for (i = 1; i < m->publish_count; i++)
      if (!mdns_write_record(m->output, &p, &m->published[i], MDNS_TTL, false)) return false;
   mdns_put16(m->output + 8, (unsigned)m->publish_count - 1);
   return m->send(m->user, m->output, p, NULL);
}

static bool mdns_send_browse(net_mdns_t *m, uint64_t now)
{
   size_t p = 12, i;
   unsigned questions = 1, answers = 0;
   memset(m->output, 0, 12);
   mdns_question(m->output, &p, &m->type, MDNS_PTR, 1);
   for (i = 0; i < MDNS_CACHE; i++)
   {
      const mdns_cache_t *entry = &m->cache[i];
      mdns_name_t instance, hostname;
      const mdns_record_t *srv;
      if (!entry->used || entry->expires <= now || entry->record.type != MDNS_PTR) continue;
      instance.size = entry->record.size; memcpy(instance.wire, entry->record.data, instance.size);
      if (questions == 64) break;
      if (mdns_needs_refresh(m, &instance, MDNS_SRV, now))
      {
         if (!mdns_question(m->output, &p, &instance, MDNS_SRV, 1)) break;
         questions++;
      }
      if (questions == 64) break;
      if (mdns_needs_refresh(m, &instance, MDNS_TXT, now))
      {
         if (!mdns_question(m->output, &p, &instance, MDNS_TXT, 1)) break;
         questions++;
      }
      srv = mdns_find(m, &instance, MDNS_SRV, now);
      if (!srv) continue;
      hostname.size = srv->size - 6; memcpy(hostname.wire, srv->data + 6, hostname.size);
      if (questions == 64) break;
      if ((!mdns_find(m, &hostname, MDNS_A, now) && !mdns_find(m, &hostname, MDNS_AAAA, now))
            || (mdns_find(m, &hostname, MDNS_A, now) && mdns_needs_refresh(m, &hostname, MDNS_A, now))
            || (mdns_find(m, &hostname, MDNS_AAAA, now) && mdns_needs_refresh(m, &hostname, MDNS_AAAA, now)))
      {
         if (!mdns_question(m->output, &p, &hostname, MDNS_ANY, 1)) break;
         questions++;
      }
   }
   for (i = 0; i < MDNS_CACHE; i++)
   {
      const mdns_cache_t *entry = &m->cache[i];
      uint32_t remaining;
      if (!entry->used || entry->expires <= now || entry->record.type != MDNS_PTR) continue;
      remaining = (uint32_t)((entry->expires - now) / 1000);
      if (remaining < entry->record.ttl / 2) continue;
      if (!mdns_write_record(m->output, &p, &entry->record, remaining, false)) break;
      answers++;
   }
   mdns_put16(m->output + 4, questions); mdns_put16(m->output + 6, answers);
   return m->send(m->user, m->output, p, NULL);
}

void net_mdns_poll(net_mdns_t *m, uint64_t now)
{
   if (!m) return;
   if (now < m->now) now = m->now;
   m->now = now;
   if (m->browsing)
   {
      mdns_refresh_services(m, now);
      if (now >= m->browse_due)
      {
         if (mdns_send_browse(m, now))
         {
            size_t i;
            m->browse_due = mdns_after(now, m->browse_interval);
            for (i = 0; i < MDNS_CACHE; i++)
            {
               const mdns_cache_t *entry = &m->cache[i];
               uint64_t refresh;
               if (!entry->used || entry->expires <= now) continue;
               refresh = entry->expires > (uint64_t)entry->record.ttl * 200
                  ? entry->expires - ((uint64_t)entry->record.ttl * 200) : 0;
               if (refresh <= now) refresh = mdns_after(now, 1000);
               if (refresh < m->browse_due) m->browse_due = refresh;
            }
            if (m->browse_interval < 60000)
               m->browse_interval = m->browse_interval > 30000 ? 60000 : m->browse_interval * 2;
         }
         else m->browse_due = mdns_after(now, 1000);
      }
   }
   if (m->state == NET_MDNS_PROBING && now >= m->publish_due)
   {
      if (m->probes == 3)
      {
         m->state = NET_MDNS_ESTABLISHED;
         m->announcements = 0;
      }
      else if (mdns_send_probe(m))
      { m->probes++; m->publish_due = mdns_after(now, 250); }
      else m->publish_due = mdns_after(now, 250);
   }
   if (m->state == NET_MDNS_ESTABLISHED && now >= m->publish_due)
   {
      if (mdns_announce(m, (1U << m->publish_count) - 1, MDNS_TTL, NULL, false))
      {
         m->announcements++;
         m->publish_due = mdns_after(now, m->announcements == 1 ? 1000 : 60000);
      }
      else m->publish_due = mdns_after(now, 250);
   }
   if (m->state == NET_MDNS_ESTABLISHED && m->reply_pending && now >= m->reply_due)
   {
      if (mdns_announce(m, m->reply_mask, MDNS_TTL,
               m->reply_unicast ? &m->reply_peer : NULL, m->legacy)) m->reply_pending = false;
      else m->reply_due = mdns_after(now, 100);
   }
}

uint32_t net_mdns_timeout(const net_mdns_t *m, uint64_t now)
{
   uint64_t due = (uint64_t)-1;
   size_t i;
   if (!m) return (uint32_t)-1;
   if (now < m->now) now = m->now;
   if (m->browsing)
   {
      due = m->browse_due;
      for (i = 0; i < MDNS_CACHE; i++)
         if (m->cache[i].used && m->cache[i].expires > m->now && m->cache[i].expires < due)
            due = m->cache[i].expires;
   }
   if ((m->state == NET_MDNS_PROBING || m->state == NET_MDNS_ESTABLISHED) && m->publish_due < due)
      due = m->publish_due;
   if (m->reply_pending && m->reply_due < due) due = m->reply_due;
   if (due == (uint64_t)-1) return (uint32_t)-1;
   if (due <= now) return 0;
   return due - now > (uint32_t)-1 ? (uint32_t)-1 : (uint32_t)(due - now);
}

#endif /* HAVE_MDNS */
