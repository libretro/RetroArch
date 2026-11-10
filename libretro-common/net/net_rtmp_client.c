/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <retro_posix_source.h>
#include <net/net_rtmp_client.h>
#include <net/net_socket.h>
#ifdef HAVE_SSL
#include <net/net_socket_ssl.h>
#endif
#include <stdlib.h>
#include <string.h>

struct net_rtmp_client
{
   net_rtmp_session_t *session;
   net_rtmp_socket_t *transport;
   net_resolve_t *resolver;
   struct addrinfo *address;
   const char *hostname;
#ifdef HAVE_SSL
   void *ssl;
#endif
   enum net_rtmp_client_state state;
   int fd;
   bool secure;
};

static bool client_scheme(const char *url, const char *scheme)
{
   while (*scheme)
      if (((unsigned char)*url++ | 32) != (unsigned char)*scheme++)
         return false;
   return true;
}

static bool client_url(char *parts, char *target, const char **hostname,
      unsigned *port, net_rtmp_publish_config_t *publish, bool secure)
{
   char *host = parts + (secure ? 8 : 7), *app, *name, *end, *digits = NULL;
   char *at;
   app = strchr(host, '/');
   if (!app || app == host)
      return false;
   *app++ = 0;
   name = strchr(app, '/');
   if (!name || name == app || !name[1])
      return false;
   target[name - parts] = 0;
   *name++ = 0;
   if (strchr(host, '@') || strchr(host, '?') || strchr(app, '?'))
      return false;
   if (*host == '[')
   {
      host++;
      end = strchr(host, ']');
      if (!end || end == host || (end[1] && end[1] != ':'))
         return false;
      if (end[1])
         digits = end + 2;
      *end = 0;
   }
   else
   {
      end = strchr(host, ':');
      if (end)
      {
         *end = 0;
         digits = end + 1;
      }
      if (strchr(host, '[') || strchr(host, ']'))
         return false;
   }
   if (!*host || strlen(host) > 1023)
      return false;
   *port = secure ? 443 : 1935;
   if (digits)
   {
      unsigned value = 0;
      if (!*digits)
         return false;
      for (at = digits; *at; at++)
      {
         if (*at < '0' || *at > '9' || value > (65535u - (unsigned)(*at - '0')) / 10)
            return false;
         value = value * 10 + (unsigned)(*at - '0');
      }
      if (!value)
         return false;
      *port = value;
   }
   *hostname = host;
   publish->application = app;
   publish->name = name;
   publish->url = target;
   return true;
}

static void client_close(net_rtmp_client_t *c)
{
   net_rtmp_socket_free(c->transport);
   c->transport = NULL;
#ifdef HAVE_SSL
   if (c->ssl)
   {
      ssl_socket_close(c->ssl);
      ssl_socket_free(c->ssl);
      c->ssl = NULL;
      c->fd = -1;
   }
#endif
   if (c->fd >= 0)
      socket_close(c->fd);
   c->fd = -1;
   net_resolve_free(c->resolver);
   c->resolver = NULL;
   c->address = NULL;
}

void net_rtmp_client_cancel(net_rtmp_client_t *c)
{
   if (!c || c->state == NET_RTMP_CLIENT_FINISHED)
      return;
   client_close(c);
   net_rtmp_session_cancel(c->session);
   c->state = NET_RTMP_CLIENT_FAILED;
}

net_rtmp_client_t *net_rtmp_client_new_caps(const net_rtmp_client_config_t *config,
      const net_rtmp_codec_caps_t *caps)
{
   net_rtmp_client_t *c;
   net_rtmp_publish_config_t publish;
   size_t length = 0, i;
   char *parts, *target;
   unsigned port;
   bool secure;
   if (!config || !config->url || !config->dispatch || !config->random)
      return NULL;
   while (length <= 4096 && config->url[length])
      length++;
   if (length < 7 || length > 4096)
      return NULL;
   for (i = 0; i < length; i++)
      if ((unsigned char)config->url[i] <= 32 || config->url[i] == 127 || config->url[i] == '#')
         return NULL;
   secure = length >= 8 && client_scheme(config->url, "rtmps://");
   if (!secure && !client_scheme(config->url, "rtmp://"))
      return NULL;
#ifndef HAVE_SSL
   if (secure)
      return NULL;
#endif
   c = (net_rtmp_client_t*)calloc(1, sizeof(*c) + 2 * (length + 1));
   if (!c)
      return NULL;
   c->fd = -1;
   c->secure = secure;
   parts = (char*)(c + 1);
   target = parts + length + 1;
   memcpy(parts, config->url, length + 1);
   memcpy(target, config->url, length + 1);
   publish.legacy_commands = config->legacy_commands;
   if (!client_url(parts, target, &c->hostname, &port, &publish, secure))
      goto failed;
   c->session = net_rtmp_session_new_caps(&publish, config->random, config->time,
         config->max_reply, config->streams, caps);
   if (!c->session)
      goto failed;
   c->resolver = net_resolve_new(c->hostname, port, config->dispatch, config->userdata);
   if (!c->resolver)
      goto failed;
   return c;
failed:
   client_close(c);
   net_rtmp_session_free(c->session);
   free(c);
   return NULL;
}

net_rtmp_client_t *net_rtmp_client_new(const net_rtmp_client_config_t *config)
{
   return net_rtmp_client_new_caps(config, NULL);
}

