#include <stdlib.h>
#include <string.h>

#include "memb.h"
#include "uip.h"
#include "uip_arch.h"
#include "uip_ip.h"
#include "uip_tcp.h"
#include "uip_pbuf.h"
#include "uip_netif.h"

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

static u8_t uip_tcp_timer;
static u8_t uip_flags,uip_recv_flags;
static u16_t uip_tcplen;
static u32_t uip_seqno,uip_ackno;

static struct uip_tcpseg uip_inseg;

static struct uip_pbuf *uip_recv_data = NULL;
static struct uip_ip_hdr *uip_iphdr = NULL;
static struct uip_tcp_hdr *uip_tcphdr = NULL;

u32_t uip_tcp_ticks;
struct uip_tcp_pcb *uip_tcp_tmp_pcb = NULL;
struct uip_tcp_pcb *uip_tcp_input_pcb = NULL;
struct uip_tcp_pcb *uip_tcp_active_pcbs = NULL;
struct uip_tcp_pcb *uip_tcp_tw_pcbs = NULL;

union uip_tcp_listen_pcbs_t uip_tcp_listen_pcbs;

const u8_t uip_tcp_backoff[13] = { 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7};

MEMB(uip_listen_tcp_pcbs,sizeof(struct uip_tcp_pcb_listen),UIP_LISTEN_TCP_PCBS);
MEMB(uip_tcp_pcbs,sizeof(struct uip_tcp_pcb),UIP_TCP_PCBS);
MEMB(uip_tcp_segs,sizeof(struct uip_tcpseg),UIP_TCP_SEGS);

static s8_t uip_tcp_nullaccept(void *arg,struct uip_tcp_pcb *pcb,s8_t err);
static s8_t uip_tcp_nullrecv(void *arg,struct uip_tcp_pcb *pcb,struct uip_pbuf *p,s8_t err);

static void uip_tcp_parseopt(struct uip_tcp_pcb *pcb);
static void uip_tcpoutput_segments(struct uip_tcpseg *seg,struct uip_tcp_pcb *pcb);
static s8_t uip_tcpinput_listen(struct uip_tcp_pcb_listen *pcb);
static s8_t uip_tcpinput_timewait(struct uip_tcp_pcb *pcb);
static s8_t uip_tcpprocess(struct uip_tcp_pcb *pcb);
static void uip_tcpreceive(struct uip_tcp_pcb *pcb);
static u16_t uip_tcp_newport();

s8_t uip_tcp_sendctrl(struct uip_tcp_pcb *pcb,u8_t flags)
{
	return uip_tcpenqueue(pcb,NULL,0,flags,1,NULL,0);
}

s8_t uip_tcp_write(struct uip_tcp_pcb *pcb,const void *arg,u16_t len,u8_t copy)
{
	if(pcb->state==UIP_ESTABLISHED || pcb->state==UIP_CLOSE_WAIT ||
		pcb->state==UIP_SYN_SENT || pcb->state==UIP_SYN_RCVD) {
		if(len>0) {
			return uip_tcpenqueue(pcb,(void*)arg,len,0,copy,NULL,0);
		}
		return UIP_ERR_OK;
	}
	return UIP_ERR_CONN;
}

s8_t uip_tcpenqueue(struct uip_tcp_pcb *pcb,void *arg,u16_t len,u8_t flags,u8_t copy,u8_t *optdata,u8_t optlen)
{
	struct uip_pbuf *p;
	struct uip_tcpseg *seg,*useg,*queue = NULL;
	u32_t left,seqno;
	u16_t seglen;
	void *ptr;
	u8_t queue_len;

	if(len>pcb->snd_buf) {
		UIP_ERROR("uip_tcpenqueue: too much data (len>pcb->snd_buf).\n");
		return UIP_ERR_MEM;
	}

	left = len;
	ptr = arg;

	seqno = pcb->snd_lbb;
	queue_len = pcb->snd_queuelen;

	if(queue_len>=UIP_TCP_SND_QUEUELEN) {
		UIP_ERROR("uip_tcpenqueue: too long queue.");
		goto memerr;
	}
	useg = seg = queue = NULL;
	seglen = 0;
	while(queue==NULL || left>0) {
		seglen = left>pcb->mss?pcb->mss:len;
		seg = memb_alloc(&uip_tcp_segs);
		if(seg==NULL) {
			UIP_ERROR("uip_tcpenqueue: could not allocate memory for tcp_seg.");
			goto memerr;
		}

		seg->next = NULL;
		seg->p = NULL;

		if(queue==NULL) queue = seg;
		else useg->next = seg;

		useg = seg;

		if(optdata!=NULL) {
			if((seg->p=uip_pbuf_alloc(UIP_PBUF_TRANSPORT,optlen,UIP_PBUF_RAM))==NULL) {
				UIP_ERROR("uip_tcpenqueue: could not allocate memory for pbuf opdata.");
				goto memerr;
			}
			++queue_len;
			seg->dataptr = seg->p->payload;
		} else if(copy) {
			if((seg->p=uip_pbuf_alloc(UIP_PBUF_TRANSPORT,seglen,UIP_PBUF_RAM))==NULL) {
				UIP_ERROR("uip_tcpenqueue: could not allocate memory for pbuf copy size.");
				goto memerr;
			}

			++queue_len;
			if(ptr!=NULL) UIP_MEMCPY(seg->p->payload,ptr,seglen);

			seg->dataptr = seg->p->payload;
		} else {
			if((p=uip_pbuf_alloc(UIP_PBUF_TRANSPORT,seglen,UIP_PBUF_ROM))==NULL) {
				UIP_ERROR("uip_tcpenqueue: could not allocate memory for zero-copy pbuf.");
				goto memerr;
			}

			++queue_len;
			p->payload = ptr;
			seg->dataptr = ptr;
			if((seg->p=uip_pbuf_alloc(UIP_PBUF_TRANSPORT,0,UIP_PBUF_RAM))==NULL) {
				UIP_LOG("uip_tcpenqueue: could not allocate memory for header pbuf.");
				uip_pbuf_free(p);
				goto memerr;
			}

			++queue_len;
			uip_pbuf_cat(seg->p,p);
			p = NULL;
		}

		if(queue_len>UIP_TCP_SND_QUEUELEN) {
			UIP_ERROR("uip_tcpenqueue: queue too long.");
			goto memerr;
		}

		seg->len = seglen;
		if(uip_pbuf_header(seg->p,UIP_TCP_HLEN)) {
			UIP_ERROR("uip_tcpenqueue: no room for TCP header in pbuf.");
			goto memerr;
		}

		seg->tcphdr = seg->p->payload;
		seg->tcphdr->src = htons(pcb->local_port);
		seg->tcphdr->dst = htons(pcb->remote_port);
		seg->tcphdr->seqno = htonl(seqno);
		seg->tcphdr->urgp = 0;
		UIP_TCPH_FLAGS_SET(seg->tcphdr,flags);
		if(optdata==NULL) UIP_TCPH_HDRLEN_SET(seg->tcphdr,5);
		else {
			UIP_TCPH_HDRLEN_SET(seg->tcphdr,(5+(optlen/4)));
			UIP_MEMCPY(seg->dataptr,optdata,optlen);
		}
		left -= seglen;
		seqno += seglen;
		ptr = (void*)((u8_t*)ptr+seglen);
	}

	if(pcb->unsent==NULL) useg = NULL;
	else {
		for(useg=pcb->unsent;useg->next!=NULL;useg=useg->next);
	}

	if(useg!=NULL &&
		UIP_TCP_TCPLEN(useg)!=0 &&
		!(UIP_TCPH_FLAGS(useg->tcphdr)&(UIP_TCP_SYN|UIP_TCP_FIN)) &&
		!(flags&(UIP_TCP_SYN|UIP_TCP_FIN)) &&
		useg->len+queue->len<=pcb->mss) {

		uip_pbuf_header(queue->p,-UIP_TCP_HLEN);
		uip_pbuf_cat(useg->p,queue->p);

		useg->len += queue->len;
		useg->next = queue->next;
		if(seg==queue) seg = NULL;

		memb_free(&uip_tcp_segs,queue);
	} else {
		if(useg==NULL) pcb->unsent = queue;
		else useg->next = queue;
	}

	if(flags&UIP_TCP_SYN || flags&UIP_TCP_FIN) len++;

	pcb->snd_lbb += len;
	pcb->snd_buf -= len;
	pcb->snd_queuelen = queue_len;

	if(seg!=NULL && seglen>0 && seg->tcphdr!=NULL) UIP_TCPH_SET_FLAG(seg->tcphdr,UIP_TCP_PSH);

	return UIP_ERR_OK;
memerr:
	if(queue!=NULL) uip_tcpsegs_free(queue);
	return UIP_ERR_MEM;
}

