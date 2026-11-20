/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#include "record_pyrowave_lan.h"
#include "record_pyrowave_source.h"
#include "../configuration.h"
#include <string.h>
#include <compat/strl.h>
#include <file/file_path.h>

bool record_pyrowave_lan_directory(char *directory, size_t size)
{
   settings_t *settings = config_get_ptr();
   if (!directory || !size || !settings) return false;
   if (*settings->paths.directory_pyrowave_shaders)
      return strlcpy(directory, settings->paths.directory_pyrowave_shaders, size) < size;
   directory[0] = '\0';
   return true;
}

#if defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE) && (defined(__linux__) || defined(__APPLE__) \
      || (defined(_WIN32) && !defined(__WINRT__) && !defined(_XBOX)))
#include <net/net_mdns_socket.h>
#include <net/net_compat.h>
#include <stdio.h>
#ifdef _WIN32
#include <process.h>
#define pyrowave_lan_process_id _getpid
#else
#include <unistd.h>
#define pyrowave_lan_process_id getpid
#endif
#include "record_pyrowave_receiver.h"
#include "drivers/record_pyrowave.h"
#include "record_driver.h"
#include "../gfx/video_driver.h"
#include "../runloop.h"
#include "../command.h"
#include "../msg_hash.h"
#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

typedef struct record_pyrowave_lan_state
{
   net_mdns_socket_t *transport;
   net_mdns_service_t advertisement;
   record_pyrowave_lan_peer_t peers[NET_MDNS_SERVICE_MAX];
   char directory[1024], device[64], identity[17], target[64];
   unsigned port, advertised_port, role, epoch;
   uint64_t retry;
   bool incoming_notified, advertised_ready, assets_notified;
} record_pyrowave_lan_state_t;

static record_pyrowave_lan_state_t pyrowave_lan;
static void pyrowave_lan_refresh(void)
{
#ifdef HAVE_MENU
   menu_state_get_ptr()->frame_work |= MENU_FRAME_WORK_PYROWAVE;
#endif
}

static bool pyrowave_lan_txt(const net_mdns_service_t *service, const char *key,
      char *out, size_t capacity)
{
   size_t p = 0, key_size = strlen(key);
   while (p < service->txt_size)
   {
      size_t n = service->txt[p++];
      if (n > service->txt_size - p) return false;
      if (n > key_size && !memcmp(service->txt + p, key, key_size)
            && service->txt[p + key_size] == '=')
      {
         size_t value = n - key_size - 1;
         if (value >= capacity || memchr(service->txt + p + key_size + 1, 0, value)) return false;
         memcpy(out, service->txt + p + key_size + 1, value); out[value] = 0;
         return true;
      }
      p += n;
   }
   return false;
}

static void pyrowave_lan_event(void *user, enum net_mdns_event event,
      const net_mdns_service_t *service)
{
   record_pyrowave_lan_state_t *state = (record_pyrowave_lan_state_t*)user;
   char id[17], version[8], ready[4], role[8];
   size_t i, empty = NET_MDNS_SERVICE_MAX;
   bool compatible;
   for (i = 0; i < NET_MDNS_SERVICE_MAX; i++)
   {
      if (!strcmp(state->peers[i].name, service->instance)) state->peers[i].name[0] = 0;
      if (!state->peers[i].name[0] && empty == NET_MDNS_SERVICE_MAX) empty = i;
   }
   compatible = event != NET_MDNS_REMOVED && service->has_ipv4 && service->port
      && pyrowave_lan_txt(service, "id", id, sizeof(id)) && strlen(id) == 16
      && strcmp(id, state->identity)
      && pyrowave_lan_txt(service, "version", version, sizeof(version)) && !strcmp(version, "1")
      && pyrowave_lan_txt(service, "role", role, sizeof(role)) && !strcmp(role, "rx")
      && pyrowave_lan_txt(service, "ready", ready, sizeof(ready)) && !strcmp(ready, "1");
   if (compatible && empty != NET_MDNS_SERVICE_MAX)
   {
      record_pyrowave_lan_peer_t *peer = &state->peers[empty];
      for (i = 0; i < 16; i++)
         if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return;
      strlcpy(peer->name, service->instance, sizeof(peer->name));
      memcpy(peer->address, service->ipv4, 4); peer->port = service->port;
      sprintf(peer->key, "%02x%02x%02x%02x%04x%s", (unsigned)peer->address[0],
            (unsigned)peer->address[1], (unsigned)peer->address[2], (unsigned)peer->address[3],
            (unsigned)peer->port, id);
   }
   pyrowave_lan_refresh();
}

