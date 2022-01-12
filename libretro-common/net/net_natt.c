/* Copyright  (C) 2016-2022 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_natt.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#if !defined(HAVE_SOCKET_LEGACY) && defined(_WIN32) && defined(_MSC_VER)
#pragma comment(lib, "Iphlpapi")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <formats/rxml.h>
#include <features/features_cpu.h>
#include <retro_miscellaneous.h>

#include <string/stdstring.h>

#include "../../tasks/tasks_internal.h"

#include <net/net_natt.h>

#if !defined(HAVE_SOCKET_LEGACY) && defined(_WIN32)
#include <iphlpapi.h>
#endif

static natt_state_t natt_st = {0, {0}, {{0}}, -1};

natt_state_t *natt_state_get_ptr(void)
{
   return &natt_st;
}

bool natt_init(void)
{
#ifndef HAVE_SOCKET_LEGACY
   static const char msearch[] =
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: 239.255.255.250:1900\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
      "MX: 5\r\n"
      "\r\n";
   static struct sockaddr_in msearch_addr = {0};
#ifdef _WIN32
   MIB_IPFORWARDROW ip_forward;
#endif
   natt_state_t *st                       = &natt_st;
   struct addrinfo *bind_addr             = NULL;

   if (msearch_addr.sin_family != AF_INET)
   {
      struct addrinfo *addr = NULL;
      struct addrinfo hints = {0};

      hints.ai_family   = AF_INET;
      hints.ai_socktype = SOCK_DGRAM;
      if (getaddrinfo_retro("239.255.255.250", "1900", &hints, &addr))
         return false;
      if (!addr)
         return false;
      memcpy(&msearch_addr, addr->ai_addr, sizeof(msearch_addr));
      freeaddrinfo_retro(addr);
   }

   if (!net_ifinfo_new(&st->interfaces))
      goto failure;
   if (!st->interfaces.size)
      goto failure;

   st->fd = socket_init((void **) &bind_addr, 0, NULL, SOCKET_TYPE_DATAGRAM);
   if (st->fd < 0)
      goto failure;
   if (!bind_addr)
      goto failure;

#ifdef _WIN32
   if (GetBestRoute(inet_addr("223.255.255.255"),
      0, &ip_forward) == NO_ERROR)
   {
      DWORD            index = ip_forward.dwForwardIfIndex;
      PMIB_IPADDRTABLE table = malloc(sizeof(*table));

      if (table)
      {
         DWORD len    = sizeof(*table);
         DWORD result = GetIpAddrTable(table, &len, FALSE);

         if (result == ERROR_INSUFFICIENT_BUFFER)
         {
            PMIB_IPADDRTABLE new_table = realloc(table, len);

            if (new_table) 
            {
               table  = new_table;
               result = GetIpAddrTable(table, &len, FALSE);
            }
         }

         if (result == NO_ERROR)
         {
            DWORD i;

            for (i = 0; i < table->dwNumEntries; i++)
            {
               PMIB_IPADDRROW ip_addr = &table->table[i];

               if (ip_addr->dwIndex == index)
               {
#ifdef IP_MULTICAST_IF
                  setsockopt(st->fd, IPPROTO_IP, IP_MULTICAST_IF,
                     (const char *) &ip_addr->dwAddr, sizeof(ip_addr->dwAddr));
#endif
                  ((struct sockaddr_in *) bind_addr->ai_addr)->sin_addr.s_addr =
                     ip_addr->dwAddr;
                  break;
               }
            }
         }

         free(table);
      }
   }
#endif

#ifdef IP_MULTICAST_TTL
   {
#ifdef _WIN32
      unsigned long ttl = 2;
      if (setsockopt(st->fd, IPPROTO_IP, IP_MULTICAST_TTL,
         (const char *)&ttl, sizeof(ttl)) < 0) { }
#else
      unsigned char ttl = 2;
      if (setsockopt(st->fd, IPPROTO_IP, IP_MULTICAST_TTL,
         &ttl, sizeof(ttl)) < 0) { }
#endif
   }
#endif

   if (!socket_bind(st->fd, bind_addr))
      goto failure;

   /* Broadcast a discovery request. */
   if (sendto(st->fd, msearch, STRLEN_CONST(msearch), 0,
         (struct sockaddr *) &msearch_addr,
         sizeof(msearch_addr)) != STRLEN_CONST(msearch))
      goto failure;

   if (!socket_nonblock(st->fd))
      goto failure;

   /* 5 seconds */
   st->timeout = cpu_features_get_time_usec() + 5000000;

   freeaddrinfo_retro(bind_addr);

   return true;

