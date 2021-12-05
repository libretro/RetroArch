/* $Id: upnperrors.c,v 1.5 2011/04/10 11:19:36 nanard Exp $ */
/* Project : miniupnp
 * Author : Thomas BERNARD
 * copyright (c) 2007 Thomas Bernard
 * All Right reserved.
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * This software is subjet to the conditions detailed in the
 * provided LICENCE file. */
#include <string.h>
#include "upnperrors.h"
#include "upnpcommands.h"
#include "miniupnpc.h"

const char * strupnperror(int err)
{
	switch(err)
   {
      case UPNPCOMMAND_SUCCESS:
         return "Success";
      case UPNPCOMMAND_UNKNOWN_ERROR:
         return "Miniupnpc Unknown Error";
      case UPNPCOMMAND_INVALID_ARGS:
         return "Miniupnpc Invalid Arguments";
      case UPNPCOMMAND_INVALID_RESPONSE:
         return "Miniupnpc Invalid response";
      case UPNPDISCOVER_SOCKET_ERROR:
         return "Miniupnpc Socket error";
      case UPNPDISCOVER_MEMORY_ERROR:
         return "Miniupnpc Memory allocation error";
      case 401:
         return "Invalid Action";
      case 402:
         return "Invalid Args";
      case 501:
         return "Action Failed";
      case 606:
         return "Action not authorized";
      case 701:
         return "PinholeSpaceExhausted";
      case 702:
         return "FirewallDisabled";
      case 703:
         return "InboundPinholeNotAllowed";
      case 704:
         return "NoSuchEntry";
      case 705:
         return "ProtocolNotSupported";
      case 706:
         return "InternalPortWildcardingNotAllowed";
      case 707:
         return "ProtocolWildcardingNotAllowed";
      case 708:
         return "WildcardNotPermittedInSrcIP";
      case 709:
         return "NoPacketSent";
      case 713:
         return "SpecifiedArrayIndexInvalid";
      case 714:
         return "NoSuchEntryInArray";
      case 715:
         return "WildCardNotPermittedInSrcIP";
      case 716:
         return "WildCardNotPermittedInExtPort";
      case 718:
         return "ConflictInMappingEntry";
      case 724:
         return "SamePortValuesRequired";
      case 725:
         return "OnlyPermanentLeasesSupported";
      case 726:
         return "RemoteHostOnlySupportsWildcard";
      case 727:
         return "ExternalPortOnlySupportsWildcard";
      default:
         break;
   }
   return "UnknownError";
}
