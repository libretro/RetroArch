#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <ogcsys.h>
#include <lwp.h>
#include <video.h>
#include <message.h>
#include <mutex.h>
#include <cond.h>
#include <semaphore.h>
#include <processor.h>
#include <lwp_threads.h>
#include <lwp_watchdog.h>
#include <lwip/debug.h>
#include <lwip/opt.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/sys.h>
#include <lwip/stats.h>
#include <lwip/ip.h>
#include <lwip/raw.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/dhcp.h>
#include <lwip/api.h>
#include <lwip/api_msg.h>
#include <lwip/tcpip.h>
#include <netif/etharp.h>
#include <netif/loopif.h>
#include <netif/gcif/gcif.h>

#include <sys/iosupport.h>

#include "network.h"

//#define _NET_DEBUG
#define ARP_TIMER_ID			0x00070041
#define TCP_TIMER_ID			0x00070042
#define DHCPCOARSE_TIMER_ID		0x00070043
#define DHCPFINE_TIMER_ID		0x00070044

#define STACKSIZE				32768
#define MQBOX_SIZE				256
#define NUM_SOCKETS				MEMP_NUM_NETCONN

struct netsocket {
	struct netconn *conn;
	struct netbuf *lastdata;
	u16 lastoffset,rcvevt,sendevt,flags;
	s32 err;
};

struct netselect_cb {
	struct netselect_cb *next;
	fd_set *readset;
	fd_set *writeset;
	fd_set *exceptset;
	u32 signaled;
	mutex_t cond_lck;
	cond_t cond;
};

typedef void (*apimsg_decode)(struct apimsg_msg *);

static u32 g_netinitiated = 0;
static u32 tcpiplayer_inited = 0;
static u64 net_tcp_ticks = 0;
static u64 net_dhcpcoarse_ticks = 0;
static u64 net_dhcpfine_ticks = 0;
static u64 net_arp_ticks = 0;
static wd_cntrl arp_time_cntrl;
static wd_cntrl tcp_timer_cntrl;
static wd_cntrl dhcp_coarsetimer_cntrl;
static wd_cntrl dhcp_finetimer_cntrl;

static struct netif g_hNetIF;
static struct netif g_hLoopIF;
static struct netsocket sockets[NUM_SOCKETS];
static struct netselect_cb *selectcb_list = NULL;

static sys_sem netsocket_sem;
static sys_sem sockselect_sem;
static sys_mbox netthread_mbox;

static sys_thread hnet_thread;
static u8 netthread_stack[STACKSIZE];

static u32 tcp_timer_active = 0;

static struct netbuf* netbuf_new();
static void netbuf_delete(struct netbuf *);
static void netbuf_copypartial(struct netbuf *,void *,u32,u32);
static void netbuf_ref(struct netbuf *,const void *,u32);

static struct netconn* netconn_new_with_callback(enum netconn_type,void (*)(struct netconn *,enum netconn_evt,u32));
static struct netconn* netconn_new_with_proto_and_callback(enum netconn_type,u16,void (*)(struct netconn *,enum netconn_evt,u32));
static err_t netconn_delete(struct netconn *);
static struct netconn* netconn_accept(struct netconn* );
static err_t netconn_peer(struct netconn *,struct ip_addr *,u16 *);
static err_t netconn_bind(struct netconn *,struct ip_addr *,u16);
static err_t netconn_listen(struct netconn *);
static struct netbuf* netconn_recv(struct netconn *);
static err_t netconn_send(struct netconn *,struct netbuf *);
static err_t netconn_write(struct netconn *,const void *,u32,u8);
static err_t netconn_connect(struct netconn *,struct ip_addr *,u16);
static err_t netconn_disconnect(struct netconn *);

static void do_newconn(struct apimsg_msg *);
static void do_delconn(struct apimsg_msg *);
static void do_bind(struct apimsg_msg *);
static void do_listen(struct apimsg_msg *);
static void do_connect(struct apimsg_msg *);
static void do_disconnect(struct apimsg_msg *);
static void do_accept(struct apimsg_msg *);
static void do_send(struct apimsg_msg *);
static void do_recv(struct apimsg_msg *);
static void do_write(struct apimsg_msg *);
static void do_close(struct apimsg_msg *);

static apimsg_decode decode[APIMSG_MAX] = {
	do_newconn,
	do_delconn,
	do_bind,
	do_connect,
	do_disconnect,
	do_listen,
	do_accept,
	do_send,
	do_recv,
	do_write,
	do_close
};

static void apimsg_post(struct api_msg *);

static err_t net_input(struct pbuf *,struct netif *);
static void net_apimsg(struct api_msg *);
static err_t net_callback(void (*)(void *),void *);
static void* net_thread(void *);

static void tmr_callback(void *arg)
{
	void (*functor)() = (void(*)())arg;
	if(functor) functor();
}

/* low level stuff */
static void __dhcpcoarse_timer(void *arg)
{
	__lwp_thread_dispatchdisable();
	net_callback(tmr_callback,(void*)dhcp_coarse_tmr);
	__lwp_wd_insert_ticks(&dhcp_coarsetimer_cntrl,net_dhcpcoarse_ticks);
	__lwp_thread_dispatchunnest();
}

static void __dhcpfine_timer(void *arg)
{
	__lwp_thread_dispatchdisable();
	net_callback(tmr_callback,(void*)dhcp_fine_tmr);
	__lwp_wd_insert_ticks(&dhcp_finetimer_cntrl,net_dhcpfine_ticks);
	__lwp_thread_dispatchunnest();
}

static void __tcp_timer(void *arg)
{
#ifdef _NET_DEBUG
	printf("__tcp_timer(%d,%p,%p)\n",tcp_timer_active,tcp_active_pcbs,tcp_tw_pcbs);
#endif
	__lwp_thread_dispatchdisable();
	net_callback(tmr_callback,(void*)tcp_tmr);
	if (tcp_active_pcbs || tcp_tw_pcbs) {
		__lwp_wd_insert_ticks(&tcp_timer_cntrl,net_tcp_ticks);
	} else
		tcp_timer_active = 0;
	__lwp_thread_dispatchunnest();
}

static void __arp_timer(void *arg)
{
	__lwp_thread_dispatchdisable();
	net_callback(tmr_callback,(void*)etharp_tmr);
	__lwp_wd_insert_ticks(&arp_time_cntrl,net_arp_ticks);
	__lwp_thread_dispatchunnest();
}

void tcp_timer_needed(void)
{
#ifdef _NET_DEBUG
	printf("tcp_timer_needed()\n");
#endif
	if(!tcp_timer_active && (tcp_active_pcbs || tcp_tw_pcbs)) {
		tcp_timer_active = 1;
		__lwp_wd_insert_ticks(&tcp_timer_cntrl,net_tcp_ticks);
	}
}

/* netbuf part */
static __inline__ u16 netbuf_len(struct netbuf *buf)
{
	return ((buf && buf->p)?buf->p->tot_len:0);
}

static __inline__ struct ip_addr* netbuf_fromaddr(struct netbuf *buf)
{
	return (buf?buf->fromaddr:NULL);
}

static __inline__ u16 netbuf_fromport(struct netbuf *buf)
{
	return (buf?buf->fromport:0);
}

static void netbuf_copypartial(struct netbuf *buf,void *dataptr,u32 len,u32 offset)
{
	struct pbuf *p;
	u16 i,left;

	left = 0;
	if(buf==NULL || dataptr==NULL) return;

	for(p=buf->p;left<len && p!=NULL;p=p->next) {
		if(offset!=0 && offset>=p->len)
			offset -= p->len;
		else {
			for(i=offset;i<p->len;i++) {
				((u8*)dataptr)[left] = ((u8*)p->payload)[i];
				if(++left>=len) return;
			}
			offset = 0;
		}
	}
}

static struct netbuf* netbuf_new()
{
	struct netbuf *buf = NULL;

	buf = memp_malloc(MEMP_NETBUF);
	if(buf) {
		buf->p = NULL;
		buf->ptr = NULL;
	}
	return buf;
}

static void netbuf_delete(struct netbuf *buf)
{
	if(buf!=NULL) {
		if(buf->p!=NULL) pbuf_free(buf->p);
		memp_free(MEMP_NETBUF,buf);
	}
}

