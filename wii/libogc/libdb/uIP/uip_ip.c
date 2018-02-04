#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "uip_ip.h"
#include "uip_tcp.h"
#include "uip_icmp.h"
#include "uip_netif.h"
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

const struct uip_ip_addr ipaddr_any = { 0x0000000UL };
const struct uip_ip_addr ipaddr_broadcast = { 0xffffffffUL };

#if UIP_IP_REASSEMBLY

#define UIP_REASS_FLAG_LASTFRAG		0x01
#define UIP_REASS_BUFSIZE			5760

static u8_t uip_reassbuf[UIP_IP_HLEN+UIP_REASS_BUFSIZE];
static u8_t uip_reassbitmap[UIP_REASS_BUFSIZE / (8 * 8)];
static const u8_t bitmap_bits[8] = {0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01};
static u16_t uip_reasslen;
static u8_t uip_reassflags;
static u8_t uip_reasstmr;
static s64 uip_reasstime = 0;

extern s64 gettime();

static struct uip_pbuf* uip_copyfrom_pbuf(struct uip_pbuf *p,u16_t *offset,u8_t *buffer,u16_t len)
{
	u16_t l;

	p->payload = (u8_t*)p->payload+(*offset);
	p->len -= (*offset);
	while(len) {
		l = len<p->len?len:p->len;
		UIP_MEMCPY(buffer,p->payload,l);
		buffer += l;
		len -= l;
		if(len) p = p->next;
		else *offset = l;
	}
	return p;
}

static struct uip_pbuf* uip_ipreass(struct uip_pbuf *p)
{
  u16_t offset, len;
  u16_t i;
  struct uip_pbuf *q;
  struct uip_ip_hdr *iphdr,*fraghdr;

  /* If ip_reasstmr is zero, no packet is present in the buffer, so we
     write the IP header of the fragment into the reassembly
     buffer. The timer is updated with the maximum age. */
  iphdr = (struct uip_ip_hdr*)uip_reassbuf;
  fraghdr = (struct uip_ip_hdr*)p->payload;
  if(uip_reasstmr == 0) {
    UIP_MEMCPY(iphdr, fraghdr, UIP_IP_HLEN);
    uip_reasstmr = UIP_REASS_MAXAGE;
    uip_reassflags = 0;
	uip_reasstime = gettime();
    /* Clear the bitmap. */
    UIP_MEMSET(uip_reassbitmap, 0,sizeof(uip_reassbitmap));
  }