void uip_tcpinput(struct uip_pbuf *p,struct uip_netif *inp)
{
	s8_t err;
	u8_t hdr_len;
	struct uip_tcp_pcb *pcb,*prev;
	struct uip_tcp_pcb_listen *lpcb;

	uip_iphdr = p->payload;
	uip_tcphdr = (struct uip_tcp_hdr*)((u8_t*)p->payload+UIP_IPH_HL(uip_iphdr)*4);

	if(uip_pbuf_header(p,-((s16_t)(UIP_IPH_HL(uip_iphdr)*4))) || p->tot_len<sizeof(struct uip_tcp_hdr)) {
		UIP_LOG("uip_tcpinput: short packet discarded.");
		uip_pbuf_free(p);
		return;
	}
	if(ip_addr_isbroadcast(&uip_iphdr->dst,inp) ||
		ip_addr_ismulticast(&uip_iphdr->dst)) {
		uip_pbuf_free(p);
		return;
	}

	if(uip_chksum_pseudo(p,&uip_iphdr->src,&uip_iphdr->dst,UIP_PROTO_TCP,p->tot_len)!=0) {
		UIP_LOG("uip_tcpinput: packet discarded due to failing checksum.");
		uip_pbuf_free(p);
		return;
	}

	hdr_len = UIP_TCPH_HDRLEN(uip_tcphdr);
	uip_pbuf_header(p,-(hdr_len*4));

	uip_tcphdr->src = ntohs(uip_tcphdr->src);
	uip_tcphdr->dst = ntohs(uip_tcphdr->dst);
	uip_seqno = uip_tcphdr->seqno = ntohl(uip_tcphdr->seqno);
	uip_ackno = uip_tcphdr->ackno = ntohl(uip_tcphdr->ackno);
	uip_tcphdr->wnd = ntohs(uip_tcphdr->wnd);

	uip_flags = UIP_TCPH_FLAGS(uip_tcphdr)&UIP_TCP_FLAGS;
	uip_tcplen = p->tot_len+((uip_flags&UIP_TCP_FIN||uip_flags&UIP_TCP_SYN)?1:0);

	prev = NULL;
	for(pcb=uip_tcp_active_pcbs;pcb!=NULL;pcb=pcb->next) {
		if(pcb->state!=UIP_CLOSED && pcb->state!=UIP_TIME_WAIT && pcb->state!=UIP_LISTEN) {
			if(pcb->remote_port==uip_tcphdr->src &&
				pcb->local_port==uip_tcphdr->dst &&
				ip_addr_cmp(&pcb->remote_ip,&uip_iphdr->src) &&
				ip_addr_cmp(&pcb->local_ip,&uip_iphdr->dst)) {
				if(prev!=NULL) {
					prev->next = pcb->next;
					pcb->next = uip_tcp_active_pcbs;
					uip_tcp_active_pcbs = pcb;
				}
				break;
			}
			prev = pcb;
		}
	}

	if(pcb==NULL) {
		for(pcb=uip_tcp_tw_pcbs;pcb!=NULL;pcb=pcb->next) {
			if(pcb->state==UIP_TIME_WAIT &&
				pcb->remote_port==uip_tcphdr->src &&
				pcb->local_port==uip_tcphdr->dst &&
				ip_addr_cmp(&pcb->remote_ip,&uip_iphdr->src) &&
				ip_addr_cmp(&pcb->local_ip,&uip_iphdr->dst)) {
				uip_tcpinput_timewait(pcb);
				return;
			}
		}

		prev = NULL;
		for(lpcb=uip_tcp_listen_pcbs.listen_pcbs;lpcb!=NULL;lpcb=lpcb->next) {
			if((ip_addr_isany(&lpcb->local_ip) || ip_addr_cmp(&lpcb->local_ip,&uip_iphdr->dst)) &&
				lpcb->local_port==uip_tcphdr->dst) {
				if(prev!=NULL) {
					((struct uip_tcp_pcb_listen*)prev)->next = lpcb->next;
					lpcb->next = uip_tcp_listen_pcbs.listen_pcbs;
					uip_tcp_listen_pcbs.listen_pcbs = lpcb;
				}
				uip_tcpinput_listen(lpcb);
				return;
			}
			prev = (struct uip_tcp_pcb*)lpcb;
		}
	}

	if(pcb!=NULL) {
		uip_inseg.next = NULL;
		uip_inseg.len = p->tot_len;
		uip_inseg.dataptr = p->payload;
		uip_inseg.p = p;
		uip_inseg.tcphdr = uip_tcphdr;

		uip_recv_data = NULL;
		uip_recv_flags = 0;

		uip_tcp_input_pcb = pcb;
		err = uip_tcpprocess(pcb);
		uip_tcp_input_pcb = NULL;

		if(err!=UIP_ERR_ABRT) {
			if(uip_recv_flags&UIP_TF_RESET) {
				if(pcb->errf) pcb->errf(pcb->cb_arg,UIP_ERR_RST);
				uip_tcp_pcbremove(&uip_tcp_active_pcbs,pcb);
				memb_free(&uip_tcp_pcbs,pcb);
			} else if(uip_recv_flags&UIP_TF_CLOSED) {
				uip_tcp_pcbremove(&uip_tcp_active_pcbs,pcb);
				memb_free(&uip_tcp_pcbs,pcb);
			} else {
				err = UIP_ERR_OK;

				if(pcb->acked>0) {
					if(pcb->sent) err = pcb->sent(pcb->cb_arg,pcb,pcb->acked);
				}

				if(uip_recv_data!=NULL) {
					if(pcb->recv) err = pcb->recv(pcb->cb_arg,pcb,uip_recv_data,UIP_ERR_OK);
				}

				if(err==UIP_ERR_OK) uip_tcpoutput(pcb);
			}
		}
		if(uip_inseg.p!=NULL) uip_pbuf_free(uip_inseg.p);
	} else {
		if(!(UIP_TCPH_FLAGS(uip_tcphdr)&UIP_TCP_RST))
			uip_tcp_rst(uip_ackno,uip_seqno+uip_tcplen,&uip_iphdr->dst,&uip_iphdr->src,uip_tcphdr->dst,uip_tcphdr->src);

		uip_pbuf_free(p);
	}
}

