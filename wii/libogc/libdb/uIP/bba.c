#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "asm.h"
#include "processor.h"
#include "exi.h"
#include "cache.h"
#include "bba.h"
#include "uip_pbuf.h"
#include "uip_netif.h"
#include "uip_arp.h"

#define IFNAME0					'e'
#define IFNAME1					't'

#define BBA_MINPKTSIZE			60

#define BBA_CID					0x04020200

#define BBA_CMD_IRMASKALL		0x00
#define BBA_CMD_IRMASKNONE		0xF8

#define BBA_NCRA				0x00		/* Network Control Register A, RW */
#define BBA_NCRA_RESET			(1<<0)	/* RESET */
#define BBA_NCRA_ST0			(1<<1)	/* ST0, Start transmit command/status */
#define BBA_NCRA_ST1			(1<<2)	/* ST1,  " */
#define BBA_NCRA_SR				(1<<3)	/* SR, Start Receive */

#define BBA_NCRB				0x01		/* Network Control Register B, RW */
#define BBA_NCRB_PR				(1<<0)	/* PR, Promiscuous Mode */
#define BBA_NCRB_CA				(1<<1)	/* CA, Capture Effect Mode */
#define BBA_NCRB_PM				(1<<2)	/* PM, Pass Multicast */
#define BBA_NCRB_PB				(1<<3)	/* PB, Pass Bad Frame */
#define BBA_NCRB_AB				(1<<4)	/* AB, Accept Broadcast */
#define BBA_NCRB_HBD			(1<<5)	/* HBD, reserved */
#define BBA_NCRB_RXINTC0		(1<<6)	/* RXINTC, Receive Interrupt Counter */
#define BBA_NCRB_RXINTC1		(1<<7)	/*  " */
#define BBA_NCRB_1_PACKET_PER_INT  (0<<6)	/* 0 0 */
#define BBA_NCRB_2_PACKETS_PER_INT (1<<6)	/* 0 1 */
#define BBA_NCRB_4_PACKETS_PER_INT (2<<6)	/* 1 0 */
#define BBA_NCRB_8_PACKETS_PER_INT (3<<6)	/* 1 1 */

#define BBA_LTPS 0x04		/* Last Transmitted Packet Status, RO */
#define BBA_LRPS 0x05		/* Last Received Packet Status, RO */

#define BBA_IMR 0x08		/* Interrupt Mask Register, RW, 00h */
#define   BBA_IMR_FRAGIM     (1<<0)	/* FRAGIM, Fragment Counter Int Mask */
#define   BBA_IMR_RIM        (1<<1)	/* RIM, Receive Interrupt Mask */
#define   BBA_IMR_TIM        (1<<2)	/* TIM, Transmit Interrupt Mask */
#define   BBA_IMR_REIM       (1<<3)	/* REIM, Receive Error Interrupt Mask */
#define   BBA_IMR_TEIM       (1<<4)	/* TEIM, Transmit Error Interrupt Mask */
#define   BBA_IMR_FIFOEIM    (1<<5)	/* FIFOEIM, FIFO Error Interrupt Mask */
#define   BBA_IMR_BUSEIM     (1<<6)	/* BUSEIM, BUS Error Interrupt Mask */
#define   BBA_IMR_RBFIM      (1<<7)	/* RBFIM, RX Buffer Full Interrupt Mask */

#define BBA_IR 0x09		/* Interrupt Register, RW, 00h */
#define   BBA_IR_FRAGI       (1<<0)	/* FRAGI, Fragment Counter Interrupt */
#define   BBA_IR_RI          (1<<1)	/* RI, Receive Interrupt */
#define   BBA_IR_TI          (1<<2)	/* TI, Transmit Interrupt */
#define   BBA_IR_REI         (1<<3)	/* REI, Receive Error Interrupt */
#define   BBA_IR_TEI         (1<<4)	/* TEI, Transmit Error Interrupt */
#define   BBA_IR_FIFOEI      (1<<5)	/* FIFOEI, FIFO Error Interrupt */
#define   BBA_IR_BUSEI       (1<<6)	/* BUSEI, BUS Error Interrupt */
#define   BBA_IR_RBFI        (1<<7)	/* RBFI, RX Buffer Full Interrupt */

