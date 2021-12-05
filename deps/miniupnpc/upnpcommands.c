/* $Id: upnpcommands.c,v 1.46 2015/07/15 12:19:00 nanard Exp $ */
/* vim: tabstop=4 shiftwidth=4 noexpandtab
 * Project : miniupnp
 * Author : Thomas Bernard
 * Copyright (c) 2005-2017 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided in this distribution.
 * */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "upnpcommands.h"
#include "miniupnpc.h"
#include "portlistingparse.h"

#if (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L)
#define STRTOUI	strtoull
#else
#define STRTOUI	strtoul
#endif

/* UPNP_GetExternalIPAddress() call the corresponding UPNP method.
 * if the third arg is not null the value is copied to it.
 * at least 16 bytes must be available
 *
 * Return values :
 * 0 : SUCCESS
 * NON ZERO : ERROR Either an UPnP error code or an unknown error.
 *
 * 402 Invalid Args - See UPnP Device Architecture section on Control.
 * 501 Action Failed - See UPnP Device Architecture section on Control.
 */
int
UPNP_GetExternalIPAddress(const char * controlURL,
      const char * servicetype,
      char * extIpAdd)
{
	struct NameValueParserData pdata;
	char * buffer;
	int bufsize;
	char * p;
	int ret = UPNPCOMMAND_UNKNOWN_ERROR;

	if(!extIpAdd || !controlURL || !servicetype)
		return UPNPCOMMAND_INVALID_ARGS;

	if(!(buffer = simpleUPnPcommand(-1, controlURL, servicetype,
               "GetExternalIPAddress", 0, &bufsize)))
		return UPNPCOMMAND_HTTP_ERROR;
	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer); buffer = NULL;
	p = GetValueFromNameValueList(&pdata, "NewExternalIPAddress");
	if(p) {
		strncpy(extIpAdd, p, 16 );
		extIpAdd[15] = '\0';
		ret = UPNPCOMMAND_SUCCESS;
	} else
		extIpAdd[0] = '\0';

	p = GetValueFromNameValueList(&pdata, "errorCode");
	if(p) {
		ret = UPNPCOMMAND_UNKNOWN_ERROR;
		sscanf(p, "%d", &ret);
	}

	ClearNameValueList(&pdata);
	return ret;
}

int
UPNP_AddPortMapping(const char * controlURL, const char * servicetype,
		    const char * extPort,
		    const char * inPort,
		    const char * inClient,
		    const char * desc,
		    const char * proto,
		    const char * remoteHost,
		    const char * leaseDuration)
{
	struct UPNParg * AddPortMappingArgs;
	char * buffer;
	int bufsize;
	struct NameValueParserData pdata;
	const char * resVal;
	int ret;

	if(!inPort || !inClient || !proto || !extPort)
		return UPNPCOMMAND_INVALID_ARGS;

	AddPortMappingArgs = (struct UPNParg*)calloc(9, sizeof(struct UPNParg));
	if(AddPortMappingArgs == NULL)
		return UPNPCOMMAND_MEM_ALLOC_ERROR;
	AddPortMappingArgs[0].elt = "NewRemoteHost";
	AddPortMappingArgs[0].val = remoteHost;
	AddPortMappingArgs[1].elt = "NewExternalPort";
	AddPortMappingArgs[1].val = extPort;
	AddPortMappingArgs[2].elt = "NewProtocol";
	AddPortMappingArgs[2].val = proto;
	AddPortMappingArgs[3].elt = "NewInternalPort";
	AddPortMappingArgs[3].val = inPort;
	AddPortMappingArgs[4].elt = "NewInternalClient";
	AddPortMappingArgs[4].val = inClient;
	AddPortMappingArgs[5].elt = "NewEnabled";
	AddPortMappingArgs[5].val = "1";
	AddPortMappingArgs[6].elt = "NewPortMappingDescription";
	AddPortMappingArgs[6].val = desc?desc:"libminiupnpc";
	AddPortMappingArgs[7].elt = "NewLeaseDuration";
	AddPortMappingArgs[7].val = leaseDuration?leaseDuration:"0";
	buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                           "AddPortMapping", AddPortMappingArgs,
	                           &bufsize);
	free(AddPortMappingArgs);
	if(!buffer)
		return UPNPCOMMAND_HTTP_ERROR;
	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer); buffer = NULL;
	resVal = GetValueFromNameValueList(&pdata, "errorCode");
	if(resVal)
   {
      ret = UPNPCOMMAND_UNKNOWN_ERROR;
      sscanf(resVal, "%d", &ret);
   }
   else
		ret = UPNPCOMMAND_SUCCESS;
	ClearNameValueList(&pdata);
	return ret;
}

