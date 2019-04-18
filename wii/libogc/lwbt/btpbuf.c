#include <stdlib.h>
#include <string.h>

#include "btmemb.h"
#include "btmemr.h"
#include "btpbuf.h"

#if STATISTICS == 1
#define STAT(s)
#else
#define STAT(s)
#endif /* STATISTICS == 1 */

MEMB(pool_pbufs,sizeof(struct pbuf)+PBUF_POOL_BUFSIZE,PBUF_POOL_NUM);
MEMB(rom_pbufs,sizeof(struct pbuf),PBUF_ROM_NUM);

void btpbuf_init()
{
	btmemb_init(&pool_pbufs);
	btmemb_init(&rom_pbufs);
}

struct pbuf* btpbuf_alloc(pbuf_layer layer,u16_t len,pbuf_flag flag)
{
	u16_t offset;
	s32_t rem_len;
	struct pbuf *p,*q,*r;

	offset = 0;
	switch(layer)	 {
		case PBUF_TRANSPORT:
			offset += TRANSPORT_HLEN;
		case PBUF_LINK:
			offset += LL_HLEN;
			break;
		case PBUF_RAW:
			break;
		default:
			ERROR("btpbuf_alloc: bad pbuf layer.\n");
			return NULL;
	}

	switch(flag) {
		case PBUF_POOL:
			p = btmemb_alloc(&pool_pbufs);
			if(p==NULL) {
				ERROR("btbtpbuf_alloc: couldn't allocate pbuf(p) from pool\n");
				return NULL;
			}

			p->next = NULL;
			p->payload = MEM_ALIGN((void*)((u8_t*)p+(sizeof(struct pbuf)+offset)));
			p->tot_len = len;
			p->len = (len>(PBUF_POOL_BUFSIZE-offset)?(PBUF_POOL_BUFSIZE-offset):len);
			p->flags = PBUF_FLAG_POOL;
			p->ref = 1;

			r = p;
			rem_len = len - p->len;
			while(rem_len>0) {
				q = btmemb_alloc(&pool_pbufs);
				if(q==NULL) {
					ERROR("btpbuf_alloc: couldn't allocate pbuf(q) from pool\n");
					btpbuf_free(p);
					return NULL;
				}

				q->next = NULL;
				r->next = q;
				q->tot_len = rem_len;
				q->len = (rem_len>PBUF_POOL_BUFSIZE?PBUF_POOL_BUFSIZE:rem_len);
				q->payload = (void*)((u8_t*)q+sizeof(struct pbuf));
				q->flags = PBUF_FLAG_POOL;
				q->ref = 1;

				rem_len -= q->len;
				r = q;
			}
			break;
		case PBUF_RAM:
			p = btmemr_malloc(MEM_ALIGN_SIZE(sizeof(struct pbuf)+offset)+MEM_ALIGN_SIZE(len));
			if(p==NULL) {
				ERROR("btpbuf_alloc: couldn't allocate pbuf from ram\n");
				return NULL;
			}
			p->payload = MEM_ALIGN((u8_t*)p+sizeof(struct pbuf)+offset);
			p->len = p->tot_len = len;
			p->next = NULL;
			p->flags = PBUF_FLAG_RAM;
			break;
		case PBUF_ROM:
		case PBUF_REF:
			p = btmemb_alloc(&rom_pbufs);
			if(p==NULL) {
				ERROR("btpbuf_alloc: couldn't allocate pbuf from rom/ref\n");
				return NULL;
			}
			p->payload = NULL;
			p->next = NULL;
			p->len = p->tot_len = len;
			p->flags = (flag==PBUF_ROM?PBUF_FLAG_ROM:PBUF_FLAG_REF);
			break;
		default:
			ERROR("btpbuf_alloc: bad flag value.\n");
			return NULL;
	}

	p->ref = 1;
	return p;
}

u8_t btpbuf_free(struct pbuf *p)
{
	u8_t cnt;
	u32 level;
	struct pbuf *q;

	if(p==NULL) return 0;

	cnt = 0;

	_CPU_ISR_Disable(level);
	while(p!=NULL) {
		p->ref--;
		if(p->ref==0) {
			q = p->next;
			if(p->flags==PBUF_FLAG_POOL) {
				btmemb_free(&pool_pbufs,p);
			} else if(p->flags==PBUF_FLAG_ROM || p->flags==PBUF_FLAG_REF) {
				btmemb_free(&rom_pbufs,p);
			} else {
				btmemr_free(p);
			}
			cnt++;
			p = q;
		} else
			p = NULL;
	}
	_CPU_ISR_Restore(level);

	return cnt;
}

void btpbuf_realloc(struct pbuf *p,u16_t new_len)
{
	u16_t rem_len;
	s16_t grow;
	struct pbuf *q;

	if(new_len>=p->tot_len) return;

	grow = new_len - p->tot_len;
	rem_len = new_len;
	q = p;
	while(rem_len>q->len) {
		rem_len -= q->len;
		q->tot_len += grow;
		q = q->next;
	}

	if(q->flags==PBUF_FLAG_RAM && rem_len!=q->len)
		btmemr_realloc(q,(u8_t*)q->payload-(u8_t*)q+rem_len);

	q->len = rem_len;
	q->tot_len = q->len;

	if(q->next!=NULL) btpbuf_free(q->next);
	q->next = NULL;
}

