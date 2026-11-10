/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_upnp.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <formats/rxml.h>
#include <net/net_compat.h>
#include <net/net_http.h>
#include <net/net_socket.h>
#include <net/net_upnp.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

/* ---- parsing ---- */

bool net_upnp_ssdp_location(const char *msg, size_t len, char *s, size_t slen)
{
   const char *end = msg + len;
   while (msg < end)
   {
      const char *eol = (const char*)memchr(msg, '\n', (size_t)(end - msg));
      const char *stop = eol ? eol : end;
      if ((size_t)(stop - msg) > 9
            && string_starts_with_case_insensitive(msg, "LOCATION:"))
      {
         const char *v = msg + 9;
         size_t      n;
         while (v < stop && (*v == ' ' || *v == '\t'))
            v++;
         n = (size_t)(stop - v);
         while (n && (v[n - 1] == '\r' || v[n - 1] == ' ' || v[n - 1] == '\t'))
            n--;
         if (n < 8 || n >= slen
               || !string_starts_with_case_insensitive(v, "http://"))
            return false;
         memcpy(s, v, n);
         s[n] = '\0';
         return true;
      }
      msg = stop + 1;
   }
   return false;
}

/* The scheme://host:port part of url, as its length */
static size_t net_upnp_origin_len(const char *url)
{
   const char *host = strstr(url, "://");
   host = host ? host + 3 : url;
   return (size_t)(host - url) + strcspn(host, "/?#");
}

static bool net_upnp_absolute_url(char *s, size_t len, const char *base,
      const char *ref)
{
   size_t n;
   if (string_starts_with_case_insensitive(ref, "http://"))
      return strlcpy(s, ref, len) < len;
   n = net_upnp_origin_len(base);
   if (n + 1 >= len)
      return false;
   memcpy(s, base, n);
   s[n] = '\0';
   if (*ref != '/')
      strlcat(s, "/", len);
   return strlcat(s, ref, len) < len;
}

static bool net_upnp_find_service(rxml_node_t *node, const char *base,
      struct net_upnp_gateway *gw)
{
   rxml_node_t *child;
   if (string_is_equal_case_insensitive(node->name, "service"))
   {
      const char *type    = NULL;
      const char *control = NULL;
      for (child = node->children; child; child = child->next)
      {
         if (string_is_equal_case_insensitive(child->name, "serviceType"))
            type = child->data;
         else if (string_is_equal_case_insensitive(child->name, "controlURL"))
            control = child->data;
      }
      if (type && control && *control
            && (strstr(type, ":WANIPConnection:")
               || strstr(type, ":WANPPPConnection:"))
            && strlcpy(gw->service_type, type, sizeof(gw->service_type))
                  < sizeof(gw->service_type)
            && net_upnp_absolute_url(gw->control_url,
                  sizeof(gw->control_url), base, control))
         return true;
   }
   for (child = node->children; child; child = child->next)
      if (net_upnp_find_service(child, base, gw))
         return true;
   return false;
}

bool net_upnp_parse_description(const char *xml, const char *desc_url,
      struct net_upnp_gateway *gw)
{
   bool             found = false;
   rxml_node_t     *child;
   rxml_document_t *doc   = rxml_load_document_string(xml);
   const char      *base  = desc_url;
   if (!doc)
      return false;
   if (rxml_root_node(doc))
   {
      /* URLBase, when the description has one, is what relative URLs
       * resolve against */
      for (child = rxml_root_node(doc)->children; child; child = child->next)
         if (string_is_equal_case_insensitive(child->name, "URLBase")
               && child->data
               && string_starts_with_case_insensitive(child->data, "http://"))
            base = child->data;
      found = net_upnp_find_service(rxml_root_node(doc), base, gw);
   }
   rxml_free_document(doc);
   return found;
}

