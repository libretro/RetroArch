#ifndef _ARPA_INET_H
#define _ARPA_INET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>
#include <sys/socket.h>

char *inet_ntoa(struct in_addr in);
const char *inet_ntop(int af, const void *cp, char *buf, socklen_t len);

int inet_aton(const char *cp, struct in_addr *inp);

int inet_pton(int af, const char *cp, void *buf);
#ifdef __cplusplus
}
#endif

#endif /* _ARPA_INET_H */
