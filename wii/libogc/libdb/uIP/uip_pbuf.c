#include <stdlib.h>
#include <string.h>

#include "memb.h"
#include "memr.h"
#include "uip_pbuf.h"

#if UIP_LOGGING == 1
#include <stdio.h>
#define UIP_LOG(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

#if UIP_ERRORING == 1
#include <stdio.h>
#define UIP_ERROR(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_ERROR(m)
#endif /* UIP_ERRORING == 1 */

#if UIP_STATISTICS == 1
struct uip_stats uip_stat;
#define UIP_STAT(s) s
#else
#define UIP_STAT(s)
#endif /* UIP_STATISTICS == 1 */

MEMB(uip_pool_pbufs,sizeof(struct uip_pbuf)+UIP_PBUF_POOL_BUFSIZE,UIP_PBUF_POOL_NUM);
MEMB(uip_rom_pbufs,sizeof(struct uip_pbuf),UIP_PBUF_ROM_NUM);

void uip_pbuf_init()
{
	memb_init(&uip_pool_pbufs);
	memb_init(&uip_rom_pbufs);
}

struct uip_pbuf* uip_pbuf_alloc(uip_pbuf_layer layer,u16_t len,uip_pbuf_flag flag)
{
	u16_t offset;
	s32_t rem_len;
	struct uip_pbuf *p,*q,*r;

	offset = 0;
	switch(layer)	 {
		case UIP_PBUF_TRANSPORT:
			offset += UIP_TRANSPORT_HLEN;
		case UIP_PBUF_IP:
			offset += UIP_IP_HLEN;
		case UIP_PBUF_LINK:
			offset += UIP_LL_HLEN;
			break;
		case UIP_PBUF_RAW:
			break;
		default:
			UIP_ERROR("uip_pbuf_alloc: bad pbuf layer.\n");
			return NULL;
	}

	switch(flag) {
		case UIP_PBUF_POOL:
			p = memb_alloc(&uip_pool_pbufs);
			if(p==NULL) {
				UIP_ERROR("uip_pbuf_alloc: couldn't allocate pbuf(p) from pool\n");
				return NULL;
			}

			p->next = NULL;
			p->payload = MEM_ALIGN((void*)((u8_t*)p+(sizeof(struct uip_pbuf)+offset)));
			p->tot_len = len;
			p->len = (len>(UIP_PBUF_POOL_BUFSIZE-offset)?(UIP_PBUF_POOL_BUFSIZE-offset):len);
			p->flags = UIP_PBUF_FLAG_POOL;
			p->ref = 1;

			r = p;
			rem_len = len - p->len;
			while(rem_len>0) {
				q = memb_alloc(&uip_pool_pbufs);
				if(q==NULL) {
					UIP_ERROR("uip_pbuf_alloc: couldn't allocate pbuf(q) from pool\n");
					uip_pbuf_free(p);
					return NULL;
				}

				q->next = NULL;
				r->next = q;
				q->tot_len = rem_len;
				q->len = (rem_len>UIP_PBUF_POOL_BUFSIZE?UIP_PBUF_POOL_BUFSIZE:rem_len);
				q->payload = (void*)((u8_t*)q+sizeof(struct uip_pbuf));
				q->flags = UIP_PBUF_FLAG_POOL;
				q->ref = 1;

				rem_len -= q->len;
				r = q;
			}
			break;
		case UIP_PBUF_RAM:
			p = memr_malloc(MEM_ALIGN_SIZE(sizeof(struct uip_pbuf)+offset)+MEM_ALIGN_SIZE(len));
			if(p==NULL) {
				UIP_ERROR("uip_pbuf_alloc: couldn't allocate pbuf from ram\n");
				return NULL;
			}
			p->payload = MEM_ALIGN((u8_t*)p+sizeof(struct uip_pbuf)+offset);
			p->len = p->tot_len = len;
			p->next = NULL;
			p->flags = UIP_PBUF_FLAG_RAM;
			break;
		case UIP_PBUF_ROM:
		case UIP_PBUF_REF:
			p = memb_alloc(&uip_rom_pbufs);
			if(p==NULL) {
				UIP_ERROR("uip_pbuf_alloc: couldn't allocate pbuf from rom/ref\n");
				return NULL;
			}
			p->payload = NULL;
			p->next = NULL;
			p->len = p->tot_len = len;
			p->flags = (flag==UIP_PBUF_ROM?UIP_PBUF_FLAG_ROM:UIP_PBUF_FLAG_REF);
			break;
		default:
			UIP_ERROR("uip_pbuf_alloc: bad flag value.\n");
			return NULL;
	}