static const char *net_upnp_find_text(rxml_node_t *node, const char *name)
{
   rxml_node_t *child;
   const char  *colon = strchr(node->name, ':');
   if (string_is_equal_case_insensitive(colon ? colon + 1 : node->name, name))
      return node->data ? node->data : "";
   for (child = node->children; child; child = child->next)
   {
      const char *v = net_upnp_find_text(child, name);
      if (v)
         return v;
   }
   return NULL;
}

bool net_upnp_parse_external_address(const char *xml, char *s, size_t len)
{
   bool             ok  = false;
   rxml_document_t *doc = rxml_load_document_string(xml);
   if (!doc)
      return false;
   if (rxml_root_node(doc))
   {
      const char *v = net_upnp_find_text(rxml_root_node(doc),
            "NewExternalIPAddress");
      struct in_addr addr;
      if (v && inet_pton(AF_INET, v, &addr) == 1)
         ok = strlcpy(s, v, len) < len;
   }
   rxml_free_document(doc);
   return ok;
}

/* ---- transport ---- */

static retro_time_t net_upnp_deadline(int timeout_ms)
{
   return cpu_features_get_time_usec() + (retro_time_t)timeout_ms * 1000;
}

static int net_upnp_remaining_ms(retro_time_t deadline)
{
   retro_time_t left = deadline - cpu_features_get_time_usec();
   return left > 0 ? (int)((left + 999) / 1000) : 0;
}

/* GET url, or POST body with headers, within the deadline. Returns the
 * NUL-terminated response body, or NULL. */
static char *net_upnp_http(const char *url, const char *headers,
      const char *body, retro_time_t deadline, int *status)
{
   struct http_connection_t *conn;
   struct http_t            *http;
   char                     *out = NULL;
   uint8_t                  *data;
   size_t                    len = 0;

   *status = 0;
   if (!(conn = net_http_connection_new(url, body ? "POST" : "GET", NULL)))
      return NULL;
   if (headers)
      net_http_connection_set_headers(conn, headers);
   if (body)
      net_http_connection_set_content(conn, "text/xml; charset=\"utf-8\"",
            strlen(body), body);
   while (!net_http_connection_iterate(conn)) { }
   if (!net_http_connection_done(conn) || !(http = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      return NULL;
   }
   for (;;)
   {
      int left;
      if (net_http_update(http, NULL, NULL))
         break;
      if (!(left = net_upnp_remaining_ms(deadline)))
         goto done;
      /* in slices: Wine's poll can miss a finished connect */
      net_http_wait(http, left < 100 ? left : 100);
   }
   *status = net_http_status(http);
   if ((data = net_http_data(http, &len, true)))
   {
      /* The body becomes ours; terminate it for the XML parser */
      if ((out = (char*)realloc(data, len + 1)))
         out[len] = '\0';
      else
         free(data);
   }
done:
   net_http_delete(http);
   net_http_connection_free(conn);
   return out;
}

static char *net_upnp_soap(const struct net_upnp_gateway *gw,
      const char *action, const char *args, retro_time_t deadline,
      int *status)
{
   char  headers[256];
   char *body;
   char *out;
   size_t len = strlen(gw->service_type) + 2 * strlen(action)
              + strlen(args) + 256;
   if (!(body = (char*)malloc(len)))
      return NULL;
   snprintf(body, len,
         "<?xml version=\"1.0\"?>"
         "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
         "<s:Body><u:%s xmlns:u=\"%s\">%s</u:%s></s:Body></s:Envelope>",
         action, gw->service_type, args, action);
   snprintf(headers, sizeof(headers), "SOAPAction: \"%s#%s\"\r\n",
         gw->service_type, action);
   out = net_upnp_http(gw->control_url, headers, body, deadline, status);
   free(body);
   return out;
}

/* This host's address on the route to the gateway at url */
static bool net_upnp_lan_addr(const char *url, char *s, size_t len)
{
   char             host[128];
   char             port[8] = "80";
   const char      *h = url + 7;
   size_t           n = strcspn(h, ":/?#");
   struct addrinfo *addr = NULL;
   struct addrinfo  hints;
   struct sockaddr_in local;
   socklen_t        local_len = sizeof(local);
   int              fd;
   bool             ok = false;

   if (n >= sizeof(host))
      return false;
   memcpy(host, h, n);
   host[n] = '\0';
   if (h[n] == ':')
   {
      size_t p = strspn(h + n + 1, "0123456789");
      if (!p || p >= sizeof(port))
         return false;
      memcpy(port, h + n + 1, p);
      port[p] = '\0';
   }
   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   if (getaddrinfo_retro(host, port, &hints, &addr) || !addr)
      return false;
   /* a connected datagram socket sends nothing, but takes the route */
   if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
   {
      if (connect(fd, addr->ai_addr, (socklen_t)addr->ai_addrlen) == 0
            && getsockname(fd, (struct sockaddr*)&local, &local_len) == 0)
         ok = inet_ntop(AF_INET, &local.sin_addr, s, (socklen_t)len) != NULL;
      socket_close(fd);
   }
   freeaddrinfo_retro(addr);
   return ok;
}

/* ---- operations ---- */

bool net_upnp_discover(struct net_upnp_gateway *gw, int timeout_ms)
{
   static const char msearch[] =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
      "MX: 2\r\n"
      "\r\n";
   /* an SSDP reply is a datagram, one Ethernet frame at most; on the
    * heap, as this can run on an 8 KiB thread stack (PSP, GX) */
   enum { NET_UPNP_SSDP_MAX = 1536 };
   retro_time_t     deadline = net_upnp_deadline(timeout_ms);
   struct addrinfo *group    = NULL;
   struct addrinfo  hints;
   char            *buf;
   int              fd;
   bool             found    = false;

   memset(gw, 0, sizeof(*gw));
   memset(&hints, 0, sizeof(hints));
   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags    = AI_NUMERICHOST | AI_NUMERICSERV;
   if (getaddrinfo_retro("239.255.255.250", "1900", &hints, &group) || !group)
      return false;
   if (!(buf = (char*)malloc(NET_UPNP_SSDP_MAX)))
   {
      freeaddrinfo_retro(group);
      return false;
   }
   if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
      free(buf);
      freeaddrinfo_retro(group);
      return false;
   }
#ifdef IP_MULTICAST_TTL
   {
#ifdef _WIN32
      unsigned long ttl = 2;
#else
      unsigned char ttl = 2;
#endif
      setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl,
            sizeof(ttl));
   }