s8_t uip_tcpoutput(struct uip_tcp_pcb *pcb)
{
	u32_t wnd;
	struct uip_pbuf *p;
	struct uip_tcp_hdr *tcphdr;
	struct uip_tcpseg *seg,*useg;

	if(uip_tcp_input_pcb==pcb) return 0;

	wnd = UIP_MIN(pcb->snd_wnd,pcb->cwnd);
	seg = pcb->unsent;
	useg = pcb->unacked;
	if(useg!=NULL) {
		for(;useg->next!=NULL;useg=useg->next);
	}

	if(pcb->flags&UIP_TF_ACK_NOW &&
		(seg==NULL || ntohl(seg->tcphdr->seqno)-pcb->lastack+seg->len>wnd)) {
		//printf("uip_tcpout: ACK - seqno = %u, ackno = %u\n",pcb->snd_nxt,pcb->rcv_nxt);
		p = uip_pbuf_alloc(UIP_PBUF_IP,UIP_TCP_HLEN,UIP_PBUF_RAM);
		if(p==NULL) {
			UIP_ERROR("uip_tcpoutput: (ACK) could not allocate pbuf.");
			return UIP_ERR_BUF;
		}
		pcb->flags &= ~(UIP_TF_ACK_DELAY|UIP_TF_ACK_NOW);

		tcphdr = p->payload;
		tcphdr->src	= htons(pcb->local_port);
		tcphdr->dst = htons(pcb->remote_port);
		tcphdr->seqno = htonl(pcb->snd_nxt);
		tcphdr->ackno = htonl(pcb->rcv_nxt);
		UIP_TCPH_FLAGS_SET(tcphdr,UIP_TCP_ACK);
		tcphdr->wnd = htons(pcb->rcv_wnd);
		tcphdr->urgp = 0;
		UIP_TCPH_HDRLEN_SET(tcphdr,5);

		tcphdr->chksum = 0;
		tcphdr->chksum = uip_chksum_pseudo(p,&pcb->local_ip,&pcb->remote_ip,UIP_PROTO_TCP,p->tot_len);

		uip_ipoutput(p,&pcb->local_ip,&pcb->remote_ip,pcb->ttl,pcb->tos,UIP_PROTO_TCP);
		uip_pbuf_free(p);

		return UIP_ERR_OK;
	}

	while(seg!=NULL && ntohl(seg->tcphdr->seqno)-pcb->lastack+seg->len<=wnd) {
		pcb->unsent = seg->next;
		if(pcb->state!=UIP_SYN_SENT) {
			UIP_TCPH_SET_FLAG(seg->tcphdr,UIP_TCP_ACK);
			pcb->flags &= ~(UIP_TF_ACK_DELAY|UIP_TF_ACK_NOW);
		}

		uip_tcpoutput_segments(seg,pcb);

		pcb->snd_nxt = ntohl(seg->tcphdr->seqno)+UIP_TCP_TCPLEN(seg);
		if(UIP_TCP_SEQ_LT(pcb->snd_max,pcb->snd_nxt)) pcb->snd_max = pcb->snd_nxt;

		if(UIP_TCP_TCPLEN(seg)>0) {
			seg->next = NULL;
			if(pcb->unacked==NULL) {
				pcb->unacked = seg;
				useg = seg;
			} else {
				if(UIP_TCP_SEQ_LT(ntohl(seg->tcphdr->seqno),ntohl(useg->tcphdr->seqno))) {
					seg->next = pcb->unacked;
					pcb->unacked = seg;
				} else {
					useg->next = seg;
					useg = useg->next;
				}
			}
		} else
			uip_tcpseg_free(seg);

		seg = pcb->unsent;
	}
	return UIP_ERR_OK;
}

void uip_tcp_tmr()
{
	uip_tcp_fasttmr();

	if(++uip_tcp_timer&1) uip_tcp_slowtmr();
}

void uip_tcp_init()
{
	memb_init(&uip_tcp_pcbs);
	memb_init(&uip_listen_tcp_pcbs);
	memb_init(&uip_tcp_segs);

	uip_tcp_listen_pcbs.listen_pcbs = NULL;
	uip_tcp_active_pcbs = NULL;
	uip_tcp_tw_pcbs = NULL;

	uip_tcp_ticks = 0;
	uip_tcp_timer = 0;
}

void uip_tcp_accept(struct uip_tcp_pcb *pcb,s8_t (*accept)(void *,struct uip_tcp_pcb *,s8_t))
{
	((struct uip_tcp_pcb_listen*)pcb)->accept = accept;
}

void uip_tcp_err(struct uip_tcp_pcb *pcb,void (*errf)(void *,s8_t))
{
	pcb->errf = errf;
}

void uip_tcp_recv(struct uip_tcp_pcb *pcb,s8_t (*recv)(void *,struct uip_tcp_pcb *,struct uip_pbuf *,s8_t))
{
	pcb->recv = recv;
}

void uip_tcp_sent(struct uip_tcp_pcb *pcb,s8_t (*sent)(void *,struct uip_tcp_pcb *,u16_t))
{
	pcb->sent = sent;
}

void uip_tcp_poll(struct uip_tcp_pcb *pcb,s8_t (*poll)(void *,struct uip_tcp_pcb *),u8_t interval)
{
	pcb->poll = poll;
	pcb->pollinterval = interval;
}

void uip_tcp_arg(struct uip_tcp_pcb *pcb,void *arg)
{
	pcb->cb_arg = arg;
}

struct uip_tcp_pcb* uip_tcp_pcballoc(u8_t prio)
{
	u32_t iss;
	struct uip_tcp_pcb *pcb = NULL;

	pcb = memb_alloc(&uip_tcp_pcbs);
	if(pcb!=NULL) {
		UIP_MEMSET(pcb,0,sizeof(struct uip_tcp_pcb));
		pcb->prio = UIP_TCP_PRIO_NORMAL;
		pcb->snd_buf = UIP_TCP_SND_BUF;
		pcb->snd_queuelen = 0;
		pcb->rcv_wnd = UIP_TCP_WND;
		pcb->tos = 0;
		pcb->ttl = UIP_TCP_TTL;
		pcb->mss = UIP_TCP_MSS;
		pcb->rto = 3000/UIP_TCP_SLOW_INTERVAL;
		pcb->sa = 0;
		pcb->sv = 3000/UIP_TCP_SLOW_INTERVAL;
		pcb->rtime = 0;
		pcb->cwnd = 1;
		iss = uip_tcpiss_next();
		pcb->snd_wl2 = iss;
		pcb->snd_nxt = iss;
		pcb->snd_max = iss;
		pcb->lastack = iss;
		pcb->snd_lbb = iss;
		pcb->tmr = uip_tcp_ticks;
		pcb->polltmr = 0;

		pcb->recv = uip_tcp_nullrecv;

		pcb->keepalive = UIP_TCP_KEEPDEFAULT;
		pcb->keepcnt = 0;
	}
	return pcb;
}

void uip_tcp_pcbremove(struct uip_tcp_pcb **pcblist,struct uip_tcp_pcb *pcb)
{
	UIP_TCP_RMV(pcblist,pcb);

	uip_tcp_pcbpurge(pcb);
	if(pcb->state!=UIP_TIME_WAIT &&
		pcb->state!=UIP_LISTEN &&
		pcb->flags&UIP_TF_ACK_DELAY) {
		pcb->flags |= UIP_TF_ACK_NOW;
		uip_tcpoutput(pcb);
	}
	pcb->state = UIP_CLOSED;
}

void uip_tcp_pcbpurge(struct uip_tcp_pcb *pcb)
{
	if(pcb->state!=UIP_CLOSED &&
		pcb->state!=UIP_TIME_WAIT &&
		pcb->state!=UIP_LISTEN) {
		uip_tcpsegs_free(pcb->ooseq);
		uip_tcpsegs_free(pcb->unsent);
		uip_tcpsegs_free(pcb->unacked);
		pcb->unsent = pcb->unacked = pcb->ooseq = NULL;
	}
}

struct uip_tcp_pcb* uip_tcp_new()
{
	return uip_tcp_pcballoc(UIP_TCP_PRIO_NORMAL);
}

