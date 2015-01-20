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

#ifndef _HTTP_LIB_H
#define _HTTP_LIB_H

extern char *http_server;

extern int http_port;

extern char *http_proxy_server;

extern int http_proxy_port;

typedef enum
{
  /* Client side errors */

  ERRHOST = -1,  /* No such host */
  ERRSOCK = -2,  /* Can't create socket */
  ERRCONN = -3,  /* Can't connect to host */
  ERRWRHD = -4,  /* Write error on socket while writing header */
  ERRWRDT = -5,  /* Write error on socket while writing data */
  ERRRDHD = -6,  /* Read error on socket while reading result */
  ERRPAHD = -7,  /* Invalid answer from data server */
  ERRNULL = -8,  /* NULL data pointer */
  ERRNOLG = -9,  /* No/Bad length in header */
  ERRMEM  = -10, /* Can't allocate memory */
  ERRRDDT = -11, /* Read error while reading data */
  ERRURLH = -12, /* Invalid URL - must start with 'http://' */
  ERRURLP = -13, /* Invalid port in URL */
  
  /* Return code by the server */

  ERR400  = 400, /* Invalid query */
  ERR403  = 403, /* Forbidden */
  ERR408  = 408, /* Request timeout */
  ERR500  = 500, /* Server error */
  ERR501  = 501, /* Not implemented */
  ERR503  = 503, /* Service overloaded */

  /* Succesful results */
  OK0     = 0,   /* successfully parsed */
  OK201   = 201, /* Resource succesfully created */
  OK200   = 200  /* Resource succesfully read */

} http_retcode;


/* prototypes */

#ifndef OSK
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
http_retcode http_put(const char *filename, const char *data, int length, 
	     int overwrite, const char *type) ;

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
http_retcode http_get(const char *filename, char **pdata,
      int *plength, char *typebuf);

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
http_retcode http_parse_url(char *url, char **pfilename);

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
http_retcode http_delete(const char *filename) ;

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
http_retcode http_head(const char *filename, int *plength, char *typebuf);

#endif

#endif
