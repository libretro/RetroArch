/* $Id: upnpcommands.h,v 1.30 2015/07/15 12:21:28 nanard Exp $ */
/* Miniupnp project : http://miniupnp.free.fr/
 * Author : Thomas Bernard
 * Copyright (c) 2005-2015 Thomas Bernard
 * This software is subject to the conditions detailed in the
 * LICENCE file provided within this distribution */
#ifndef UPNPCOMMANDS_H_INCLUDED
#define UPNPCOMMANDS_H_INCLUDED

#include "upnpreplyparse.h"
#include "portlistingparse.h"
#include "miniupnpctypes.h"

/* MiniUPnPc return codes : */
#define UPNPCOMMAND_SUCCESS (0)
#define UPNPCOMMAND_UNKNOWN_ERROR (-1)
#define UPNPCOMMAND_INVALID_ARGS (-2)
#define UPNPCOMMAND_HTTP_ERROR (-3)
#define UPNPCOMMAND_INVALID_RESPONSE (-4)
#define UPNPCOMMAND_MEM_ALLOC_ERROR (-5)

#ifdef __cplusplus
extern "C" {
#endif

/* UPNP_GetExternalIPAddress() call the corresponding UPNP method.
 * if the third arg is not null the value is copied to it.
 * at least 16 bytes must be available
 *
 * Return values :
 * 0 : SUCCESS
 * NON ZERO : ERROR Either an UPnP error code or an unknown error.
 *
 * possible UPnP Errors :
 * 402 Invalid Args - See UPnP Device Architecture section on Control.
 * 501 Action Failed - See UPnP Device Architecture section on Control. */
 int
UPNP_GetExternalIPAddress(const char * controlURL,
                          const char * servicetype,
                          char * extIpAdd);

/* UPNP_AddPortMapping()
 * if desc is NULL, it will be defaulted to "libminiupnpc"
 * remoteHost is usually NULL because IGD don't support it.
 *
 * Return values :
 * 0 : SUCCESS
 * NON ZERO : ERROR. Either an UPnP error code or an unknown error.
 *
 * List of possible UPnP errors for AddPortMapping :
 * errorCode errorDescription (short) - Description (long)
 * 402 Invalid Args - See UPnP Device Architecture section on Control.
 * 501 Action Failed - See UPnP Device Architecture section on Control.
 * 606 Action not authorized - The action requested REQUIRES authorization and
 *                             the sender was not authorized.
 * 715 WildCardNotPermittedInSrcIP - The source IP address cannot be
 *                                   wild-carded
 * 716 WildCardNotPermittedInExtPort - The external port cannot be wild-carded
 * 718 ConflictInMappingEntry - The port mapping entry specified conflicts
 *                     with a mapping assigned previously to another client
 * 724 SamePortValuesRequired - Internal and External port values
 *                              must be the same
 * 725 OnlyPermanentLeasesSupported - The NAT implementation only supports
 *                  permanent lease times on port mappings
 * 726 RemoteHostOnlySupportsWildcard - RemoteHost must be a wildcard
 *                             and cannot be a specific IP address or DNS name
 * 727 ExternalPortOnlySupportsWildcard - ExternalPort must be a wildcard and
 *                                        cannot be a specific port value
 * 728 NoPortMapsAvailable - There are not enough free ports available to
 *                           complete port mapping.
 * 729 ConflictWithOtherMechanisms - Attempted port mapping is not allowed
 *                                   due to conflict with other mechanisms.
 * 732 WildCardNotPermittedInIntPort - The internal port cannot be wild-carded
 */
 int
UPNP_AddPortMapping(const char * controlURL, const char * servicetype,
		    const char * extPort,
		    const char * inPort,
		    const char * inClient,
		    const char * desc,
		    const char * proto,
		    const char * remoteHost,
		    const char * leaseDuration);

/* UPNP_AddAnyPortMapping()
 * if desc is NULL, it will be defaulted to "libminiupnpc"
 * remoteHost is usually NULL because IGD don't support it.
 *
 * Return values :
 * 0 : SUCCESS
 * NON ZERO : ERROR. Either an UPnP error code or an unknown error.
 *
 * List of possible UPnP errors for AddPortMapping :
 * errorCode errorDescription (short) - Description (long)
 * 402 Invalid Args - See UPnP Device Architecture section on Control.
 * 501 Action Failed - See UPnP Device Architecture section on Control.
 * 606 Action not authorized - The action requested REQUIRES authorization and
 *                             the sender was not authorized.
 * 715 WildCardNotPermittedInSrcIP - The source IP address cannot be
 *                                   wild-carded
 * 716 WildCardNotPermittedInExtPort - The external port cannot be wild-carded
 * 728 NoPortMapsAvailable - There are not enough free ports available to
 *                           complete port mapping.
 * 729 ConflictWithOtherMechanisms - Attempted port mapping is not allowed
 *                                   due to conflict with other mechanisms.
 * 732 WildCardNotPermittedInIntPort - The internal port cannot be wild-carded
 */
 int
UPNP_AddAnyPortMapping(const char * controlURL, const char * servicetype,
		       const char * extPort,
		       const char * inPort,
		       const char * inClient,
		       const char * desc,
		       const char * proto,
		       const char * remoteHost,
		       const char * leaseDuration,
		       char * reservedPort);

/* UPNP_DeletePortMapping()
 * Use same argument values as what was used for AddPortMapping().
 * remoteHost is usually NULL because IGD don't support it.
 * Return Values :
 * 0 : SUCCESS
 * NON ZERO : error. Either an UPnP error code or an undefined error.
 *
 * List of possible UPnP errors for DeletePortMapping :
 * 402 Invalid Args - See UPnP Device Architecture section on Control.
 * 606 Action not authorized - The action requested REQUIRES authorization
 *                             and the sender was not authorized.
 * 714 NoSuchEntryInArray - The specified value does not exist in the array */
 int
UPNP_DeletePortMapping(const char * controlURL, const char * servicetype,
		       const char * extPort, const char * proto,
		       const char * remoteHost);

/* UPNP_DeletePortRangeMapping()
 * Use same argument values as what was used for AddPortMapping().
 * remoteHost is usually NULL because IGD don't support it.
 * Return Values :
 * 0 : SUCCESS
 * NON ZERO : error. Either an UPnP error code or an undefined error.
 *
 * List of possible UPnP errors for DeletePortMapping :
 * 606 Action not authorized - The action requested REQUIRES authorization
 *                             and the sender was not authorized.
 * 730 PortMappingNotFound - This error message is returned if no port
 *			     mapping is found in the specified range.
 * 733 InconsistentParameters - NewStartPort and NewEndPort values are not consistent. */
 int
UPNP_DeletePortMappingRange(const char * controlURL, const char * servicetype,
        		    const char * extPortStart, const char * extPortEnd,
        		    const char * proto,
        		    const char * manage);

/* UPNP_GetPortMappingNumberOfEntries()
 * not supported by all routers */
 int
UPNP_GetPortMappingNumberOfEntries(const char* controlURL,
                                   const char* servicetype,
                                   unsigned int * num);

/* UPNP_GetListOfPortMappings()      Available in IGD v2
 *
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
                           struct PortMappingParserData * data);

 int
UPNP_GetOutboundPinholeTimeout(const char * controlURL, const char * servicetype,
                    const char * remoteHost,
                    const char * remotePort,
                    const char * intClient,
                    const char * intPort,
                    const char * proto,
                    int * opTimeout);

#ifdef __cplusplus
}
#endif

#endif