#define BBA_BP   0x0a/*+0x0b*/	/* Boundary Page Pointer Register */
#define BBA_TLBP 0x0c/*+0x0d*/	/* TX Low Boundary Page Pointer Register */
#define BBA_TWP  0x0e/*+0x0f*/	/* Transmit Buffer Write Page Pointer Register */
#define BBA_TRP  0x12/*+0x13*/	/* Transmit Buffer Read Page Pointer Register */
#define BBA_RWP  0x16/*+0x17*/	/* Receive Buffer Write Page Pointer Register */
#define BBA_RRP  0x18/*+0x19*/	/* Receive Buffer Read Page Pointer Register */
#define BBA_RHBP 0x1a/*+0x1b*/	/* Receive High Boundary Page Pointer Register */

#define BBA_RXINTT    0x14/*+0x15*/	/* Receive Interrupt Timer Register */

#define BBA_NAFR_PAR0 0x20	/* Physical Address Register Byte 0 */
#define BBA_NAFR_PAR1 0x21	/* Physical Address Register Byte 1 */
#define BBA_NAFR_PAR2 0x22	/* Physical Address Register Byte 2 */
#define BBA_NAFR_PAR3 0x23	/* Physical Address Register Byte 3 */
#define BBA_NAFR_PAR4 0x24	/* Physical Address Register Byte 4 */
#define BBA_NAFR_PAR5 0x25	/* Physical Address Register Byte 5 */

#define BBA_NWAYC 0x30		/* NWAY Configuration Register, RW, 84h */
#define   BBA_NWAYC_FD       (1<<0)	/* FD, Full Duplex Mode */
#define   BBA_NWAYC_PS100    (1<<1)	/* PS100/10, Port Select 100/10 */
#define   BBA_NWAYC_ANE      (1<<2)	/* ANE, Autonegotiation Enable */
#define   BBA_NWAYC_ANS_RA   (1<<3)	/* ANS, Restart Autonegotiation */
#define   BBA_NWAYC_LTE      (1<<7)	/* LTE, Link Test Enable */

#define BBA_NWAYS 0x31
#define   BBA_NWAYS_LS10	 (1<<0)
#define   BBA_NWAYS_LS100	 (1<<1)
#define   BBA_NWAYS_LPNWAY   (1<<2)
#define   BBA_NWAYS_ANCLPT	 (1<<3)
#define   BBA_NWAYS_100TXF	 (1<<4)
#define   BBA_NWAYS_100TXH	 (1<<5)
#define   BBA_NWAYS_10TXF	 (1<<6)
#define   BBA_NWAYS_10TXH	 (1<<7)

#define BBA_GCA 0x32		/* GMAC Configuration A Register, RW, 00h */
#define   BBA_GCA_ARXERRB    (1<<3)	/* ARXERRB, Accept RX packet with error */

#define BBA_MISC 0x3d		/* MISC Control Register 1, RW, 3ch */
#define   BBA_MISC_BURSTDMA  (1<<0)
#define   BBA_MISC_DISLDMA   (1<<1)

#define BBA_TXFIFOCNT 0x3e/*0x3f*/	/* Transmit FIFO Counter Register */
#define BBA_WRTXFIFOD 0x48/*-0x4b*/	/* Write TX FIFO Data Port Register */

