/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Alfred Agrell
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <assert.h>
#include "http_intf.h"
#include "retroarch_logger.h"
#include "general.h"

int http_intf_command(unsigned mode, char *url)
{
   int ret, lg, blocksize, r;
   char typebuf[70];
   char *data=NULL, *filename = NULL, *proxy = NULL;

   if (mode == HTTP_INTF_ERROR)
      return -1;

#if 0
   if ((proxy = getenv("http_proxy")))
   {
      ret=http_parse_url(proxy,&filename);
      if (ret<0)
         return ret;
      http_proxy_server=http_server;
      http_server=NULL;
      http_proxy_port=http_port;
   }
#endif

   ret = http_parse_url(url, &filename);

   if (ret<0)
   {
      if (proxy)
         free(http_proxy_server);
      return ret;
   }

   switch (mode)
   {
      /* *** PUT *** */
      case HTTP_INTF_PUT:
         RARCH_LOG("reading stdin...\n");
         /* read stdin into memory */
         blocksize=16384;
         lg=0;
         if (!(data=(char*)malloc(blocksize)))
            return 3;
         while (1)
         {
            r=read(0, data + lg, blocksize - lg);
            if (r<=0)
               break;
            lg+=r;

            if ((3 * lg / 2) > blocksize)
            {
               blocksize *= 4;
               RARCH_LOG("read to date: %9d bytes, reallocating buffer to %9d\n",
                     lg, blocksize);
               if (!(data=(char*)realloc(data,blocksize)))
                  return 4;
            }
         }
         RARCH_LOG("read %d bytes\n", lg);
         ret=http_put(filename,data,lg,0,NULL);
         RARCH_LOG("res=%d\n",ret);
         break;
      case HTTP_INTF_GET:
         /* *** GET *** */
         ret = http_get(filename, &data, &lg, typebuf);
         RARCH_LOG("res=%d,type='%s',lg=%d\n",ret,typebuf,lg);
         fwrite(data,lg,1,stdout);
         break;
      case HTTP_INTF_HEAD:
         /* *** HEAD *** */
         ret=http_head(filename,&lg,typebuf);
         RARCH_LOG("res=%d,type='%s',lg=%d\n",ret,typebuf,lg);
         break;
      case HTTP_INTF_DELETE:
         /* *** DELETE *** */
         ret=http_delete(filename);
         RARCH_LOG("res=%d\n",ret);
         break;
         /* impossible... */
      default:
         RARCH_LOG("impossible mode value=%d\n", mode);
         return 5;
   }
   if (data)
      free(data);
   free(filename);
   free(http_server);
   if (proxy)
      free(http_proxy_server);
   return ( (ret == 201) || (ret == 200) ) ? 0 : ret;
}
