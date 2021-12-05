/* $Id: miniupnpc.c,v 1.148 2016/01/24 17:24:36 nanard Exp $ */
/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * Project : miniupnp
 * Web : http://miniupnp.free.fr/
 * Author : Thomas BERNARD
 * copyright (c) 2005-2016 Thomas Bernard
 * This software is subjet to the conditions detailed in the
 * provided LICENSE file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
/* Win32 Specific includes and defines */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <iphlpapi.h>
#define snprintf _snprintf
#ifndef strncasecmp
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define strncasecmp _memicmp
#else /* defined(_MSC_VER) && (_MSC_VER >= 1400) */
#define strncasecmp memicmp
#endif /* defined(_MSC_VER) && (_MSC_VER >= 1400) */
#endif /* #ifndef strncasecmp */
#define MAXHOSTNAMELEN 64
#else /* #ifdef _WIN32 */
/* Standard POSIX includes */
#include <unistd.h>
#if defined(__amigaos__) && !defined(__amigaos4__)
/* Amiga OS 3 specific stuff */
#define socklen_t int
#else
#include <sys/select.h>
#endif
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#if !defined(__amigaos__) && !defined(__amigaos4__)
#include <poll.h>
#endif
#include <strings.h>
#include <errno.h>
#ifndef closesocket
#define closesocket close
#endif
#endif /* #else _WIN32 */
#ifdef __GNU__
#define MAXHOSTNAMELEN 64
#endif


#include "miniupnpc.h"
#include "minissdpc.h"
#include "miniwget.h"
#include "minisoap.h"
#include "minixml.h"
#include "upnpcommands.h"
#include "connecthostport.h"

/* compare the begining of a string with a constant string */
#define COMPARE(str, cstr) (0==memcmp(str, cstr, sizeof(cstr) - 1))

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define SOAPPREFIX "s"
#define SERVICEPREFIX "u"
#define SERVICEPREFIX2 'u'

/* root description parsing */
void parserootdesc(const char * buffer, int bufsize, struct IGDdatas * data)
{
	struct xmlparser parser;
	/* xmlparser object */
	parser.xmlstart     = buffer;
	parser.xmlsize      = bufsize;
	parser.data         = data;
	parser.starteltfunc = IGDstartelt;
	parser.endeltfunc   = IGDendelt;
	parser.datafunc     = IGDdata;
	parser.attfunc      = 0;
	parsexml(&parser);
}

/* simpleUPnPcommand2 :
 * not so simple !
 * return values :
 *   pointer - OK
 *   NULL - error */
char * simpleUPnPcommand2(int s, const char * url, const char * service,
		       const char * action, struct UPNParg * args,
		       int * bufsize, const char * httpversion)
{
	char hostname[MAXHOSTNAMELEN+1];
	unsigned short port = 0;
	char * path;
	char soapact[128];
	char soapbody[2048];
	int soapbodylen;
	char * buf;
	int n;
	int status_code;

	*bufsize = 0;
	snprintf(soapact, sizeof(soapact), "%s#%s", service, action);
	if(!args)
	{
		soapbodylen = snprintf(soapbody, sizeof(soapbody),
						  "<?xml version=\"1.0\"?>\r\n"
						  "<" SOAPPREFIX ":Envelope "
						  "xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\" "
						  SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
						  "<" SOAPPREFIX ":Body>"
						  "<" SERVICEPREFIX ":%s xmlns:" SERVICEPREFIX "=\"%s\">"
						  "</" SERVICEPREFIX ":%s>"
						  "</" SOAPPREFIX ":Body></" SOAPPREFIX ":Envelope>"
						  "\r\n", action, service, action);
		if ((unsigned int)soapbodylen >= sizeof(soapbody))
			return NULL;
	}
	else
	{
		char * p;
		const char * pe, * pv;
		const char * const pend = soapbody + sizeof(soapbody);
		soapbodylen = snprintf(soapbody, sizeof(soapbody),
						"<?xml version=\"1.0\"?>\r\n"
						"<" SOAPPREFIX ":Envelope "
						"xmlns:" SOAPPREFIX "=\"http://schemas.xmlsoap.org/soap/envelope/\" "
						SOAPPREFIX ":encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
						"<" SOAPPREFIX ":Body>"
						"<" SERVICEPREFIX ":%s xmlns:" SERVICEPREFIX "=\"%s\">",
						action, service);
		if ((unsigned int)soapbodylen >= sizeof(soapbody))
			return NULL;
		p = soapbody + soapbodylen;
		while(args->elt)
		{
			if(p >= pend) /* check for space to write next byte */
				return NULL;
			*(p++) = '<';

			pe = args->elt;
			while(p < pend && *pe)
				*(p++) = *(pe++);

			if(p >= pend) /* check for space to write next byte */
				return NULL;
			*(p++) = '>';

			if((pv = args->val))
			{
				while(p < pend && *pv)
					*(p++) = *(pv++);
			}

			if((p+2) > pend) /* check for space to write next 2 bytes */
				return NULL;
			*(p++) = '<';
			*(p++) = '/';

			pe = args->elt;
			while(p < pend && *pe)
				*(p++) = *(pe++);

			if(p >= pend) /* check for space to write next byte */
				return NULL;
			*(p++) = '>';

			args++;
		}
		if((p+4) > pend) /* check for space to write next 4 bytes */
			return NULL;
		*(p++) = '<';
		*(p++) = '/';
		*(p++) = SERVICEPREFIX2;
		*(p++) = ':';

		pe = action;
		while(p < pend && *pe)
			*(p++) = *(pe++);

		strncpy(p, "></" SOAPPREFIX ":Body></" SOAPPREFIX ":Envelope>\r\n",
		        pend - p);
		if(soapbody[sizeof(soapbody)-1]) /* strncpy pads buffer with 0s, so if it doesn't end in 0, could not fit full string */
			return NULL;
	}
	if(!parseURL(url, hostname, &port, &path, NULL)) return NULL;
	if(s < 0)
	{
		s = connecthostport(hostname, port, 0);
		if(s < 0) /* failed to connect */
			return NULL;
	}

	n = soapPostSubmit(s, path, hostname, port, soapact, soapbody, httpversion);
	if(n<=0)
   {
      closesocket(s);
      return NULL;
   }

	buf = (char*)getHTTPResponse(s, bufsize, &status_code);
	closesocket(s);
	return buf;
}