  /* Check if the incoming fragment matches the one currently present
     in the reasembly buffer. If so, we proceed with copying the
     fragment into the buffer. */
  if(ip_addr_cmp(&iphdr->src,&fraghdr->src)
	  && ip_addr_cmp(&iphdr->dst,&fraghdr->dst)
	  && UIP_IPH_ID(iphdr) == UIP_IPH_ID(fraghdr)) {

	len = ntohs(UIP_IPH_LEN(fraghdr)) - UIP_IPH_HL(fraghdr)*4;
	offset = (ntohs(UIP_IPH_OFFSET(fraghdr))&UIP_IP_OFFMASK)*8;

    /* If the offset or the offset + fragment length overflows the
       reassembly buffer, we discard the entire packet. */
    if(offset > UIP_REASS_BUFSIZE ||
       offset + len > UIP_REASS_BUFSIZE) {
      uip_reasstmr = 0;
	  uip_reasstime = 0;
      goto nullreturn;
    }

    /* Copy the fragment into the reassembly buffer, at the right
       offset. */
	i = UIP_IPH_HL(fraghdr)*4;
    uip_copyfrom_pbuf(p,&i,&uip_reassbuf[UIP_IP_HLEN+offset],len);

    /* Update the bitmap. */
    if(offset / (8 * 8) == (offset + len) / (8 * 8)) {
      /* If the two endpoints are in the same byte, we only update
	 that byte. */

      uip_reassbitmap[offset / (8 * 8)] |=
	     bitmap_bits[(offset / 8 ) & 7] &
	     ~bitmap_bits[((offset + len) / 8 ) & 7];
    } else {
      /* If the two endpoints are in different bytes, we update the
	 bytes in the endpoints and fill the stuff inbetween with
	 0xff. */
      uip_reassbitmap[offset / (8 * 8)] |=
	bitmap_bits[(offset / 8 ) & 7];
      for(i = 1 + offset / (8 * 8); i < (offset + len) / (8 * 8); ++i) {
	uip_reassbitmap[i] = 0xff;
      }
      uip_reassbitmap[(offset + len) / (8 * 8)] |=
	~bitmap_bits[((offset + len) / 8 ) & 7];
    }

    /* If this fragment has the More Fragments flag set to zero, we
       know that this is the last fragment, so we can calculate the
       size of the entire packet. We also set the
       IP_REASS_FLAG_LASTFRAG flag to indicate that we have received
       the final fragment. */

    if((ntohs(UIP_IPH_OFFSET(fraghdr))&UIP_IP_MF)==0) {
      uip_reassflags |= UIP_REASS_FLAG_LASTFRAG;
      uip_reasslen = offset + len;
    }

    /* Finally, we check if we have a full packet in the buffer. We do
       this by checking if we have the last fragment and if all bits
       in the bitmap are set. */
    if(uip_reassflags & UIP_REASS_FLAG_LASTFRAG) {
      /* Check all bytes up to and including all but the last byte in
	 the bitmap. */
      for(i = 0; i < uip_reasslen / (8 * 8) - 1; ++i) {
	if(uip_reassbitmap[i] != 0xff) {
	  goto nullreturn;
	}
      }
      /* Check the last byte in the bitmap. It should contain just the
	 right amount of bits. */
      if(uip_reassbitmap[uip_reasslen / (8 * 8)] !=
	 (u8_t)~bitmap_bits[uip_reasslen / 8 & 7]) {
	goto nullreturn;
      }

	  uip_reasslen += UIP_IP_HLEN;

      /* Pretend to be a "normal" (i.e., not fragmented) IP packet
	 from now on. */
	  UIP_IPH_LEN_SET(iphdr,htons(uip_reasslen));
	  UIP_IPH_OFFSET_SET(iphdr,0);
	  UIP_IPH_CHKSUM_SET(iphdr,0);
	  UIP_IPH_CHKSUM_SET(iphdr,uip_ipchksum(iphdr,UIP_IP_HLEN));

      /* If we have come this far, we have a full packet in the
	 buffer, so we allocate a pbuf and copy the packet into it. We
	 also reset the timer. */
      uip_reasstmr = 0;
	  uip_reasstime = 0;
	  uip_pbuf_free(p);
	  p = uip_pbuf_alloc(UIP_PBUF_LINK,uip_reasslen,UIP_PBUF_POOL);
	  if(p==NULL) return NULL;

	  i = 0;
	  for(q=p;q!=NULL;q=q->next) {
		  UIP_MEMCPY(q->payload,&uip_reassbuf[i],((q->len>(uip_reasslen-i))?(uip_reasslen-i):q->len));
		  i += q->len;
	  }
      return p;
    }
  }

 nullreturn:
  uip_pbuf_free(p);
  return NULL;
}
#endif /* UIP_IP_REASSEMBLY */

#if UIP_IP_FRAG
#define MAX_MTU			1500
static u8_t buf[MEM_ALIGN_SIZE(MAX_MTU)];

s8_t uip_ipfrag(struct uip_pbuf *p,struct uip_netif *netif,struct uip_ip_addr *ipaddr)
{
	struct uip_pbuf *rambuf;
	struct uip_pbuf *header;
	struct uip_ip_hdr *iphdr;
	u16_t left,cop,ofo,omf,last,tmp;
	u16_t mtu = netif->mtu;
	u16_t poff = UIP_IP_HLEN;
	u16_t nfb = 0;

	rambuf = uip_pbuf_alloc(UIP_PBUF_LINK,0,UIP_PBUF_REF);
	rambuf->tot_len = rambuf->len = mtu;
	rambuf->payload = MEM_ALIGN(buf);

	iphdr = rambuf->payload;
	UIP_MEMCPY(iphdr,p->payload,UIP_IP_HLEN);

	tmp = ntohs(UIP_IPH_OFFSET(iphdr));
	ofo = tmp&UIP_IP_OFFMASK;
	omf = tmp&UIP_IP_MF;

	left = p->tot_len - UIP_IP_HLEN;
	while(left) {
		last = (left<=(mtu-UIP_IP_HLEN));

		ofo += nfb;
		tmp = omf|(UIP_IP_OFFMASK&ofo);

		if(!last) tmp |= UIP_IP_MF;
		UIP_IPH_OFFSET_SET(iphdr,htons(tmp));

		nfb = (mtu - UIP_IP_HLEN)/8;
		cop = last?left:nfb*8;

		p = uip_copyfrom_pbuf(p,&poff,(u8_t*)iphdr+UIP_IP_HLEN,cop);

		UIP_IPH_LEN_SET(iphdr,htons(cop+UIP_IP_HLEN));
		UIP_IPH_CHKSUM_SET(iphdr,0);
		UIP_IPH_CHKSUM_SET(iphdr,uip_ipchksum(iphdr,UIP_IP_HLEN));

		if(last) uip_pbuf_realloc(rambuf,left+UIP_IP_HLEN);

		header = uip_pbuf_alloc(UIP_PBUF_LINK,0,UIP_PBUF_RAM);
		uip_pbuf_chain(header,rambuf);
		netif->output(netif,header,ipaddr);
		uip_pbuf_free(header);

		left -= cop;
	}
	uip_pbuf_free(rambuf);
	return UIP_ERR_OK;
}
#endif /* UIP_IP_FRAG */

