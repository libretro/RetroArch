/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (net_natt.h).
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

#ifndef _LIBRETRO_SDK_NET_NATT_H
#define _LIBRETRO_SDK_NET_NATT_H

#include <net/net_compat.h>
#include <net/net_socket.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct natt_status
{
   /** nfds for select when checking for input */
   int nfds;

   /** The fdset to be selected upon to check for responses */
   fd_set fds;

   /** True if there might be a request outstanding */
   bool request_outstanding;

   /** True if we've resolved an external IPv4 address */
   bool have_inet4;

   /** External IPv4 address */
   struct sockaddr_in ext_inet4_addr;

   /** True if we've resolved an external IPv6 address */
   bool have_inet6;

#if defined(AF_INET6) && !defined(HAVE_SOCKET_LEGACY)
   /** External IPv6 address */
   struct sockaddr_in6 ext_inet6_addr;
#endif

   /** Internal status (currently unused) */
   void *internal;
};

/**
 * Initialize global NAT traversal structures (must be called once to use other
 * functions) */
void natt_init(void);

/** Initialize a NAT traversal status object */
bool natt_new(struct natt_status *status);

/** Free a NAT traversal status object */
void natt_free(struct natt_status *status);

/**
 * Make a port forwarding request when only the port is known. Forwards any
 * address it can find. */
bool natt_open_port_any(struct natt_status *status, uint16_t port,
   enum socket_protocol proto);

/** Check for port forwarding responses */
bool natt_read(struct natt_status *status);

RETRO_END_DECLS

#endif