#define BBA_MISC2 0x50		/* MISC Control Register 2, RW, 00h */
#define   BBA_MISC2_HBRLEN0		(1<<0)	/* HBRLEN, Host Burst Read Length */
#define   BBA_MISC2_HBRLEN1		(1<<1)	/*  " */
#define   BBA_MISC2_RUNTSIZE	(1<<2)	/*  " */
#define   BBA_MISC2_DREQBCTRL	(1<<3)	/*  " */
#define   BBA_MISC2_RINTSEL		(1<<4)	/*  " */
#define   BBA_MISC2_ITPSEL		(3<<5)	/*  " */
#define   BBA_MISC2_AUTORCVR	(1<<7)	/* Auto RX Full Recovery */

#define BBA_RX_STATUS_BF      (1<<0)
#define BBA_RX_STATUS_CRC     (1<<1)
#define BBA_RX_STATUS_FAE     (1<<2)
#define BBA_RX_STATUS_FO      (1<<3)
#define BBA_RX_STATUS_RW      (1<<4)
#define BBA_RX_STATUS_MF      (1<<5)
#define BBA_RX_STATUS_RF      (1<<6)
#define BBA_RX_STATUS_RERR    (1<<7)

#define BBA_TX_STATUS_CC0     (1<<0)
#define BBA_TX_STATUS_CC1     (1<<1)
#define BBA_TX_STATUS_CC2     (1<<2)
#define BBA_TX_STATUS_CC3     (1<<3)
#define  BBA_TX_STATUS_CCMASK (0x0f)
#define BBA_TX_STATUS_CRSLOST (1<<4)
#define BBA_TX_STATUS_UF      (1<<5)
#define BBA_TX_STATUS_OWC     (1<<6)
#define BBA_TX_STATUS_OWN     (1<<7)
#define BBA_TX_STATUS_TERR    (1<<7)

#define BBA_TX_MAX_PACKET_SIZE 1518	/* 14+1500+4 */
#define BBA_RX_MAX_PACKET_SIZE 1536	/* 6 pages * 256 bytes */

#define BBA_INIT_TLBP	0x00
#define BBA_INIT_BP		0x01
#define BBA_INIT_RHBP	0x0f
#define BBA_INIT_RWP	BBA_INIT_BP
#define BBA_INIT_RRP	BBA_INIT_BP

#define BBA_NAPI_WEIGHT 16

#define RX_BUFFERS		16

#define cpu_to_be16(x) (x)
#define cpu_to_be32(x) (x)
static inline u16 cpu_to_le16(u16 x) { return (x<<8) | (x>>8);}
static inline u32 cpu_to_le32(u32 x) { return((x>>24) | ((x>>8)&0xff00) | ((x<<8)&0xff0000) | (x<<24));}

#define cpu_to_le16p(addr) (cpu_to_le16(*(addr)))
#define cpu_to_le32p(addr) (cpu_to_le32(*(addr)))
#define cpu_to_be16p(addr) (cpu_to_be16(*(addr)))
#define cpu_to_be32p(addr) (cpu_to_be32(*(addr)))

static inline void cpu_to_le16s(u16 *a) {*a = cpu_to_le16(*a);}
static inline void cpu_to_le32s(u32 *a) {*a = cpu_to_le32(*a);}
static inline void cpu_to_be16s(u16 *a) {*a = cpu_to_be16(*a);}
static inline void cpu_to_be32s(u32 *a) {*a = cpu_to_be32(*a);}

#define le16_to_cpup(x) cpu_to_le16p(x)
#define le32_to_cpup(x) cpu_to_le32p(x)
#define be16_to_cpup(x) cpu_to_be16p(x)
#define be32_to_cpup(x) cpu_to_be32p(x)

#define le16_to_cpus(x) cpu_to_le16s(x)
#define le32_to_cpus(x) cpu_to_le32s(x)
#define be16_to_cpus(x) cpu_to_be16s(x)
#define be32_to_cpus(x) cpu_to_be32s(x)

struct bba_priv {
	u8 revid;
	u16 devid;
	u8 acstart;
	s8_t state;
	struct uip_eth_addr *ethaddr;
};

#define X(a,b)  b,a
struct bba_descr {
	u32 X(X(next_packet_ptr:12, packet_len:12), status:8);
} __attribute((packed));

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