struct uip_netif* uip_iproute(struct uip_ip_addr *dst)
{
	struct uip_netif *netif;

	for(netif=uip_netif_list;netif!=NULL;netif=netif->next) {
		if(ip_addr_netcmp(dst,&netif->ip_addr,&netif->netmask)) return netif;
	}

	return uip_netif_default;
}

u8_t uip_ipaddr_isbroadcast(struct uip_ip_addr *addr,struct uip_netif *netif)
{
	if(addr->addr==ipaddr_broadcast.addr
		|| addr->addr==ipaddr_any.addr)
		return 1;
	else if(!(netif->flags&UIP_NETIF_FLAG_BROADCAST))
		return 0;
	else if(addr->addr==netif->ip_addr.addr)
		return 0;
	else if(ip_addr_netcmp(addr,&netif->ip_addr,&netif->netmask)
		&& ((addr->addr&~netif->netmask.addr)==(ipaddr_broadcast.addr&~netif->netmask.addr)))
		return 1;
	else
		return 0;
}

s8_t uip_ipinput(struct uip_pbuf *p,struct uip_netif *inp)
{
	u16_t iphdr_len;
	struct uip_ip_hdr *iphdr;
	struct uip_netif *netif;

	iphdr = p->payload;
	if(UIP_IPH_V(iphdr)!=4) {
		UIP_ERROR("uip_ipinput: ip packet dropped due to bad version number.\n");
		uip_pbuf_free(p);
		return 0;
	}

	iphdr_len = UIP_IPH_HL(iphdr);
	iphdr_len *= 4;

	if(iphdr_len>p->len) {
		UIP_ERROR("uip_ipinput: ip packet dropped due to too small packet size.\n");
		uip_pbuf_free(p);
		return 0;
	}

	if(uip_ipchksum(iphdr,iphdr_len)!=0) {
	    UIP_STAT(++uip_stat.ip.drop);
	    UIP_STAT(++uip_stat.ip.chkerr);
		UIP_ERROR("uip_ipinput: bad checksum.\n");
		uip_pbuf_free(p);
		return 0;
	}

	uip_pbuf_realloc(p,ntohs(UIP_IPH_LEN(iphdr)));

	for(netif=uip_netif_list;netif!=NULL;netif=netif->next) {
		if(uip_netif_isup(netif) && !ip_addr_isany(&netif->ip_addr)) {
			if(ip_addr_cmp(&iphdr->dst,&netif->ip_addr) ||
				ip_addr_isbroadcast(&iphdr->dst,netif)) break;
		}
	}

	if(!netif) {
		UIP_ERROR("uip_ipinput: no route found.\n");
		uip_pbuf_free(p);
		return 0;
	}

	if((UIP_IPH_OFFSET(iphdr)&htons(UIP_IP_OFFMASK|UIP_IP_MF))!=0) {
#if UIP_IP_REASSEMBLY
		p = uip_ipreass(p);
		if(p==NULL) return UIP_ERR_OK;

		iphdr = (struct uip_ip_hdr*)p->payload;
#else
		uip_pbuf_free(p);
	    UIP_STAT(++uip_stat.ip.drop);
		UIP_ERROR("ip: fragment dropped.\n");
		return 0;
#endif
	}

	switch(UIP_IPH_PROTO(iphdr)) {
		case UIP_PROTO_TCP:
			uip_tcpinput(p,inp);
			break;
		case UIP_PROTO_ICMP:
			uip_icmpinput(p,inp);
			break;
		default:
			UIP_LOG("uip_ipinput: Unsupported protocol.\n");
			if(!ip_addr_isbroadcast(&(iphdr->dst),inp)
				&& !ip_addr_ismulticast(&(iphdr->dst))) {
				p->payload = iphdr;
				uip_icmp_destunreach(p,UIP_ICMP_DUR_PROTO);
			}
			uip_pbuf_free(p);
			break;
	}
	return 0;
}

