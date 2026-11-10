/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <net/net_rtmp_client.h>
#include <net/net_socket.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void (*work[4])(void*);
static void *jobs[4];
static unsigned queued;
static uint8_t random_bytes[1528];
static char long_url[4098];

static bool dispatch(void (*callback)(void*), void *job, void *userdata)
{
   assert(userdata == &queued && queued < 4);
   work[queued] = callback;
   jobs[queued++] = job;
   return true;
}

static void retire(void)
{
   while (queued)
   {
      unsigned i = --queued;
      work[i](jobs[i]);
   }
}

int main(void)
{
   static const char *const valid[] =
   {
      "rtmp://example.invalid/app/key",
      "RTMP://example.invalid:01935/app/folder/key?token=a%2Fb",
      "rtmp://[::1]:1935/app/key", "rtmp://host:65535/app/key"
   };
   static const char *const invalid[] =
   {
      "", "rtmp://", "https://host/app/key", "rtmp://host/app",
      "rtmp:///app/key", "rtmp://host//key", "rtmp://host/app/",
      "rtmp://user@host/app/key", "rtmp://host:0/app/key",
      "rtmp://host:65536/app/key", "rtmp://host:/app/key",
      "rtmp://host:+1/app/key", "rtmp://host:one/app/key",
      "rtmp://::1/app/key", "rtmp://[::1/app/key",
      "rtmp://[::1]oops/app/key", "rtmp://host/app?x/key",
      "rtmp://host/app/key#fragment", "rtmp://host/app/k ey",
      "rtmp://host/app/key\n"
   };
   net_rtmp_client_config_t config;
   net_rtmp_client_t *client;
   net_rtmp_client_view_t view;
   unsigned i;
   assert(network_init());
   memset(&config, 0, sizeof(config));
   config.random = random_bytes;
   config.dispatch = dispatch;
   config.userdata = &queued;
   config.max_reply = 65536;
   config.streams = 8;
   assert(!net_rtmp_client_new(NULL) && net_rtmp_client_free(NULL));
   for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
   {
      config.url = invalid[i];
      assert(!net_rtmp_client_new(&config) && !queued);
   }
   for (i = 0; i < sizeof(valid) / sizeof(valid[0]); i++)
   {
      config.url = valid[i];
      client = net_rtmp_client_new(&config);
      assert(client && queued == 1 && !net_rtmp_client_free(client));
      assert(net_rtmp_client_step(client, 0, &view) == NET_RTMP_CLIENT_RESOLVING);
      assert(view.session && view.fd == -1 && !view.again && !view.read && !view.write);
      net_rtmp_client_cancel(client);
      net_rtmp_client_cancel(client);
      assert(net_rtmp_client_step(client, 0, &view) == NET_RTMP_CLIENT_FAILED);
      assert(net_rtmp_client_free(client));
      retire();
   }
   memcpy(long_url, "rtmp://host/app/", 16);
   memset(long_url + 16, 'x', sizeof(long_url) - 17);
   long_url[4096] = 0;
   config.url = long_url;
   client = net_rtmp_client_new(&config);
   assert(client);
   net_rtmp_client_cancel(client);
   assert(net_rtmp_client_free(client));
   retire();
   long_url[4096] = 'x';
   long_url[4097] = 0;
   assert(!net_rtmp_client_new(&config) && !queued);
   config.url = valid[0];
   for (i = 0; i < 4; i++)
   {
      client = net_rtmp_client_new(&config);
      assert(client);
      net_rtmp_client_cancel(client);
      assert(net_rtmp_client_free(client));
   }
   assert(queued == 4 && !net_rtmp_client_new(&config));
   retire();
   client = net_rtmp_client_new(&config);
   assert(client);
   net_rtmp_client_cancel(client);
   assert(net_rtmp_client_free(client));
   retire();
   puts("PASS RTMP URL admission, pending resolution cancellation and bounded reuse");
   return 0;
}