s8_t uip_tcp_bind(struct uip_tcp_pcb *pcb,struct uip_ip_addr *ipaddr,u16_t port)
{
	struct uip_tcp_pcb *cpcb;

	if(port==0) port = uip_tcp_newport();

	for(cpcb=(struct uip_tcp_pcb*)uip_tcp_listen_pcbs.pcbs;cpcb!=NULL;cpcb=cpcb->next) {
		if(cpcb->local_port==port) {
			if(ip_addr_isany(&cpcb->local_ip) ||
				ip_addr_isany(ipaddr) ||
				ip_addr_cmp(&cpcb->local_ip,ipaddr)) return UIP_ERR_USE;
		}
	}

	for(cpcb=uip_tcp_active_pcbs;cpcb!=NULL;cpcb=cpcb->next) {
		if(cpcb->local_port==port) {
			if(ip_addr_isany(&cpcb->local_ip) ||
				ip_addr_isany(ipaddr) ||
				ip_addr_cmp(&cpcb->local_ip,ipaddr)) return UIP_ERR_USE;
		}
	}

	if(!ip_addr_isany(ipaddr)) pcb->local_ip = *ipaddr;
	pcb->local_port = port;

	return UIP_ERR_OK;
}

struct uip_tcp_pcb* uip_tcp_listen(struct uip_tcp_pcb *pcb)
{
	struct uip_tcp_pcb_listen *lpcb;

	if(pcb->state==UIP_LISTEN) return pcb;

	lpcb = memb_alloc(&uip_listen_tcp_pcbs);
	if(lpcb==NULL) return NULL;

	lpcb->cb_arg = pcb->cb_arg;
	lpcb->local_port = pcb->local_port;
	lpcb->state = UIP_LISTEN;
	lpcb->so_options = pcb->so_options;
	lpcb->so_options |= UIP_SOF_ACCEPTCONN;
	lpcb->ttl = pcb->ttl;
	lpcb->tos = pcb->tos;
	ip_addr_set(&lpcb->local_ip,&pcb->local_ip);

	memb_free(&uip_tcp_pcbs,pcb);

	lpcb->accept = uip_tcp_nullaccept;

	UIP_TCP_REG(&uip_tcp_listen_pcbs.listen_pcbs,lpcb);
	return (struct uip_tcp_pcb*)lpcb;
}

void uip_tcp_recved(struct uip_tcp_pcb *pcb,u16_t len)
{
	if((u32_t)pcb->rcv_wnd+len>UIP_TCP_WND) pcb->rcv_wnd = UIP_TCP_WND;
	else pcb->rcv_wnd += len;

	if(!(pcb->flags&UIP_TF_ACK_DELAY) && !(pcb->flags&UIP_TF_ACK_NOW)) {
		uip_tcp_ack(pcb);
	} else if(pcb->flags&UIP_TF_ACK_DELAY && pcb->rcv_wnd>=UIP_TCP_WND/2) {
		uip_tcp_acknow(pcb);
	}
}

s8_t uip_tcp_close(struct uip_tcp_pcb *pcb)
{
	s8_t err;

	switch(pcb->state) {
		case UIP_CLOSED:
			err = UIP_ERR_OK;
			memb_free(&uip_tcp_pcbs,pcb);
			pcb = NULL;
			break;
		case UIP_LISTEN:
			err = UIP_ERR_OK;
			uip_tcp_pcbremove((struct uip_tcp_pcb**)&uip_tcp_listen_pcbs.pcbs,pcb);
			memb_free(&uip_listen_tcp_pcbs,pcb);
			pcb = NULL;
			break;
		case UIP_SYN_SENT:
			err = UIP_ERR_OK;
			uip_tcp_pcbremove(&uip_tcp_active_pcbs,pcb);
			memb_free(&uip_tcp_pcbs,pcb);
			pcb = NULL;
			break;
		case UIP_SYN_RCVD:
		case UIP_ESTABLISHED:
			err = uip_tcp_sendctrl(pcb,UIP_TCP_FIN);
			if(err==UIP_ERR_OK) pcb->state = UIP_FIN_WAIT_1;
			break;
		case UIP_CLOSE_WAIT:
			err = uip_tcp_sendctrl(pcb,UIP_TCP_FIN);
			if(err==UIP_ERR_OK) pcb->state = UIP_LAST_ACK;
			break;
		default:
			err	= UIP_ERR_OK;
			pcb = NULL;
			break;
	}
	if(pcb!=NULL && err==UIP_ERR_OK) uip_tcpoutput(pcb);

	return err;
}

void uip_tcp_rst(u32_t seqno,u32_t ackno,struct uip_ip_addr *lipaddr,struct uip_ip_addr *ripaddr,u16_t lport,u16_t rport)
{
	struct uip_pbuf *p;
	struct uip_tcp_hdr *tcphdr;

	p = uip_pbuf_alloc(UIP_PBUF_IP,UIP_TCP_HLEN,UIP_PBUF_RAM);
	if(p==NULL) {
		UIP_LOG("uip_tcp_rst: could not allocate memory for pbuf.\n");
		return;
	}

	tcphdr = p->payload;
	tcphdr->src = htons(lport);
	tcphdr->dst = htons(rport);
	tcphdr->seqno = htonl(seqno);
	tcphdr->ackno = htonl(ackno);
	UIP_TCPH_FLAGS_SET(tcphdr,UIP_TCP_RST|UIP_TCP_ACK);
	tcphdr->wnd = htons(UIP_TCP_WND);
	tcphdr->urgp = 0;
	UIP_TCPH_HDRLEN_SET(tcphdr,5);

	tcphdr->chksum = 0;
	tcphdr->chksum = uip_chksum_pseudo(p,lipaddr,ripaddr,UIP_PROTO_TCP,p->tot_len);

	uip_ipoutput(p,lipaddr,ripaddr,UIP_TCP_TTL,0,UIP_PROTO_TCP);
	uip_pbuf_free(p);
}

void uip_tcp_abort(struct uip_tcp_pcb *pcb)
{
	u32_t seqno,ackno;
	u16_t remote_port,local_port;
	struct uip_ip_addr remote_ip,local_ip;
	void (*errf)(void *arg,s8_t err);
	void *errf_arg;

	if(pcb->state==UIP_TIME_WAIT) {
		memb_free(&uip_tcp_pcbs,pcb);
	} else {
		seqno = pcb->snd_nxt;
		ackno = pcb->rcv_nxt;

		ip_addr_set(&local_ip,&pcb->local_ip);
		ip_addr_set(&remote_ip,&pcb->remote_ip);
		local_port = pcb->local_port;
		remote_port = pcb->remote_port;

		errf = pcb->errf;
		errf_arg = pcb->cb_arg;

		uip_tcp_pcbremove(&uip_tcp_active_pcbs,pcb);

		if(pcb->unacked!=NULL)
			uip_tcpsegs_free(pcb->unacked);
		if(pcb->unsent!=NULL)
			uip_tcpsegs_free(pcb->unsent);
		if(pcb->ooseq!=NULL)
			uip_tcpsegs_free(pcb->ooseq);

		memb_free(&uip_tcp_pcbs,pcb);
		if(errf) errf(errf_arg,UIP_ERR_ABRT);

		uip_tcp_rst(seqno,ackno,&local_ip,&remote_ip,local_port,remote_port);
	}
}

void uip_tcp_keepalive(struct uip_tcp_pcb *pcb)
{
	struct uip_pbuf *p;
	struct uip_tcp_hdr *tcphdr;

	p = uip_pbuf_alloc(UIP_PBUF_IP,UIP_TCP_HLEN,UIP_PBUF_RAM);
	if(p==NULL) return;

	tcphdr = p->payload;
	tcphdr->src = htons(pcb->local_port);
	tcphdr->dst = htons(pcb->remote_port);
	tcphdr->seqno = htonl(pcb->snd_nxt-1);
	tcphdr->ackno = htonl(pcb->rcv_nxt);
	tcphdr->wnd = htons(pcb->rcv_wnd);
	tcphdr->urgp = 0;
	UIP_TCPH_HDRLEN_SET(tcphdr,5);

	tcphdr->chksum = 0;
	tcphdr->chksum = uip_chksum_pseudo(p,&pcb->local_ip,&pcb->remote_ip,UIP_PROTO_TCP,p->tot_len);

	uip_ipoutput(p,&pcb->local_ip,&pcb->remote_ip,pcb->ttl,0,UIP_PROTO_TCP);
	uip_pbuf_free(p);
}