static void netbuf_ref(struct netbuf *buf, const void *dataptr,u32 size)
{
	if(buf->p!=NULL) pbuf_free(buf->p);
	buf->p = pbuf_alloc(PBUF_TRANSPORT,0,PBUF_REF);
	buf->p->payload = (void*)dataptr;
	buf->p->len = buf->p->tot_len = size;
	buf->ptr = buf->p;
}

/* netconn part */
static inline enum netconn_type netconn_type(struct netconn *conn)
{
	return conn->type;
}

static struct netconn* netconn_new_with_callback(enum netconn_type t,void (*cb)(struct netconn*,enum netconn_evt,u32))
{
	return netconn_new_with_proto_and_callback(t,0,cb);
}

static struct netconn* netconn_new_with_proto_and_callback(enum netconn_type t,u16 proto,void (*cb)(struct netconn *,enum netconn_evt,u32))
{
	u32 dummy = 0;
	struct netconn *conn;
	struct api_msg *msg;

	conn = memp_malloc(MEMP_NETCONN);
	if(!conn) return NULL;

	conn->err = ERR_OK;
	conn->type = t;
	conn->pcb.tcp = NULL;

	if(MQ_Init(&conn->mbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) {
		memp_free(MEMP_NETCONN,conn);
		return NULL;
	}
	if(LWP_SemInit(&conn->sem,0,1)==-1) {
		MQ_Close(conn->mbox);
		memp_free(MEMP_NETCONN,conn);
		return NULL;
	}

	conn->acceptmbox = SYS_MBOX_NULL;
	conn->recvmbox = SYS_MBOX_NULL;
	conn->state = NETCONN_NONE;
	conn->socket = 0;
	conn->callback = cb;
	conn->recvavail = 0;

	msg = memp_malloc(MEMP_API_MSG);
	if(!msg) {
		MQ_Close(conn->mbox);
		memp_free(MEMP_NETCONN,conn);
		return NULL;
	}

	msg->type = APIMSG_NEWCONN;
	msg->msg.msg.bc.port = proto;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);

	if(conn->err!=ERR_OK) {
		MQ_Close(conn->mbox);
		memp_free(MEMP_NETCONN,conn);
		return NULL;
	}
	return conn;
}

static err_t netconn_delete(struct netconn *conn)
{
	u32 dummy = 0;
	struct api_msg *msg;
	sys_mbox mbox;
	void *mem;

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_delete(%p)\n", conn));

	if(!conn) return ERR_OK;

	msg = memp_malloc(MEMP_API_MSG);
	if(!msg) return ERR_MEM;

	msg->type = APIMSG_DELCONN;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);

	mbox = conn->recvmbox;
	conn->recvmbox = SYS_MBOX_NULL;
	if(mbox!=SYS_MBOX_NULL) {
		while(MQ_Receive(mbox,(mqmsg_t)&mem,MQ_MSG_NOBLOCK)==TRUE) {
			if(mem!=NULL) {
				if(conn->type==NETCONN_TCP)
					pbuf_free((struct pbuf*)mem);
				else
					netbuf_delete((struct netbuf*)mem);
			}
		}
		MQ_Close(mbox);
	}

	mbox = conn->acceptmbox;
	conn->acceptmbox = SYS_MBOX_NULL;
	if(mbox!=SYS_MBOX_NULL) {
		while(MQ_Receive(mbox,(mqmsg_t)&mem,MQ_MSG_NOBLOCK)==TRUE) {
			if(mem!=NULL) netconn_delete((struct netconn*)mem);
		}
		MQ_Close(mbox);
	}

	MQ_Close(conn->mbox);
	conn->mbox = SYS_MBOX_NULL;

	LWP_SemDestroy(conn->sem);
	conn->sem = SYS_SEM_NULL;

	memp_free(MEMP_NETCONN,conn);
	return ERR_OK;
}

static struct netconn* netconn_accept(struct netconn* conn)
{
	struct netconn *newconn;

	if(conn==NULL) return NULL;

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_accept(%p)\n", conn));
	MQ_Receive(conn->acceptmbox,(mqmsg_t)&newconn,MQ_MSG_BLOCK);
	if(conn->callback)
		(*conn->callback)(conn,NETCONN_EVTRCVMINUS,0);

	return newconn;
}

static err_t netconn_peer(struct netconn *conn,struct ip_addr *addr,u16 *port)
{
	switch(conn->type) {
		case NETCONN_RAW:
			return ERR_CONN;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			if(conn->pcb.udp==NULL || ((conn->pcb.udp->flags&UDP_FLAGS_CONNECTED)==0))
				return ERR_CONN;
			*addr = conn->pcb.udp->remote_ip;
			*port = conn->pcb.udp->remote_port;
			break;
		case NETCONN_TCP:
			if(conn->pcb.tcp==NULL)
				return ERR_CONN;
			*addr = conn->pcb.tcp->remote_ip;
			*port = conn->pcb.tcp->remote_port;
			break;
	}
	return (conn->err = ERR_OK);
}

static err_t netconn_bind(struct netconn *conn,struct ip_addr *addr,u16 port)
{
	u32 dummy = 0;
	struct api_msg *msg;

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_bind(%p)\n", conn));

	if(conn==NULL) return ERR_VAL;
	if(conn->type!=NETCONN_TCP && conn->recvmbox==SYS_MBOX_NULL) {
		if(MQ_Init(&conn->recvmbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) return ERR_MEM;
	}

	if((msg=memp_malloc(MEMP_API_MSG))==NULL)
		return (conn->err = ERR_MEM);

	msg->type = APIMSG_BIND;
	msg->msg.conn = conn;
	msg->msg.msg.bc.ipaddr = addr;
	msg->msg.msg.bc.port = port;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);
	return conn->err;
}

static err_t netconn_listen(struct netconn *conn)
{
	u32 dummy = 0;
	struct api_msg *msg;

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_listen(%p)\n", conn));

	if(conn==NULL) return -1;
	if(conn->acceptmbox==SYS_MBOX_NULL) {
		if(MQ_Init(&conn->acceptmbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) return ERR_MEM;
	}

	if((msg=memp_malloc(MEMP_API_MSG))==NULL) return (conn->err = ERR_MEM);
	msg->type = APIMSG_LISTEN;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);
	return conn->err;
}

static struct netbuf* netconn_recv(struct netconn *conn)
{
	u32 dummy = 0;
	struct api_msg *msg;
	struct netbuf *buf;
	struct pbuf *p;
	u16 len;

	if(conn==NULL) return NULL;
	if(conn->recvmbox==SYS_MBOX_NULL) {
		conn->err = ERR_CONN;
		return NULL;
	}
	if(conn->err!=ERR_OK) return NULL;

	if(conn->type==NETCONN_TCP) {
		if(conn->pcb.tcp->state==LISTEN) {
			conn->err = ERR_CONN;
			return NULL;
		}

		buf = memp_malloc(MEMP_NETBUF);
		if(buf==NULL) {
			conn->err = ERR_MEM;
			return NULL;
		}

		MQ_Receive(conn->recvmbox,(mqmsg_t)&p,MQ_MSG_BLOCK);
		if(p!=NULL) {
			len = p->tot_len;
			conn->recvavail -= len;
		} else
			len = 0;

		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVMINUS,len);

		if(p==NULL) {
			memp_free(MEMP_NETBUF,buf);
			MQ_Close(conn->recvmbox);
			conn->recvmbox = SYS_MBOX_NULL;
			return NULL;
		}

		buf->p = p;
		buf->ptr = p;
		buf->fromport = 0;
		buf->fromaddr = NULL;

		if((msg=memp_malloc(MEMP_API_MSG))==NULL) {
			conn->err = ERR_MEM;
			return buf;
		}

		msg->type = APIMSG_RECV;
		msg->msg.conn = conn;
		if(buf!=NULL)
			msg->msg.msg.len = len;
		else
			msg->msg.msg.len = 1;

		apimsg_post(msg);
		MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
		memp_free(MEMP_API_MSG,msg);
	} else {
		MQ_Receive(conn->recvmbox,(mqmsg_t)&buf,MQ_MSG_BLOCK);
		conn->recvavail -= buf->p->tot_len;
		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVMINUS,buf->p->tot_len);
	}

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_recv: received %p (err %d)\n", (void *)buf, conn->err));
	return buf;
}

