/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_upnp.h).
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

#ifndef __LIBRETRO_SDK_NET_UPNP_H
#define __LIBRETRO_SDK_NET_UPNP_H

/* UPnP Internet Gateway Device client: find the router (SSDP), learn
 * its external address and forward ports through it.
 *
 * The operations block on the network, for up to the timeout given,
 * so they belong on a worker thread. The parsers are plain functions
 * over buffers. */

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define NET_UPNP_URL_MAX 512

struct net_upnp_gateway
{
   char control_url[NET_UPNP_URL_MAX]; /* WAN connection service control */
   char service_type[128];             /* its urn:...:WANIPConnection:n  */
   char lan_addr[64];                  /* this host's address towards it */
   char desc_url[NET_UPNP_URL_MAX];    /* its device description         */
};

/* Searches for a gateway for up to timeout_ms. Returns true and fills
 * gw for the first one with a WANIPConnection or WANPPPConnection
 * service. */
bool net_upnp_discover(struct net_upnp_gateway *gw, int timeout_ms);

/* The gateway whose device description is at desc_url, as discovery
 * found it before (gw->desc_url), without searching again. Returns true
 * and fills gw when the description still names a WAN connection
 * service. */
bool net_upnp_gateway_from_url(struct net_upnp_gateway *gw,
      const char *desc_url, int timeout_ms);

/* The gateway's external IPv4 address, dotted, into s. */
bool net_upnp_external_address(const struct net_upnp_gateway *gw,
      char *s, size_t len, int timeout_ms);

/* Forwards external port to the same port on gw->lan_addr, for UDP or
 * TCP, without a lease time. */
bool net_upnp_add_port_mapping(const struct net_upnp_gateway *gw,
      unsigned port, bool udp, const char *description, int timeout_ms);
/* As net_upnp_add_port_mapping, from external_port to internal_port,
 * for a caller that tries other external ports when one is taken. */
bool net_upnp_add_port_mapping_ext(const struct net_upnp_gateway *gw,
      unsigned external_port, unsigned internal_port, bool udp,
      const char *description, int timeout_ms);
bool net_upnp_delete_port_mapping(const struct net_upnp_gateway *gw,
      unsigned port, bool udp, int timeout_ms);

/* The LOCATION header of an SSDP response, into s. */
bool net_upnp_ssdp_location(const char *msg, size_t len, char *s, size_t slen);

/* The WAN connection service of a device description fetched from
 * desc_url: its control URL, made absolute, and its service type. */
bool net_upnp_parse_description(const char *xml, const char *desc_url,
      struct net_upnp_gateway *gw);

/* NewExternalIPAddress from a GetExternalIPAddress response, into s. */
bool net_upnp_parse_external_address(const char *xml, char *s, size_t len);

RETRO_END_DECLS

#endif
