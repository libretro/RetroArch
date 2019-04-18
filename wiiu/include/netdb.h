#ifndef	_NETDB_H
#define _NETDB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef uint32_t socklen_t;

#define NI_NUMERICHOST  2
#define NI_NUMERICSERV  8

#define AI_PASSIVE      1

#define NI_MAXHOST      1025
#define NI_MAXSERV      32

struct addrinfo {
  int     ai_flags;     /* AI_PASSIVE, AI_CANONNAME,
                           AI_NUMERICHOST, .. */
  int     ai_family;    /* AF_xxx */
  int     ai_socktype;  /* SOCK_xxx */
  int     ai_protocol;  /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
  int     ai_addrlen;   /* length of ai_addr */
  char   *ai_canonname; /* canonical name for node name */
  struct sockaddr  *ai_addr; /* binary address */
  struct addrinfo  *ai_next; /* next structure in linked list */
};

int getaddrinfo(const char *node, const char *service, struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *__ai);
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);

#ifdef __cplusplus
}
#endif

#endif	/* _NETDB_H */