	p->ref = 1;
	return p;
}

u8_t uip_pbuf_free(struct uip_pbuf *p)
{
	u8_t cnt;
	struct uip_pbuf *q;

	if(p==NULL) return 0;

	cnt = 0;
	while(p!=NULL) {
		p->ref--;
		if(p->ref==0) {
			q = p->next;
			if(p->flags==UIP_PBUF_FLAG_POOL) {
				memb_free(&uip_pool_pbufs,p);
			} else if(p->flags==UIP_PBUF_FLAG_ROM || p->flags==UIP_PBUF_FLAG_REF) {
				memb_free(&uip_rom_pbufs,p);
			} else {
				memr_free(p);
			}
			cnt++;
			p = q;
		} else
			p = NULL;
	}
	return cnt;
}

void uip_pbuf_realloc(struct uip_pbuf *p,u16_t new_len)
{
	u16_t rem_len;
	s16_t grow;
	struct uip_pbuf *q;

	if(new_len>=p->tot_len) return;

	grow = new_len - p->tot_len;
	rem_len = new_len;
	q = p;
	while(rem_len>q->len) {
		rem_len -= q->len;
		q->tot_len += grow;
		q = q->next;
	}

	if(q->flags==UIP_PBUF_FLAG_RAM && rem_len!=q->len)
		memr_realloc(q,(u8_t*)q->payload-(u8_t*)q+rem_len);

	q->len = rem_len;
	q->tot_len = q->len;

	if(q->next!=NULL) uip_pbuf_free(q->next);
	q->next = NULL;
}

u8_t uip_pbuf_header(struct uip_pbuf *p,s16_t hdr_size_inc)
{
	void *payload;

	if(hdr_size_inc==0 || p==NULL) return 0;

	payload = p->payload;
	if(p->flags==UIP_PBUF_FLAG_POOL || p->flags==UIP_PBUF_FLAG_RAM) {
		p->payload = (u8_t*)p->payload-hdr_size_inc;
		if((u8_t*)p->payload<(u8_t*)p+sizeof(struct uip_pbuf)) {
			p->payload = payload;
			return 1;
		}
	} else if(p->flags==UIP_PBUF_FLAG_ROM || p->flags==UIP_PBUF_FLAG_REF) {
		if(hdr_size_inc<0 && hdr_size_inc-p->len<=0) p->payload = (u8_t*)p->payload-hdr_size_inc;
		else return 1;
	}
	p->tot_len += hdr_size_inc;
	p->len += hdr_size_inc;

	return 0;
}

u8_t uip_pbuf_clen(struct uip_pbuf *p)
{
	u8_t len;

	len = 0;
	while(p!=NULL) {
		len++;
		p = p->next;
	}
	return len;
}

void uip_pbuf_ref(struct uip_pbuf *p)
{
	if(p!=NULL) {
		++(p->ref);
	}
}

void uip_pbuf_cat(struct uip_pbuf *h,struct uip_pbuf *t)
{
	struct uip_pbuf *p;

	if(h==NULL || t==NULL) return;

	for(p=h;p->next!=NULL;p=p->next) {
		p->tot_len += t->tot_len;
	}
	p->tot_len += t->tot_len;
	p->next = t;
}

void uip_pbuf_queue(struct uip_pbuf *p,struct uip_pbuf *n)
{
	if(p==NULL || n==NULL || p==n) return;

	while(p->next!=NULL) p = p->next;

	p->next = n;
	uip_pbuf_ref(n);
}

struct uip_pbuf* uip_pbuf_dequeue(struct uip_pbuf *p)
{
	struct uip_pbuf *q;
	u8_t tail_gone = 1;

	if(p==NULL) return NULL;

	while(p->tot_len!=p->len) p = p->next;

	q = p->next;
	if(q!=NULL) {
		p->next = NULL;
		tail_gone = uip_pbuf_free(q);
	}
	return (tail_gone>0?NULL:q);
}

void uip_pbuf_chain(struct uip_pbuf *h,struct uip_pbuf *t)
{
	uip_pbuf_cat(h,t);
	uip_pbuf_ref(t);
}

struct uip_pbuf* uip_pbuf_dechain(struct uip_pbuf *p)
{
	struct uip_pbuf *q;
	u8_t tail_gone = 1;

	q = p->next;
	if(q!=NULL) {
		q->tot_len = p->tot_len - p->len;
		p->next = NULL;
		p->tot_len = p->len;

		tail_gone = uip_pbuf_free(q);
	}

	return (tail_gone>0?NULL:q);
}
