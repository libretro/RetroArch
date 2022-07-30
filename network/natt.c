/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2021-2022 - Roberto V. Rampim
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>

#include <string/stdstring.h>
#include <formats/rxml.h>
#include <features/features_cpu.h>

#include <retro_miscellaneous.h>

#if !defined(HAVE_SOCKET_LEGACY) || defined(VITA) || defined(GEKKO)
#include <net/net_ifinfo.h>
#endif

#include "../tasks/tasks_internal.h"

#include "natt.h"

static bool translate_addr(struct sockaddr_in *addr,
   char *host, size_t hostlen, char *port, size_t portlen)
{
#ifndef HAVE_SOCKET_LEGACY
   if (getnameinfo((struct sockaddr *) addr, sizeof(*addr),
         host, hostlen, port, portlen,
         NI_NUMERICHOST | NI_NUMERICSERV))
      return false;
#else
   /* We need to do the conversion/translation manually. */
   {
      int res;
      uint8_t  *addr8 = (uint8_t *) &addr->sin_addr;
      uint16_t port16 = ntohs(addr->sin_port);

      if (host)
      {
         res = snprintf(host, hostlen, "%d.%d.%d.%d",
            (int) addr8[0], (int) addr8[1],
            (int) addr8[2], (int) addr8[3]);
         if (res < 0 || res >= hostlen)
            return false;
      }
      if (port)
      {
         res = snprintf(port, portlen, "%hu", port16);
         if (res < 0 || res >= portlen)
            return false;
      }
   }
#endif

   return true;
}

bool natt_init(struct natt_discovery *discovery)
{
   static const char msearch[] =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
      "MX: 5\r\n"
      "\r\n";
   bool ret;
   int fd                        = -1;
   struct addrinfo *msearch_addr = NULL;
   struct addrinfo *bind_addr    = NULL;
   struct addrinfo hints         = {0};

   if (!discovery)
      return false;

   hints.ai_family   = AF_INET;
   hints.ai_socktype = SOCK_DGRAM;
   if (getaddrinfo_retro("239.255.255.250", "1900", &hints, &msearch_addr))
      goto failure;
   if (!msearch_addr)
      goto failure;

   fd = socket_init((void **) &bind_addr, 0, NULL, SOCKET_TYPE_DATAGRAM);
   if (fd < 0)
      goto failure;
   if (!bind_addr)
      goto failure;

#if !defined(HAVE_SOCKET_LEGACY) || defined(VITA) || defined(GEKKO)
   {
      struct sockaddr_in *addr = (struct sockaddr_in *) bind_addr->ai_addr;

      if (net_ifinfo_best("223.255.255.255", &addr->sin_addr, false))
      {
#ifdef IP_MULTICAST_IF
         setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF,
            (const char *) &addr->sin_addr, sizeof(addr->sin_addr));
#endif
      }
   }
#endif

#ifdef IP_MULTICAST_TTL
   {
#ifdef _WIN32
      unsigned long ttl = 2;
      if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
         (const char *) &ttl, sizeof(ttl)) < 0) { }
#else
      unsigned char ttl = 2;
      if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
         &ttl, sizeof(ttl)) < 0) { }
#endif
   }
#endif

   if (!socket_bind(fd, bind_addr))
      goto failure;

   if (sendto(fd, msearch, STRLEN_CONST(msearch), 0,
            msearch_addr->ai_addr, msearch_addr->ai_addrlen)
         != STRLEN_CONST(msearch))
      goto failure;

   if (!socket_nonblock(fd))
      goto failure;

   discovery->fd      = fd;
   discovery->timeout = cpu_features_get_time_usec() + 5000000;

   ret = true;

   goto done;

failure:
   if (fd >= 0)
      socket_close(fd);

   discovery->fd      = -1;
   discovery->timeout = -1;

   ret = false;

done:
   freeaddrinfo_retro(msearch_addr);
   freeaddrinfo_retro(bind_addr);

   return ret;
}

bool natt_device_next(struct natt_discovery *discovery,
   struct natt_device *device)
{
   char    buf[2048];
   ssize_t recvd;
   char    *data;
   size_t  remaining;
   struct sockaddr_storage addr = {0};
   socklen_t addr_size          = sizeof(addr);

   if (!discovery || !device)
      return false;

   if (discovery->fd < 0)
      return false;

   /* This is faster than memsetting the whole thing. */
   memset(&device->addr, 0, sizeof(device->addr));
   memset(&device->ext_addr, 0, sizeof(device->ext_addr));
   *device->desc         = '\0';
   *device->control      = '\0';
   *device->service_type = '\0';
   device->busy          = false;