int
UPNP_AddAnyPortMapping(const char * controlURL, const char * servicetype,
		       const char * extPort,
		       const char * inPort,
		       const char * inClient,
		       const char * desc,
		       const char * proto,
		       const char * remoteHost,
		       const char * leaseDuration,
		       char * reservedPort)
{
	struct UPNParg * AddPortMappingArgs;
	char * buffer;
	int bufsize;
	struct NameValueParserData pdata;
	const char * resVal;
	int ret;

	if(!inPort || !inClient || !proto || !extPort)
		return UPNPCOMMAND_INVALID_ARGS;

	AddPortMappingArgs = (struct UPNParg*)calloc(9, sizeof(struct UPNParg));
	if(AddPortMappingArgs == NULL)
		return UPNPCOMMAND_MEM_ALLOC_ERROR;
	AddPortMappingArgs[0].elt = "NewRemoteHost";
	AddPortMappingArgs[0].val = remoteHost;
	AddPortMappingArgs[1].elt = "NewExternalPort";
	AddPortMappingArgs[1].val = extPort;
	AddPortMappingArgs[2].elt = "NewProtocol";
	AddPortMappingArgs[2].val = proto;
	AddPortMappingArgs[3].elt = "NewInternalPort";
	AddPortMappingArgs[3].val = inPort;
	AddPortMappingArgs[4].elt = "NewInternalClient";
	AddPortMappingArgs[4].val = inClient;
	AddPortMappingArgs[5].elt = "NewEnabled";
	AddPortMappingArgs[5].val = "1";
	AddPortMappingArgs[6].elt = "NewPortMappingDescription";
	AddPortMappingArgs[6].val = desc?desc:"libminiupnpc";
	AddPortMappingArgs[7].elt = "NewLeaseDuration";
	AddPortMappingArgs[7].val = leaseDuration?leaseDuration:"0";
	buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                           "AddAnyPortMapping", AddPortMappingArgs,
	                           &bufsize);
	free(AddPortMappingArgs);
	if(!buffer) {
		return UPNPCOMMAND_HTTP_ERROR;
	}
	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer); buffer = NULL;
	resVal = GetValueFromNameValueList(&pdata, "errorCode");
	if(resVal) {
		ret = UPNPCOMMAND_UNKNOWN_ERROR;
		sscanf(resVal, "%d", &ret);
	} else {
		char *p;

		p = GetValueFromNameValueList(&pdata, "NewReservedPort");
		if(p)
      {
         strncpy(reservedPort, p, 6);
         reservedPort[5] = '\0';
         ret = UPNPCOMMAND_SUCCESS;
      }
      else
			ret = UPNPCOMMAND_INVALID_RESPONSE;
	}
	ClearNameValueList(&pdata);
	return ret;
}

int
UPNP_DeletePortMapping(const char * controlURL, const char * servicetype,
                       const char * extPort, const char * proto,
                       const char * remoteHost)
{
	/*struct NameValueParserData pdata;*/
	struct UPNParg * DeletePortMappingArgs;
	char * buffer;
	int bufsize;
	struct NameValueParserData pdata;
	const char * resVal;
	int ret;

	if(!extPort || !proto)
		return UPNPCOMMAND_INVALID_ARGS;

	DeletePortMappingArgs = (struct UPNParg*)calloc(4, sizeof(struct UPNParg));
	if(DeletePortMappingArgs == NULL)
		return UPNPCOMMAND_MEM_ALLOC_ERROR;
	DeletePortMappingArgs[0].elt = "NewRemoteHost";
	DeletePortMappingArgs[0].val = remoteHost;
	DeletePortMappingArgs[1].elt = "NewExternalPort";
	DeletePortMappingArgs[1].val = extPort;
	DeletePortMappingArgs[2].elt = "NewProtocol";
	DeletePortMappingArgs[2].val = proto;
	buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                          "DeletePortMapping",
	                          DeletePortMappingArgs, &bufsize);
	free(DeletePortMappingArgs);
	if(!buffer)
		return UPNPCOMMAND_HTTP_ERROR;
	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer);
   buffer = NULL;
	resVal = GetValueFromNameValueList(&pdata, "errorCode");
	if(resVal)
   {
      ret = UPNPCOMMAND_UNKNOWN_ERROR;
      sscanf(resVal, "%d", &ret);
   }
   else
		ret = UPNPCOMMAND_SUCCESS;
	ClearNameValueList(&pdata);
	return ret;
}

 int