static err_t netconn_send(struct netconn *conn,struct netbuf *buf)
{
	u32 dummy = 0;
	struct api_msg *msg;

	if(conn==NULL) return ERR_VAL;
	if(conn->err!=ERR_OK) return conn->err;
	if((msg=memp_malloc(MEMP_API_MSG))==NULL) return (conn->err = ERR_MEM);

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_send: sending %d bytes\n", buf->p->tot_len));
	msg->type = APIMSG_SEND;
	msg->msg.conn = conn;
	msg->msg.msg.p = buf->p;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);
	return conn->err;
}

static err_t netconn_write(struct netconn *conn,const void *dataptr,u32 size,u8 copy)
{
	u32 dummy = 0;
	struct api_msg *msg;
	u16 len,snd_buf;

	LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write(%d)\n",conn->err));

	if(conn==NULL) return ERR_VAL;
	if(conn->err!=ERR_OK) return conn->err;

	if((msg=memp_malloc(MEMP_API_MSG))==NULL) return (conn->err = ERR_MEM);

	msg->type = APIMSG_WRITE;
	msg->msg.conn = conn;
	conn->state = NETCONN_WRITE;
	while(conn->err==ERR_OK && size>0) {
		msg->msg.msg.w.dataptr = (void*)dataptr;
		msg->msg.msg.w.copy = copy;
		if(conn->type==NETCONN_TCP) {
			while((snd_buf=tcp_sndbuf(conn->pcb.tcp))==0) {
				LWIP_DEBUGF(API_LIB_DEBUG,("netconn_write: tcp_sndbuf = 0,err = %d\n", conn->err));
				LWP_SemWait(conn->sem);
				if(conn->err!=ERR_OK) goto ret;
			}
			if(size>snd_buf)
				len = snd_buf;
			else
				len = size;
		} else
			len = size;

		LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write: writing %d bytes (%d)\n", len, copy));
		msg->msg.msg.w.len = len;
		apimsg_post(msg);
		MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
		if(conn->err==ERR_OK) {
			LWIP_DEBUGF(API_LIB_DEBUG, ("netconn_write: %d bytes written\n",len));
			dataptr = (void*)((char*)dataptr+len);
			size -= len;
		} else if(conn->err==ERR_MEM) {
			LWIP_DEBUGF(API_LIB_DEBUG,("netconn_write: mem err\n"));
			conn->err = ERR_OK;
			LWP_SemWait(conn->sem);
		} else {
			LWIP_DEBUGF(API_LIB_DEBUG,("netconn_write: err = %d\n", conn->err));
			break;
		}
	}
ret:
	memp_free(MEMP_API_MSG,msg);
	conn->state = NETCONN_NONE;

	return conn->err;
}

static err_t netconn_connect(struct netconn *conn,struct ip_addr *addr,u16 port)
{
	u32 dummy = 0;
	struct api_msg *msg;

	if(conn==NULL) return -1;
	if(conn->recvmbox==SYS_MBOX_NULL) {
		if(MQ_Init(&conn->recvmbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) return ERR_MEM;
	}

	if((msg=memp_malloc(MEMP_API_MSG))==NULL) return ERR_MEM;

	msg->type = APIMSG_CONNECT;
	msg->msg.conn = conn;
	msg->msg.msg.bc.ipaddr = addr;
	msg->msg.msg.bc.port = port;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);
	return conn->err;
}

static err_t netconn_disconnect(struct netconn *conn)
{
	u32 dummy = 0;
	struct api_msg *msg;

	if(conn==NULL) return ERR_VAL;
	if((msg=memp_malloc(MEMP_API_MSG))==NULL) return ERR_MEM;

	msg->type = APIMSG_DISCONNECT;
	msg->msg.conn = conn;
	apimsg_post(msg);
	MQ_Receive(conn->mbox,(mqmsg_t)&dummy,MQ_MSG_BLOCK);
	memp_free(MEMP_API_MSG,msg);
	return conn->err;
}

/* api msg part */
static u8_t recv_raw(void *arg,struct raw_pcb *pcb,struct pbuf *p,struct ip_addr *addr)
{
	struct netbuf *buf;
	struct netconn *conn = (struct netconn*)arg;

	if(!conn) return 0;

	if(conn->recvmbox!=SYS_MBOX_NULL) {
		if((buf=memp_malloc(MEMP_NETBUF))==NULL) return 0;

		pbuf_ref(p);
		buf->p = p;
		buf->ptr = p;
		buf->fromaddr = addr;
		buf->fromport = pcb->protocol;

		conn->recvavail += p->tot_len;
		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVPLUS,p->tot_len);
		MQ_Send(conn->recvmbox,buf,MQ_MSG_BLOCK);
	}
	return 0;
}

static void recv_udp(void *arg,struct udp_pcb *pcb,struct pbuf *p,struct ip_addr *addr,u16 port)
{
	struct netbuf *buf;
	struct netconn *conn = (struct netconn*)arg;

	if(!conn) {
		pbuf_free(p);
		return;
	}

	if(conn->recvmbox!=SYS_MBOX_NULL) {
		buf = memp_malloc(MEMP_NETBUF);
		if(!buf) {
			pbuf_free(p);
			return;
		}
		buf->p = p;
		buf->ptr = p;
		buf->fromaddr = addr;
		buf->fromport = port;

		conn->recvavail += p->tot_len;
		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVPLUS,p->tot_len);

		MQ_Send(conn->recvmbox,buf,MQ_MSG_BLOCK);
	}
}

static err_t recv_tcp(void *arg,struct tcp_pcb *pcb,struct pbuf *p,err_t err)
{
	u16 len;
	struct netconn *conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: recv_tcp(%p,%p,%p,%d)\n",arg,pcb,p,err));

	if(conn==NULL) {
		pbuf_free(p);
		return ERR_VAL;
	}

	if(conn->recvmbox!=SYS_MBOX_NULL) {
		conn->err = err;
		if(p!=NULL) {
			len = p->tot_len;
			conn->recvavail += len;
		} else len = 0;

		if(conn->callback)
			(*conn->callback)(conn,NETCONN_EVTRCVPLUS,len);

		MQ_Send(conn->recvmbox,p,MQ_MSG_BLOCK);
	}
	return ERR_OK;
}

static void err_tcp(void *arg,err_t err)
{
	u32 dummy = 0;
	struct netconn *conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: err_tcp: %d\n",err));
	if(conn) {
		conn->err = err;
		conn->pcb.tcp = NULL;
		if(conn->recvmbox!=SYS_MBOX_NULL) {
			if(conn->callback) (*conn->callback)(conn,NETCONN_EVTRCVPLUS,0);
			MQ_Send(conn->recvmbox,&dummy,MQ_MSG_BLOCK);
		}
		if(conn->mbox!=SYS_MBOX_NULL) {
			MQ_Send(conn->mbox,&dummy,MQ_MSG_BLOCK);
		}
		if(conn->acceptmbox!=SYS_MBOX_NULL) {
			if(conn->callback) (*conn->callback)(conn,NETCONN_EVTRCVPLUS,0);
			MQ_Send(conn->acceptmbox,&dummy,MQ_MSG_BLOCK);
		}
		if(conn->sem!=SYS_SEM_NULL) {
			LWP_SemPost(conn->sem);
		}
	}
}

static err_t poll_tcp(void *arg,struct tcp_pcb *pcb)
{
	struct netconn *conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: poll_tcp\n"));
	if(conn && conn->sem!=SYS_SEM_NULL && (conn->state==NETCONN_WRITE || conn->state==NETCONN_CLOSE))
		LWP_SemPost(conn->sem);

	return ERR_OK;
}

static err_t sent_tcp(void *arg,struct tcp_pcb *pcb,u16 len)
{
	struct netconn *conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: sent_tcp: sent %d bytes\n",len));
	if(conn && conn->sem!=SYS_SEM_NULL)
		LWP_SemPost(conn->sem);

	if(conn && conn->callback) {
		if(tcp_sndbuf(conn->pcb.tcp)>TCP_SNDLOWAT)
			(*conn->callback)(conn,NETCONN_EVTSENDPLUS,len);
	}
	return ERR_OK;
}

static void setuptcp(struct netconn *conn)
{
	struct tcp_pcb *pcb = conn->pcb.tcp;

	tcp_arg(pcb,conn);
	tcp_recv(pcb,recv_tcp);
	tcp_sent(pcb,sent_tcp);
	tcp_poll(pcb,poll_tcp,4);
	tcp_err(pcb,err_tcp);
}

