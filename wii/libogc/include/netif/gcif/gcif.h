#ifndef __GCIF_H__
#define __GCIF_H__

#include <lwip/netif.h>

#define ERR_NODATA		-12
#define ERR_ALLREAD		-13
#define ERR_TXERROR		-14
#define ERR_RXERROR		-14
#define ERR_NODEV		-16
#define ERR_PKTSIZE		-17
#define ERR_TXPENDING	-18

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

typedef void* dev_s;

dev_s bba_create(struct netif *);
err_t bba_init(struct netif *);
void bba_process(struct pbuf *p,struct netif *dev);

#endif
