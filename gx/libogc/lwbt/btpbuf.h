#ifndef __BTPBUF_H__
#define __BTPBUF_H__

#include "bt.h"

/* Definitions for the pbuf flag field. These are NOT the flags that
 * are passed to pbuf_alloc(). */
#define PBUF_FLAG_RAM   0x00U    /* Flags that pbuf data is stored in RAM */
#define PBUF_FLAG_ROM   0x01U    /* Flags that pbuf data is stored in ROM */
#define PBUF_FLAG_POOL  0x02U    /* Flags that the pbuf comes from the pbuf pool */
#define PBUF_FLAG_REF   0x04U    /* Flags thet the pbuf payload refers to RAM */

typedef enum {
	PBUF_TRANSPORT,
	PBUF_LINK,
	PBUF_RAW
} pbuf_layer;

typedef enum {
	PBUF_POOL,
	PBUF_RAM,
	PBUF_ROM,
	PBUF_REF
} pbuf_flag;

struct pbuf {
	struct pbuf *next;
	void *payload;
	u16_t tot_len;
	u16_t len;
	u16_t flags;
	u16_t ref;
};

void btpbuf_init();
struct pbuf* btpbuf_alloc(pbuf_layer layer,u16_t len,pbuf_flag flag);
u8_t btpbuf_free(struct pbuf *p);
void btpbuf_realloc(struct pbuf *p,u16_t new_len);
u8_t btpbuf_header(struct pbuf *p,s16_t hdr_size_inc);
void btpbuf_cat(struct pbuf *h,struct pbuf *t);
u8_t btpbuf_clen(struct pbuf *p);
void btpbuf_queue(struct pbuf *p,struct pbuf *n);
void btpbuf_ref(struct pbuf *p);
void btpbuf_chain(struct pbuf *h,struct pbuf *t);
struct pbuf* btpbuf_dequeue(struct pbuf *p);
struct pbuf* btpbuf_dechain(struct pbuf *p);
struct pbuf* btpbuf_take(struct pbuf *p);

#endif
