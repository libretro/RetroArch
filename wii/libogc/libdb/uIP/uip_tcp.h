#ifndef __UIP_TCP_H__
#define __UIP_TCP_H__

#include "uip.h"
#include "uip_ip.h"
#include "uip_pbuf.h"

#define UIP_TCP_PRIO_MIN			1
#define UIP_TCP_PRIO_NORMAL			64
#define UIP_TCP_PRIO_MAX			127

/* Keepalive values */
#define UIP_TCP_KEEPDEFAULT			7200000                       /* KEEPALIVE timer in miliseconds */
#define UIP_TCP_KEEPINTVL		    75000                         /* Time between KEEPALIVE probes in miliseconds */
#define UIP_TCP_KEEPCNT				9                             /* Counter for KEEPALIVE probes */
#define UIP_TCP_MAXIDLE				(UIP_TCP_KEEPCNT*UIP_TCP_KEEPINTVL)   /* Maximum KEEPALIVE probe time */

#define UIP_TCP_TMR_INTERVAL		250
#define UIP_TCP_SLOW_INTERVAL		(2*UIP_TCP_TMR_INTERVAL)

#define UIP_TCP_OOSEQ_TIMEOUT       6 /* x RTO */

#define UIP_TCP_MSL					60000  /* The maximum segment lifetime in microseconds */

#define UIP_TCP_SEQ_LT(a,b)			((s32_t)((a)-(b)) < 0)
#define UIP_TCP_SEQ_LEQ(a,b)		((s32_t)((a)-(b)) <= 0)
#define UIP_TCP_SEQ_GT(a,b)			((s32_t)((a)-(b)) > 0)
#define UIP_TCP_SEQ_GEQ(a,b)		((s32_t)((a)-(b)) >= 0)

#define UIP_TCP_SEQ_BETWEEN(a,b,c)	(UIP_TCP_SEQ_GEQ(a,b) && UIP_TCP_SEQ_LEQ(a,c))
#define UIP_TCP_FIN					0x01U
#define UIP_TCP_SYN					0x02U
#define UIP_TCP_RST					0x04U
#define UIP_TCP_PSH					0x08U
#define UIP_TCP_ACK					0x10U
#define UIP_TCP_URG					0x20U
#define UIP_TCP_ECE					0x40U
#define UIP_TCP_CWR					0x80U

#define UIP_TCP_FLAGS				0x3fU

/* Length of the TCP header, excluding options. */
#define UIP_TCP_HLEN				20

#define UIP_TCP_FIN_WAIT_TIMEOUT	20000 /* milliseconds */
#define UIP_TCP_SYN_RCVD_TIMEOUT	20000 /* milliseconds */

#define UIP_TCPH_OFFSET(phdr)		(ntohs((phdr)->_hdrlen_rsvd_flags) >> 8)
#define UIP_TCPH_HDRLEN(phdr)		(ntohs((phdr)->_hdrlen_rsvd_flags) >> 12)
#define UIP_TCPH_FLAGS(phdr)		(ntohs((phdr)->_hdrlen_rsvd_flags) & UIP_TCP_FLAGS)

#define UIP_TCPH_OFFSET_SET(phdr, offset)	(phdr)->_hdrlen_rsvd_flags = htons(((offset) << 8)|UIP_TCPH_FLAGS(phdr))
#define UIP_TCPH_HDRLEN_SET(phdr, len)		(phdr)->_hdrlen_rsvd_flags = htons(((len)<<12)|UIP_TCPH_FLAGS(phdr))
#define UIP_TCPH_FLAGS_SET(phdr, flags)		(phdr)->_hdrlen_rsvd_flags = htons((ntohs((phdr)->_hdrlen_rsvd_flags)&~UIP_TCP_FLAGS)|(flags))
#define UIP_TCPH_SET_FLAG(phdr, flags )		(phdr)->_hdrlen_rsvd_flags = htons(ntohs((phdr)->_hdrlen_rsvd_flags)|(flags))
#define UIP_TCPH_UNSET_FLAG(phdr, flags)	(phdr)->_hdrlen_rsvd_flags = htons(ntohs((phdr)->_hdrlen_rsvd_flags)|(UIP_TCPH_FLAGS(phdr)&~(flags)) )

#define UIP_TCP_TCPLEN(seg)					((seg)->len+((UIP_TCPH_FLAGS((seg)->tcphdr)&UIP_TCP_FIN || UIP_TCPH_FLAGS((seg)->tcphdr)&UIP_TCP_SYN)?1:0))

