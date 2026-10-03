/* Address text. */

#ifndef GEKKO_ARPA_INET_H
#define GEKKO_ARPA_INET_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

int         inet_aton(const char *s, struct in_addr *addr);
in_addr_t   inet_addr(const char *s);
char       *inet_ntoa(struct in_addr addr);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t len);
int         inet_pton(int af, const char *src, void *dst);

#ifdef __cplusplus
}
#endif

#endif
