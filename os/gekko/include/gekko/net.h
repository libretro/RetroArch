/* Wii: the network through IOS.  The BSD socket calls (sys/socket.h,
 * poll.h, netdb.h, arpa/inet.h) bring it up on first use too. */

#ifndef GEKKO_NET_H
#define GEKKO_NET_H

#include <gekko/gekko.h>

/* Brings the interface up and waits for an address (DHCP) up to
 * timeout_ms; 0, or a negative errno.  Later calls return at once. */
int gk_net_init(unsigned timeout_ms);

/* This console's IPv4 address, network order; 0 while it has none. */
uint32_t gk_net_address(void);

#endif