failure:
   /* Failed to broadcast. */
   freeaddrinfo_retro(bind_addr);
   natt_deinit();
#endif

   return false;
}

void natt_deinit(void)
{
#ifndef HAVE_SOCKET_LEGACY
   natt_state_t *st = &natt_st;

   natt_device_end();
   natt_interfaces_destroy();

   /* This is faster than memsetting the whole thing. */
   *st->device.desc         = '\0';
   *st->device.control      = '\0';
   *st->device.service_type = '\0';
   memset(&st->device.addr, 0, sizeof(st->device.addr));
   memset(&st->device.ext_addr, 0, sizeof(st->device.ext_addr));
   st->device.busy          = false;
#endif
}

void natt_interfaces_destroy(void)
{
#ifndef HAVE_SOCKET_LEGACY
   natt_state_t *st = &natt_st;

   net_ifinfo_free(&st->interfaces);
   memset(&st->interfaces, 0, sizeof(st->interfaces));
#endif
}

bool natt_device_next(struct natt_device *device)
{
#ifndef HAVE_SOCKET_LEGACY
   fd_set  fds;
   char    buf[2048];
   ssize_t recvd;
   char    *data;
   size_t  remaining;
   struct timeval tv   = {0};
   socklen_t addr_size = sizeof(device->addr);
   natt_state_t *st    = &natt_st;

   if (!device)
      return false;

   if (st->fd < 0)
      return false;

   /* This is faster than memsetting the whole thing. */
   *device->desc         = '\0';
   *device->control      = '\0';
   *device->service_type = '\0';
   memset(&device->addr, 0, sizeof(device->addr));
   memset(&device->ext_addr, 0, sizeof(device->ext_addr));
   device->busy          = false;

   /* Check our file descriptor to see if a device sent data to it. */
   FD_ZERO(&fds);
   FD_SET(st->fd, &fds);
   if (socket_select(st->fd + 1, &fds, NULL, NULL, &tv) < 0)
      return false;
   /* If there was no data, check for timeout. */
   if (!FD_ISSET(st->fd, &fds))
      return cpu_features_get_time_usec() < st->timeout;

   recvd = recvfrom(st->fd, buf, sizeof(buf), 0,
      (struct sockaddr *) &device->addr, &addr_size);
   if (recvd <= 0)
      return false;

   /* Parse the data we received.
      We are only looking for the 'Location' HTTP header. */
   data      = buf;
   remaining = (size_t) recvd;
   do
   {
      char *lnbreak = (char *) memchr(data, '\n', remaining);
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
#else
   return false;
#endif
}

void natt_device_end(void)
{
#ifndef HAVE_SOCKET_LEGACY
   natt_state_t *st = &natt_st;

   if (st->fd >= 0)
   {
      socket_close(st->fd);
      st->fd = -1;
   }
#endif
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
   char *xml;
   rxml_document_t *document;
   http_transfer_data_t *data = task_data;
   struct natt_device *device = user_data;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   xml = malloc(data->len + 1);
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
#ifndef HAVE_SOCKET_LEGACY
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
#else
   return false;
#endif
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

      do
      {
         if (parse_external_address_node(child, device))
            return true;
      } while ((child = child->next));
   }

   return false;
}

static void natt_external_address_cb(retro_task_t *task, void *task_data,
   void *user_data, const char *error)
{
   char *xml;
   rxml_document_t *document;
   http_transfer_data_t *data = task_data;
   struct natt_device *device = user_data;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   xml = malloc(data->len + 1);
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
      memcpy(&request->addr.sin_addr, &request->device->ext_addr.sin_addr,
         sizeof(request->addr.sin_addr));

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

      request->success = true;
      request->addr.sin_port = htons(ext_port);
      memcpy(&request->addr.sin_addr, &request->device->ext_addr.sin_addr,
         sizeof(request->addr.sin_addr));

      return true;
   }
   else
   {
      /* XML recursion */
      rxml_node_t *child = node->children;

      do
      {
         if (parse_open_port_node(child, request))
            return true;
      } while ((child = child->next));
   }

   return false;
}

