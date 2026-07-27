/*
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "ps3_compat.h"

int nfs_getnameinfo(const struct sockaddr *sa, socklen_t salen,
char *host, size_t hostlen,
char *serv, size_t servlen, int flags)
{
  struct sockaddr_in *sin = (struct sockaddr_in *)sa;

  if (host) {
    snprintf(host, hostlen, inet_ntoa(sin->sin_addr));
  }

  return 0;
}

int nfs_getaddrinfo(const char *node, const char*service,
const struct addrinfo *hints,
struct addrinfo **res)
{
  struct sockaddr_in *sin;

  sin = malloc(sizeof(struct sockaddr_in));
  sin->sin_len = sizeof(struct sockaddr_in);
  sin->sin_family=AF_INET;

  /* Some error checking would be nice */
  sin->sin_addr.s_addr = inet_addr(node);

  sin->sin_port=0;
  if (service) {
    sin->sin_port=htons(atoi(service));
  } 

  *res = malloc(sizeof(struct addrinfo));
  memset(*res, 0, sizeof(struct addrinfo));

  (*res)->ai_family = AF_INET;
  (*res)->ai_addrlen = sizeof(struct sockaddr_in);
  (*res)->ai_addr = (struct sockaddr *)sin;

  return 0;
}

void nfs_freeaddrinfo(struct addrinfo *res)
{
  free(res->ai_addr);
  free(res);
}
