/* Host names, IPv4 only. */

#ifndef GEKKO_NETDB_H
#define GEKKO_NETDB_H

#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hostent
{
   char  *h_name;
   char **h_aliases;
   int    h_addrtype;
   int    h_length;
   char **h_addr_list;
};
#define h_addr h_addr_list[0]

/* One result per thread is not kept: the last lookup's, any thread. */
struct hostent *gethostbyname(const char *name);

#ifdef __cplusplus
}
#endif

#endif