s8_t uip_ipoutput_if(struct uip_pbuf *p,struct uip_ip_addr *src,struct uip_ip_addr *dst,u8_t ttl,u8_t tos,u8_t proto,struct uip_netif *netif)
{
	struct uip_ip_hdr *iphdr = NULL;
	u16_t ip_id = 0;

	if(dst!=NULL) {
		if(uip_pbuf_header(p,UIP_IP_HLEN)) {
			UIP_ERROR("uip_ipoutput_if: not enough room for IP header in pbuf.\n");
			return UIP_ERR_BUF;
		}

		iphdr = p->payload;

		UIP_IPH_TTL_SET(iphdr,ttl);
		UIP_IPH_PROTO_SET(iphdr,proto);

		ip_addr_set(&iphdr->dst,dst);

		UIP_IPH_VHLTOS_SET(iphdr,4,(UIP_IP_HLEN/4),tos);
		UIP_IPH_LEN_SET(iphdr,htons(p->tot_len));
		UIP_IPH_OFFSET_SET(iphdr,htons(UIP_IP_DF));
		UIP_IPH_ID_SET(iphdr,htons(ip_id));
		++ip_id;

		if(ip_addr_isany(src)) ip_addr_set(&iphdr->src,&netif->ip_addr);
		else ip_addr_set(&iphdr->src,src);

		UIP_IPH_CHKSUM_SET(iphdr,0);
		UIP_IPH_CHKSUM_SET(iphdr,uip_ipchksum(iphdr,UIP_IP_HLEN));
	} else {
		iphdr = p->payload;
		dst = &iphdr->dst;
	}
#if UIP_IP_FRAG
	if(netif->mtu && p->tot_len>netif->mtu)
		return uip_ipfrag(p,netif,dst);
#endif
	return netif->output(netif,p,dst);
}

s8_t uip_ipoutput(struct uip_pbuf *p,struct uip_ip_addr *src,struct uip_ip_addr *dst,u8_t ttl,u8_t tos,u8_t proto)
{
	struct uip_netif *netif;

	if((netif=uip_iproute(dst))==NULL) {
		UIP_ERROR("uip_ipoutput: No route found.\n");
		return UIP_ERR_RTE;
	}
	return uip_ipoutput_if(p,src,dst,ttl,tos,proto,netif);
}

void uip_ipinit()
{

}

s32_t uip_ipaton(const u8_t *cp,struct in_addr *addr)
{
	u32_t val;
	u8_t c;
	u32_t parts[4];
	u32_t *pp = parts;
	int base,n;

	c = *cp;
	for(;;) {
		if(!isdigit(c)) return 0;

		val = 0; base = 10;
		if(c=='0') {
			c = *++cp;
			if(c=='x' || c=='X')
				base = 16, c = *++cp;
			else
				base = 8;
		}
		for(;;) {
			if(isdigit(c)) {
				val = (val*base)+(int)(c-'0');
				c = *++cp;
			} else if(base==16 && isxdigit(c)) {
				val = (val<<4)|(int)(c+10-(islower(c)?'a':'A'));
				c = *++cp;
			} else
				break;
		}
		if(c=='.') {
			if(pp>=parts+3) return 0;
			*pp++ = val;
			c = *++cp;
		} else
			break;
	}

	if(c!='\0' && (!isascii(c) || isspace(c))) return 0;

	n = pp-parts+1;
	switch(n) {
		case 0:
			return 0;
		case 1:
			break;
		case 2:
			if(val>0x00ffffff) return 0;

			val |= (parts[0]<<24);
			break;
		case 3:
			if(val>0x0000ffff) return 0;

			val |= (parts[0]<<24)|(parts[1]<<16);
			break;
		case 4:
			if(val>0x000000ff) return 0;

			val |= (parts[0]<<24)|(parts[1]<<16)|(parts[2]<<8);
			break;
	}
	if(addr)
		addr->s_addr = htonl(val);

	return 1;
}

u32_t uip_ipaddr(const u8_t *cp)
{
	struct in_addr val;

	if(uip_ipaton(cp,&val)) return (val.s_addr);

	return (UIP_INADDR_NONE);
}