UPNP_DeletePortMappingRange(const char * controlURL, const char * servicetype,
        		    const char * extPortStart, const char * extPortEnd,
        		    const char * proto,
			    const char * manage)
{
	struct UPNParg * DeletePortMappingArgs;
	char * buffer;
	int bufsize;
	struct NameValueParserData pdata;
	const char * resVal;
	int ret;

	if(!extPortStart || !extPortEnd || !proto || !manage)
		return UPNPCOMMAND_INVALID_ARGS;

	DeletePortMappingArgs = (struct UPNParg*)calloc(5, sizeof(struct UPNParg));
	if(DeletePortMappingArgs == NULL)
		return UPNPCOMMAND_MEM_ALLOC_ERROR;
	DeletePortMappingArgs[0].elt = "NewStartPort";
	DeletePortMappingArgs[0].val = extPortStart;
	DeletePortMappingArgs[1].elt = "NewEndPort";
	DeletePortMappingArgs[1].val = extPortEnd;
	DeletePortMappingArgs[2].elt = "NewProtocol";
	DeletePortMappingArgs[2].val = proto;
	DeletePortMappingArgs[3].elt = "NewManage";
	DeletePortMappingArgs[3].val = manage;

	buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                           "DeletePortMappingRange",
	                           DeletePortMappingArgs, &bufsize);
	free(DeletePortMappingArgs);
	if(!buffer)
		return UPNPCOMMAND_HTTP_ERROR;
	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer); buffer = NULL;
	resVal = GetValueFromNameValueList(&pdata, "errorCode");
	if(resVal)
   {
      ret = UPNPCOMMAND_UNKNOWN_ERROR;
      sscanf(resVal, "%d", &ret);
   }
   else
		ret = UPNPCOMMAND_SUCCESS;
	ClearNameValueList(&pdata);
	return ret;
}

 int
UPNP_GetPortMappingNumberOfEntries(const char * controlURL,
                                   const char * servicetype,
                                   unsigned int * numEntries)
{
 	struct NameValueParserData pdata;
 	char * buffer;
 	int bufsize;
 	char* p;
	int ret = UPNPCOMMAND_UNKNOWN_ERROR;
 	if(!(buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                                "GetPortMappingNumberOfEntries", 0,
	                                &bufsize))) {
		return UPNPCOMMAND_HTTP_ERROR;
	}
 	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer); buffer = NULL;

 	p = GetValueFromNameValueList(&pdata, "NewPortMappingNumberOfEntries");
 	if(numEntries && p) {
		*numEntries = 0;
 		sscanf(p, "%u", numEntries);
		ret = UPNPCOMMAND_SUCCESS;
 	}

	p = GetValueFromNameValueList(&pdata, "errorCode");
	if(p) {
		ret = UPNPCOMMAND_UNKNOWN_ERROR;
		sscanf(p, "%d", &ret);
	}

 	ClearNameValueList(&pdata);
	return ret;
}

/* UPNP_GetListOfPortMappings()
 *
 * Possible UPNP Error codes :
 * 606 Action not Authorized
 * 730 PortMappingNotFound - no port mapping is found in the specified range.
 * 733 InconsistantParameters - NewStartPort and NewEndPort values are not
 *                              consistent.
 */
 int
UPNP_GetListOfPortMappings(const char * controlURL,
                           const char * servicetype,
                           const char * startPort,
                           const char * endPort,
                           const char * protocol,
                           const char * numberOfPorts,
                           struct PortMappingParserData * data)
{
	struct NameValueParserData pdata;
	struct UPNParg * GetListOfPortMappingsArgs;
	const char * p;
	char * buffer;
	int bufsize;
	int ret = UPNPCOMMAND_UNKNOWN_ERROR;

	if(!startPort || !endPort || !protocol)
		return UPNPCOMMAND_INVALID_ARGS;