#define UIP_TCP_REG(pcbs,npcb)				\
	do {									\
		npcb->next = *pcbs;					\
		*(pcbs) = npcb;						\
		tcp_tmr_needed();					\
	} while(0)

#define UIP_TCP_RMV(pcbs,npcb)				\
	do {									\
		if(*(pcbs)==npcb) {					\
			*(pcbs) = (*pcbs)->next;		\
		} else {							\
			for(uip_tcp_tmp_pcb=*pcbs;uip_tcp_tmp_pcb!=NULL;uip_tcp_tmp_pcb=uip_tcp_tmp_pcb->next) {	\
				if(uip_tcp_tmp_pcb->next!=NULL && uip_tcp_tmp_pcb->next==npcb) {						\
					uip_tcp_tmp_pcb->next = npcb->next;													\
					break;					\
				}							\
			}								\
		}									\
	} while(0)

#define uip_tcp_sndbuf(pcb)					(pcb)->snd_buf

#define uip_tcp_acknow(pcb)					\
	(pcb)->flags |= UIP_TF_ACK_NOW;			\
	uip_tcpoutput((pcb))

#define uip_tcp_ack(pcb)					\
	if((pcb)->flags&UIP_TF_ACK_DELAY) {		\
		(pcb)->flags &= ~UIP_TF_ACK_DELAY;	\
		(pcb)->flags |= UIP_TF_ACK_NOW;		\
		uip_tcpoutput((pcb));				\
	} else {								\
		(pcb)->flags |= UIP_TF_ACK_DELAY;	\
	}

/* The TCP Header */
PACK_STRUCT_BEGIN
struct uip_tcp_hdr {
	PACK_STRUCT_FIELD(u16_t src);
	PACK_STRUCT_FIELD(u16_t dst);
	PACK_STRUCT_FIELD(u32_t seqno);
	PACK_STRUCT_FIELD(u32_t ackno);
	PACK_STRUCT_FIELD(u16_t _hdrlen_rsvd_flags);
	PACK_STRUCT_FIELD(u16_t wnd);
	PACK_STRUCT_FIELD(u16_t chksum);
	PACK_STRUCT_FIELD(u16_t urgp);
} PACK_STRUCT_STRUCT;
PACK_STRUCT_END

enum uip_tcp_state {
	UIP_CLOSED      = 0,
	UIP_LISTEN      = 1,
	UIP_SYN_SENT    = 2,
	UIP_SYN_RCVD    = 3,
	UIP_ESTABLISHED = 4,
	UIP_FIN_WAIT_1  = 5,
	UIP_FIN_WAIT_2  = 6,
	UIP_CLOSE_WAIT  = 7,
	UIP_CLOSING     = 8,
	UIP_LAST_ACK    = 9,
	UIP_TIME_WAIT   = 10
};

struct uip_tcpseg {
	struct uip_tcpseg *next;
	struct uip_pbuf *p;
	u8_t *dataptr;
	u16_t len;
	struct uip_tcp_hdr *tcphdr;
};

struct uip_tcp_pcb {
	UIP_IP_PCB;

	struct uip_tcp_pcb *next;
	enum uip_tcp_state state;

	u8_t prio;
	void *cb_arg;

	u16_t local_port;
	u16_t remote_port;

	u8_t flags;
#define UIP_TF_ACK_DELAY (u8_t)0x01U   /* Delayed ACK. */
#define UIP_TF_ACK_NOW   (u8_t)0x02U   /* Immediate ACK. */
#define UIP_TF_INFR      (u8_t)0x04U   /* In fast recovery. */
#define UIP_TF_RESET     (u8_t)0x08U   /* Connection was reset. */
#define UIP_TF_CLOSED    (u8_t)0x10U   /* Connection was sucessfully closed. */
#define UIP_TF_GOT_FIN   (u8_t)0x20U   /* Connection was closed by the remote end. */
#define UIP_TF_NODELAY   (u8_t)0x40U   /* Disable Nagle algorithm */

	u32_t rcv_nxt;
	u16_t rcv_wnd;

	u32_t tmr;
	u8_t polltmr,pollinterval;

	u16_t rtime;
	u16_t mss;

	u32_t rttest;
	u32_t rtseq;
	s16_t sa,sv;

	u16_t rto;
	u8_t nrtx;

	u32_t lastack;
	u8_t dupacks;

	u16_t cwnd;
	u16_t ssthresh;

	u32_t snd_nxt,snd_max,snd_wnd,snd_wl1,snd_wl2,snd_lbb;

	u16_t acked;
	u16_t snd_buf;
	u8_t snd_queuelen;

	struct uip_tcpseg *unsent;
	struct uip_tcpseg *unacked;
	struct uip_tcpseg *ooseq;

