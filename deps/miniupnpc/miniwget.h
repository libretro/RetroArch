/* $Id: miniwget.h,v 1.12 2016/01/24 17:24:36 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2016 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#ifndef MINIWGET_H_INCLUDED
#define MINIWGET_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

 void * getHTTPResponse(int s, int * size, int * status_code);

 void * miniwget(const char *, int *, unsigned int, int *);

 void * miniwget_getaddr(const char *, int *, char *, int, unsigned int, int *);

int parseURL(const char *, char *, unsigned short *, char * *, unsigned int *);

#ifdef __cplusplus
}
#endif

#endif

