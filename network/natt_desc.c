/*  RetroArch - A frontend for libretro.
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
#include <string.h>

#include <string/stdstring.h>
#include <compat/strl.h>
#include <formats/rxml.h>

#include "natt.h"
#include "natt_desc.h"

bool natt_url_matches_device(const char *url,
      const struct natt_device *device)
{
   const char *host;
   const char *host_end;
   const char *scan;
   char expected[16];
   size_t host_len;

   if (   !url || !device || device->addr.sin_family != AF_INET
       || strncmp(url, "http://", STRLEN_CONST("http://")) != 0)
      return false;

   /* Keep control characters out of the request target and headers. */
   for (scan = url; *scan; scan++)
      if ((unsigned char)*scan <= 0x20 || *scan == 0x7f || *scan == '#')
         return false;

   host     = url + STRLEN_CONST("http://");
   host_end = host;
   while (   *host_end && *host_end != ':'
          && *host_end != '/' && *host_end != '?')
      host_end++;

   host_len = (size_t)(host_end - host);
   if (   !inet_ntop(AF_INET, &device->addr.sin_addr, expected,
            (socklen_t)sizeof(expected))
       || strlen(expected) != host_len
       || memcmp(host, expected, host_len) != 0)
      return false;

   if (*host_end == ':')
   {
      char *port_end;
      unsigned long port;

      if (host_end[1] < '0' || host_end[1] > '9')
         return false;
      port = strtoul(host_end + 1, &port_end, 10);
      if (!port || port > 65535)
         return false;
      host_end = port_end;
   }

   return !*host_end || *host_end == '/' || *host_end == '?';
}

static bool natt_build_control_url(
      rxml_node_t *control_url,
      struct natt_device *device)
{
   if (!control_url->data || !*control_url->data)
      return false;

   /* Do we already have the full url? */
   if (string_starts_with(control_url->data, "http://"))
   {
      /* Make sure the control URL isn't too long. */
      if (strlcpy(device->control, control_url->data,
         sizeof(device->control)) >= sizeof(device->control))
         goto error;
   }
   else
   {
      /* We don't have a full url.
         Build one using the desc url. */
      char *control_path;

      if (strlcpy(device->control, device->desc,
            sizeof(device->control)) >= sizeof(device->control))
         goto error;

      control_path = (char *)strpbrk(device->control +
         STRLEN_CONST("http://"), "/?");

      if (control_path)
         *control_path = '\0';
      if (control_url->data[0] != '/')
      {
         if (strlcat(device->control, "/", sizeof(device->control)) >=
               sizeof(device->control))
            goto error;
      }
      /* Make sure the control URL isn't too long. */
      if (strlcat(device->control, control_url->data,
         sizeof(device->control)) >= sizeof(device->control))
         goto error;
   }

   if (natt_url_matches_device(device->control, device))
      return true;

error:
   *device->control = '\0';
   return false;
}

bool natt_service_type_valid(const char *service_type)
{
   static const char ip[] =
      "urn:schemas-upnp-org:service:WANIPConnection:";
   static const char ppp[] =
      "urn:schemas-upnp-org:service:WANPPPConnection:";
   const char *version;

   if (!service_type)
      return false;

   if (string_starts_with(service_type, ip))
      version = service_type + STRLEN_CONST(ip);
   else if (string_starts_with(service_type, ppp))
      version = service_type + STRLEN_CONST(ppp);
   else
      return false;

   if (!*version)
      return false;

   version += strspn(version, "0123456789");

   return *version == '\0';
}

bool natt_parse_desc_node(rxml_node_t *node,
      struct natt_device *device)
{
   rxml_node_t *child;

   if (!node || !device)
      return false;

   /* If this node is a <service>, look for serviceType + controlURL
    * among its children and try to bind them. */
   if (string_is_equal_case_insensitive(node->name, "service"))
   {
      rxml_node_t *service_type = NULL;
      rxml_node_t *control_url  = NULL;

      for (child = node->children; child; child = child->next)
      {
         if (string_is_equal_case_insensitive(child->name, "serviceType"))
            service_type = child;
         else if (string_is_equal_case_insensitive(child->name, "controlURL"))
            control_url  = child;
         if (service_type && control_url)
            break;
      }

      if (   service_type && service_type->data
         && control_url  && control_url->data)
      {
         /* These two are the only IGD service types we can work with. */
         if (natt_service_type_valid(service_type->data))
         {
            if (natt_build_control_url(control_url, device))
            {
               if (strlcpy(device->service_type, service_type->data,
                     sizeof(device->service_type)) <
                     sizeof(device->service_type))
                  return true;

               *device->control      = '\0';
               *device->service_type = '\0';
            }
         }
      }
   }

   /* Always recurse into children; UPnP descriptions nest <service>
    * inside <serviceList> inside <device> inside <root>, so the parent
    * node we were called on is rarely the <service> itself. */
   for (child = node->children; child; child = child->next)
      if (natt_parse_desc_node(child, device))
         return true;

   return false;
}