u8_t btpbuf_header(struct pbuf *p,s16_t hdr_size_inc)
{
	void *payload;

	if(hdr_size_inc==0 || p==NULL) return 0;

	payload = p->payload;
	if(p->flags==PBUF_FLAG_POOL || p->flags==PBUF_FLAG_RAM) {
		p->payload = (u8_t*)p->payload-hdr_size_inc;
		if((u8_t*)p->payload<(u8_t*)p+sizeof(struct pbuf)) {
			p->payload = payload;
			return 1;
		}
	} else if(p->flags==PBUF_FLAG_ROM || p->flags==PBUF_FLAG_REF) {
		if(hdr_size_inc<0 && hdr_size_inc-p->len<=0) p->payload = (u8_t*)p->payload-hdr_size_inc;
		else return 1;
	}
	p->tot_len += hdr_size_inc;
	p->len += hdr_size_inc;

	return 0;
}

u8_t btpbuf_clen(struct pbuf *p)
{
	u8_t len;

	len = 0;
	while(p!=NULL) {
		len++;
		p = p->next;
	}
	return len;
}

void btpbuf_ref(struct pbuf *p)
{
	u32 level;

	if(p!=NULL) {
		_CPU_ISR_Disable(level);
		++(p->ref);
		_CPU_ISR_Restore(level);
	}
}

void btpbuf_cat(struct pbuf *h,struct pbuf *t)
{
	struct pbuf *p;

	if(h==NULL || t==NULL) return;

	for(p=h;p->next!=NULL;p=p->next) {
		p->tot_len += t->tot_len;
	}
	p->tot_len += t->tot_len;
	p->next = t;
}

void btpbuf_queue(struct pbuf *p,struct pbuf *n)
{
	if(p==NULL || n==NULL || p==n) return;

	while(p->next!=NULL) p = p->next;

	p->next = n;
	btpbuf_ref(n);
}

struct pbuf* btpbuf_dequeue(struct pbuf *p)
{
	struct pbuf *q;

	if(p==NULL) return NULL;

	while(p->tot_len!=p->len) p = p->next;

	q = p->next;
	p->next = NULL;

	return q;
}

void btpbuf_chain(struct pbuf *h,struct pbuf *t)
{
	btpbuf_cat(h,t);
	btpbuf_ref(t);
}

struct pbuf* btpbuf_dechain(struct pbuf *p)
{
	struct pbuf *q;
	u8_t tail_gone = 1;

	q = p->next;
	if(q!=NULL) {
		q->tot_len = p->tot_len - p->len;
		p->next = NULL;
		p->tot_len = p->len;

		tail_gone = btpbuf_free(q);
	}

	return (tail_gone>0?NULL:q);
}

struct pbuf* btpbuf_take(struct pbuf *p)
{
	struct pbuf *q , *prev, *head;

	prev = NULL;
	head = p;
	/* iterate through pbuf chain */
	do
	{
		/* pbuf is of type PBUF_REF? */
		if (p->flags == PBUF_FLAG_REF) {
			LOG("pbuf_take: encountered PBUF_REF %p\n", (void *)p);
			/* allocate a pbuf (w/ payload) fully in RAM */
			/* PBUF_POOL buffers are faster if we can use them */
			if (p->len <= PBUF_POOL_BUFSIZE) {
				q = btpbuf_alloc(PBUF_RAW, p->len, PBUF_POOL);
				if (q == NULL) {
					LOG("pbuf_take: Could not allocate PBUF_POOL\n");
				}
			} else {
				/* no replacement pbuf yet */
				q = NULL;
				LOG("pbuf_take: PBUF_POOL too small to replace PBUF_REF\n");
			}
			/* no (large enough) PBUF_POOL was available? retry with PBUF_RAM */
			if (q == NULL) {
				q = btpbuf_alloc(PBUF_RAW, p->len, PBUF_RAM);
				if (q == NULL) {
					LOG("pbuf_take: Could not allocate PBUF_RAM\n");
				}
			}
			/* replacement pbuf could be allocated? */
			if (q != NULL)
			{
				/* copy p to q */
				/* copy successor */
				q->next = p->next;
				/* remove linkage from original pbuf */
				p->next = NULL;
				/* remove linkage to original pbuf */
				if (prev != NULL) {
					/* break chain and insert new pbuf instead */
					prev->next = q;
					/* prev == NULL, so we replaced the head pbuf of the chain */
				} else {
					head = q;
				}
				/* copy pbuf payload */
				memcpy(q->payload, p->payload, p->len);
				q->tot_len = p->tot_len;
				q->len = p->len;
				/* in case p was the first pbuf, it is no longer refered to by
				 * our caller, as the caller MUST do p = pbuf_take(p);
				 * in case p was not the first pbuf, it is no longer refered to
				 * by prev. we can safely free the pbuf here.
				 * (note that we have set p->next to NULL already so that
				 * we will not free the rest of the chain by accident.)
				 */
				btpbuf_free(p);
				/* do not copy ref, since someone else might be using the old buffer */
				LOG("pbuf_take: replaced PBUF_REF %p with %p\n", (void *)p, (void *)q);
				p = q;
			} else {
				/* deallocate chain */
				btpbuf_free(head);
				LOG("pbuf_take: failed to allocate replacement pbuf for %p\n", (void *)p);
				return NULL;
			}
		/* p->flags != PBUF_FLAG_REF */
		} else {
			LOG("pbuf_take: skipping pbuf not of type PBUF_REF\n");
		}
		/* remember this pbuf */
		prev = p;
		/* proceed to next pbuf in original chain */
		p = p->next;
	} while (p);
	LOG("pbuf_take: end of chain reached.\n");

	return head;
}