#endif
   if (sendto(fd, msearch, sizeof(msearch) - 1, 0, group->ai_addr,
            (socklen_t)group->ai_addrlen) == (ssize_t)(sizeof(msearch) - 1)
         && socket_nonblock(fd))
   {
      while (!found)
      {
         char    location[NET_UPNP_URL_MAX];
         bool    rd  = true;
         int     left = net_upnp_remaining_ms(deadline);
         ssize_t got;
         if (!left || !socket_wait(fd, &rd, NULL, left) || !rd)
            break;
         got = recvfrom(fd, buf, NET_UPNP_SSDP_MAX, 0, NULL, NULL);
         if (got <= 0)
            continue;
         if (net_upnp_ssdp_location(buf, (size_t)got, location,
                  sizeof(location)))
         {
            int   status;
            char *xml = net_upnp_http(location, NULL, NULL, deadline, &status);
            if (xml && status == 200 && net_upnp_parse_description(xml,
                     location, gw)
                  && net_upnp_lan_addr(gw->control_url, gw->lan_addr,
                     sizeof(gw->lan_addr)))
            {
               strlcpy(gw->desc_url, location, sizeof(gw->desc_url));
               found = true;
            }
            free(xml);
         }
      }
   }
   socket_close(fd);
   free(buf);
   freeaddrinfo_retro(group);
   if (!found)
      memset(gw, 0, sizeof(*gw));
   return found;
}

