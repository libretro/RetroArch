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

#ifndef __NATT_DESC_H
#define __NATT_DESC_H

#include <boolean.h>
#include <formats/rxml.h>

/* The UPnP IGD description-XML walk, split out of natt.c so the regression
 * test samples/tasks/natt_desc/natt_desc_parse_test.c can drive the real
 * parser with hand-built (and deliberately hostile) rxml trees. The input
 * is an XML document served by a router on the LAN -- untrusted -- so the
 * walk must never crash on an empty/leaf/NULL-data tree, and must find a
 * <service> however deeply it nests. */

struct natt_device; /* full definition in natt.h */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * natt_parse_desc_node:
 * @node   : a node of the parsed IGD description document (may be NULL).
 * @device : the device whose control URL + service type are filled in on a
 *           successful match.
 *
 * Recursively searches @node and its descendants for a <service> whose
 * serviceType is a WANIPConnection / WANPPPConnection and whose controlURL
 * can be resolved, binding the first such service into @device. Returns true
 * when a service was bound, false otherwise. Safe on a NULL node, a leaf
 * node, and a <service> whose serviceType/controlURL children carry no data.
 */
bool natt_parse_desc_node(rxml_node_t *node, struct natt_device *device);

/* Returns true only for an HTTP URL whose numeric IPv4 host is the
 * address from which the device's SSDP response was received. */
bool natt_url_matches_device(const char *url,
      const struct natt_device *device);

/* Accept only the two service URNs used by this client, with a decimal
 * version suffix and no trailing data. */
bool natt_service_type_valid(const char *service_type);

#ifdef __cplusplus
}
#endif

#endif