void uip_tcp_rexmit(struct uip_tcp_pcb *pcb)
{
	struct uip_tcpseg *seg;

	if(pcb->unacked==NULL) return;

	seg = pcb->unacked->next;
	pcb->unacked->next = pcb->unsent;
	pcb->unsent = pcb->unacked;
	pcb->unacked = seg;

	pcb->snd_nxt = ntohl(pcb->unsent->tcphdr->seqno);
	pcb->nrtx++;
	pcb->rttest = 0;

	uip_tcpoutput(pcb);
}

void uip_tcp_rexmit_rto(struct uip_tcp_pcb *pcb)
{
	struct uip_tcpseg *seg;

	if(pcb->unacked==NULL) return;

	for(seg=pcb->unacked;seg->next!=NULL;seg=seg->next);

	seg->next = pcb->unsent;
	pcb->unsent = pcb->unacked;
	pcb->unacked = NULL;

	pcb->snd_nxt = ntohl(pcb->unsent->tcphdr->seqno);
	pcb->nrtx++;

	uip_tcpoutput(pcb);
}

void uip_tcp_fasttmr()
{
	struct uip_tcp_pcb *pcb;

	for(pcb=uip_tcp_active_pcbs;pcb!=NULL;pcb=pcb->next) {
		if(pcb->flags&UIP_TF_ACK_DELAY) {
			uip_tcp_acknow(pcb);
			pcb->flags &= ~(UIP_TF_ACK_DELAY|UIP_TF_ACK_NOW);
		}
	}
}

void uip_tcp_slowtmr()
{
	struct uip_tcp_pcb *prev,*pcb,*pcb2;
	u32 eff_wnd;
	u8_t pcb_remove;
	s8_t err;

	err = UIP_ERR_OK;

	uip_tcp_ticks++;

	prev = NULL;
	pcb = uip_tcp_active_pcbs;
	while(pcb!=NULL) {
		pcb_remove = 0;

		if(pcb->state==UIP_SYN_SENT && pcb->nrtx==UIP_MAXSYNRTX) pcb_remove++;
		else if(pcb->nrtx==UIP_MAXRTX) pcb_remove++;
		else {
			pcb->rtime++;
			if(pcb->unacked!=NULL && pcb->rtime>=pcb->rto) {
				if(pcb->state==UIP_SYN_SENT) pcb->rto = ((pcb->sa>>3)+pcb->sv)<<uip_tcp_backoff[pcb->nrtx];

				eff_wnd = UIP_MIN(pcb->cwnd,pcb->snd_wnd);
				pcb->ssthresh = eff_wnd>>1;

				if(pcb->ssthresh<pcb->mss) pcb->ssthresh = pcb->mss*2;
				pcb->cwnd = pcb->mss;

				uip_tcp_rexmit_rto(pcb);
			}
		}

		if(pcb->state==UIP_FIN_WAIT_2) {
			if((u32_t)(uip_tcp_ticks-pcb->tmr)>UIP_TCP_FIN_WAIT_TIMEOUT/UIP_TCP_SLOW_INTERVAL) pcb_remove++;
		}

		if(pcb->so_options&UIP_SOF_KEEPALIVE &&
			(pcb->state==UIP_ESTABLISHED || pcb->state==UIP_CLOSE_WAIT)) {
			if((u32_t)(uip_tcp_ticks-pcb->tmr)>(pcb->keepalive+UIP_TCP_MAXIDLE)/UIP_TCP_SLOW_INTERVAL) uip_tcp_abort(pcb);
			else if((u32_t)(uip_tcp_ticks-pcb->tmr)>(pcb->keepalive+pcb->keepcnt*UIP_TCP_KEEPINTVL)/UIP_TCP_SLOW_INTERVAL) {
				uip_tcp_keepalive(pcb);
				pcb->keepcnt++;
			}
		}

		if(pcb->ooseq!=NULL && (u32_t)uip_tcp_ticks-pcb->tmr>=pcb->rto*UIP_TCP_OOSEQ_TIMEOUT) {
			uip_tcpsegs_free(pcb->ooseq);
			pcb->ooseq = NULL;
		}
		if(pcb->state==UIP_SYN_RCVD) {
			if((u32_t)(uip_tcp_ticks-pcb->tmr)>UIP_TCP_SYN_RCVD_TIMEOUT/UIP_TCP_SLOW_INTERVAL) pcb_remove++;
		}

		if(pcb_remove) {
			uip_tcp_pcbpurge(pcb);

			if(prev!=NULL) prev->next = pcb->next;
			else uip_tcp_active_pcbs = pcb->next;

			if(pcb->errf) pcb->errf(pcb->cb_arg,UIP_ERR_ABRT);

			pcb2 = pcb->next;
			memb_free(&uip_tcp_pcbs,pcb);
			pcb = pcb2;
		} else {
			pcb->polltmr++;
			if(pcb->polltmr>=pcb->pollinterval) {
				pcb->polltmr = 0;
				if(pcb->poll) err = pcb->poll(pcb->cb_arg,pcb);

				if(err==UIP_ERR_OK) uip_tcpoutput(pcb);
			}

			prev = pcb;
			pcb = pcb->next;
		}
	}

	prev = NULL;
	pcb = uip_tcp_tw_pcbs;
	while(pcb!=NULL) {
		pcb_remove = 0;

		if((u32_t)(uip_tcp_ticks-pcb->tmr)>2*UIP_TCP_MSL/UIP_TCP_SLOW_INTERVAL) pcb_remove++;

		if(pcb_remove) {
			uip_tcp_pcbpurge(pcb);

			if(prev!=NULL) prev->next = pcb->next;
			else uip_tcp_tw_pcbs = pcb->next;

			pcb2 = pcb->next;
			memb_free(&uip_tcp_pcbs,pcb);
			pcb = pcb2;
		} else {
			prev = pcb;
			pcb = pcb->next;
		}
	}
}

u32_t uip_tcpiss_next()
{
	static u32_t iss = 6510;
	iss += uip_tcp_ticks;
	return iss;
}

struct uip_tcpseg* uip_tcpseg_copy(struct uip_tcpseg *seg)
{
	struct uip_tcpseg *cseg;

	cseg = memb_alloc(&uip_tcp_segs);
	if(cseg==NULL) return NULL;

	UIP_MEMCPY(cseg,seg,sizeof(struct uip_tcpseg));
	uip_pbuf_ref(cseg->p);

	return cseg;
}

u8_t uip_tcpsegs_free(struct uip_tcpseg *seg)
{
	u8_t cnt = 0;
	struct uip_tcpseg *next;

	while(seg!=NULL) {
		next = seg->next;
		cnt += uip_tcpseg_free(seg);
		seg = next;
	}

	return cnt;
}

u8_t uip_tcpseg_free(struct uip_tcpseg *seg)
{
	u8_t cnt = 0;

	if(seg!=NULL) {
		if(seg->p!=NULL) {
			cnt = uip_pbuf_free(seg->p);
		}
		memb_free(&uip_tcp_segs,seg);
	}
	return cnt;
}

#ifndef TCP_LOCAL_PORT_RANGE_START
#define TCP_LOCAL_PORT_RANGE_START 4096
#define TCP_LOCAL_PORT_RANGE_END   0x7fff
#endif

