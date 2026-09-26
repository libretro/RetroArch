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

static bool natt_build_control_url(
      rxml_node_t *control_url,
      struct natt_device *device)
{
   if (!control_url->data || !*control_url->data)
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
      size_t _len = strlcpy(device->control, device->desc,
         sizeof(device->control));

      control_path = (char *)strchr(device->control +
         STRLEN_CONST("http://"), '/');

      if (control_path)
         *control_path = '\0';
      if (control_url->data[0] != '/')
         strlcpy_lit(device->control + _len, "/",
               sizeof(device->control) - _len);
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

bool natt_parse_desc_node(rxml_node_t *node,
      struct natt_device *device)
{
   rxml_node_t *child;

   if (!node)
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
         if (  strstr(service_type->data, ":WANIPConnection:")
            || strstr(service_type->data, ":WANPPPConnection:"))
         {
            if (natt_build_control_url(control_url, device))
            {
               strlcpy(device->service_type, service_type->data,
                  sizeof(device->service_type));
               return true;
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