bool net_upnp_gateway_from_url(struct net_upnp_gateway *gw,
      const char *desc_url, int timeout_ms)
{
   int   status;
   bool  found = false;
   char *xml;

   memset(gw, 0, sizeof(*gw));
   if (!desc_url || !string_starts_with_case_insensitive(desc_url, "http://")
         || strlcpy(gw->desc_url, desc_url, sizeof(gw->desc_url))
               >= sizeof(gw->desc_url))
   {
      memset(gw, 0, sizeof(*gw));
      return false;
   }
   xml = net_upnp_http(desc_url, NULL, NULL, net_upnp_deadline(timeout_ms),
         &status);
   if (xml && status == 200 && net_upnp_parse_description(xml, desc_url, gw)
         && net_upnp_lan_addr(gw->control_url, gw->lan_addr,
            sizeof(gw->lan_addr)))
      found = true;
   free(xml);
   if (!found)
      memset(gw, 0, sizeof(*gw));
   return found;
}

bool net_upnp_external_address(const struct net_upnp_gateway *gw,
      char *s, size_t len, int timeout_ms)
{
   int   status;
   bool  ok;
   char *xml = net_upnp_soap(gw, "GetExternalIPAddress", "",
         net_upnp_deadline(timeout_ms), &status);
   ok = xml && status == 200 && net_upnp_parse_external_address(xml, s, len);
   free(xml);
   return ok;
}

/* text with XML's markup characters escaped, into s */
static void net_upnp_escape(char *s, size_t len, const char *text)
{
   size_t n = 0;
   for (; text && *text && n + 6 < len; text++)
   {
      const char *e = *text == '&' ? "&amp;" : *text == '<' ? "&lt;"
                    : *text == '>' ? "&gt;" : NULL;
      if (e)
         n += strlcpy(s + n, e, len - n);
      else
         s[n++] = *text;
   }
   s[n] = '\0';
}

bool net_upnp_add_port_mapping(const struct net_upnp_gateway *gw,
      unsigned port, bool udp, const char *description, int timeout_ms)
{
   return net_upnp_add_port_mapping_ext(gw, port, port, udp, description,
         timeout_ms);
}

bool net_upnp_add_port_mapping_ext(const struct net_upnp_gateway *gw,
      unsigned external_port, unsigned internal_port, bool udp,
      const char *description, int timeout_ms)
{
   char  args[640];
   char  desc[128];
   char *xml;
   int   status;
   net_upnp_escape(desc, sizeof(desc), description);
   snprintf(args, sizeof(args),
         "<NewRemoteHost></NewRemoteHost>"
         "<NewExternalPort>%u</NewExternalPort>"
         "<NewProtocol>%s</NewProtocol>"
         "<NewInternalPort>%u</NewInternalPort>"
         "<NewInternalClient>%s</NewInternalClient>"
         "<NewEnabled>1</NewEnabled>"
         "<NewPortMappingDescription>%s</NewPortMappingDescription>"
         "<NewLeaseDuration>0</NewLeaseDuration>",
         external_port, udp ? "UDP" : "TCP", internal_port, gw->lan_addr,
         desc);
   xml = net_upnp_soap(gw, "AddPortMapping", args,
         net_upnp_deadline(timeout_ms), &status);
   free(xml);
   return status == 200;
}

bool net_upnp_delete_port_mapping(const struct net_upnp_gateway *gw,
      unsigned port, bool udp, int timeout_ms)
{
   char  args[256];
   char *xml;
   int   status;
   snprintf(args, sizeof(args),
         "<NewRemoteHost></NewRemoteHost>"
         "<NewExternalPort>%u</NewExternalPort>"
         "<NewProtocol>%s</NewProtocol>",
         port, udp ? "UDP" : "TCP");
   xml = net_upnp_soap(gw, "DeletePortMapping", args,
         net_upnp_deadline(timeout_ms), &status);
   free(xml);
   return status == 200;
}