	s8_t (*accept)(void *arg,struct uip_tcp_pcb *newpcb,s8_t err);
	s8_t (*connected)(void *arg,struct uip_tcp_pcb *newpcb,s8_t err);
	s8_t (*poll)(void *arg,struct uip_tcp_pcb *pcb);

	s8_t (*sent)(void *arg,struct uip_tcp_pcb *pcb,u16_t space);
	s8_t (*recv)(void *arg,struct uip_tcp_pcb *pcb,struct uip_pbuf *p,s8_t err);

	void (*errf)(void *arg,s8_t err);

	u32_t keepalive;
	u8_t keepcnt;
};

struct uip_tcp_pcb_listen {
	UIP_IP_PCB;

	struct uip_tcp_pcb_listen *next;
	enum uip_tcp_state state;

	u8_t prio;
	void *cb_arg;

	u16_t local_port;

	s8_t (*accept)(void *arg,struct uip_tcp_pcb *newpcb,s8_t err);
};

union uip_tcp_listen_pcbs_t {
	struct uip_tcp_pcb_listen *listen_pcbs;
	struct uip_tcp_pcb *pcbs;
};

extern struct uip_tcp_pcb *uip_tcp_tmp_pcb;
extern struct uip_tcp_pcb *uip_tcp_active_pcbs;
extern struct uip_tcp_pcb *uip_tcp_tw_pcbs;
extern union uip_tcp_listen_pcbs_t uip_tcp_listen_pcbs;

void uip_tcp_tmr();
void uip_tcp_slowtmr();
void uip_tcp_fasttmr();
void uip_tcp_init();
void uip_tcp_rst(u32_t seqno,u32_t ackno,struct uip_ip_addr *lipaddr,struct uip_ip_addr *ripaddr,u16_t lport,u16_t rport);
void uip_tcp_abort(struct uip_tcp_pcb *pcb);
void uip_tcp_pcbremove(struct uip_tcp_pcb **pcblist,struct uip_tcp_pcb *pcb);
void uip_tcp_pcbpurge(struct uip_tcp_pcb *pcb);
void uip_tcp_rexmit(struct uip_tcp_pcb *pcb);
void uip_tcp_rexmit_rto(struct uip_tcp_pcb *pcb);
void uip_tcp_accept(struct uip_tcp_pcb *pcb,s8_t (*accept)(void *,struct uip_tcp_pcb *,s8_t));
void uip_tcp_recved(struct uip_tcp_pcb *pcb,u16_t len);
void uip_tcp_err(struct uip_tcp_pcb *pcb,void (*errf)(void *,s8_t));
void uip_tcp_keepalive(struct uip_tcp_pcb *pcb);
void uip_tcp_arg(struct uip_tcp_pcb *pcb,void *arg);
void uip_tcp_recv(struct uip_tcp_pcb *pcb,s8_t (*recv)(void *,struct uip_tcp_pcb *,struct uip_pbuf *,s8_t));
void uip_tcp_sent(struct uip_tcp_pcb *pcb,s8_t (*sent)(void *,struct uip_tcp_pcb *,u16_t));
void uip_tcp_poll(struct uip_tcp_pcb *pcb,s8_t (*poll)(void *,struct uip_tcp_pcb *),u8_t interval);
s8_t uip_tcp_close(struct uip_tcp_pcb *pcb);
s8_t uip_tcp_bind(struct uip_tcp_pcb *pcb,struct uip_ip_addr *ipaddr,u16_t port);
s8_t uip_tcp_write(struct uip_tcp_pcb *pcb,const void *arg,u16_t len,u8_t copy);
struct uip_tcp_pcb* uip_tcp_listen(struct uip_tcp_pcb *pcb);
struct uip_tcp_pcb* uip_tcp_new();
struct uip_tcp_pcb* uip_tcp_pcballoc(u8_t prio);

s8_t uip_tcp_sendctrl(struct uip_tcp_pcb *pcb,u8_t flags);
s8_t uip_tcpoutput(struct uip_tcp_pcb *pcb);
s8_t uip_tcpenqueue(struct uip_tcp_pcb *pcb,void *arg,u16_t len,u8_t flags,u8_t copy,u8_t *optdata,u8_t optlen);
u8_t uip_tcpseg_free(struct uip_tcpseg *seg);
u8_t uip_tcpsegs_free(struct uip_tcpseg *seg);
u32_t uip_tcpiss_next();
void uip_tcpinput(struct uip_pbuf *p,struct uip_netif *inp);
struct uip_tcpseg* uip_tcpseg_copy(struct uip_tcpseg *seg);

#endif
