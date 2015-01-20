/*
 *  Http put/get mini lib
 *  written by L. Demailly
 *  (c) 1998 Laurent Demailly - http://www.demailly.com/~dl/
 *  (c) 1996 Observatoire de Paris - Meudon - France
 *  see LICENSE for terms, conditions and DISCLAIMER OF ALL WARRANTIES
 *
 * $Id: http_lib.c,v 3.5 1998/09/23 06:19:15 dl Exp $ 
 *
 * Description : Use http protocol, connects to server to echange data
 *
 * $Log: http_lib.c,v $
 * Revision 3.5  1998/09/23 06:19:15  dl
 * portability and http 1.x (1.1 and later) compatibility
 *
 * Revision 3.4  1998/09/23 05:44:27  dl
 * added support for HTTP/1.x answers
 *
 * Revision 3.3  1996/04/25 19:07:22  dl
 * using intermediate variable for htons (port) so it does not yell
 * on freebsd  (thx pp for report)
 *
 * Revision 3.2  1996/04/24  13:56:08  dl
 * added proxy support through http_proxy_server & http_proxy_port
 * some httpd *needs* cr+lf so provide them
 * simplification + cleanup
 *
 * Revision 3.1  1996/04/18  13:53:13  dl
 * http-tiny release 1.0
 *
 *
 */

#define VERBOSE

/* http_lib - Http data exchanges mini library.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include "netplay_compat.h"
#include "http_lib.h"

#define SERVER_DEFAULT "adonis"

/* pointer to a mallocated string containing server name or NULL */
char *http_server=NULL ;
/* server port number */
int  http_port=5757;
/* pointer to proxy server name or NULL */
char *http_proxy_server=NULL;
/* proxy server port number or 0 */
int http_proxy_port=0;
/* user agent id string */
static const char *http_user_agent="adlib/3 ($Date: 1998/09/23 06:19:15 $)";

/**
 * http_read_line:
 * @fd                : File descriptor to read from.
 * @buffer            : Placeholder for data.
 * @max               : Maximum number of bytes to read.
 *
 * Reads a line from file descriptor
 * returns the number of bytes read. negative if a read error occured
 * before the end of line or the max.
 * cariage returns (CR) are ignored.
 *
 * Returns: number of bytes read, negative if error occurred.
 **/
static int http_read_line (int fd, char *buffer, int max) 
{
   /* not efficient on long lines (multiple unbuffered 1 char reads) */
   int n = 0;

   while (n<max)
   {
      if (read(fd, buffer, 1) != 1)
      {
         n= -n;
         break;
      }
      n++;
      if (*buffer == '\015')
         continue; /* ignore CR */
      if (*buffer == '\012')
         break;    /* LF is the separator */
      buffer++;
   }
   *buffer=0;
   return n;
}


/**
 * http_read_buffer:
 * @fd                : File descriptor to read from.
 * @buffer            : Placeholder for data.
 * @max               : Maximum number of bytes to read.
 *
 * Reads data from file descriptor.
 * Retries reading until the number of bytes requested is read.
 * Returns the number of bytes read. negative if a read error (EOF) occured
 * before the requested length.
 *
 * Returns: number of bytes read, negative if error occurred.
 **/
static int http_read_buffer (int fd, char *buffer, int length) 
{
   int n,r;

   for (n = 0; n < length; n += r)
   {
      r = read(fd, buffer, length-n);
      if (r <= 0)
         return -n;
      buffer += r;
   }

   return n;
}


typedef enum 
{
   /* Close the socket after the query (for put) */
   CLOSE,
   /* Keep it open */
   KEEP_OPEN
} querymode;

#ifndef OSK

static http_retcode http_query(const char *command, const char *url,
      const char *additional_header, querymode mode, 
      const char* data, int length, int *pfd);
#endif

/* beware that filename+type+rest of header must not exceed MAXBUF */
/* so we limit filename to 256 and type to 64 chars in put & get */
#define MAXBUF 512

/*
 * http_query:
 * @command           : Command to send.
 * @url               : URL / filename queried.
 * @additional_header : Additional header.
 * @mode              : Type of query.
 * @data              : Data to send after header. If NULL,
 *                      no data is sent.
 * @length            : Size of data.
 * @pfd               : Pointer to variable where to set file
 *                      descriptor value.
 *
 * Pseudo general HTTP query.
 *
 * Sends a command and additional headers to the HTTP server.
 * optionally through the proxy (if http_proxy_server and http_proxy_port are
 * set).
 *
 * Limitations: the url is truncated to first 256 chars and
 * the server name to 128 in case of proxy request.
 *
 * Returns: HTTP return code.
 **/
