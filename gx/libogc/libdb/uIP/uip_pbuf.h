#ifndef __UIP_PBUF_H__
#define __UIP_PBUF_H__

#include "uip.h"

/* Definitions for the pbuf flag field. These are NOT the flags that
 * are passed to pbuf_alloc(). */
#define UIP_PBUF_FLAG_RAM   0x00U    /* Flags that pbuf data is stored in RAM */
#define UIP_PBUF_FLAG_ROM   0x01U    /* Flags that pbuf data is stored in ROM */
#define UIP_PBUF_FLAG_POOL  0x02U    /* Flags that the pbuf comes from the pbuf pool */
#define UIP_PBUF_FLAG_REF   0x04U    /* Flags thet the pbuf payload refers to RAM */

typedef enum {
	UIP_PBUF_TRANSPORT,
	UIP_PBUF_IP,
	UIP_PBUF_LINK,
	UIP_PBUF_RAW
} uip_pbuf_layer;

typedef enum {
	UIP_PBUF_POOL,
	UIP_PBUF_RAM,
	UIP_PBUF_ROM,
	UIP_PBUF_REF
} uip_pbuf_flag;

struct uip_pbuf {
	struct uip_pbuf *next;
	void *payload;
	u16_t tot_len;
	u16_t len;
	u16_t flags;
	u16_t ref;
};

void uip_pbuf_init();
struct uip_pbuf* uip_pbuf_alloc(uip_pbuf_layer layer,u16_t len,uip_pbuf_flag flag);
u8_t uip_pbuf_free(struct uip_pbuf *p);
void uip_pbuf_realloc(struct uip_pbuf *p,u16_t new_len);
u8_t uip_pbuf_header(struct uip_pbuf *p,s16_t hdr_size_inc);
void uip_pbuf_cat(struct uip_pbuf *h,struct uip_pbuf *t);
u8_t uip_pbuf_clen(struct uip_pbuf *p);
void uip_pbuf_queue(struct uip_pbuf *p,struct uip_pbuf *n);
void uip_pbuf_ref(struct uip_pbuf *p);
void uip_pbuf_chain(struct uip_pbuf *h,struct uip_pbuf *t);
struct uip_pbuf* uip_pbuf_dequeue(struct uip_pbuf *p);
struct uip_pbuf* uip_pbuf_dechain(struct uip_pbuf *p);

#endif