static u16_t uip_tcp_newport()
{
  struct uip_tcp_pcb *pcb;
  static u16_t port = TCP_LOCAL_PORT_RANGE_START;

again:
  if(++port>=TCP_LOCAL_PORT_RANGE_END) port = TCP_LOCAL_PORT_RANGE_START;

  for(pcb=uip_tcp_active_pcbs;pcb!=NULL;pcb=pcb->next) {
	  if(pcb->local_port==port) goto again;
  }
  for(pcb=uip_tcp_tw_pcbs;pcb!=NULL;pcb=pcb->next) {
	  if(pcb->local_port==port) goto again;
  }
  for(pcb=(struct uip_tcp_pcb*)uip_tcp_listen_pcbs.pcbs;pcb!=NULL;pcb=pcb->next) {
	  if(pcb->local_port==port) goto again;
  }
  return port;
}

static void uip_tcp_parseopt(struct uip_tcp_pcb *pcb)
{
	u8_t c;
	u8_t *opts,opt;
	u16_t mss;

	opts = (u8_t*)uip_tcphdr+UIP_TCP_HLEN;
	if(UIP_TCPH_HDRLEN(uip_tcphdr)>0x05) {
		for(c=0;c<((UIP_TCPH_HDRLEN(uip_tcphdr)-5)<<2);) {
			opt = opts[c];
			if(opt==0x00) break;
			else if(opt==0x01) c++;
			else if(opt==0x02 && opts[c+1]==0x04) {
				mss = (opts[c+2]<<8|opts[c+3]);
				pcb->mss = mss>UIP_TCP_MSS?UIP_TCP_MSS:mss;
				break;
			} else {
				if(opts[c+1]==0) break;
				c += opts[c+1];
			}
		}
	}
}

static void uip_tcpoutput_segments(struct uip_tcpseg *seg,struct uip_tcp_pcb *pcb)
{
	u16_t len;
	struct uip_netif *netif;

	seg->tcphdr->ackno = htonl(pcb->rcv_nxt);
	if(pcb->rcv_wnd<pcb->mss) seg->tcphdr->wnd = 0;
	else seg->tcphdr->wnd = htons(pcb->rcv_wnd);

	if(ip_addr_isany(&pcb->local_ip)) {
		netif = uip_iproute(&pcb->remote_ip);
		if(netif==NULL) {
			UIP_ERROR("uip_tcpoutput_segments: no route found.");
			return;
		}
		ip_addr_set(&pcb->local_ip,&netif->ip_addr);
	}

	pcb->rtime = 0;
	if(pcb->rttest==0) {
		pcb->rttest = uip_tcp_ticks;
		pcb->rtseq = ntohl(seg->tcphdr->seqno);
	}

	len = (u16_t)((u8_t*)seg->tcphdr-(u8_t*)seg->p->payload);
	seg->p->len -= len;
	seg->p->tot_len -= len;
	seg->p->payload = seg->tcphdr;

	seg->tcphdr->chksum = 0;
	seg->tcphdr->chksum = uip_chksum_pseudo(seg->p,&pcb->local_ip,&pcb->remote_ip,UIP_PROTO_TCP,seg->p->tot_len);

	uip_ipoutput(seg->p,&pcb->local_ip,&pcb->remote_ip,pcb->ttl,pcb->tos,UIP_PROTO_TCP);
}

static s8_t uip_tcpinput_listen(struct uip_tcp_pcb_listen *pcb)
{
	u32_t optdata;
	struct uip_tcp_pcb *npcb;

	if(uip_flags&UIP_TCP_ACK) {
		UIP_LOG("uip_tcp_listen_input: ACK in LISTEN, sending reset.\n");
		uip_tcp_rst(uip_ackno+1,uip_seqno+uip_tcplen,&uip_iphdr->dst,&uip_iphdr->src,uip_tcphdr->dst,uip_tcphdr->src);
	} else if(uip_flags&UIP_TCP_SYN) {
		npcb = uip_tcp_pcballoc(pcb->prio);
		if(!npcb) return UIP_ERR_MEM;

		ip_addr_set(&npcb->local_ip,&uip_iphdr->dst);
		npcb->local_port = pcb->local_port;
		ip_addr_set(&npcb->remote_ip,&uip_iphdr->src);
		npcb->remote_port = uip_tcphdr->src;
		npcb->state = UIP_SYN_RCVD;
		npcb->rcv_nxt = uip_seqno+1;
		npcb->snd_wnd = uip_tcphdr->wnd;
		npcb->ssthresh = npcb->snd_wnd;
		npcb->snd_wl1 = uip_seqno-1;
		npcb->cb_arg = pcb->cb_arg;
		npcb->accept = pcb->accept;

		npcb->so_options = pcb->so_options&(UIP_SOF_DEBUG|UIP_SOF_DONTROUTE|UIP_SOF_KEEPALIVE|UIP_SOF_OOBINLINE|UIP_SOF_LINGER);

		UIP_TCP_REG(&uip_tcp_active_pcbs,npcb);

		uip_tcp_parseopt(npcb);

		optdata = htonl((((u32_t)2<<24)|((u32_t)4<<16)|(((u32_t)npcb->mss/256)<<8)|((u32_t)npcb->mss&255)));

		uip_tcpenqueue(npcb,NULL,0,UIP_TCP_SYN|UIP_TCP_ACK,0,(u8_t*)&optdata,4);
		return uip_tcpoutput(npcb);
	}
	return UIP_ERR_OK;
}

static s8_t uip_tcpinput_timewait(struct uip_tcp_pcb *pcb)
{
	if(UIP_TCP_SEQ_GT(uip_seqno+uip_tcplen,pcb->rcv_nxt)) pcb->rcv_nxt = uip_seqno+uip_tcplen;
	if(uip_tcplen>0) {
		uip_tcp_acknow(pcb);
	}

	return uip_tcpoutput(pcb);
}

