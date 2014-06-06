#ifndef __BBA_DBG_H__
#define __BBA_DBG_H__

#include "uip.h"

struct uip_netif;

typedef void* uipdev_s;

uipdev_s uip_bba_create(struct uip_netif *dev);
s8_t uip_bba_init(struct uip_netif *dev);
void uip_bba_poll(struct uip_netif *dev);

#endif