static bool pyrowave_lan_supported(void)
{
   video_device_info_t device;
   memset(&device, 0, sizeof(device));
   if (!video_driver_get_device(&device)) return false;
   return (device.api == VIDEO_DEVICE_OPENGL && !strcmp(video_driver_get_ident(), "glcore")) || device.api == VIDEO_DEVICE_VULKAN
      || device.api == VIDEO_DEVICE_D3D11 || device.api == VIDEO_DEVICE_D3D12;
}

static bool pyrowave_lan_assets(void)
{
   video_device_info_t device;
   char path[1200];
   const char *extension;
   static const char *const names[] = { "color", "dwt", "packet" };
   unsigned i;
   memset(&device, 0, sizeof(device));
   if (!video_driver_get_device(&device)) return false;
   extension = device.api == VIDEO_DEVICE_OPENGL ? "comp" :
      device.api == VIDEO_DEVICE_VULKAN ? "spv" : (*pyrowave_lan.directory ? "hlsl" : "dxbc");
   for (i = 0; i < 3; i++)
   {
      char name[24];
      snprintf(name, sizeof(name), "%s.%s", names[i], extension);
      if (!*pyrowave_lan.directory)
      {
         if (!record_pyrowave_source_shader_available(name)) return false;
      }
      else if (fill_pathname_join(path, pyrowave_lan.directory, name, sizeof(path)) >= sizeof(path)
            || !path_is_valid(path)) return false;
   }
   return true;
}

static void pyrowave_lan_append_txt(net_mdns_service_t *service, const char *text)
{
   size_t n = strlen(text);
   if (n > 255 || service->txt_size + n + 1 > sizeof(service->txt)) return;
   service->txt[service->txt_size++] = (uint8_t)n;
   memcpy(service->txt + service->txt_size, text, n); service->txt_size += n;
}

void record_pyrowave_lan_stop(void)
{
   bool changed = pyrowave_lan.transport != NULL;
   net_mdns_socket_free(pyrowave_lan.transport); pyrowave_lan.transport = NULL;
   memset(pyrowave_lan.peers, 0, sizeof(pyrowave_lan.peers));
   pyrowave_lan.port = pyrowave_lan.advertised_port = 0;
   pyrowave_lan.role = 0; pyrowave_lan.target[0] = 0;
   pyrowave_lan.incoming_notified = false;
   pyrowave_lan.retry = 0;
   if (changed) pyrowave_lan_refresh();
}

size_t record_pyrowave_lan_count(void)
{
   size_t i, count = 0;
   for (i = 0; i < NET_MDNS_SERVICE_MAX; i++) if (pyrowave_lan.peers[i].name[0]) count++;
   return count;
}

const record_pyrowave_lan_peer_t *record_pyrowave_lan_peer(size_t index)
{
   size_t i;
   for (i = 0; i < NET_MDNS_SERVICE_MAX; i++)
      if (pyrowave_lan.peers[i].name[0] && !index--) return &pyrowave_lan.peers[i];
   return NULL;
}

bool record_pyrowave_lan_send(const char *key)
{
   size_t i;
   if (!key || !pyrowave_lan.transport || pyrowave_lan.role || recording_state_get_ptr()->enable
         || record_pyrowave_receiver_active()) return false;
   for (i = 0; i < NET_MDNS_SERVICE_MAX; i++)
      if (pyrowave_lan.peers[i].name[0] && !strcmp(key, pyrowave_lan.peers[i].key))
      {
         const record_pyrowave_lan_peer_t *peer = &pyrowave_lan.peers[i];
         sprintf(pyrowave_lan.target, "rpws://%u.%u.%u.%u:%u", (unsigned)peer->address[0],
               (unsigned)peer->address[1], (unsigned)peer->address[2], (unsigned)peer->address[3],
               (unsigned)peer->port);
         pyrowave_lan.role = 1;
         record_pyrowave_receiver_stop();
         return true;
      }
   return false;
}