   recvd = recvfrom(discovery->fd, buf, sizeof(buf), 0,
      (struct sockaddr*)&addr, &addr_size);
   if (recvd < 0)
   {
      /* If there was no data, check for timeout. */
      if (isagain((int)recvd))
         return cpu_features_get_time_usec() < discovery->timeout;

      return false;
   }
   /* Zero-length datagrams are valid, but we can't do anything with them.
      Don't treat them as an error. */
   if (!recvd)
      return true;

   /* Make sure we've an IPv4. */
   if (!addr_6to4(&addr))
      return true;

   memcpy(&device->addr, &addr, sizeof(device->addr));

   /* Parse the data we received.
      We are only looking for the 'Location' HTTP header. */
   data      = buf;
   remaining = (size_t)recvd;
   do
   {
      char *lnbreak = (char*)memchr(data, '\n', remaining);

      if (!lnbreak)
         break;

      *lnbreak++ = '\0';

      /* This also gets rid of any trailing carriage return. */
      string_trim_whitespace(data);

      if (string_starts_with_case_insensitive(data, "Location:"))
      {
         char *location = string_trim_whitespace_left(
            data + STRLEN_CONST("Location:"));

         if (string_starts_with_case_insensitive(location, "http://"))
         {
            /* Make sure the description URL isn't too long. */
            if (strlcpy(device->desc, location, sizeof(device->desc)) <
                  sizeof(device->desc))
               return true;
            *device->desc = '\0';
         }
      }

      remaining -= (size_t)lnbreak - (size_t)data;
      data = lnbreak;
   } while (remaining);

   /* This is not a failure.
      We just don't yet have a valid device to report. */
   return true;
}

void natt_device_end(struct natt_discovery *discovery)
{
   if (discovery)
   {
      if (discovery->fd >= 0)
         socket_close(discovery->fd);

      discovery->fd      = -1;
      discovery->timeout = -1;
   }
}

static bool build_control_url(rxml_node_t *control_url,
   struct natt_device *device)
{
   if (string_is_empty(control_url->data))
      return false;

   /* Do we already have the full url? */
   if (string_starts_with_case_insensitive(control_url->data, "http://"))
   {
      /* Make sure the control URL isn't too long. */
      if (strlcpy(device->control, control_url->data,
         sizeof(device->control)) >= sizeof(device->control))
      {
         *device->control = '\0';
         return false;
      }
   }
   else
   {
      /* We don't have a full url.
         Build one using the desc url. */
      char *control_path;

      strlcpy(device->control, device->desc,
         sizeof(device->control));

      control_path = (char *) strchr(device->control +
         STRLEN_CONST("http://"), '/');

      if (control_path)
         *control_path = '\0';
      if (control_url->data[0] != '/')
         strlcat(device->control, "/", sizeof(device->control));
      /* Make sure the control URL isn't too long. */
      if (strlcat(device->control, control_url->data,
         sizeof(device->control)) >= sizeof(device->control))
      {
         *device->control = '\0';
         return false;
      }
   }

   return true;
}

static bool parse_desc_node(rxml_node_t *node,
   struct natt_device *device)
{
   rxml_node_t *child = node->children;

   if (!child)
      return false;

   /* We only care for services. */
   if (string_is_equal_case_insensitive(node->name, "service"))
   {
      rxml_node_t *service_type = NULL;
      rxml_node_t *control_url  = NULL;

      do
      {
        if (string_is_equal_case_insensitive(child->name, "serviceType"))
           service_type = child;
        else if (string_is_equal_case_insensitive(child->name, "controlURL"))
           control_url  = child;
        if (service_type && control_url)
           break;
      } while ((child = child->next));

      if (!service_type || !control_url)
         return false;

      /* These two are the only IGD service types we can work with. */
      if (!strstr(service_type->data, ":WANIPConnection:") &&
            !strstr(service_type->data, ":WANPPPConnection:"))
         return false;
      if (!build_control_url(control_url, device))
         return false;

      strlcpy(device->service_type, service_type->data,
         sizeof(device->service_type));

      return true;
   }

   /* XML recursion */
   do
   {
      if (parse_desc_node(child, device))
         return true;
   } while ((child = child->next));

   return false;
}