static void natt_open_port_cb(retro_task_t *task, void *task_data,
   void *user_data, const char *error)
{
   char *xml;
   rxml_document_t *document;
   http_transfer_data_t *data   = task_data;
   struct natt_request *request = user_data;
   struct natt_device *device   = request->device;

   request->success = false;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   xml = malloc(data->len + 1);
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
   http_transfer_data_t *data   = task_data;
   struct natt_request *request = user_data;
   struct natt_device *device   = request->device;

   request->success = false;

   if (error)
      goto done;
   if (!data || !data->data || !data->len)
      goto done;
   if (data->status != 200)
      goto done;

   /* We don't need to do anything special here.
    * Just clear up the request. */
   memset(request, 0, sizeof(*request));
   request->success = true;

done:
   device->busy = false;
}

static bool natt_action(struct natt_device *device,
   const char *action, const char *data, retro_task_callback_t cb,
   struct natt_request *request)
{
   static const char headers_template[] =
      "Content-Type: text/xml\r\n"
      "SOAPAction: \"%s#%s\"\r\n";
   char headers[512];
   void *obj;

   snprintf(headers, sizeof(headers), headers_template,
      device->service_type, action);

   if (request)
   {
      request->device = device;
      obj = request;
   }
   else
      obj = device;

   return task_push_http_post_transfer_with_headers(device->control,
      data, true, NULL, headers, cb, obj) != NULL;
}

bool natt_external_address(struct natt_device *device, bool block)
{
#ifndef HAVE_SOCKET_LEGACY
   static const char template[] =
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

   if (string_is_empty(device->control))
      return false;

   if (device->busy)
      return false;
   device->busy = true;

   snprintf(buf, sizeof(buf), template,
      device->service_type);

   if (!natt_action(device, "GetExternalIPAddress", buf,
         natt_external_address_cb, NULL))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
#else
   return false; 
#endif
}

bool natt_open_port(struct natt_device *device,
   struct natt_request *request, enum natt_forward_type forward_type,
   bool block)
{
#ifndef HAVE_SOCKET_LEGACY
   static const char template[] =
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
   const char *action;
   char host[256], port[6];

   if (!device || !request)
      return false;

   if (string_is_empty(device->control))
      return false;

   if (device->ext_addr.sin_family != AF_INET)
      return false;

   if (!request->addr.sin_port)
      return false;

   if (getnameinfo((struct sockaddr *) &request->addr,
         sizeof(request->addr), host, sizeof(host),
         port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV))
      return false;

   if (device->busy)
      return false;
   device->busy = true;

   action = (forward_type == NATT_FORWARD_TYPE_ANY) ?
      "AddAnyPortMapping" : "AddPortMapping";

   snprintf(buf, sizeof(buf), template,
      action, device->service_type, port,
      (request->proto == SOCKET_PROTOCOL_UDP) ?
         "UDP" : "TCP",
      port, host,
      action);

   if (!natt_action(device, action, buf,
         natt_open_port_cb, request))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
#else
   return false; 
#endif
}

bool natt_close_port(struct natt_device *device,
   struct natt_request *request, bool block)
{
#ifndef HAVE_SOCKET_LEGACY
   static const char template[] =
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
   char port[6];

   if (!device || !request)
      return false;

   if (string_is_empty(device->control))
      return false;

   if (device->ext_addr.sin_family != AF_INET)
      return false;

   if (!request->addr.sin_port)
      return false;

   if (getnameinfo((struct sockaddr *) &request->addr,
         sizeof(request->addr), NULL, 0,
         port, sizeof(port), NI_NUMERICSERV))
      return false;

   if (device->busy)
      return false;
   device->busy = true;

   snprintf(buf, sizeof(buf), template,
      device->service_type, port,
      (request->proto == SOCKET_PROTOCOL_UDP) ?
         "UDP" : "TCP");

   if (!natt_action(device, "DeletePortMapping", buf,
         natt_close_port_cb, request))
   {
      device->busy = false;
      return false;
   }

   if (block)
      task_queue_wait(NULL, NULL);

   return true;
#else
   return false; 
#endif
}