static err_t accept_func(void *arg,struct tcp_pcb *newpcb,err_t err)
{
	sys_mbox mbox;
	struct netconn *newconn,*conn = (struct netconn*)arg;

	LWIP_DEBUGF(API_LIB_DEBUG, ("accept_func: %p %p %d\n",arg,newpcb,err));

	mbox = conn->acceptmbox;
	newconn = memp_malloc(MEMP_NETCONN);
	if(newconn==NULL) return ERR_MEM;

	if(MQ_Init(&newconn->recvmbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) {
		memp_free(MEMP_NETCONN,newconn);
		return ERR_MEM;
	}

	if(MQ_Init(&newconn->mbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) {
		MQ_Close(newconn->recvmbox);
		memp_free(MEMP_NETCONN,newconn);
		return ERR_MEM;
	}

	if(LWP_SemInit(&newconn->sem,0,1)==-1) {
		MQ_Close(newconn->recvmbox);
		MQ_Close(newconn->mbox);
		memp_free(MEMP_NETCONN,newconn);
		return ERR_MEM;
	}

	newconn->type = NETCONN_TCP;
	newconn->pcb.tcp = newpcb;
	setuptcp(newconn);

	newconn->acceptmbox = SYS_MBOX_NULL;
	newconn->err = err;

	if(conn->callback) {
		(*conn->callback)(conn,NETCONN_EVTRCVPLUS,0);
	}
	newconn->callback = conn->callback;
	newconn->socket = -1;
	newconn->recvavail = 0;

	MQ_Send(mbox,newconn,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void do_newconn(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
		return;
	}

	msg->conn->err = ERR_OK;
	switch(msg->conn->type) {
		case NETCONN_RAW:
			msg->conn->pcb.raw = raw_new(msg->msg.bc.port);
			raw_recv(msg->conn->pcb.raw,recv_raw,msg->conn);
			break;
		case NETCONN_UDPLITE:
			msg->conn->pcb.udp = udp_new();
			if(!msg->conn->pcb.udp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_UDPLITE);
			udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
			break;
		case NETCONN_UDPNOCHKSUM:
			msg->conn->pcb.udp = udp_new();
			if(!msg->conn->pcb.udp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_NOCHKSUM);
			udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
			break;
		case NETCONN_UDP:
			msg->conn->pcb.udp = udp_new();
			if(!msg->conn->pcb.udp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
			break;
		case NETCONN_TCP:
			msg->conn->pcb.tcp = tcp_new();
			if(!msg->conn->pcb.tcp) {
				msg->conn->err = ERR_MEM;
				break;
			}
			setuptcp(msg->conn);
			break;
		default:
			break;
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_delconn(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				raw_remove(msg->conn->pcb.raw);
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				msg->conn->pcb.udp->recv_arg = NULL;
				udp_remove(msg->conn->pcb.udp);
				break;
			case NETCONN_TCP:
				if(msg->conn->pcb.tcp->state==LISTEN) {
					tcp_arg(msg->conn->pcb.tcp,NULL);
					tcp_accept(msg->conn->pcb.tcp,NULL);
					tcp_close(msg->conn->pcb.tcp);
				} else {
					tcp_arg(msg->conn->pcb.tcp,NULL);
					tcp_sent(msg->conn->pcb.tcp,NULL);
					tcp_recv(msg->conn->pcb.tcp,NULL);
					tcp_poll(msg->conn->pcb.tcp,NULL,0);
					tcp_err(msg->conn->pcb.tcp,NULL);
					if(tcp_close(msg->conn->pcb.tcp)!=ERR_OK)
						tcp_abort(msg->conn->pcb.tcp);
				}
				break;
			default:
				break;
		}
	}
	if(msg->conn->callback) {
		(*msg->conn->callback)(msg->conn,NETCONN_EVTRCVPLUS,0);
		(*msg->conn->callback)(msg->conn,NETCONN_EVTSENDPLUS,0);
	}
	if(msg->conn->mbox!=SYS_MBOX_NULL)
		MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_bind(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp==NULL) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				msg->conn->pcb.raw = raw_new(msg->msg.bc.port);
				raw_recv(msg->conn->pcb.raw,recv_raw,msg->conn);
				break;
			case NETCONN_UDPLITE:
				msg->conn->pcb.udp = udp_new();
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_UDPLITE);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDPNOCHKSUM:
				msg->conn->pcb.udp = udp_new();
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_NOCHKSUM);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDP:
				msg->conn->pcb.udp = udp_new();
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_TCP:
				msg->conn->pcb.tcp = tcp_new();
				setuptcp(msg->conn);
				break;
			default:
				break;
		}
	}
	switch(msg->conn->type) {
		case NETCONN_RAW:
			msg->conn->err = raw_bind(msg->conn->pcb.raw,msg->msg.bc.ipaddr);
			break;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			msg->conn->err = udp_bind(msg->conn->pcb.udp,msg->msg.bc.ipaddr,msg->msg.bc.port);
			break;
		case NETCONN_TCP:
			msg->conn->err = tcp_bind(msg->conn->pcb.tcp,msg->msg.bc.ipaddr,msg->msg.bc.port);
			break;
		default:
			break;
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static err_t do_connected(void *arg,struct tcp_pcb *pcb,err_t err)
{
	u32 dummy = 0;
	struct netconn *conn = (struct netconn*)arg;

	if(!conn) return ERR_VAL;

	conn->err = err;
	if(conn->type==NETCONN_TCP && err==ERR_OK) setuptcp(conn);

	MQ_Send(conn->mbox,&dummy,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void do_connect(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(!msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				msg->conn->pcb.raw = raw_new(msg->msg.bc.port);
				raw_recv(msg->conn->pcb.raw,recv_raw,msg->conn);
				break;
			case NETCONN_UDPLITE:
				msg->conn->pcb.udp = udp_new();
				if(!msg->conn->pcb.udp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_UDPLITE);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDPNOCHKSUM:
				msg->conn->pcb.udp = udp_new();
				if(!msg->conn->pcb.udp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				udp_setflags(msg->conn->pcb.udp,UDP_FLAGS_NOCHKSUM);
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_UDP:
				msg->conn->pcb.udp = udp_new();
				if(!msg->conn->pcb.udp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				udp_recv(msg->conn->pcb.udp,recv_udp,msg->conn);
				break;
			case NETCONN_TCP:
				msg->conn->pcb.tcp = tcp_new();
				if(!msg->conn->pcb.tcp) {
					msg->conn->err = ERR_MEM;
					MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
					return;
				}
				break;
			default:
				break;
		}
	}
	switch(msg->conn->type) {
		case NETCONN_RAW:
			raw_connect(msg->conn->pcb.raw,msg->msg.bc.ipaddr);
			MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
			break;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			udp_connect(msg->conn->pcb.udp,msg->msg.bc.ipaddr,msg->msg.bc.port);
			MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
			break;
		case NETCONN_TCP:
			setuptcp(msg->conn);
			tcp_connect(msg->conn->pcb.tcp,msg->msg.bc.ipaddr,msg->msg.bc.port,do_connected);
			break;
		default:
			break;
	}
}

static void do_disconnect(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	switch(msg->conn->type) {
		case NETCONN_RAW:
			break;
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
		case NETCONN_UDP:
			udp_disconnect(msg->conn->pcb.udp);
			break;
		case NETCONN_TCP:
			break;
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_listen(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp!=NULL) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: listen RAW: cannot listen for RAW.\n"));
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: listen UDP: cannot listen for UDP.\n"));
				break;
			case NETCONN_TCP:
				msg->conn->pcb.tcp = tcp_listen(msg->conn->pcb.tcp);
				if(msg->conn->pcb.tcp==NULL)
					msg->conn->err = ERR_MEM;
				else {
					if(msg->conn->acceptmbox==SYS_MBOX_NULL) {
						if(MQ_Init(&msg->conn->acceptmbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) {
							msg->conn->err = ERR_MEM;
							break;
						}
					}
					tcp_arg(msg->conn->pcb.tcp,msg->conn);
					tcp_accept(msg->conn->pcb.tcp,accept_func);
				}
				break;
			default:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_accept(struct apimsg_msg *msg)
{
	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: accept RAW: cannot accept for RAW.\n"));
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: accept UDP: cannot accept for UDP.\n"));
				break;
			case NETCONN_TCP:
				break;
		}
	}
}

static void do_send(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
				raw_send(msg->conn->pcb.raw,msg->msg.p);
				break;
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				udp_send(msg->conn->pcb.udp,msg->msg.p);
				break;
			case NETCONN_TCP:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_recv(struct apimsg_msg *msg)
{
	u32 dummy = 0;

	if(msg->conn->pcb.tcp && msg->conn->type==NETCONN_TCP) {
		tcp_recved(msg->conn->pcb.tcp,msg->msg.len);
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_write(struct apimsg_msg *msg)
{
	err_t err;
	u32 dummy = 0;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				msg->conn->err = ERR_VAL;
				break;
			case NETCONN_TCP:
				err = tcp_write(msg->conn->pcb.tcp,msg->msg.w.dataptr,msg->msg.w.len,msg->msg.w.copy);
				if(err==ERR_OK && (!msg->conn->pcb.tcp->unacked || (msg->conn->pcb.tcp->flags&TF_NODELAY)
					|| msg->conn->pcb.tcp->snd_queuelen>1)) {
					LWIP_DEBUGF(API_MSG_DEBUG, ("api_msg: TCP write: tcp_output.\n"));
					tcp_output(msg->conn->pcb.tcp);
				}
				msg->conn->err = err;
				if(msg->conn->callback) {
					if(err==ERR_OK) {
						if(tcp_sndbuf(msg->conn->pcb.tcp)<=TCP_SNDLOWAT)
							(*msg->conn->callback)(msg->conn,NETCONN_EVTSENDMINUS,msg->msg.w.len);
					}
				}
				break;
			default:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void do_close(struct apimsg_msg *msg)
{
	u32 dummy = 0;
	err_t err = ERR_OK;

	if(msg->conn->pcb.tcp) {
		switch(msg->conn->type) {
			case NETCONN_RAW:
			case NETCONN_UDPLITE:
			case NETCONN_UDPNOCHKSUM:
			case NETCONN_UDP:
				break;
			case NETCONN_TCP:
				if(msg->conn->pcb.tcp->state==LISTEN)
					err = tcp_close(msg->conn->pcb.tcp);
				else if(msg->conn->pcb.tcp->state==CLOSE_WAIT)
					err = tcp_output(msg->conn->pcb.tcp);
				msg->conn->err = err;
				break;
			default:
				break;
		}
	}
	MQ_Send(msg->conn->mbox,&dummy,MQ_MSG_BLOCK);
}

static void apimsg_input(struct api_msg *msg)
{
	decode[msg->type](&(msg->msg));
}

static void apimsg_post(struct api_msg *msg)
{
	net_apimsg(msg);
}

/* tcpip thread part */
static err_t net_input(struct pbuf *p,struct netif *inp)
{
	struct net_msg *msg = memp_malloc(MEMP_TCPIP_MSG);

	LWIP_DEBUGF(TCPIP_DEBUG, ("net_input: %p %p\n", p,inp));

	if(msg==NULL) {
		LWIP_ERROR(("net_input: msg out of memory.\n"));
		pbuf_free(p);
		return ERR_MEM;
	}

	msg->type = NETMSG_INPUT;
	msg->msg.inp.p = p;
	msg->msg.inp.net = inp;
	MQ_Send(netthread_mbox,msg,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void net_apimsg(struct api_msg *apimsg)
{
	struct net_msg *msg = memp_malloc(MEMP_TCPIP_MSG);

	LWIP_DEBUGF(TCPIP_DEBUG, ("net_apimsg: %p\n",apimsg));
	if(msg==NULL) {
		LWIP_ERROR(("net_apimsg: msg out of memory.\n"));
		memp_free(MEMP_API_MSG,apimsg);
		return;
	}

	msg->type = NETMSG_API;
	msg->msg.apimsg = apimsg;
	MQ_Send(netthread_mbox,msg,MQ_MSG_BLOCK);
}

static err_t net_callback(void (*f)(void *),void *ctx)
{
	struct net_msg *msg = memp_malloc(MEMP_TCPIP_MSG);

	LWIP_DEBUGF(TCPIP_DEBUG, ("net_callback: %p(%p)\n", f,ctx));

	if(msg==NULL) {
		LWIP_ERROR(("net_apimsg: msg out of memory.\n"));
		return ERR_MEM;
	}

	msg->type = NETMSG_CALLBACK;
	msg->msg.cb.f = f;
	msg->msg.cb.ctx = ctx;
	MQ_Send(netthread_mbox,msg,MQ_MSG_BLOCK);
	return ERR_OK;
}

static void* net_thread(void *arg)
{
	struct net_msg *msg;
	struct timespec tb;
	sys_sem sem = (sys_sem)arg;

	etharp_init();
	ip_init();
	udp_init();
	tcp_init();

	tb.tv_sec = ARP_TMR_INTERVAL/TB_MSPERSEC;
	tb.tv_nsec = 0;
	net_arp_ticks = __lwp_wd_calc_ticks(&tb);
	__lwp_wd_initialize(&arp_time_cntrl,__arp_timer,ARP_TIMER_ID,NULL);
	__lwp_wd_insert_ticks(&arp_time_cntrl,net_arp_ticks);

	tb.tv_sec = 0;
	tb.tv_nsec = TCP_TMR_INTERVAL*TB_NSPERMS;
	net_tcp_ticks = __lwp_wd_calc_ticks(&tb);
	__lwp_wd_initialize(&tcp_timer_cntrl,__tcp_timer,TCP_TIMER_ID,NULL);

	LWP_SemPost(sem);

	LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread(%p)\n",arg));

	while(1) {
		MQ_Receive(netthread_mbox,(mqmsg_t)&msg,MQ_MSG_BLOCK);
		switch(msg->type) {
			case NETMSG_API:
			    LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread: API message %p\n", (void *)msg));
				apimsg_input(msg->msg.apimsg);
				break;
			case NETMSG_INPUT:
			    LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread: IP packet %p\n", (void *)msg));
				bba_process(msg->msg.inp.p,msg->msg.inp.net);
				break;
			case NETMSG_CALLBACK:
			    LWIP_DEBUGF(TCPIP_DEBUG, ("net_thread: CALLBACK %p\n", (void *)msg));
				msg->msg.cb.f(msg->msg.cb.ctx);
				break;
			default:
				break;
		}
		memp_free(MEMP_TCPIP_MSG,msg);
	}
	return NULL;
}

/* sockets part */
static s32 alloc_socket(struct netconn *conn)
{
	s32 i;

	LWP_SemWait(netsocket_sem);

	for(i=0;i<NUM_SOCKETS;i++) {
		if(!sockets[i].conn) {
			sockets[i].conn = conn;
			sockets[i].lastdata = NULL;
			sockets[i].lastoffset = 0;
			sockets[i].rcvevt = 0;
			sockets[i].sendevt = 1;
			sockets[i].flags = 0;
			sockets[i].err = 0;
			LWP_SemPost(netsocket_sem);
			return i;
		}
	}

	LWP_SemPost(netsocket_sem);
	return -1;
}

static struct netsocket* get_socket(s32 s)
{
	struct netsocket *sock;
	if(s<0 || s>NUM_SOCKETS) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("get_socket(%d): invalid\n", s));
		return NULL;
	}
	sock = &sockets[s];
	if(!sock->conn) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("get_socket(%d): no active\n", s));
		return NULL;
	}

	return sock;
}

static void evt_callback(struct netconn *conn,enum netconn_evt evt,u32 len)
{
	s32 s;
	struct netsocket *sock;
	struct netselect_cb *scb;

	if(conn) {
		s = conn->socket;
		if(s<0) {
			if(evt==NETCONN_EVTRCVPLUS)
				conn->socket--;
			return;
		}
		sock = get_socket(s);
		if(!sock) return;
	} else
		return;

	LWP_SemWait(sockselect_sem);
	switch(evt) {
		case NETCONN_EVTRCVPLUS:
			sock->rcvevt++;
			break;
		case NETCONN_EVTRCVMINUS:
			sock->rcvevt--;
			break;
		case NETCONN_EVTSENDPLUS:
			sock->sendevt = 1;
			break;
		case NETCONN_EVTSENDMINUS:
			sock->sendevt = 0;
			break;
	}
	LWP_SemPost(sockselect_sem);

	while(1) {
		LWP_SemWait(sockselect_sem);
		for(scb = selectcb_list;scb;scb = scb->next) {
			if(scb->signaled==0) {
				if(scb->readset && FD_ISSET(s,scb->readset))
					if(sock->rcvevt) break;
				if(scb->writeset && FD_ISSET(s,scb->writeset))
					if(sock->sendevt) break;
			}
		}
		if(scb) {
			scb->signaled = 1;
			LWP_SemPost(sockselect_sem);
			LWP_MutexLock(scb->cond_lck);
			LWP_CondSignal(scb->cond);
			LWP_MutexUnlock(scb->cond_lck);
		} else {
			LWP_SemPost(sockselect_sem);
			break;
		}
	}

}

extern const devoptab_t dotab_stdnet;

s32 if_configex(struct in_addr *local_ip,struct in_addr *netmask,struct in_addr *gateway,bool use_dhcp, int max_retries)
{
	s32 ret = 0;
	struct ip_addr loc_ip, mask, gw;
	struct netif *pnet;
	struct timespec tb;
	dev_s hbba = NULL;

	if(g_netinitiated) return 0;
	g_netinitiated = 1;

//	AddDevice(&dotab_stdnet);
#ifdef STATS
	stats_init();
#endif /* STATS */

	sys_init();
	mem_init();
	memp_init();
	pbuf_init();
	netif_init();

	// init tcpip thread message box
	if(MQ_Init(&netthread_mbox,MQBOX_SIZE)!=MQ_ERROR_SUCCESSFUL) return -1;

	// create & setup interface
	loc_ip.addr = 0;
	mask.addr = 0;
	gw.addr = 0;

	if(use_dhcp==FALSE) {
		if( !gateway || gateway->s_addr==0
			|| !local_ip || local_ip->s_addr==0
			|| !netmask || netmask->s_addr==0 ) return -EINVAL;
			loc_ip.addr = local_ip->s_addr;
			mask.addr = netmask->s_addr;
			gw.addr = gateway->s_addr;
	}
	hbba = bba_create(&g_hNetIF);
	pnet = netif_add(&g_hNetIF,&loc_ip, &mask, &gw, hbba, bba_init, net_input);
	if(pnet) {
		netif_set_up(pnet);
		netif_set_default(pnet);
#if (LWIP_DHCP)
		if(use_dhcp==TRUE) {
			//setup coarse timer
			tb.tv_sec = DHCP_COARSE_TIMER_SECS;
			tb.tv_nsec = 0;
			net_dhcpcoarse_ticks = __lwp_wd_calc_ticks(&tb);
			__lwp_wd_initialize(&dhcp_coarsetimer_cntrl, __dhcpcoarse_timer, DHCPCOARSE_TIMER_ID, NULL);
			__lwp_wd_insert_ticks(&dhcp_coarsetimer_cntrl, net_dhcpcoarse_ticks);

			//setup fine timer
			tb.tv_sec = 0;
			tb.tv_nsec = DHCP_FINE_TIMER_MSECS * TB_NSPERMS;
			net_dhcpfine_ticks = __lwp_wd_calc_ticks(&tb);
			__lwp_wd_initialize(&dhcp_finetimer_cntrl, __dhcpfine_timer, DHCPFINE_TIMER_ID, NULL);
			__lwp_wd_insert_ticks(&dhcp_finetimer_cntrl, net_dhcpfine_ticks);

			//now start dhcp client
			dhcp_start(pnet);
		}
#endif
	} else
		return -ENXIO;

	// setup loopinterface
	IP4_ADDR(&loc_ip, 127, 0, 0, 1);
	IP4_ADDR(&mask, 255, 0, 0, 0);
	IP4_ADDR(&gw, 127, 0, 0, 1);
	pnet = netif_add(&g_hLoopIF, &loc_ip, &mask, &gw, NULL, loopif_init, net_input);

	//last and least start the tcpip layer
	ret = net_init();

	if ( ret == 0 && use_dhcp == TRUE ) {

		int retries = max_retries;
		// wait for dhcp to bind
		while ( g_hNetIF.dhcp->state != DHCP_BOUND && retries > 0 ) {
			retries--;
			usleep(500000);
		}

		if ( retries > 0 ) {
			//copy back network addresses
			if ( local_ip != NULL ) local_ip->s_addr = g_hNetIF.ip_addr.addr;
			if ( gateway != NULL ) gateway->s_addr = g_hNetIF.gw.addr;
			if ( netmask != NULL ) netmask->s_addr = g_hNetIF.netmask.addr;
		} else {
			ret = -ETIMEDOUT;
		}
	}

	return ret;
}

s32 if_config(char *local_ip, char *netmask, char *gateway,bool use_dhcp, int max_retries)
{
	s32 ret = 0;
	struct in_addr loc_ip, mask, gw;

	loc_ip.s_addr = 0;
	mask.s_addr = 0;
	gw.s_addr = 0;

	if ( local_ip != NULL ) loc_ip.s_addr = inet_addr(local_ip);
	if ( netmask != NULL ) mask.s_addr = inet_addr(netmask);
	if ( gateway != NULL ) gw.s_addr = inet_addr(gateway);

	ret = if_configex( &loc_ip, &mask, &gw, use_dhcp, max_retries);

	if (ret<0) return ret;

	if ( use_dhcp == TRUE ) {
		//copy back network addresses
		if ( local_ip != NULL ) strcpy(local_ip, inet_ntoa( loc_ip ));
		if ( netmask != NULL ) strcpy(netmask, inet_ntoa( mask));
		if ( gateway != NULL ) strcpy(gateway, inet_ntoa( gw ));
	}
	return ret;
}

s32 net_init()
{
	sys_sem sem;

	if(tcpiplayer_inited) return 1;

	if(LWP_SemInit(&netsocket_sem,1,1)==-1) return -1;
	if(LWP_SemInit(&sockselect_sem,1,1)==-1) {
		LWP_SemDestroy(netsocket_sem);
		return -1;
	}
	if(LWP_SemInit(&sem,0,1)==-1) {
		LWP_SemDestroy(netsocket_sem);
		LWP_SemDestroy(sockselect_sem);
		return -1;
	}

	if(LWP_CreateThread(&hnet_thread,net_thread,(void*)sem,netthread_stack,STACKSIZE,220)==-1) {
		LWP_SemDestroy(netsocket_sem);
		LWP_SemDestroy(sockselect_sem);
		LWP_SemDestroy(sem);
		return -1;
	}
	LWP_SemWait(sem);
	LWP_SemDestroy(sem);

	tcpiplayer_inited = 1;

	return 0;
}

s32 net_shutdown(s32 s,u32 how)
{
	return -1;
}

s32 net_fcntl(s32 s, u32 cmd, u32 flags)
{
	return -1;
}

s32 net_socket(u32 domain,u32 type,u32 protocol)
{
	s32 i;
	struct netconn *conn;

	switch(type) {
		case SOCK_RAW:
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_socket(SOCK_RAW)\n"));
			conn = netconn_new_with_proto_and_callback(NETCONN_RAW,protocol,evt_callback);
			break;
		case SOCK_DGRAM:
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_socket(SOCK_DGRAM)\n"));
			conn = netconn_new_with_callback(NETCONN_UDP,evt_callback);
			break;
		case SOCK_STREAM:
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_socket(SOCK_STREAM)\n"));
			conn = netconn_new_with_callback(NETCONN_TCP,evt_callback);
			break;
		default:
			return -1;
	}
	if(!conn) return -1;

	i = alloc_socket(conn);
	if(i==-1) {
		netconn_delete(conn);
		return -1;
	}

	conn->socket = i;
	return i;
}

s32 net_accept(s32 s,struct sockaddr *addr,socklen_t *addrlen)
{
	struct netsocket *sock;
	struct netconn *newconn;
	struct ip_addr naddr = {0};
	u16 port = 0;
	s32 newsock;
	struct sockaddr_in sin;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_accept(%d)\n", s));

	sock = get_socket(s);
	if(!sock) return -ENOTSOCK;

	newconn = netconn_accept(sock->conn);
	netconn_peer(newconn,&naddr,&port);

	memset(&sin,0,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = naddr.addr;

	if(*addrlen>sizeof(sin))
		*addrlen = sizeof(sin);
	memcpy(addr,&sin,*addrlen);

	newsock = alloc_socket(newconn);
	if(newsock==-1) {
		netconn_delete(newconn);
		return -1;
	}

	newconn->callback = evt_callback;
	sock = get_socket(newsock);

	LWP_SemWait(netsocket_sem);
	sock->rcvevt += -1 - newconn->socket;
	newconn->socket = newsock;
	LWP_SemPost(netsocket_sem);

	return newsock;
}

s32 net_bind(s32 s,struct sockaddr *name,socklen_t namelen)
{
	struct netsocket *sock;
	struct ip_addr loc_addr;
	u16 loc_port;
	err_t err;

	sock = get_socket(s);
	if(!sock) return -ENOTSOCK;

	loc_addr.addr = ((struct sockaddr_in*)name)->sin_addr.s_addr;
	loc_port = ((struct sockaddr_in*)name)->sin_port;

	err = netconn_bind(sock->conn,&loc_addr,ntohs(loc_port));
	if(err!=ERR_OK) return -1;

	return 0;
}

s32 net_listen(s32 s,u32 backlog)
{
	struct netsocket *sock;
	err_t err;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_listen(%d, backlog=%d)\n", s, backlog));
	sock = get_socket(s);
	if(!sock) return -ENOTSOCK;

	err = netconn_listen(sock->conn);
	if(err!=ERR_OK) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_listen(%d) failed, err=%d\n", s, err));
		return -1;
	}
	return 0;
}

s32 net_recvfrom(s32 s,void *mem,s32 len,u32 flags,struct sockaddr *from,socklen_t *fromlen)
{
	struct netsocket *sock;
	struct netbuf *buf;
	u16 buflen,copylen;
	struct ip_addr *addr;
	u16 port;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d, %p, %d, 0x%x, ..)\n", s, mem, len, flags));
	if(mem==NULL || len<=0) return -EINVAL;

	sock = get_socket(s);
	if(!sock) return -ENOTSOCK;

	if(sock->lastdata)
		buf = sock->lastdata;
	else {
		if(((flags&MSG_DONTWAIT) || (sock->flags&O_NONBLOCK)) && !sock->rcvevt) {
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d): returning EWOULDBLOCK\n", s));
			return -EAGAIN;
		}
		buf = netconn_recv(sock->conn);
		if(!buf) {
		    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d): buf == NULL!\n", s));
			return 0;
		}
	}

	buflen = netbuf_len(buf);
	buflen -= sock->lastoffset;
	if(buflen<=0)
		return 0;
	if(len>buflen)
		copylen = buflen;
	else
		copylen = len;

	netbuf_copypartial(buf,mem,copylen,sock->lastoffset);

	if(from && fromlen) {
		struct sockaddr_in sin;

		addr = netbuf_fromaddr(buf);
		port = netbuf_fromport(buf);

		memset(&sin,0,sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(port);
		sin.sin_addr.s_addr = addr->addr;

		if(*fromlen>sizeof(sin))
			*fromlen = sizeof(sin);

		memcpy(from,&sin,*fromlen);

		LWIP_DEBUGF(SOCKETS_DEBUG, ("net_recvfrom(%d): addr=", s));
		ip_addr_debug_print(SOCKETS_DEBUG, addr);
		LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%u len=%u\n", port, copylen));
	}
	if(netconn_type(sock->conn)==NETCONN_TCP && (buflen-copylen)>0) {
		sock->lastdata = buf;
		sock->lastoffset += copylen;
	} else {
		sock->lastdata = NULL;
		sock->lastoffset = 0;
		netbuf_delete(buf);
	}
	return copylen;
}

s32 net_read(s32 s,void *mem,s32 len)
{
	return net_recvfrom(s,mem,len,0,NULL,NULL);
}

s32 net_recv(s32 s,void *mem,s32 len,u32 flags)
{
	return net_recvfrom(s,mem,len,flags,NULL,NULL);
}

s32 net_sendto(s32 s,const void *data,s32 len,u32 flags,struct sockaddr *to,socklen_t tolen)
{
	struct netsocket *sock;
	struct ip_addr remote_addr, addr;
	u16_t remote_port, port = 0;
	s32 ret,connected;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_sendto(%d, data=%p, size=%d, flags=0x%x)\n", s, data, len, flags));
	if(data==NULL || len<=0) return -EINVAL;

	sock = get_socket(s);
	if (!sock) return -ENOTSOCK;

	/* get the peer if currently connected */
	connected = (netconn_peer(sock->conn, &addr, &port) == ERR_OK);

	remote_addr.addr = ((struct sockaddr_in *)to)->sin_addr.s_addr;
	remote_port = ((struct sockaddr_in *)to)->sin_port;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_sendto(%d, data=%p, size=%d, flags=0x%x to=", s, data, len, flags));
	ip_addr_debug_print(SOCKETS_DEBUG, &remote_addr);
	LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%u\n", ntohs(remote_port)));

	netconn_connect(sock->conn, &remote_addr, ntohs(remote_port));

	ret = net_send(s, data, len, flags);

	/* reset the remote address and port number
	of the connection */
	if (connected)
		netconn_connect(sock->conn, &addr, port);
	else
		netconn_disconnect(sock->conn);
	return ret;
}

s32 net_send(s32 s,const void *data,s32 len,u32 flags)
{
	struct netsocket *sock;
	struct netbuf *buf;
	err_t err;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d, data=%p, size=%d, flags=0x%x)\n", s, data, len, flags));
	if(data==NULL || len<=0) return -EINVAL;

	sock = get_socket(s);
	if(!sock) return -ENOTSOCK;

	switch(netconn_type(sock->conn)) {
		case NETCONN_RAW:
		case NETCONN_UDP:
		case NETCONN_UDPLITE:
		case NETCONN_UDPNOCHKSUM:
			buf = netbuf_new();
			if(!buf) {
				LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d) ENOBUFS\n", s));
				return -ENOBUFS;
			}
			netbuf_ref(buf,data,len);
			err = netconn_send(sock->conn,buf);
			netbuf_delete(buf);
			break;
		case NETCONN_TCP:
			err = netconn_write(sock->conn,data,len,NETCONN_COPY);
			break;
		default:
			err = ERR_ARG;
			break;
	}
	if(err!=ERR_OK) {
		LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d) err=%d\n", s, err));
		return -1;
	}

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_send(%d) ok size=%d\n", s, len));
	return len;
}

s32 net_write(s32 s,const void *data,s32 size)
{
	return net_send(s,data,size,0);
}

s32 net_connect(s32 s,struct sockaddr *name,socklen_t namelen)
{
	struct netsocket *sock;
	err_t err;

	sock = get_socket(s);
	if(!sock) return -ENOTSOCK;

	if(((struct sockaddr_in*)name)->sin_family==AF_UNSPEC) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d, AF_UNSPEC)\n", s));
		err = netconn_disconnect(sock->conn);
	} else {
		struct ip_addr remote_addr;
		u16 remote_port;

		remote_addr.addr = ((struct sockaddr_in*)name)->sin_addr.s_addr;
		remote_port = ((struct sockaddr_in*)name)->sin_port;

		LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d, addr=", s));
		ip_addr_debug_print(SOCKETS_DEBUG, &remote_addr);
		LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%u)\n", ntohs(remote_port)));

		err = netconn_connect(sock->conn,&remote_addr,ntohs(remote_port));
	}
	if(err!=ERR_OK) {
	    LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d) failed, err=%d\n", s, err));
		return -1;
	}

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_connect(%d) succeeded\n", s));
	return -EISCONN;
}