/* new functions */
#define bba_select()		EXI_Select(EXI_CHANNEL_0,EXI_DEVICE_2,EXI_SPEED32MHZ)
#define bba_deselect()		EXI_Deselect(EXI_CHANNEL_0)

#define bba_in12(reg)		((bba_in8(reg)&0xff)|((bba_in8((reg)+1)&0x0f)<<8))
#define bba_out12(reg,val)	do { \
									bba_out8((reg),(val)&0xff); \
									bba_out8((reg)+1,((val)&0x0f00)>>8); \
							} while(0)

#if UIP_LOGGING == 1
#include <stdio.h>
#define UIP_LOG(m) uip_log(__FILE__,__LINE__,m)
#else
#define UIP_LOG(m)
#endif /* UIP_LOGGING == 1 */

#if UIP_STATISTICS == 1
struct uip_stats uip_stat;
#define UIP_STAT(s) s
#else
#define UIP_STAT(s)
#endif /* UIP_STATISTICS == 1 */

static s64 bba_arp_tmr = 0;

static struct uip_pbuf *bba_recv_pbufs = NULL;
static struct uip_netif *bba_netif = NULL;
static struct bba_priv bba_device;
static struct bba_descr cur_descr;

static void bba_cmd_ins(u32 reg,void *val,u32 len);
static void bba_cmd_outs(u32 reg,void *val,u32 len);
static void bba_ins(u32 reg,void *val,u32 len);
static void bba_outs(u32 reg,void *val,u32 len);

static void bba_devpoll(u16 *pstatus);

extern void udelay(int us);
extern u32 diff_msec(long long start,long long end);
extern u32 diff_usec(long long start,long long end);
extern long long gettime();

static __inline__ void bba_cmd_insnosel(u32 reg,void *val,u32 len)
{
	u16 req;
	req = reg<<8;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_READ);
}

static void bba_cmd_ins(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_cmd_insnosel(reg,val,len);
	bba_deselect();
}

static __inline__ void bba_cmd_outsnosel(u32 reg,void *val,u32 len)
{
	u16 req;
	req = (reg<<8)|0x4000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_WRITE);
}

static void bba_cmd_outs(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_cmd_outsnosel(reg,val,len);
	bba_deselect();
}

static inline u8 bba_cmd_in8(u32 reg)
{
	u8 val;
	bba_cmd_ins(reg,&val,sizeof(val));
	return val;
}

static inline u8 bba_cmd_in8_slow(u32 reg)
{
	u8 val;
	bba_select();
	bba_cmd_insnosel(reg,&val,sizeof(val));
	udelay(200);			//usleep doesn't work on this amount, decrementer is based on 10ms, wait is 200us
	bba_deselect();
	return val;
}

static inline void bba_cmd_out8(u32 reg,u8 val)
{
	bba_cmd_outs(reg,&val,sizeof(val));
}

static inline u8 bba_in8(u32 reg)
{
	u8 val;
	bba_ins(reg,&val,sizeof(val));
	return val;
}

static inline void bba_out8(u32 reg,u8 val)
{
	bba_outs(reg,&val,sizeof(val));
}

static inline void bba_insnosel(u32 reg,void *val,u32 len)
{
	u32 req;
	req = (reg<<8)|0x80000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_READ);
}

static void bba_ins(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_insnosel(reg,val,len);
	bba_deselect();
}

static inline void bba_outsnoselect(u32 reg,void *val,u32 len)
{
	u32 req;
	req = (reg<<8)|0xC0000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_WRITE);
}

static void bba_outs(u32 reg,void *val,u32 len)
{
	bba_select();
	bba_outsnoselect(reg,val,len);
	bba_deselect();
}

static inline void bba_insregister(u32 reg)
{
	u32 req;
	req = (reg<<8)|0x80000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
}

static inline void bba_insdata(void *val,u32 len)
{
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_READ);
}