static http_retcode http_query(const char *command, const char *url,
      const char *additional_header, querymode mode,
      const char *data, int length, int *pfd) 
{
   int     s;
   struct  hostent *hp;
   struct  sockaddr_in     server;
   char header[MAXBUF];
   int  hlg;
   http_retcode ret;
   int  proxy = (http_proxy_server != NULL && http_proxy_port != 0);
   int  port  = proxy ? http_proxy_port : http_port ;

   if (pfd)
      *pfd=-1;

   /* get host info by name :*/
   if ((hp = gethostbyname( proxy ? http_proxy_server 
               : ( http_server ? http_server 
                  : SERVER_DEFAULT )
               )))
   {
      memset((char *) &server,0, sizeof(server));
      memmove((char *) &server.sin_addr, hp->h_addr, hp->h_length);

      server.sin_family = hp->h_addrtype;
      server.sin_port   = (unsigned short) htons( port );
   }
   else
      return ERRHOST;

   /* create socket */
   if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return ERRSOCK;
   setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, 0, 0);

   /* connect to server */
   if (connect(s, (struct sockaddr*)&server, sizeof(server)) < 0) 
      ret=ERRCONN;
   else
   {
      if (pfd) *pfd=s;

      /* create header */
      if (proxy)
      {
         sprintf(header,
               "%s http://%.128s:%d/%.256s HTTP/1.0\015\012User-Agent: %s\015\012Host: %s\015\012%s\015\012",
               command,
               http_server,
               http_port,
               url,
               http_user_agent,
               http_server,
               additional_header
               );
      }
      else
      {
         sprintf(header,
               "%s /%.256s HTTP/1.0\015\012User-Agent: %s\015\012Host: %s\015\012%s\015\012",
               command,
               url,
               http_user_agent,
               http_server,
               additional_header
               );
      }

      hlg=strlen(header);

      /* send header */
      if (write(s, header, hlg) != hlg)
         ret= ERRWRHD;

      /* send data */
      else if (length && data && (write(s, data, length) != length) ) 
         ret= ERRWRDT;
      else
      {
         /* read result & check */
         int linelen=http_read_line(s,header,MAXBUF-1);
#ifdef VERBOSE
         fputs(header,stderr);
         putc('\n',stderr);
#endif	
         if (linelen<=0) 
            ret=ERRRDHD;
         else if (sscanf(header, "HTTP/1.%*d %03d", (int*)&ret) != 1) 
            ret=ERRPAHD;
         else if (mode == KEEP_OPEN)
            return ret;
      }
   }
   /* close socket */
   close(s);
   return ret;
}


/**
 * http_put:  
 * @filename          : Name of the resource to create.
 * @data              : Pointer to the data to send.
 * @length            : Length of the data to send.
 * @overwrite         : Flag to request to overwrite the
 *                      resource if it already exists.
 * @type              : Type of data.
 *
 * Put data on the server
 *
 * This function sends data to the http data server.
 * The data will be stored under the ressource name filename.
 * returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters 
 *              and type to 64.
 *
 * Returns: HTTP return code.
 **/
http_retcode http_put(const char *filename, const char *data,
      int length, int overwrite, const char *type) 
{
   char header[MAXBUF];
   if (type) 
      sprintf(header, "Content-length: %d\015\012Content-type: %.64s\015\012%s",
            length, type, overwrite ? "Control: overwrite=1\015\012" : "");
   else
      sprintf(header, "Content-length: %d\015\012%s",length,
            overwrite ? "Control: overwrite=1\015\012" : "");
   return http_query("PUT", filename, header, CLOSE, data, length, NULL);
}


/**
 * http_get:
 * @filename          : Name of the resource to create.
 * @pdata             : Address of pointer which will be set
 *                      to point toward allocated memory containing
 *                      read data.
 * @typebuf           : Allocated buffer where the read data
 *                      type is returned. If NULL, the type is
 *                      not returned.
 *
 * Gets data from the server
 *
 * This function gets data from the http data server.
 * The data is read from the ressource named filename.
 * Address of new new allocated memory block is filled in pdata
 * whose length is returned via plength.
 * 
 * Returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters
 *
 * Returns: HTTP error code.
 **/
http_retcode http_get(const char *filename,
      char **pdata, int *plength, char *typebuf) 
{
   http_retcode ret;

   char header[MAXBUF];
   char *pc;
   int  fd;
   int  n,length=-1;

   if (!pdata)
      return ERRNULL;

   *pdata = NULL;

   if (plength)
      *plength = 0;
   if (typebuf)
      *typebuf = '\0';

   ret = http_query("GET", filename, "", KEEP_OPEN, NULL, 0, &fd);

   if (ret == 200)
   {
      while (1)
      {
         n=http_read_line(fd,header,MAXBUF-1);
#ifdef VERBOSE
         fputs(header,stderr);
         putc('\n',stderr);
#endif	
         if (n <= 0)
         {
            close(fd);
            return ERRRDHD;
         }
         /* empty line ? (=> end of header) */
         if ( n>0 && (*header) == '\0')
            break;
         /* try to parse some keywords : */
         /* convert to lower case 'till a : is found or end of string */
         for (pc=header; (*pc != ':' && *pc) ; pc++)
            *pc=tolower(*pc);
         sscanf(header,"content-length: %d",&length);
         if (typebuf)
            sscanf(header,"content-type: %s",typebuf);
      }

      if (length<=0)
      {
         close(fd);
         return ERRNOLG;
      }
      if (plength)
         *plength=length;
      if (!(*pdata = (char*)malloc(length)))
      {
         close(fd);
         return ERRMEM;
      }
      n=http_read_buffer(fd, *pdata, length);
      close(fd);
      if (n != length)
         ret = ERRRDDT;
   }
   else if (ret >= 0)
      close(fd);

   return ret;
}