/* simpleUPnPcommand :
 * not so simple !
 * return values :
 *   pointer - OK
 *   NULL    - error */
char * simpleUPnPcommand(int s, const char * url, const char * service,
		       const char * action, struct UPNParg * args,
		       int * bufsize)
{
	return simpleUPnPcommand2(s, url, service, action, args, bufsize, "1.1");
}

/* upnpDiscoverDevices() :
 * return a chained list of all devices found or NULL if
 * no devices was found.
 * It is up to the caller to free the chained list
 * delay is in millisecond (poll).
 * UDA v1.1 says :
 *   The TTL for the IP packet SHOULD default to 2 and
 *   SHOULD be configurable. */
struct UPNPDev *
upnpDiscoverDevices(const char * const deviceTypes[],
                    int delay, const char * multicastif,
                    const char * minissdpdsock, int localport,
                    int ipv6, unsigned char ttl,
                    int * error,
                    int searchalltypes)
{
	struct UPNPDev * tmp;
	struct UPNPDev * devlist = 0;
#if !defined(_WIN32) && !defined(__amigaos__) && !defined(__amigaos4__)
	int deviceIndex;
#endif /* !defined(_WIN32) && !defined(__amigaos__) && !defined(__amigaos4__) */

	if(error)
		*error = UPNPDISCOVER_UNKNOWN_ERROR;
#if !defined(_WIN32) && !defined(__amigaos__) && !defined(__amigaos4__)
	/* first try to get infos from minissdpd ! */
	if(!minissdpdsock)
		minissdpdsock = "/var/run/minissdpd.sock";
	for(deviceIndex = 0; deviceTypes[deviceIndex]; deviceIndex++) {
		struct UPNPDev * minissdpd_devlist;
		int only_rootdevice = 1;
		minissdpd_devlist = getDevicesFromMiniSSDPD(deviceTypes[deviceIndex],
		                                            minissdpdsock, 0);
		if(minissdpd_devlist) {
			if(!strstr(minissdpd_devlist->st, "rootdevice"))
				only_rootdevice = 0;
			for(tmp = minissdpd_devlist; tmp->pNext != NULL; tmp = tmp->pNext) {
				if(!strstr(tmp->st, "rootdevice"))
					only_rootdevice = 0;
			}
			tmp->pNext = devlist;
			devlist = minissdpd_devlist;
			if(!searchalltypes && !only_rootdevice)
				break;
		}
	}
	for(tmp = devlist; tmp != NULL; tmp = tmp->pNext) {
		/* We return what we have found if it was not only a rootdevice */
		if(!strstr(tmp->st, "rootdevice")) {
			if(error)
				*error = UPNPDISCOVER_SUCCESS;
			return devlist;
		}
	}
#endif	/* !defined(_WIN32) && !defined(__amigaos__) && !defined(__amigaos4__) */

	/* direct discovery if minissdpd responses are not sufficient */
	{
		struct UPNPDev * discovered_devlist;
		discovered_devlist = ssdpDiscoverDevices(deviceTypes, delay, multicastif, localport,
		                                         ipv6, ttl, error, searchalltypes);
		if(!devlist)
			devlist = discovered_devlist;
		else
      {
         for(tmp = devlist; tmp->pNext != NULL; tmp = tmp->pNext);
         tmp->pNext = discovered_devlist;
      }
	}
	return devlist;
}

