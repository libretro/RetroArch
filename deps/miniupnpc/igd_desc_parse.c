/* $Id: igd_desc_parse.c,v 1.17 2015/09/15 13:30:04 nanard Exp $ */
/* Project : miniupnp
 * http://miniupnp.free.fr/
 * Author : Thomas Bernard
 * Copyright (c) 2005-2015 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution. */

#include "igd_desc_parse.h"
#include <string.h>

/* Start element handler :
 * update nesting level counter and copy element name */
void IGDstartelt(void * d, const char * name, int l)
{
   struct IGDdatas * datas = (struct IGDdatas *)d;
   if(l >= MINIUPNPC_URL_MAXSIZE)
      l = MINIUPNPC_URL_MAXSIZE-1;
   memcpy(datas->cureltname, name, l);
   datas->cureltname[l] = '\0';
   datas->level++;
   if( (l==7) && !memcmp(name, "service", l) ) {
      datas->tmp.controlurl[0] = '\0';
      datas->tmp.eventsuburl[0] = '\0';
      datas->tmp.scpdurl[0] = '\0';
      datas->tmp.servicetype[0] = '\0';
   }
}

#define COMPARE(str, cstr) (0==memcmp(str, cstr, sizeof(cstr) - 1))

/* End element handler :
 * update nesting level counter and update parser state if
 * service element is parsed */
void IGDendelt(void * d, const char * name, int l)
{
   struct IGDdatas * datas = (struct IGDdatas *)d;
   datas->level--;
   if( (l==7) && !memcmp(name, "service", l) )
   {
      if(COMPARE(datas->tmp.servicetype,
               "urn:schemas-upnp-org:service:WANCommonInterfaceConfig:")) {
         memcpy(&datas->CIF, &datas->tmp, sizeof(struct IGDdatas_service));
      } else if(COMPARE(datas->tmp.servicetype,
               "urn:schemas-upnp-org:service:WANIPv6FirewallControl:")) {
         memcpy(&datas->IPv6FC, &datas->tmp, sizeof(struct IGDdatas_service));
      } else if(COMPARE(datas->tmp.servicetype,
               "urn:schemas-upnp-org:service:WANIPConnection:")
            || COMPARE(datas->tmp.servicetype,
               "urn:schemas-upnp-org:service:WANPPPConnection:") ) {
         if(datas->first.servicetype[0] == '\0') {
            memcpy(&datas->first, &datas->tmp, sizeof(struct IGDdatas_service));
         } else {
            memcpy(&datas->second, &datas->tmp, sizeof(struct IGDdatas_service));
         }
      }
   }
}

/* Data handler :
 * copy data depending on the current element name and state */
void IGDdata(void * d, const char * data, int l)
{
	struct IGDdatas * datas = (struct IGDdatas *)d;
	char * dstmember = 0;
	if( !strcmp(datas->cureltname, "URLBase") )
		dstmember = datas->urlbase;
	else if( !strcmp(datas->cureltname, "presentationURL") )
		dstmember = datas->presentationurl;
	else if( !strcmp(datas->cureltname, "serviceType") )
		dstmember = datas->tmp.servicetype;
	else if( !strcmp(datas->cureltname, "controlURL") )
		dstmember = datas->tmp.controlurl;
	else if( !strcmp(datas->cureltname, "eventSubURL") )
		dstmember = datas->tmp.eventsuburl;
	else if( !strcmp(datas->cureltname, "SCPDURL") )
		dstmember = datas->tmp.scpdurl;
	if(dstmember)
	{
		if(l>=MINIUPNPC_URL_MAXSIZE)
			l = MINIUPNPC_URL_MAXSIZE-1;
		memcpy(dstmember, data, l);
		dstmember[l] = '\0';
	}
}