static bool client_connected(net_rtmp_client_t *c)
{
   net_resolve_free(c->resolver);
   c->resolver = NULL;
   c->address = NULL;
   if (c->secure)
      c->state = NET_RTMP_CLIENT_TLS;
   else
   {
      c->transport = net_rtmp_socket_new(c->session, c->fd);
      if (!c->transport)
         return false;
      c->state = NET_RTMP_CLIENT_NEGOTIATING;
   }
   return true;
}

enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t receive_time, net_rtmp_client_view_t *view)
{
   int result;
   net_rtmp_span_t span;
   enum net_rtmp_session_state state;
   void *addresses = NULL;
   if (!view)
      return NET_RTMP_CLIENT_FAILED;
   view->session = c ? c->session : NULL;
   view->fd = c ? c->fd : -1;
   view->read = view->write = view->again = false;
   if (!c || c->state == NET_RTMP_CLIENT_FAILED || c->state == NET_RTMP_CLIENT_FINISHED)
      return c ? c->state : NET_RTMP_CLIENT_FAILED;
   if (c->state == NET_RTMP_CLIENT_RESOLVING)
   {
      result = net_resolve_poll(c->resolver, &addresses);
      if (result == NET_RESOLVE_ERROR)
         goto failed;
      if (result == NET_RESOLVE_READY)
      {
         c->address = (struct addrinfo*)addresses;
         if (!c->address)
            goto failed;
         c->state = NET_RTMP_CLIENT_CONNECTING;
         view->again = true;
      }
      return c->state;
   }
   if (c->state == NET_RTMP_CLIENT_CONNECTING)
   {
      if (c->fd < 0)
      {
         c->fd = socket(c->address->ai_family, c->address->ai_socktype, c->address->ai_protocol);
         result = c->fd < 0 ? SOCKET_CONNECT_ERROR : socket_connect_begin(c->fd, c->address);
      }
      else
         result = socket_connect_step(c->fd, c->address);
      if (result == SOCKET_CONNECT_ERROR)
      {
         if (c->fd >= 0)
            socket_close(c->fd);
         c->fd = -1;
         c->address = c->address->ai_next;
         if (!c->address)
            goto failed;
         view->again = true;
      }
      else if (result == SOCKET_CONNECT_READY)
      {
         if (!client_connected(c))
            goto failed;
         view->again = true;
      }
      else
         view->write = true;
      view->fd = c->fd;
      return c->state;
   }
#ifdef HAVE_SSL
   if (c->state == NET_RTMP_CLIENT_TLS)
   {
      if (!c->ssl)
         c->ssl = ssl_socket_init(c->fd, c->hostname);
      if (!c->ssl)
         goto failed;
      result = ssl_socket_handshake_step(c->ssl, &view->read, &view->write);
      if (result < 0)
         goto failed;
      if (result > 0)
      {
         c->transport = net_rtmp_socket_new_tls(c->session, c->fd, c->ssl);
         if (!c->transport)
            goto failed;
         c->state = NET_RTMP_CLIENT_NEGOTIATING;
      }
      view->again = result > 0 || (!view->read && !view->write);
      return c->state;
   }
#endif
   if (c->state == NET_RTMP_CLIENT_CLOSING)
   {
      char input[256];
      ssize_t count = recv(c->fd, input, sizeof(input), 0);
      if (!count)
      {
         client_close(c);
         c->state = NET_RTMP_CLIENT_FINISHED;
         view->fd = -1;
      }
      else
      {
         if (count < 0 && !isagain((int)count))
            goto failed;
         view->read = true;
         view->again = count > 0;
      }
      return c->state;
   }
   result = net_rtmp_socket_step(c->transport, receive_time);
   if (result == NET_RTMP_SOCKET_ERROR)
      goto failed;
   state = net_rtmp_session_status(c->session, NULL, NULL);
   if (state == NET_RTMP_SESSION_FINISHED)
   {
      result = net_rtmp_socket_flush(c->transport);
      if (result < 0)
         goto failed;
      if (result)
      {
         if (c->secure)
         {
            client_close(c);
            c->state = NET_RTMP_CLIENT_FINISHED;
            view->fd = -1;
         }
         else
         {
#ifdef _WIN32
            if (shutdown(c->fd, SD_SEND))
#else
            if (shutdown(c->fd, SHUT_WR))
#endif
               goto failed;
            c->state = NET_RTMP_CLIENT_CLOSING;
            view->read = view->again = true;
         }
      }
      else
      {
         c->state = NET_RTMP_CLIENT_DRAINING;
         view->write = true;
      }
      return c->state;
   }
   c->state = state == NET_RTMP_SESSION_READY ? NET_RTMP_CLIENT_READY
      : state == NET_RTMP_SESSION_DRAINING ? NET_RTMP_CLIENT_DRAINING : NET_RTMP_CLIENT_NEGOTIATING;
   view->read = true;
   view->again = result == NET_RTMP_SOCKET_PROGRESS;
   view->write = net_rtmp_session_get(c->session, &span);
#if defined(HAVE_SSL) && defined(HAVE_RETROSSL)
   if (c->ssl)
   {
      bool write = false;
      view->again = ssl_socket_retro_pending(c->ssl, &write) || view->again;
      view->write = write || view->write;
   }
#endif
   return c->state;
failed:
   net_rtmp_client_cancel(c);
   view->fd = -1;
   view->read = view->write = view->again = false;
   return NET_RTMP_CLIENT_FAILED;
}

bool net_rtmp_client_free(net_rtmp_client_t *c)
{
   if (!c)
      return true;
   if (c->state != NET_RTMP_CLIENT_FAILED && c->state != NET_RTMP_CLIENT_FINISHED)
      return false;
   if (!net_rtmp_session_free(c->session))
      return false;
   free(c);
   return true;
}