static inline void bba_outsregister(u32 reg)
{
	u32 req;
	req = (reg<<8)|0xC0000000;
	EXI_Imm(EXI_CHANNEL_0,&req,sizeof(req),EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
}

static inline void bba_outsdata(void *val,u32 len)
{
	EXI_ImmEx(EXI_CHANNEL_0,val,len,EXI_WRITE);
}

static __inline__ u32 __linkstate()
{
	u8 nways = 0;

	nways = bba_in8(BBA_NWAYS);
	if(nways&BBA_NWAYS_LS10 || nways&BBA_NWAYS_LS100) return 1;
	return 0;
}

static u32 __bba_getlink_state_async()
{
	u32 ret;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL)==0) return 0;
	ret = __linkstate();
	EXI_Unlock(EXI_CHANNEL_0);
	return ret;
}

static u32 __bba_read_cid()
{
	u16 cmd = 0;
	u32 cid = 0;

	bba_select();
	EXI_Imm(EXI_CHANNEL_0,&cmd,2,EXI_WRITE,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	EXI_Imm(EXI_CHANNEL_0,&cid,4,EXI_READ,NULL);
	EXI_Sync(EXI_CHANNEL_0);
	bba_deselect();

	return cid;
}
static void __bba_reset()
{
	bba_out8(0x60,0x00);
	udelay(10000);
	bba_cmd_in8_slow(0x0F);
	udelay(10000);
	bba_out8(BBA_NCRA,BBA_NCRA_RESET);
	bba_out8(BBA_NCRA,0x00);
}

static void __bba_recv_init()
{
	bba_out8(BBA_NCRB,(BBA_NCRB_CA|BBA_NCRB_AB));
	bba_out8(BBA_MISC2,(BBA_MISC2_AUTORCVR));

	bba_out12(BBA_TLBP, BBA_INIT_TLBP);
	bba_out12(BBA_BP,BBA_INIT_BP);
	bba_out12(BBA_RWP,BBA_INIT_RWP);
	bba_out12(BBA_RRP,BBA_INIT_RRP);
	bba_out12(BBA_RHBP,BBA_INIT_RHBP);

	bba_out8(BBA_GCA,BBA_GCA_ARXERRB);
	bba_out8(BBA_NCRA,BBA_NCRA_SR);
}

static void bba_process(struct uip_pbuf *p,struct uip_netif *dev)
{
	struct uip_eth_hdr *ethhdr = NULL;
	struct bba_priv *priv = (struct bba_priv*)dev->state;
	const s32 ethhlen = sizeof(struct uip_eth_hdr);

	if(p) {
		ethhdr = p->payload;
		switch(htons(ethhdr->type)) {
			case UIP_ETHTYPE_IP:
				uip_arp_ipin(dev,p);
				uip_pbuf_header(p,-(ethhlen));
				dev->input(p,dev);
				break;
			case UIP_ETHTYPE_ARP:
				uip_arp_arpin(dev,priv->ethaddr,p);
				break;
			default:
				uip_pbuf_free(p);
				break;
		}
	}
}

static s8_t bba_start_rx(struct uip_netif *dev,u32 budget)
{
	s32 size;
	u16 top,pos,rwp,rrp;
	u32 pkt_status,recvd;
	struct uip_pbuf *p,*q;

	UIP_LOG("bba_start_rx()\n");

	recvd = 0;
	rwp = bba_in12(BBA_RWP);
	rrp = bba_in12(BBA_RRP);
	while(recvd<budget && rrp!=rwp) {
		bba_ins(rrp<<8,(void*)(&cur_descr),sizeof(struct bba_descr));
		le32_to_cpus((u32*)((void*)(&cur_descr)));

		size = cur_descr.packet_len - 4;
		pkt_status = cur_descr.status;
		if(size>(BBA_RX_MAX_PACKET_SIZE+4)) {
			UIP_LOG("bba_start_rx: packet dropped due to big buffer.\n");
			continue;
		}

		if(pkt_status&(BBA_RX_STATUS_RERR|BBA_RX_STATUS_FAE)) {
			UIP_LOG("bba_start_rx: packet dropped due to receive errors.\n");
			rwp = bba_in12(BBA_RWP);
			rrp = bba_in12(BBA_RRP);
			continue;
		}

		pos = (rrp<<8)+4;
		top = (BBA_INIT_RHBP+1)<<8;

		p = uip_pbuf_alloc(UIP_PBUF_RAW,size,UIP_PBUF_POOL);
		if(p) {
			for(q=p;q!=NULL;q=q->next) {
				bba_select();
				bba_insregister(pos);
				if((pos+size)<top) {
					bba_insdata(q->payload,size);
				} else {
					s32 chunk = top-pos;

					size -= chunk;
					pos = BBA_INIT_RRP<<8;
					bba_insdata(q->payload,chunk);
					bba_deselect();

					bba_select();
					bba_insregister(pos);
					bba_insdata(q->payload+chunk,size);
				}
				bba_deselect();
				pos += size;
			}

			EXI_Unlock(EXI_CHANNEL_0);
			bba_process(p,dev);
			EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL);
		} else
			break;

		recvd++;

		bba_out12(BBA_RRP,(rrp=cur_descr.next_packet_ptr));
		rwp = bba_in12(BBA_RWP);
	}
	return UIP_ERR_OK;
}

