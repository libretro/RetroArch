#ifndef _HTTP_INTF_H
#define _HTTP_INTF_H

#include "netplay_compat.h"
#include "http_lib.h"

enum
{
   HTTP_INTF_ERROR = 0,
   HTTP_INTF_PUT,
   HTTP_INTF_GET,
   HTTP_INTF_DELETE,
   HTTP_INTF_HEAD
};

int http_intf_command(unsigned mode, char *url);

#endif
