#include <stdio.h>
#include <string.h>

#include "card_cmn.h"
#include "card_buf.h"

#define BUF_POOL_CNT			3

typedef struct _buf_node {
	struct _buf_node *next;
	u8 data[SECTOR_SIZE+2];

} BufNode;

static BufNode s_buf[BUF_POOL_CNT];
static BufNode *s_freepool;

void sdgecko_initBufferPool()
{
	u32 i;
	for(i=0;i<BUF_POOL_CNT-1;++i) {
		s_buf[i].next = s_buf+i+1;
	}
	s_buf[i].next = NULL;
	s_freepool = s_buf;
}

u8*	sdgecko_allocBuffer()
{
	u8 *buf = NULL;

	if(s_freepool) {
		buf = s_freepool->data;
		s_freepool = s_freepool->next;
	}

	return buf;
}

void sdgecko_freeBuffer(u8 *buf)
{
	if(buf) {
		BufNode *node = (BufNode*)(buf-offsetof(BufNode,data));
		node->next = s_freepool;
		s_freepool = node;
	}
}