static void natt_query_device_cb(retro_task_t *task, void *task_data,
   void *user_data, const char *error)
{
   char *xml                  = NULL;
   rxml_document_t *document  = NULL;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   struct natt_device *device = (struct natt_device*)user_data;

   *device->control           = '\0';
   *device->service_type      = '\0';

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   xml                        = (char*)malloc(data->len + 1);
   if (!xml)
      goto done;
   memcpy(xml, data->data, data->len);
   xml[data->len] = '\0';

   /* Parse the device's description XML. */
   document = rxml_load_document_string(xml);
   if (document)
   {
      rxml_node_t *root = rxml_root_node(document);
      if (root)
         parse_desc_node(root, device);

      rxml_free_document(document);
   }

   free(xml);

done:
   device->busy = false;
}

bool natt_query_device(struct natt_device *device, bool block)
{
   if (!device)
      return false;

   if (string_is_empty(device->desc))
      return false;

   if (device->busy)
      return false;

   device->busy = true;
   if (!task_push_http_transfer(device->desc,
      true, NULL, natt_query_device_cb, device))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
}

static bool parse_external_address_node(rxml_node_t *node,
   struct natt_device *device)
{
   if (string_is_equal_case_insensitive(node->name, "NewExternalIPAddress"))
   {
      struct addrinfo *addr = NULL;
      struct addrinfo hints = {0};

      if (string_is_empty(node->data))
         return false;

      hints.ai_family = AF_INET;
      if (getaddrinfo_retro(node->data, "0", &hints, &addr))
         return false;
      if (!addr)
         return false;
      memcpy(&device->ext_addr, addr->ai_addr,
         sizeof(device->ext_addr));
      freeaddrinfo_retro(addr);

      return true;
   }
   else
   {
      /* XML recursion */
      rxml_node_t *child = node->children;

      if (child)
      {
         do
         {
            if (parse_external_address_node(child, device))
               return true;
         } while ((child = child->next));
      }
   }

   return false;
}

static void natt_external_address_cb(retro_task_t *task, void *task_data,
   void *user_data, const char *error)
{
   char *xml                  = NULL;
   rxml_document_t *document  = NULL;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   struct natt_device *device = (struct natt_device*)user_data;

   memset(&device->ext_addr, 0, sizeof(device->ext_addr));

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   xml                        = (char*)malloc(data->len + 1);
   if (!xml)
      goto done;
   memcpy(xml, data->data, data->len);
   xml[data->len] = '\0';

   /* Parse the returned external ip address. */
   document = rxml_load_document_string(xml);
   if (document)
   {
      rxml_node_t *root = rxml_root_node(document);
      if (root)
         parse_external_address_node(root, device);

      rxml_free_document(document);
   }

   free(xml);

done:
   device->busy = false;
}

static bool parse_open_port_node(rxml_node_t *node,
   struct natt_request *request)
{
   if (string_is_equal_case_insensitive(node->name, "u:AddPortMappingResponse"))
   {
      request->success = true;

      return true;
   }
   else if (string_is_equal_case_insensitive(node->name, "NewReservedPort"))
   {
      uint16_t ext_port = 0;

      if (string_is_empty(node->data))
         return false;

      sscanf(node->data, "%hu", &ext_port);
      if (!ext_port)
         return false;

      request->addr.sin_port = htons(ext_port);
      request->success = true;

      return true;
   }
   else
   {
      /* XML recursion */
      rxml_node_t *child = node->children;

      if (child)
      {
         do
         {
            if (parse_open_port_node(child, request))
               return true;
         } while ((child = child->next));
      }
   }

   return false;
}

static void natt_open_port_cb(retro_task_t *task, void *task_data,
   void *user_data, const char *error)
{
   char *xml                    = NULL;
   rxml_document_t *document    = NULL;
   http_transfer_data_t *data   = (http_transfer_data_t*)task_data;
   struct natt_request *request = (struct natt_request*)user_data;
   struct natt_device *device   = (struct natt_device*)request->device;

   request->success             = false;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   xml                          = (char*)malloc(data->len + 1);
   if (!xml)
      goto done;
   memcpy(xml, data->data, data->len);
   xml[data->len] = '\0';

   /* Parse the device's port forwarding response. */
   document = rxml_load_document_string(xml);
   if (document)
   {
      rxml_node_t *root = rxml_root_node(document);
      if (root)
         parse_open_port_node(root, request);

      rxml_free_document(document);
   }

   free(xml);

done:
   device->busy = false;
}

static void natt_close_port_cb(retro_task_t *task, void *task_data,
   void *user_data, const char *error)
{
   http_transfer_data_t *data   = (http_transfer_data_t*)task_data;
   struct natt_request *request = (struct natt_request*)user_data;
   struct natt_device *device   = (struct natt_device*)request->device;

   request->success             = false;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   /* We don't need to do anything special here. */
   request->success             = true;

done:
   device->busy                 = false;
}