/**
 * http_head:
 * @filename          : Name of the resource to create.
 * @plength           : Address of integer variable which will be set
 *                      to length of the data.
 * @typebuf           : Allocated buffer where the read data
 *                      type is returned. If NULL, the type is
 *                      not returned.
 *
 * Requests the header.
 *
 * This function outputs the header of thehttp data server.
 * The header is from the ressource named filename.
 * The length and type of data is eventually returned (like for http_get(3))
 *
 * Returns a negative error code or a positive code from the server
 * 
 * limitations: filename is truncated to first 256 characters
 *
 * Returns: HTTP return code.
 **/
http_retcode http_head(const char *filename, int *plength, char *typebuf) 
{
   /* mostly copied from http_get : */
   http_retcode ret;

   char header[MAXBUF];
   char *pc;
   int  fd;
   int  n,length=-1;

   if (plength)
      *plength=0;
   if (typebuf)
      *typebuf='\0';

   ret=http_query("HEAD", filename, "", KEEP_OPEN, NULL, 0, &fd);
   if (ret == 200)
   {
      while (1)
      {
         n = http_read_line(fd, header, MAXBUF-1);
#ifdef VERBOSE
         fputs(header, stderr);
         putc('\n', stderr);
#endif	
         if (n<=0)
         {
            close(fd);
            return ERRRDHD;
         }

         /* Empty line ? (=> end of header) */
         if (n > 0 && (*header) == '\0')
            break;

         /* Try to parse some keywords : */
         /* convert to lower case 'till a : is found or end of string */
         for (pc=header; (*pc != ':' && *pc) ; pc++)
            *pc=tolower(*pc);
         sscanf(header, "content-length: %d", &length);

         if (typebuf)
            sscanf(header, "content-type: %s", typebuf);
      }
      if (plength)
         *plength=length;
      close(fd);
   }
   else if (ret>=0)
      close(fd);
   return ret;
}



/**
 * http_delete:
 * @filename          : Name of the resource to create.
 *
 * Deletes data on the server
 *
 * Request a DELETE on the HTTP data server.
 *
 * Returns a negative error code or a positive code from the server
 *
 * limitations: filename is truncated to first 256 characters 
 *
 * Returns: HTTP return code.
 **/
http_retcode http_delete(const char *filename) 
{
   return http_query("DELETE", filename, "", CLOSE, NULL, 0, NULL);
}

/**
 * http_parse_url:
 * @url               : Writable copy of an URL.
 * @pfilename         : Address of a pointer that will be filled with
 *                      allocated filename. The pointer must be equal
 *                      to NULL before calling it or it will be automatically
 *                      freed (free(3)).
 * Parses an url : setting the http_server and http_port global variables
 * and returning the filename to pass to http_get/put/...
 * returns a negative error code or 0 if sucessfully parsed.
 *
 * Returns: 0 if successfully parsed, negative error code if not.
 **/
http_retcode http_parse_url(char *url, char **pfilename)
{
   char *pc, c;

   http_port = 80;

   if (http_server)
   {
      free(http_server);
      http_server=NULL;
   }
   if (*pfilename)
   {
      free(*pfilename);
      *pfilename=NULL;
   }

   if (strncasecmp("http://", url, 7))
   {
#ifdef VERBOSE
      fprintf(stderr,"invalid url (must start with 'http://')\n");
#endif
      return ERRURLH;
   }
   url+=7;

   for (pc = url, c = *pc; (c && c != ':' && c != '/');)
      c=*pc++;

   *(pc-1) = 0;

   if (c == ':')
   {
      if (sscanf(pc, "%d", &http_port) != 1)
      {
#ifdef VERBOSE
         fprintf(stderr,"invalid port in url\n");
#endif
         return ERRURLP;
      }
      for (pc++; (*pc && *pc != '/') ; pc++) ;
      if (*pc)
         pc++;
   }

   http_server = strdup(url);
   *pfilename  = strdup ( c ? pc : "") ;

#ifdef VERBOSE
   fprintf(stderr,"host=(%s), port=%d, filename=(%s)\n",
         http_server, http_port, *pfilename);
#endif
   return OK0;
}