	GetListOfPortMappingsArgs = (struct UPNParg*)calloc(6, sizeof(struct UPNParg));
	if(GetListOfPortMappingsArgs == NULL)
		return UPNPCOMMAND_MEM_ALLOC_ERROR;
	GetListOfPortMappingsArgs[0].elt = "NewStartPort";
	GetListOfPortMappingsArgs[0].val = startPort;
	GetListOfPortMappingsArgs[1].elt = "NewEndPort";
	GetListOfPortMappingsArgs[1].val = endPort;
	GetListOfPortMappingsArgs[2].elt = "NewProtocol";
	GetListOfPortMappingsArgs[2].val = protocol;
	GetListOfPortMappingsArgs[3].elt = "NewManage";
	GetListOfPortMappingsArgs[3].val = "1";
	GetListOfPortMappingsArgs[4].elt = "NewNumberOfPorts";
	GetListOfPortMappingsArgs[4].val = numberOfPorts?numberOfPorts:"1000";

	buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                           "GetListOfPortMappings",
	                           GetListOfPortMappingsArgs, &bufsize);
	free(GetListOfPortMappingsArgs);
	if(!buffer)
		return UPNPCOMMAND_HTTP_ERROR;

	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer);
   buffer = NULL;

	if(pdata.portListing)
	{
		ParsePortListing(pdata.portListing, pdata.portListingLength,
		                 data);
		ret = UPNPCOMMAND_SUCCESS;
	}

	p = GetValueFromNameValueList(&pdata, "errorCode");
	if(p) {
		ret = UPNPCOMMAND_UNKNOWN_ERROR;
		sscanf(p, "%d", &ret);
	}
	ClearNameValueList(&pdata);

	return ret;
}

 int
UPNP_GetOutboundPinholeTimeout(
      const char * controlURL,
      const char * servicetype,
      const char * remoteHost,
      const char * remotePort,
      const char * intClient,
      const char * intPort,
      const char * proto,
      int * opTimeout)
{
	struct UPNParg * GetOutboundPinholeTimeoutArgs;
	char * buffer;
	int bufsize;
	struct NameValueParserData pdata;
	const char * resVal;
	char * p;
	int ret;

	if(!intPort || !intClient || !proto || !remotePort || !remoteHost)
		return UPNPCOMMAND_INVALID_ARGS;

	GetOutboundPinholeTimeoutArgs = (struct UPNParg*)
      calloc(6, sizeof(struct UPNParg));
	if(!GetOutboundPinholeTimeoutArgs)
		return UPNPCOMMAND_MEM_ALLOC_ERROR;
	GetOutboundPinholeTimeoutArgs[0].elt = "RemoteHost";
	GetOutboundPinholeTimeoutArgs[0].val = remoteHost;
	GetOutboundPinholeTimeoutArgs[1].elt = "RemotePort";
	GetOutboundPinholeTimeoutArgs[1].val = remotePort;
	GetOutboundPinholeTimeoutArgs[2].elt = "Protocol";
	GetOutboundPinholeTimeoutArgs[2].val = proto;
	GetOutboundPinholeTimeoutArgs[3].elt = "InternalPort";
	GetOutboundPinholeTimeoutArgs[3].val = intPort;
	GetOutboundPinholeTimeoutArgs[4].elt = "InternalClient";
	GetOutboundPinholeTimeoutArgs[4].val = intClient;
	buffer = simpleUPnPcommand(-1, controlURL, servicetype,
	                           "GetOutboundPinholeTimeout", GetOutboundPinholeTimeoutArgs, &bufsize);
	free(GetOutboundPinholeTimeoutArgs);
	if(!buffer)
		return UPNPCOMMAND_HTTP_ERROR;
	ParseNameValue(buffer, bufsize, &pdata);
	free(buffer);
   buffer = NULL;
	resVal = GetValueFromNameValueList(&pdata, "errorCode");
	if(resVal)
	{
		ret = UPNPCOMMAND_UNKNOWN_ERROR;
		sscanf(resVal, "%d", &ret);
	}
	else
	{
		ret = UPNPCOMMAND_SUCCESS;
		p = GetValueFromNameValueList(&pdata, "OutboundPinholeTimeout");
		if(p)
			*opTimeout = (int)((UNSIGNED_INTEGER)STRTOUI(p, NULL, 0));
	}
	ClearNameValueList(&pdata);
	return ret;
}