static s8_t uip_tcpprocess(struct uip_tcp_pcb *pcb)
{
	struct uip_tcpseg *rseg;
	u8_t acceptable = 0;
	s8_t err;

	err = 0;
	if(uip_flags&UIP_TCP_RST) {
		if(pcb->state==UIP_SYN_SENT) {
			if(uip_ackno==pcb->snd_nxt) acceptable = 1;
		} else {
			if(UIP_TCP_SEQ_BETWEEN(uip_seqno,pcb->rcv_nxt,pcb->rcv_nxt+pcb->rcv_wnd)) acceptable = 1;
		}
		if(acceptable) {
			uip_recv_flags = UIP_TF_RESET;
			pcb->flags &= ~UIP_TF_ACK_DELAY;
			return UIP_ERR_RST;
		} else
			return UIP_ERR_OK;
	}

	pcb->tmr = uip_tcp_ticks;
	pcb->keepcnt = 0;

	switch(pcb->state) {
		case UIP_SYN_SENT:
			if(uip_flags&UIP_TCP_ACK && uip_flags&UIP_TCP_SYN &&
				uip_ackno==ntohl(pcb->unacked->tcphdr->seqno)+1) {
				pcb->snd_buf++;
				pcb->rcv_nxt = uip_seqno+1;
				pcb->lastack = uip_ackno;
				pcb->snd_wnd = uip_tcphdr->wnd;
				pcb->snd_wl1 = uip_seqno-1;
				pcb->state = UIP_ESTABLISHED;
				pcb->cwnd = pcb->mss;
				pcb->snd_queuelen--;

				rseg = pcb->unacked;
				pcb->unacked = rseg->next;
				uip_tcpseg_free(rseg);

				uip_tcp_parseopt(pcb);

				if(pcb->connected!=NULL) err = pcb->connected(pcb->cb_arg,pcb,UIP_ERR_OK);

				uip_tcp_ack(pcb);
			} else if(uip_flags&UIP_TCP_ACK) {
				uip_tcp_rst(uip_ackno,uip_seqno+uip_tcplen,&uip_iphdr->dst,&uip_iphdr->src,uip_tcphdr->dst,uip_tcphdr->src);
			}
			break;
		case UIP_SYN_RCVD:
			if(uip_flags&UIP_TCP_ACK && !(uip_flags&UIP_TCP_RST)) {
				if(UIP_TCP_SEQ_BETWEEN(uip_ackno,pcb->lastack+1,pcb->snd_nxt)) {
					pcb->state = UIP_ESTABLISHED;

					if(pcb->accept!=NULL) err = pcb->accept(pcb->cb_arg,pcb,UIP_ERR_OK);
					if(err!=UIP_ERR_OK)	 {
						uip_tcp_abort(pcb);
						return UIP_ERR_ABRT;
					}
					uip_tcpreceive(pcb);
					pcb->cwnd = pcb->mss;
				} else {
					uip_tcp_rst(uip_ackno,uip_seqno+uip_tcplen,&uip_iphdr->dst,&uip_iphdr->src,uip_tcphdr->dst,uip_tcphdr->src);
				}
			}
			break;
		case UIP_CLOSE_WAIT:
		case UIP_ESTABLISHED:
			uip_tcpreceive(pcb);
			if(uip_flags&UIP_TCP_FIN) {
				uip_tcp_acknow(pcb);
				pcb->state = UIP_CLOSE_WAIT;
			}
			break;
		case UIP_FIN_WAIT_1:
			uip_tcpreceive(pcb);
			if(uip_flags&UIP_TCP_FIN) {
				if(uip_flags&UIP_TCP_ACK && uip_ackno==pcb->snd_nxt) {
					uip_tcp_acknow(pcb);
					uip_tcp_pcbpurge(pcb);
					UIP_TCP_RMV(&uip_tcp_active_pcbs,pcb);
					pcb->state = UIP_TIME_WAIT;
					UIP_TCP_REG(&uip_tcp_tw_pcbs,pcb);
				} else {
					uip_tcp_acknow(pcb);
					pcb->state = UIP_CLOSING;
				}
			} else if(uip_flags&UIP_TCP_ACK && uip_ackno==pcb->snd_nxt) {
				pcb->state = UIP_FIN_WAIT_2;
			}
			break;
		case UIP_FIN_WAIT_2:
			uip_tcpreceive(pcb);
			if(uip_flags&UIP_TCP_FIN) {
				uip_tcp_acknow(pcb);
				uip_tcp_pcbpurge(pcb);
				UIP_TCP_RMV(&uip_tcp_active_pcbs,pcb);
				pcb->state = UIP_TIME_WAIT;
				UIP_TCP_REG(&uip_tcp_tw_pcbs,pcb);
			}
			break;
		case UIP_CLOSING:
			uip_tcpreceive(pcb);
			if(uip_flags&UIP_TCP_ACK && uip_ackno==pcb->snd_nxt) {
				uip_tcp_acknow(pcb);
				uip_tcp_pcbpurge(pcb);
				UIP_TCP_RMV(&uip_tcp_active_pcbs,pcb);
				pcb->state = UIP_TIME_WAIT;
				UIP_TCP_REG(&uip_tcp_tw_pcbs,pcb);
			}
			break;
		case UIP_LAST_ACK:
			uip_tcpreceive(pcb);
			if(uip_flags&UIP_TCP_ACK && uip_ackno==pcb->snd_nxt) {
				pcb->state = UIP_CLOSED;
				uip_recv_flags = UIP_TF_CLOSED;
			}
			break;
		default:
			break;

	}
	return UIP_ERR_OK;
}