s32 net_close(s32 s)
{
	struct netsocket *sock;

	LWIP_DEBUGF(SOCKETS_DEBUG, ("net_close(%d)\n", s));

	LWP_SemWait(netsocket_sem);

	sock = get_socket(s);
	if(!sock) {
		LWP_SemPost(netsocket_sem);
		return -ENOTSOCK;
	}

	netconn_delete(sock->conn);
	if(sock->lastdata) netbuf_delete(sock->lastdata);

	sock->lastdata = NULL;
	sock->lastoffset = 0;
	sock->conn = NULL;

	LWP_SemPost(netsocket_sem);
	return 0;
}

static s32 net_selscan(s32 maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset)
{
	s32 i,nready = 0;
	fd_set lreadset,lwriteset,lexceptset;
	struct netsocket *sock;

	FD_ZERO(&lreadset);
	FD_ZERO(&lwriteset);
	FD_ZERO(&lexceptset);

	for(i=0;i<maxfdp1;i++) {
		if(FD_ISSET(i,readset)) {
			sock = get_socket(i);
			if(sock && (sock->lastdata || sock->rcvevt)) {
				FD_SET(i,&lreadset);
				nready++;
			}
		}
		if(FD_ISSET(i,writeset)) {
			sock = get_socket(i);
			if(sock && sock->sendevt) {
				FD_SET(i,&lwriteset);
				nready++;
			}
		}
	}
	*readset = lreadset;
	*writeset = lwriteset;
	FD_ZERO(exceptset);

	return nready;
}