static inline void bba_interrupt(u16 *pstatus)
{
	u8 ir,imr,status;

	ir = bba_in8(BBA_IR);
	imr = bba_in8(BBA_IMR);
	status = ir&imr;

	if(status&BBA_IR_FRAGI) {
		bba_out8(BBA_IR,BBA_IR_FRAGI);
	}
	if(status&BBA_IR_RI) {
		bba_start_rx(bba_netif,0x10);
		bba_out8(BBA_IR,BBA_IR_RI);
	}
	if(status&BBA_IR_REI) {
		bba_out8(BBA_IR,BBA_IR_REI);
	}
	if(status&BBA_IR_TI) {
		bba_out8(BBA_IR,BBA_IR_TI);
	}
	if(status&BBA_IR_TEI) {
		bba_out8(BBA_IR,BBA_IR_TEI);
	}
	if(status&BBA_IR_FIFOEI) {
		bba_out8(BBA_IR,BBA_IR_FIFOEI);
	}
	if(status&BBA_IR_BUSEI) {
		bba_out8(BBA_IR,BBA_IR_BUSEI);
	}
	if(status&BBA_IR_RBFI) {
		bba_start_rx(bba_netif,0x10);
		bba_out8(BBA_IR,BBA_IR_RBFI);
	}
	*pstatus |= status;
}

static s8_t bba_dochallengeresponse()
{
	u16 status;
	s32 cnt;

	UIP_LOG("bba_dochallengeresponse()\n");
	/* as we do not have interrupts we've to poll the irqs */
	cnt = 0;
	do {
		cnt++;
		bba_devpoll(&status);
		if(status==0x1000) cnt = 0;
	} while(cnt<100 && !(status&0x0800));

	if(cnt>=1000) return UIP_ERR_IF;
	return UIP_ERR_OK;
}

static s8_t __bba_init(struct uip_netif *dev)
{
	struct bba_priv *priv = (struct bba_priv*)dev->state;
	if(!priv) return UIP_ERR_IF;

	__bba_reset();

	priv->revid = bba_cmd_in8(0x01);

	bba_cmd_outs(0x04,&priv->devid,2);
	bba_cmd_out8(0x05,priv->acstart);

	bba_out8(0x5b, (bba_in8(0x5b)&~0x80));
	bba_out8(0x5e, 0x01);
	bba_out8(0x5c, (bba_in8(0x5c)|0x04));

	__bba_recv_init();

	bba_ins(BBA_NAFR_PAR0,priv->ethaddr->addr, 6);

	bba_out8(BBA_IR,0xFF);
	bba_out8(BBA_IMR,0xFF&~BBA_IMR_FIFOEIM);

	bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);

	return UIP_ERR_OK;
}