bool record_pyrowave_lan_sending(void) { return pyrowave_lan.role == 2; }
const char *record_pyrowave_lan_target(void)
{ return record_pyrowave_lan_sending() ? pyrowave_lan.target : NULL; }

void record_pyrowave_lan_poll(uint64_t now)
{
   settings_t *settings = config_get_ptr();
   runloop_state_t *loop = runloop_state_get_ptr();
   unsigned port;
   bool ready;
   /* A content load rereads the on-disk configuration. Keep the live
    * session's runtime opt-in; an explicit menu opt-out closes it first. */
   if (settings && pyrowave_lan.role == 2 && record_pyrowave_frontend_active())
      settings->bools.pyrowave_discovery_enable = true;
   if (settings && !loop->quit_pending
         && record_pyrowave_frontend_active() && (loop->content_closing
            || runloop_is_content_switching() || recording_gpu_reinit_pending()))
   {
      net_mdns_socket_poll(pyrowave_lan.transport, now);
      return;
   }
   if (!settings || !settings->bools.pyrowave_discovery_enable || loop->quit_pending
         || loop->content_closing || runloop_is_content_switching() || recording_gpu_reinit_pending())
   {
      if (pyrowave_lan.role)
      {
         pyrowave_lan.role = 0;
         command_event(CMD_EVENT_RECORD_DEINIT, NULL);
      }
      if (pyrowave_lan.transport) record_pyrowave_lan_stop();
      if (record_pyrowave_receiver_port() || !record_pyrowave_receiver_active()) record_pyrowave_receiver_stop();
      return;
   }
   if (!video_state_get_ptr()->data || !pyrowave_lan_supported() || now < pyrowave_lan.retry) return;
   if (!pyrowave_lan.transport)
   {
      uint32_t seed = (uint32_t)now ^ ++pyrowave_lan.epoch;
      if (!record_pyrowave_lan_directory(pyrowave_lan.directory, sizeof(pyrowave_lan.directory))
            || !pyrowave_lan_assets())
      {
         if (!pyrowave_lan.assets_notified)
         {
            const char *message = msg_hash_to_str(MSG_PYROWAVE_SHADERS_MISSING);
            runloop_msg_queue_push(message, strlen(message), 1, 600, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            pyrowave_lan.assets_notified = true;
         }
         pyrowave_lan.retry = now + 5000;
         return;
      }
      pyrowave_lan.assets_notified = false;
      pyrowave_lan.transport = net_mdns_socket_new("_pyrowave._tcp.local.", NULL, seed,
            pyrowave_lan_event, &pyrowave_lan);
      if (!pyrowave_lan.transport) { pyrowave_lan.retry = now + 5000; return; }
      net_mdns_socket_address(pyrowave_lan.transport, pyrowave_lan.advertisement.ipv4);
      sprintf(pyrowave_lan.identity, "%02x%02x%02x%02x%08lx",
            (unsigned)pyrowave_lan.advertisement.ipv4[0], (unsigned)pyrowave_lan.advertisement.ipv4[1],
            (unsigned)pyrowave_lan.advertisement.ipv4[2], (unsigned)pyrowave_lan.advertisement.ipv4[3],
            (unsigned long)(seed ^ (uint32_t)pyrowave_lan_process_id()));
      memset(&pyrowave_lan.advertisement, 0, sizeof(pyrowave_lan.advertisement));
      if (*settings->arrays.pyrowave_device_name)
         strlcpy(pyrowave_lan.device, settings->arrays.pyrowave_device_name, sizeof(pyrowave_lan.device));
      else if (gethostname(pyrowave_lan.device, sizeof(pyrowave_lan.device) - 1))
         strlcpy(pyrowave_lan.device, "RetroArch", sizeof(pyrowave_lan.device));
      pyrowave_lan.device[63] = 0;
      strlcpy(pyrowave_lan.advertisement.instance, pyrowave_lan.device, 64);
      sprintf(pyrowave_lan.advertisement.hostname, "retroarch-%08lx", (unsigned long)seed);
      net_mdns_socket_address(pyrowave_lan.transport, pyrowave_lan.advertisement.ipv4);
      pyrowave_lan.advertisement.has_ipv4 = true;
      net_mdns_browse(net_mdns_socket_engine(pyrowave_lan.transport), true, now);
   }
   if (pyrowave_lan.role == 1 && !record_pyrowave_receiver_pending())
   {
      pyrowave_lan.role = 2;
      streaming_set_state(true);
      command_event(CMD_EVENT_RECORD_INIT, NULL);
   }
   if (pyrowave_lan.role == 2 && !recording_state_get_ptr()->enable
         && !recording_state_get_ptr()->start_pending && !record_pyrowave_pending())
   { pyrowave_lan.role = 0; pyrowave_lan.target[0] = 0; pyrowave_lan_refresh(); }
   if (!pyrowave_lan.role && !recording_state_get_ptr()->enable && !recording_gpu_pending())
      record_pyrowave_receiver_listen(pyrowave_lan.directory);
   port = record_pyrowave_receiver_port();
   ready = port && !record_pyrowave_receiver_active() && !record_pyrowave_receiver_incoming()
      && !pyrowave_lan.role && !recording_state_get_ptr()->enable;
   if (port) pyrowave_lan.port = port;
   if (pyrowave_lan.port && (!pyrowave_lan.advertised_port || pyrowave_lan.port != pyrowave_lan.advertised_port || ready != pyrowave_lan.advertised_ready))
   {
      char id[20];
      net_mdns_service_t *advert = &pyrowave_lan.advertisement;
      advert->port = (uint16_t)pyrowave_lan.port; advert->txt_size = 0;
      pyrowave_lan_append_txt(advert, "version=1"); pyrowave_lan_append_txt(advert, "role=rx");
      pyrowave_lan_append_txt(advert, ready ? "ready=1" : "ready=0");
      sprintf(id, "id=%s", pyrowave_lan.identity); pyrowave_lan_append_txt(advert, id);
      if (net_mdns_publish(net_mdns_socket_engine(pyrowave_lan.transport), advert, now))
      { pyrowave_lan.advertised_port = pyrowave_lan.port; pyrowave_lan.advertised_ready = ready; }
   }
   if (net_mdns_published(net_mdns_socket_engine(pyrowave_lan.transport)) == NET_MDNS_CONFLICT)
   {
      sprintf(pyrowave_lan.advertisement.instance, "%.44s (%08lx)", pyrowave_lan.device,
            (unsigned long)((uint32_t)now ^ ++pyrowave_lan.epoch));
      net_mdns_publish(net_mdns_socket_engine(pyrowave_lan.transport), &pyrowave_lan.advertisement, now);
   }
   net_mdns_socket_poll(pyrowave_lan.transport, now);
   if (record_pyrowave_receiver_incoming() != pyrowave_lan.incoming_notified)
   {
      pyrowave_lan.incoming_notified = record_pyrowave_receiver_incoming();
      if (pyrowave_lan.incoming_notified)
      {
         const char *message = msg_hash_to_str(MSG_PYROWAVE_INCOMING);
         runloop_msg_queue_push(message, strlen(message), 1, 600, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      pyrowave_lan_refresh();
   }
}
#else
void record_pyrowave_lan_poll(uint64_t now) { (void)now; }
void record_pyrowave_lan_stop(void) { }
size_t record_pyrowave_lan_count(void) { return 0; }
const record_pyrowave_lan_peer_t *record_pyrowave_lan_peer(size_t index) { (void)index; return NULL; }
bool record_pyrowave_lan_send(const char *key) { (void)key; return false; }
bool record_pyrowave_lan_sending(void) { return false; }
const char *record_pyrowave_lan_target(void) { return NULL; }
#endif