s32 net_select(s32 maxfdp1,fd_set *readset,fd_set *writeset,fd_set *exceptset,struct timeval *timeout)
{
	s32 i,nready;
	fd_set lreadset,lwriteset,lexceptset;
	struct timespec tb,*p_tb;
	struct netselect_cb sel_cb;
	struct netselect_cb *psel_cb;

	sel_cb.next = NULL;
	sel_cb.readset = readset;
	sel_cb.writeset = writeset;
	sel_cb.exceptset = exceptset;
	sel_cb.signaled = 0;

	LWP_SemWait(sockselect_sem);

	if(readset)
		lreadset = *readset;
	else
		FD_ZERO(&lreadset);

	if(writeset)
		lwriteset = *writeset;
	else
		FD_ZERO(&lwriteset);

	if(exceptset)
		lexceptset = *exceptset;
	else
		FD_ZERO(&lexceptset);

	nready = net_selscan(maxfdp1,&lreadset,&lwriteset,&lexceptset);
	if(!nready) {
		if(timeout && timeout->tv_sec==0 && timeout->tv_usec==0) {
			LWP_SemPost(sockselect_sem);
			if(readset)
				FD_ZERO(readset);
			if(writeset)
				FD_ZERO(writeset);
			if(exceptset)
				FD_ZERO(exceptset);
			return 0;
		}

		LWP_MutexInit(&sel_cb.cond_lck,FALSE);
		LWP_CondInit(&sel_cb.cond);
		sel_cb.next = selectcb_list;
		selectcb_list = &sel_cb;

		LWP_SemPost(sockselect_sem);
		if(timeout==NULL)
			p_tb = NULL;
		else {
			tb.tv_sec = timeout->tv_sec;
			tb.tv_nsec = (timeout->tv_usec+500)*TB_NSPERUS;
			p_tb = &tb;
		}

		LWP_MutexLock(sel_cb.cond_lck);
		i = LWP_CondTimedWait(sel_cb.cond,sel_cb.cond_lck,p_tb);
		LWP_MutexUnlock(sel_cb.cond_lck);

		LWP_SemWait(sockselect_sem);
		if(selectcb_list==&sel_cb)
			selectcb_list = sel_cb.next;
		else {
			for(psel_cb = selectcb_list;psel_cb;psel_cb = psel_cb->next) {
				if(psel_cb->next==&sel_cb) {
					psel_cb->next = sel_cb.next;
					break;
				}
			}
		}
		LWP_CondDestroy(sel_cb.cond);
		LWP_MutexDestroy(sel_cb.cond_lck);

		LWP_SemPost(sockselect_sem);

		if(i==ETIMEDOUT) {
			if(readset)
				FD_ZERO(readset);
			if(writeset)
				FD_ZERO(writeset);
			if(exceptset)
				FD_ZERO(exceptset);
			return 0;
		}

		if(readset)
			lreadset = *readset;
		else
			FD_ZERO(&lreadset);

		if(writeset)
			lwriteset = *writeset;
		else
			FD_ZERO(&lwriteset);

		if(exceptset)
			lexceptset = *exceptset;
		else
			FD_ZERO(&lexceptset);

		nready = net_selscan(maxfdp1,&lreadset,&lwriteset,&lexceptset);
	} else
		LWP_SemPost(sockselect_sem);

	if(readset)
		*readset = lreadset;
	if(writeset)
		*writeset = lwriteset;
	if(exceptset)
		*exceptset = lexceptset;

	return nready;
}