/* upnpDiscover() Discover IGD device */
struct UPNPDev *
upnpDiscover(int delay, const char * multicastif,
             const char * minissdpdsock, int localport,
             int ipv6, unsigned char ttl,
             int * error)
{
	static const char * const deviceList[] = {
		"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
		"urn:schemas-upnp-org:service:WANIPConnection:1",
		"urn:schemas-upnp-org:service:WANPPPConnection:1",
		"upnp:rootdevice",
		/*"ssdp:all",*/
		0
	};
	return upnpDiscoverDevices(deviceList,
	                           delay, multicastif, minissdpdsock, localport,
	                           ipv6, ttl, error, 0);
}

/* upnpDiscoverDevice() Discover a specific device */
struct UPNPDev *
upnpDiscoverDevice(const char * device, int delay, const char * multicastif,
                const char * minissdpdsock, int localport,
                int ipv6, unsigned char ttl,
                int * error)
{
	const char * const deviceList[] = {
		device,
		0
	};
	return upnpDiscoverDevices(deviceList,
	                           delay, multicastif, minissdpdsock, localport,
	                           ipv6, ttl, error, 0);
}

static char *
build_absolute_url(const char * baseurl, const char * descURL,
                   const char * url, unsigned int scope_id)
{
	int l, n;
	char * s;
	const char * base;
	char * p;
#if defined(IF_NAMESIZE) && !defined(_WIN32)
	char ifname[IF_NAMESIZE];
#else /* defined(IF_NAMESIZE) && !defined(_WIN32) */
	char scope_str[8];
#endif	/* defined(IF_NAMESIZE) && !defined(_WIN32) */

	if(  (url[0] == 'h')
	   &&(url[1] == 't')
	   &&(url[2] == 't')
	   &&(url[3] == 'p')
	   &&(url[4] == ':')
	   &&(url[5] == '/')
	   &&(url[6] == '/'))
		return strdup(url);
	base = (baseurl[0] == '\0') ? descURL : baseurl;
	n    = (int)strlen(base);
	if(n > 7)
   {
      p = (char*)strchr(base + 7, '/');
      if(p)
         n = (int)(p - base);
   }
	l = (int)(n + strlen(url) + 1);
	if(url[0] != '/')
		l++;
	if(scope_id != 0) {
#if defined(IF_NAMESIZE) && !defined(_WIN32)
		if(if_indextoname(scope_id, ifname))
			l += 3 + strlen(ifname);	/* 3 == strlen(%25) */
#else /* defined(IF_NAMESIZE) && !defined(_WIN32) */
		/* under windows, scope is numerical */
		l += 3 + snprintf(scope_str, sizeof(scope_str), "%u", scope_id);
#endif /* defined(IF_NAMESIZE) && !defined(_WIN32) */
	}
	s = (char*)malloc(l);
	if(s == NULL) return NULL;
	memcpy(s, base, n);
	if(scope_id != 0)
   {
      s[n] = '\0';
      if(0 == memcmp(s, "http://[fe80:", 13))
      {
         /* this is a linklocal IPv6 address */
         p = strchr(s, ']');
         if(p)
         {
            /* insert %25<scope> into URL */
#if defined(IF_NAMESIZE) && !defined(_WIN32)
            memmove(p + 3 + strlen(ifname), p, strlen(p) + 1);
            memcpy(p, "%25", 3);
            memcpy(p + 3, ifname, strlen(ifname));
            n += 3 + strlen(ifname);
#else /* defined(IF_NAMESIZE) && !defined(_WIN32) */
            memmove(p + 3 + strlen(scope_str), p, strlen(p) + 1);
            memcpy(p, "%25", 3);
            memcpy(p + 3, scope_str, strlen(scope_str));
            n += 3 + strlen(scope_str);
#endif /* defined(IF_NAMESIZE) && !defined(_WIN32) */
         }
      }
   }
	if(url[0] != '/')
		s[n++] = '/';
	memcpy(s + n, url, l - n);
	return s;
}

/* Prepare the Urls for usage...
 */
void
GetUPNPUrls(struct UPNPUrls * urls, struct IGDdatas * data,
            const char * descURL, unsigned int scope_id)
{
	/* strdup descURL */
	urls->rootdescURL    = strdup(descURL);
	/* get description of WANIPConnection */
	urls->ipcondescURL   = build_absolute_url(data->urlbase, descURL,
         data->first.scpdurl, scope_id);
	urls->controlURL     = build_absolute_url(data->urlbase, descURL,
         data->first.controlurl, scope_id);
	urls->controlURL_CIF = build_absolute_url(data->urlbase, descURL,
         data->CIF.controlurl, scope_id);
	urls->controlURL_6FC = build_absolute_url(data->urlbase, descURL,
         data->IPv6FC.controlurl, scope_id);
}

void
FreeUPNPUrls(struct UPNPUrls * urls)
{
	if(!urls)
		return;
	free(urls->controlURL);
	urls->controlURL = 0;
	free(urls->ipcondescURL);
	urls->ipcondescURL = 0;
	free(urls->controlURL_CIF);
	urls->controlURL_CIF = 0;
	free(urls->controlURL_6FC);
	urls->controlURL_6FC = 0;
	free(urls->rootdescURL);
	urls->rootdescURL = 0;
}
