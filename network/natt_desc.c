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

#include <string.h>

#include <string/stdstring.h>
#include <compat/strl.h>
#include <formats/rxml.h>

#include "natt.h"
#include "natt_desc.h"

/* Parses one decimal octet, no leading zeros (other parsers read those
 * as octal); returns the character after it or NULL. */
static const char *natt_parse_octet(const char *s, unsigned *octet)
{
   unsigned v = 0;
   unsigned n = 0;

   if (s[0] == '0' && s[1] >= '0' && s[1] <= '9')
      return NULL;

   while (*s >= '0' && *s <= '9')
   {
      if (++n > 3)
         return NULL;
      v = v * 10 + (unsigned)(*s++ - '0');
   }

   if (!n || v > 255)
      return NULL;

   *octet = v;
   return s;
}

bool natt_url_matches_device(const char *url,
      const struct natt_device *device)
{
   const unsigned char *addr;
   const char *s;
   unsigned octet;
   int i;

   if (   !url || !device || device->addr.sin_family != AF_INET
       || !string_starts_with_case_insensitive(url, "http://"))
      return false;

   /* Nothing that could end the request line or a header. */
   for (s = url; *s; s++)
      if ((unsigned char)*s <= 0x20 || *s == 0x7f || *s == '#')
         return false;

   /* The host is the numeric IPv4 address the SSDP reply came from;
    * sin_addr is in network order, so its bytes are the octets. */
   addr = (const unsigned char *)&device->addr.sin_addr;
   s    = url + STRLEN_CONST("http://");
   for (i = 0; i < 4; i++)
   {
      if (i && *s++ != '.')
         return false;
      if (!(s = natt_parse_octet(s, &octet)) || octet != addr[i])
         return false;
   }

   if (*s == ':')
   {
      unsigned long port = 0;
      unsigned      n    = 0;

      for (s++; *s >= '0' && *s <= '9'; s++)
      {
         if (++n > 5)
            return false;
         port = port * 10 + (unsigned long)(*s - '0');
      }

      if (!port || port > 65535)
         return false;
   }

   return !*s || *s == '/' || *s == '?';
}

bool natt_service_type_valid(const char *service_type)
{
   static const char ip[]  =
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

   while (*version >= '0' && *version <= '9')
      version++;

   return !*version;
}

static bool natt_build_control_url(
      rxml_node_t *control_url,
      struct natt_device *device)
{
   if (!control_url->data || !*control_url->data)
      return false;

   if (string_starts_with_case_insensitive(control_url->data, "http://"))
   {
      if (strlcpy(device->control, control_url->data,
            sizeof(device->control)) >= sizeof(device->control))
         goto error;
   }
   else
   {
      /* A relative URL is resolved against the description URL's
       * scheme and authority. */
      char *control_path;

      if (strlcpy(device->control, device->desc,
            sizeof(device->control)) >= sizeof(device->control))
         goto error;

      if ((control_path = strpbrk(device->control +
            STRLEN_CONST("http://"), "/?")))
         *control_path = '\0';
      if (     control_url->data[0] != '/'
            && strlcat(device->control, "/",
               sizeof(device->control)) >= sizeof(device->control))
         goto error;
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
         if (     natt_service_type_valid(service_type->data)
               && strlcpy(device->service_type, service_type->data,
                  sizeof(device->service_type)) < sizeof(device->service_type))
         {
            if (natt_build_control_url(control_url, device))
               return true;
         }
         *device->service_type = '\0';
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