static void uip_tcpreceive(struct uip_tcp_pcb *pcb)
{
	struct uip_tcpseg *next,*prev;
	struct uip_tcpseg *cseg;
	struct uip_pbuf *p;
	s32_t off,m;
	u32_t right_wnd_edge;
	u16_t new_tot_len;

	if(uip_flags&UIP_TCP_ACK) {
		right_wnd_edge = pcb->snd_wnd+pcb->snd_wl1;
		if(UIP_TCP_SEQ_LT(pcb->snd_wl1,uip_seqno) ||
			(pcb->snd_wl1==uip_seqno && UIP_TCP_SEQ_LT(pcb->snd_wl2,uip_ackno)) ||
			(pcb->snd_wl2==uip_ackno && uip_tcphdr->wnd>pcb->snd_wnd)) {
			pcb->snd_wnd = uip_tcphdr->wnd;
			pcb->snd_wl1 = uip_seqno;
			pcb->snd_wl2 = uip_ackno;
		}

		if(pcb->lastack==uip_ackno) {
			pcb->acked = 0;
			if(pcb->snd_wl1+pcb->snd_wnd==right_wnd_edge) {
				pcb->dupacks++;
				if(pcb->dupacks>=3 && pcb->unacked!=NULL) {
					if(!(uip_flags&UIP_TF_INFR)) {
						UIP_LOG("uip_tcpreceive: dupacks, fast retransmit.");
						uip_tcp_rexmit(pcb)	;

						if(pcb->cwnd>pcb->snd_wnd) pcb->ssthresh = pcb->snd_wnd/2;
						else pcb->ssthresh = pcb->cwnd/2;

						pcb->cwnd = pcb->ssthresh+3*pcb->mss;
						pcb->flags |= UIP_TF_INFR;
					} else {
						if((u16_t)(pcb->cwnd+pcb->mss)>pcb->cwnd) pcb->cwnd += pcb->mss;
					}
				}
			}
		} else if(UIP_TCP_SEQ_BETWEEN(uip_ackno,pcb->lastack+1,pcb->snd_max)) {
			if(pcb->flags&UIP_TF_INFR) {
				pcb->flags &= ~UIP_TF_INFR;
				pcb->cwnd = pcb->ssthresh;
			}

			pcb->nrtx = 0;
			pcb->rto = (pcb->sa>>3)+pcb->sv;
			pcb->acked = uip_ackno-pcb->lastack;
			pcb->snd_buf += pcb->acked;
			pcb->dupacks = 0;
			pcb->lastack = uip_ackno;

			if(pcb->state>=UIP_ESTABLISHED) {
				if(pcb->cwnd<pcb->ssthresh) {
					if((u16_t)(pcb->cwnd+pcb->mss)>pcb->cwnd) pcb->cwnd += pcb->mss;

				} else {
					u16_t new_cwnd = (pcb->cwnd+pcb->mss*pcb->mss/pcb->cwnd);
					if(new_cwnd>pcb->cwnd) pcb->cwnd = new_cwnd;
				}
			}

			while(pcb->unacked!=NULL &&
				UIP_TCP_SEQ_LEQ(ntohl(pcb->unacked->tcphdr->seqno)+UIP_TCP_TCPLEN(pcb->unacked),uip_ackno)) {

				next = pcb->unacked;
				pcb->unacked = pcb->unacked->next;
				pcb->snd_queuelen -= uip_pbuf_clen(next->p);

				uip_tcpseg_free(next);
			}
			pcb->polltmr = 0;
		}

		while(pcb->unsent!=NULL &&
			UIP_TCP_SEQ_BETWEEN(uip_ackno,ntohl(pcb->unsent->tcphdr->seqno)+UIP_TCP_TCPLEN(pcb->unsent),pcb->snd_max)) {

			next = pcb->unsent;
			pcb->unsent = pcb->unsent->next;
			pcb->snd_queuelen -= uip_pbuf_clen(next->p);

			uip_tcpseg_free(next);

			if(pcb->unsent!=NULL) pcb->snd_nxt = htonl(pcb->unsent->tcphdr->seqno);
		}

		if(pcb->rttest && UIP_TCP_SEQ_LT(pcb->rtseq,uip_ackno)) {
			m = uip_tcp_ticks - pcb->rttest;
			m = m - (pcb->sa>>3);
			pcb->sa += m;

			if(m<0) m -= m;

			m = m - (pcb->sv>>2);
			pcb->sv += m;
			pcb->rto = (pcb->sa>>3)+pcb->sv;

			pcb->rttest = 0;
		}
	}

	if(uip_tcplen>0) {
		if(UIP_TCP_SEQ_BETWEEN(pcb->rcv_nxt,uip_seqno+1,uip_seqno+uip_tcplen-1)) {
			off = pcb->rcv_nxt - uip_seqno;
			p = uip_inseg.p;
			if(uip_inseg.p->len<off) {
				new_tot_len = uip_inseg.p->tot_len - off;
				while(p->len<off) {
					off -= p->len;
					p->tot_len = new_tot_len;
					p->len = 0;
					p = p->next;
				}
				uip_pbuf_header(p,-off);
			} else {
				uip_pbuf_header(uip_inseg.p,-off);
			}

			uip_inseg.dataptr = p->payload;
			uip_inseg.len -= pcb->rcv_nxt - uip_seqno;
			uip_inseg.tcphdr->seqno = uip_seqno = pcb->rcv_nxt;
		} else {
			if(UIP_TCP_SEQ_LT(uip_seqno,pcb->rcv_nxt)) {
				UIP_LOG("uip_tcpreceive: duplicate seqno.");
				uip_tcp_acknow(pcb);
			}
		}

		if(UIP_TCP_SEQ_BETWEEN(uip_seqno,pcb->rcv_nxt,pcb->rcv_nxt+pcb->rcv_wnd-1)) {
			//printf("uip_tcpreceive: seqno = %u, rcv_nxt = %u, wnd = %u\n",uip_seqno,pcb->rcv_nxt,(pcb->rcv_nxt+pcb->rcv_wnd-1));
			if(pcb->rcv_nxt==uip_seqno) {
				if(pcb->ooseq!=NULL && UIP_TCP_SEQ_LEQ(pcb->ooseq->tcphdr->seqno,uip_seqno+uip_inseg.len)) {
					uip_inseg.len = pcb->ooseq->tcphdr->seqno - uip_seqno;
					uip_pbuf_realloc(uip_inseg.p,uip_inseg.len);
				}

				uip_tcplen = UIP_TCP_TCPLEN(&uip_inseg);
				if(pcb->state!=UIP_CLOSE_WAIT) pcb->rcv_nxt += uip_tcplen;
				//printf("uip_tcpreceive: uip_tcplen = %u, rcv_nxt = %u\n",uip_tcplen,pcb->rcv_nxt);

				if(pcb->rcv_wnd<uip_tcplen) pcb->rcv_wnd = 0;
				else pcb->rcv_wnd -= uip_tcplen;

				if(uip_inseg.p->tot_len>0) {
					uip_recv_data = uip_inseg.p;
					uip_inseg.p = NULL;
				}

				if(UIP_TCPH_FLAGS(uip_inseg.tcphdr)&UIP_TCP_FIN) {
					UIP_LOG("uip_tcpreceive: received FIN.\n");
					uip_recv_flags = UIP_TF_GOT_FIN;
				}

				while(pcb->ooseq!=NULL && pcb->ooseq->tcphdr->seqno==pcb->rcv_nxt) {
					cseg = pcb->ooseq;
					uip_seqno = pcb->ooseq->tcphdr->seqno;

					pcb->rcv_nxt += UIP_TCP_TCPLEN(cseg);
					if(pcb->rcv_wnd<UIP_TCP_TCPLEN(cseg)) pcb->rcv_wnd = 0;
					else pcb->rcv_wnd -= UIP_TCP_TCPLEN(cseg);

					if(cseg->p->tot_len>0) {
						if(uip_recv_data) uip_pbuf_cat(uip_recv_data,cseg->p);
						else uip_recv_data = cseg->p;

						cseg->p = NULL;
					}

					if(UIP_TCPH_FLAGS(cseg->tcphdr)&UIP_TCP_FIN) {
						UIP_LOG("uip_tcpreceive: dequeued FIN.\n");
						uip_recv_flags = UIP_TF_GOT_FIN;
					}

					pcb->ooseq = cseg->next;
					uip_tcpseg_free(cseg);
				}
				//printf("uip_tcpreceive: pcb->flags = %02x\n",pcb->flags);
				uip_tcp_ack(pcb);
				//printf("uip_tcpreceive: pcb->flags = %02x\n",pcb->flags);
			} else {
				UIP_LOG("uip_tcpreceive: packet out-of-sequence.");
				uip_tcp_acknow(pcb);
				if(pcb->ooseq==NULL)
					uip_tcpseg_copy(&uip_inseg);
				else {
					prev = NULL;
					for(next=pcb->ooseq;next!=NULL;next=next->next) {
						if(uip_seqno==next->tcphdr->seqno) {
							if(uip_inseg.len>next->len) {
								cseg = uip_tcpseg_copy(&uip_inseg);
								if(cseg!=NULL) {
									cseg->next = next->next;
									if(prev!=NULL) prev->next = cseg;
									else pcb->ooseq = cseg;
								}
								break;
							} else
								break;
						} else {
							if(prev==NULL) {
								if(UIP_TCP_SEQ_LT(uip_seqno,next->tcphdr->seqno)) {
									if(UIP_TCP_SEQ_GT(uip_seqno+uip_inseg.len,next->tcphdr->seqno)) {
										uip_inseg.len = next->tcphdr->seqno - uip_seqno;
										uip_pbuf_realloc(uip_inseg.p,uip_inseg.len);
									}

									cseg = uip_tcpseg_copy(&uip_inseg);
									if(cseg!=NULL) {
										cseg->next = next;
										pcb->ooseq = cseg;
									}
									break;
								}
							} else if(UIP_TCP_SEQ_BETWEEN(uip_seqno,prev->tcphdr->seqno+1,next->tcphdr->seqno-1)) {
								if(UIP_TCP_SEQ_GT(uip_seqno+uip_inseg.len,next->tcphdr->seqno)) {
									uip_inseg.len = next->tcphdr->seqno - uip_seqno;
									uip_pbuf_realloc(uip_inseg.p,uip_inseg.len);
								}

								cseg = uip_tcpseg_copy(&uip_inseg);
								if(cseg!=NULL) {
									cseg->next = next;
									prev->next = cseg;
									if(UIP_TCP_SEQ_GT(prev->tcphdr->seqno+prev->len,uip_seqno)) {
										prev->len = uip_seqno - prev->tcphdr->seqno;
										uip_pbuf_realloc(prev->p,prev->len);
									}
								}
								break;
							}

							if(next->next==NULL && UIP_TCP_SEQ_GT(uip_seqno,next->tcphdr->seqno)) {
								next->next = uip_tcpseg_copy(&uip_inseg);
								if(next->next!=NULL) {
									if(UIP_TCP_SEQ_GT(next->tcphdr->seqno+next->len,uip_seqno)) {
										next->len = uip_seqno - next->tcphdr->seqno;
										uip_pbuf_realloc(next->p,next->len);
									}
								}
							}
						}
						prev = next;
					}
				}
			}
		} else {
			if(!UIP_TCP_SEQ_BETWEEN(uip_seqno,pcb->rcv_nxt,pcb->rcv_nxt+pcb->rcv_wnd-1)) {
				uip_tcp_acknow(pcb);
			}
		}
	} else {
		if(!UIP_TCP_SEQ_BETWEEN(uip_seqno,pcb->rcv_nxt,pcb->rcv_nxt+pcb->rcv_wnd-1)) {
			uip_tcp_acknow(pcb);
		}
	}
}

static s8_t uip_tcp_nullaccept(void *arg,struct uip_tcp_pcb *pcb,s8_t err)
{
	return UIP_ERR_ABRT;
}

static s8_t uip_tcp_nullrecv(void *arg,struct uip_tcp_pcb *pcb,struct uip_pbuf *p,s8_t err)
{
	if(p!=NULL) uip_pbuf_free(p);
	else if(err==UIP_ERR_OK) return uip_tcp_close(pcb);

	return UIP_ERR_OK;
}