static bool natt_action(struct natt_device *device,
   const char *action, const char *data, retro_task_callback_t cb,
   struct natt_request *request)
{
   static const char headers_tmpl[] =
      "Content-Type: text/xml\r\n"
      "SOAPAction: \"%s#%s\"\r\n";
   char headers[512];
   void *obj;

   if (string_is_empty(device->control))
      return false;

   snprintf(headers, sizeof(headers), headers_tmpl,
      device->service_type, action);

   if (request)
   {
      request->device = device;
      obj             = request;
   }
   else
      obj             = device;

   return task_push_http_post_transfer_with_headers(device->control,
      data, true, NULL, headers, cb, obj) != NULL;
}

bool natt_external_address(struct natt_device *device, bool block)
{
   static const char tmpl[] =
      "<?xml version=\"1.0\"?>"
      "<s:Envelope "
         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" "
         "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
      ">"
         "<s:Body>"
            "<u:GetExternalIPAddress xmlns:u=\"%s\"/>"
         "</s:Body>"
      "</s:Envelope>";
   char buf[1024];

   if (!device)
      return false;

   snprintf(buf, sizeof(buf), tmpl,
      device->service_type);

   if (device->busy)
      return false;

   device->busy = true;
   if (!natt_action(device, "GetExternalIPAddress", buf,
      natt_external_address_cb, NULL))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
}

bool natt_open_port(struct natt_device *device,
   struct natt_request *request, enum natt_forward_type forward_type,
   bool block)
{
   static const char tmpl[] =
      "<?xml version=\"1.0\"?>"
      "<s:Envelope "
         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" "
         "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
      ">"
         "<s:Body>"
            "<u:%s xmlns:u=\"%s\">"
               "<NewRemoteHost></NewRemoteHost>"
               "<NewExternalPort>%s</NewExternalPort>"
               "<NewProtocol>%s</NewProtocol>"
               "<NewInternalPort>%s</NewInternalPort>"
               "<NewInternalClient>%s</NewInternalClient>"
               "<NewEnabled>1</NewEnabled>"
               "<NewPortMappingDescription>retroarch</NewPortMappingDescription>"
               "<NewLeaseDuration>0</NewLeaseDuration>"
            "</u:%s>"
         "</s:Body>"
      "</s:Envelope>";
   char buf[1280];
   const char *action, *protocol;
   char host[256], port[6];

   if (!device || !request)
      return false;

   if (!request->addr.sin_port)
      return false;

   if (!translate_addr(&request->addr,
         host, sizeof(host), port, sizeof(port)))
      return false;

   action   = (forward_type == NATT_FORWARD_TYPE_ANY) ?
      "AddAnyPortMapping" : "AddPortMapping";
   protocol = (request->proto == SOCKET_PROTOCOL_UDP) ?
      "UDP" : "TCP";
   snprintf(buf, sizeof(buf), tmpl,
      action, device->service_type,
      port, protocol, port, host,
      action);

   if (device->busy)
      return false;

   device->busy = true;
   if (!natt_action(device, action, buf,
      natt_open_port_cb, request))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
}

bool natt_close_port(struct natt_device *device,
   struct natt_request *request, bool block)
{
   static const char tmpl[] =
      "<?xml version=\"1.0\"?>"
      "<s:Envelope "
         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" "
         "xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
      ">"
         "<s:Body>"
            "<u:DeletePortMapping xmlns:u=\"%s\">"
               "<NewRemoteHost></NewRemoteHost>"
               "<NewExternalPort>%s</NewExternalPort>"
               "<NewProtocol>%s</NewProtocol>"
            "</u:DeletePortMapping>"
         "</s:Body>"
      "</s:Envelope>";
   char buf[1024];
   const char *protocol;
   char port[6];

   if (!device || !request)
      return false;

   if (!request->addr.sin_port)
      return false;

   if (!translate_addr(&request->addr,
         NULL, 0, port, sizeof(port)))
      return false;

   protocol = (request->proto == SOCKET_PROTOCOL_UDP) ?
      "UDP" : "TCP";
   snprintf(buf, sizeof(buf), tmpl,
      device->service_type, port, protocol);

   if (device->busy)
      return false;

   device->busy = true;
   if (!natt_action(device, "DeletePortMapping", buf,
      natt_close_port_cb, request))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
}