static s8_t bba_init_one(struct uip_netif *dev)
{
	s32 ret;
	struct bba_priv *priv = (struct bba_priv*)dev->state;

	if(!priv) return UIP_ERR_IF;

	priv->revid = 0x00;
	priv->devid = 0xD107;
	priv->acstart = 0x4E;

	ret = __bba_init(dev);

	return ret;
}

static s8_t bba_probe(struct uip_netif *dev)
{
	s32 ret;
	u32 cid;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL)==0) return -1;

	cid = __bba_read_cid();
	if(cid!=BBA_CID) {
		EXI_Unlock(EXI_CHANNEL_0);
		return -1;
	}

	ret = bba_init_one(dev);

	EXI_Unlock(EXI_CHANNEL_0);
	return ret;
}

static u32 bba_calc_response(struct uip_netif *dev,u32 val)
{
	u8 revid_0, revid_eth_0, revid_eth_1;
	struct bba_priv *priv = (struct bba_priv*)dev->state;

	UIP_LOG("bba_calc_response()\n");

	revid_0 = priv->revid;
	revid_eth_0 = _SHIFTR(priv->devid,8,8);
	revid_eth_1 = priv->devid&0xff;

	u8 i0, i1, i2, i3;
	i0 = (val & 0xff000000) >> 24;
	i1 = (val & 0x00ff0000) >> 16;
	i2 = (val & 0x0000ff00) >> 8;
	i3 = (val & 0x000000ff);

	u8 c0, c1, c2, c3;
	c0 = ((i0 + i1 * 0xc1 + 0x18 + revid_0) ^ (i3 * i2 + 0x90)
	    ) & 0xff;
	c1 = ((i1 + i2 + 0x90) ^ (c0 + i0 - 0xc1)
	    ) & 0xff;
	c2 = ((i2 + 0xc8) ^ (c0 + ((revid_eth_0 + revid_0 * 0x23) ^ 0x19))
	    ) & 0xff;
	c3 = ((i0 + 0xc1) ^ (i3 + ((revid_eth_1 + 0xc8) ^ 0x90))
	    ) & 0xff;

	return ((c0 << 24) | (c1 << 16) | (c2 << 8) | c3);
}

static void bba_devpoll(u16 *pstatus)
{
	u8 status;
	s64 now;

	UIP_LOG("bba_devpoll()\n");

	now = gettime();
	if(diff_msec(bba_arp_tmr,now)>=UIP_ARP_TMRINTERVAL) {
		uip_arp_timer();
		bba_arp_tmr = gettime();
	}

	status = 0;
	*pstatus = 0;
	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL)==1) {
		status = bba_cmd_in8(0x03);
		if(status) {
			bba_cmd_out8(0x02,BBA_CMD_IRMASKALL);
			if(status&0x80) {
				*pstatus |= (status<<8);
				bba_interrupt(pstatus);
				bba_cmd_out8(0x03,0x80);
				bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
				EXI_Unlock(EXI_CHANNEL_0);
				return;
			}
			if(status&0x40) {
				*pstatus |= (status<<8);
				__bba_init(bba_netif);
				bba_cmd_out8(0x03, 0x40);
				bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
				EXI_Unlock(EXI_CHANNEL_0);
				return;
			}
			if(status&0x20) {
				*pstatus |= (status<<8);
				bba_cmd_out8(0x03, 0x20);
				bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
				EXI_Unlock(EXI_CHANNEL_0);
				return;
			}
			if(status&0x10) {
				u32 response,challange;

				*pstatus |= (status<<8);
				bba_cmd_out8(0x05,bba_device.acstart);
				bba_cmd_ins(0x08,&challange,sizeof(challange));
				response = bba_calc_response(bba_netif,challange);
				bba_cmd_outs(0x09,&response,sizeof(response));
				bba_cmd_out8(0x03, 0x10);
				bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
				EXI_Unlock(EXI_CHANNEL_0);
				return;
			}
			if(status&0x08) {
				*pstatus |= (status<<8);
				bba_cmd_out8(0x03, 0x08);
				bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
				EXI_Unlock(EXI_CHANNEL_0);
				return;
			}

			*pstatus |= (status<<8);
			bba_interrupt(pstatus);
			bba_cmd_out8(0x02,BBA_CMD_IRMASKNONE);
		}
		EXI_Unlock(EXI_CHANNEL_0);
	}
}

