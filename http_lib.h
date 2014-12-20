/*
 *  Http put/get mini lib
 *  written by L. Demailly
 *  (c) 1998 Laurent Demailly - http://www.demailly.com/~dl/
 *  (c) 1996 Observatoire de Paris - Meudon - France
 *  see LICENSE for terms, conditions and DISCLAIMER OF ALL WARRANTIES
 *
 * $Id: http_lib.h,v 1.4 1998/09/23 06:14:15 dl Exp $
 *
 */

 /* declarations */


extern char *http_server;

extern int http_port;

extern char *http_proxy_server;

extern int http_proxy_port;


/* return type */
typedef enum {

  /* Client side errors */
  ERRHOST=-1, /* No such host */
  ERRSOCK=-2, /* Can't create socket */
  ERRCONN=-3, /* Can't connect to host */
  ERRWRHD=-4, /* Write error on socket while writing header */
  ERRWRDT=-5, /* Write error on socket while writing data */
  ERRRDHD=-6, /* Read error on socket while reading result */
  ERRPAHD=-7, /* Invalid answer from data server */
  ERRNULL=-8, /* Null data pointer */
  ERRNOLG=-9, /* No/Bad length in header */
  ERRMEM=-10, /* Can't allocate memory */
  ERRRDDT=-11,/* Read error while reading data */
  ERRURLH=-12,/* Invalid url - must start with 'http://' */
  ERRURLP=-13,/* Invalid port in url */
  

  /* Return code by the server */
  ERR400=400, /* Invalid query */
  ERR403=403, /* Forbidden */
  ERR408=408, /* Request timeout */
  ERR500=500, /* Server error */
  ERR501=501, /* Not implemented */
  ERR503=503, /* Service overloaded */

  /* Succesful results */
  OK0 = 0,   /* successfull parse */
  OK201=201, /* Ressource succesfully created */
  OK200=200  /* Ressource succesfully read */

} http_retcode;


/* prototypes */

#ifndef OSK
http_retcode http_put(char *filename, char *data, int length, 
	     int overwrite, char *type) ;
http_retcode http_get(char *filename, char **pdata,int *plength, char *typebuf);

http_retcode http_parse_url(char *url, char **pfilename);

http_retcode http_delete(char *filename) ;

http_retcode http_head(char *filename, int *plength, char *typebuf);

#endif