s32 net_setsockopt(s32 s,u32 level,u32 optname,const void *optval,socklen_t optlen)
{
	s32 err = 0;
	struct netsocket *sock;

	sock = get_socket(s);
	if(sock==NULL) return -ENOTSOCK;
	if(optval==NULL) return -EINVAL;

	switch(level) {
		case SOL_SOCKET:
		{
			switch(optname) {
				case SO_BROADCAST:
				case SO_KEEPALIVE:
				case SO_REUSEADDR:
				case SO_REUSEPORT:
					if(optlen<sizeof(u32)) err = EINVAL;
					break;
				default:
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, SOL_SOCKET, UNIMPL: optname=0x%x, ..)\n", s, optname));
					err = ENOPROTOOPT;
			}
		}
		break;

		case IPPROTO_IP:
		{
			switch(optname) {
				case IP_TTL:
				case IP_TOS:
					if(optlen<sizeof(u32)) err = EINVAL;
					break;
				default:
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_IP, UNIMPL: optname=0x%x, ..)\n", s, optname));
					err = ENOPROTOOPT;
			}
		}
		break;

		case  IPPROTO_TCP:
		{
			if(optlen<sizeof(u32)) {
				err = EINVAL;
				break;
			}
			if(sock->conn->type!=NETCONN_TCP) return 0;

			switch(optname) {
				case TCP_NODELAY:
				case TCP_KEEPALIVE:
					break;
				default:
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_TCP, UNIMPL: optname=0x%x, ..)\n", s, optname));
					err = ENOPROTOOPT;
			}
		}
		break;

		default:
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, level=0x%x, UNIMPL: optname=0x%x, ..)\n", s, level, optname));
			err = ENOPROTOOPT;
	}
	if(err!=0) return -1;

	switch(level) {
		case SOL_SOCKET:
		{
			switch(optname) {
				case SO_BROADCAST:
				case SO_KEEPALIVE:
				case SO_REUSEADDR:
				case SO_REUSEPORT:
					if(*(u32*)optval)
						sock->conn->pcb.tcp->so_options |= optname;
					else
						sock->conn->pcb.tcp->so_options &= ~optname;
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, SOL_SOCKET, optname=0x%x, ..) -> %s\n", s, optname, (*(u32*)optval?"on":"off")));
					break;
			}
		}
		break;

		case IPPROTO_IP:
		{
			switch(optname) {
				case IP_TTL:
					sock->conn->pcb.tcp->ttl = (u8)(*(u32*)optval);
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_IP, IP_TTL, ..) -> %u\n", s, sock->conn->pcb.tcp->ttl));
					break;
				case IP_TOS:
					sock->conn->pcb.tcp->tos = (u8)(*(u32*)optval);
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_IP, IP_TOS, ..)-> %u\n", s, sock->conn->pcb.tcp->tos));
					break;
			}
		}
		break;

		case  IPPROTO_TCP:
		{
			switch(optname) {
				case TCP_NODELAY:
					if(*(u32*)optval)
						sock->conn->pcb.tcp->flags |= TF_NODELAY;
					else
						sock->conn->pcb.tcp->flags &= ~TF_NODELAY;
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_TCP, TCP_NODELAY) -> %s\n", s, (*(u32*)optval)?"on":"off") );
					break;
				case TCP_KEEPALIVE:
					sock->conn->pcb.tcp->keepalive = (u32)(*(u32*)optval);
					LWIP_DEBUGF(SOCKETS_DEBUG, ("net_setsockopt(%d, IPPROTO_TCP, TCP_KEEPALIVE) -> %u\n", s, sock->conn->pcb.tcp->keepalive));
					break;
			}
		}
	}
	return err?-1:0;
}

s32 net_ioctl(s32 s, u32 cmd, void *argp)
{
	struct netsocket *sock = get_socket(s);

	if(!sock) return -ENOTSOCK;

	switch (cmd) {
		case FIONREAD:
			if(!argp) return -EINVAL;

			*((u16_t*)argp) = sock->conn->recvavail;

			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_ioctl(%d, FIONREAD, %p) = %u\n", s, argp, *((u16*)argp)));
			return 0;

		case FIONBIO:
			if(argp && *(u32*)argp)
				sock->flags |= O_NONBLOCK;
			else
				sock->flags &= ~O_NONBLOCK;
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_ioctl(%d, FIONBIO, %d)\n", s, !!(sock->flags&O_NONBLOCK)));
			return 0;

		default:
			LWIP_DEBUGF(SOCKETS_DEBUG, ("net_ioctl(%d, UNIMPL: 0x%lx, %p)\n", s, cmd, argp));
			return -EINVAL;
	}
}