static s8_t __bba_start_tx(struct uip_netif *dev,struct uip_pbuf *p,struct uip_ip_addr *ipaddr)
{
	return uip_arp_out(dev,ipaddr,p);
}

static s8_t __bba_link_tx(struct uip_netif *dev,struct uip_pbuf *p)
{
	u8 pad[60];
	u32 len;
	struct uip_pbuf *tmp;

	if(EXI_Lock(EXI_CHANNEL_0,EXI_DEVICE_2,NULL)==0) return UIP_ERR_IF;

	if(p->tot_len>BBA_TX_MAX_PACKET_SIZE) {
		UIP_LOG("__bba_link_tx: packet dropped due to big buffer.\n");
		EXI_Unlock(EXI_CHANNEL_0);
		return UIP_ERR_PKTSIZE;
	}

	if(!__linkstate()) {
		EXI_Unlock(EXI_CHANNEL_0);
		return UIP_ERR_ABRT;
	}

	while((bba_in8(BBA_NCRA)&(BBA_NCRA_ST0|BBA_NCRA_ST1)));

	len = p->tot_len;
	bba_out12(BBA_TXFIFOCNT,len);

	bba_select();
	bba_outsregister(BBA_WRTXFIFOD);
	for(tmp=p;tmp!=NULL;tmp=tmp->next) {
		bba_outsdata(tmp->payload,tmp->len);
	}
	if(len<BBA_MINPKTSIZE) {
		len = (BBA_MINPKTSIZE-len);
		bba_outsdata(pad,len);
	}
	bba_deselect();

	bba_out8(BBA_NCRA,((bba_in8(BBA_NCRA)&~BBA_NCRA_ST0)|BBA_NCRA_ST1));		//&~BBA_NCRA_ST0
	EXI_Unlock(EXI_CHANNEL_0);
	return UIP_ERR_OK;
}

s8_t uip_bba_init(struct uip_netif *dev)
{
	s8_t ret;
	s32_t cnt;

	ret = bba_probe(dev);
	if(ret<0) return ret;

	ret = bba_dochallengeresponse();
	if(ret<0) return ret;

	cnt = 0;
	do {
		udelay(500);
		cnt++;
	} while((ret=__bba_getlink_state_async())==0 && cnt<10000);
	if(!ret) return UIP_ERR_IF;

	dev->flags |= UIP_NETIF_FLAG_LINK_UP;
	uip_netif_setup(dev);
	uip_arp_init();

	bba_recv_pbufs = NULL;
	bba_arp_tmr = gettime();

	return UIP_ERR_OK;
}

uipdev_s uip_bba_create(struct uip_netif *dev)
{
	dev->name[0] = IFNAME0;
	dev->name[1] = IFNAME1;

	dev->output = __bba_start_tx;
	dev->linkoutput = __bba_link_tx;
	dev->mtu = 1500;
	dev->flags = UIP_NETIF_FLAG_BROADCAST;
	dev->hwaddr_len = 6;

	bba_device.ethaddr = (struct uip_eth_addr*)dev->hwaddr;
	bba_device.state = UIP_ERR_OK;

	bba_netif = dev;
	return &bba_device;
}

void uip_bba_poll(struct uip_netif *dev)
{
	u16 status;

	UIP_LOG("uip_bba_poll()\n");

	bba_devpoll(&status);

}
