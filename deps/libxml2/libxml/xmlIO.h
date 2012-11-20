/*
 * Summary: interface for the I/O interfaces used by the parser
 * Description: interface for the I/O interfaces used by the parser
 *
 * Copy: See Copyright for the status of this software.
 *
 * Author: Daniel Veillard
 */

#ifndef __XML_IO_H__
#define __XML_IO_H__

#include <stdio.h>
#include <libxml/xmlversion.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Those are the functions and datatypes for the parser input
 * I/O structures.
 */

/**
 * xmlInputMatchCallback:
 * @filename: the filename or URI
 *
 * Callback used in the I/O Input API to detect if the current handler 
 * can provide input fonctionnalities for this resource.
 *
 * Returns 1 if yes and 0 if another Input module should be used
 */
typedef int (XMLCALL *xmlInputMatchCallback) (char const *filename);
/**
 * xmlInputOpenCallback:
 * @filename: the filename or URI
 *
 * Callback used in the I/O Input API to open the resource
 *
 * Returns an Input context or NULL in case or error
 */
typedef void * (XMLCALL *xmlInputOpenCallback) (char const *filename);
/**
 * xmlInputReadCallback:
 * @context:  an Input context
 * @buffer:  the buffer to store data read
 * @len:  the length of the buffer in bytes
 *
 * Callback used in the I/O Input API to read the resource
 *
 * Returns the number of bytes read or -1 in case of error
 */
typedef int (XMLCALL *xmlInputReadCallback) (void * context, char * buffer, int len);
/**
 * xmlInputCloseCallback:
 * @context:  an Input context
 *
 * Callback used in the I/O Input API to close the resource
 *
 * Returns 0 or -1 in case of error
 */
typedef int (XMLCALL *xmlInputCloseCallback) (void * context);


#ifdef __cplusplus
}
#endif

#include <libxml/globals.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/encoding.h>

#ifdef __cplusplus
extern "C" {
#endif
struct _xmlParserInputBuffer {
    void*                  context;
    xmlInputReadCallback   readcallback;
    xmlInputCloseCallback  closecallback;
    
    xmlCharEncodingHandlerPtr encoder; /* I18N conversions to UTF-8 */
    
    xmlBufferPtr buffer;    /* Local buffer encoded in UTF-8 */
    xmlBufferPtr raw;       /* if encoder != NULL buffer for raw input */
    int	compressed;	    /* -1=unknown, 0=not compressed, 1=compressed */
    int error;
    unsigned long rawconsumed;/* amount consumed from raw */
};


/*
 * Interfaces for input
 */
XMLPUBFUN void XMLCALL	
	xmlCleanupInputCallbacks		(void);

XMLPUBFUN int XMLCALL
	xmlPopInputCallbacks			(void);

XMLPUBFUN void XMLCALL	
	xmlRegisterDefaultInputCallbacks	(void);
XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlAllocParserInputBuffer		(xmlCharEncoding enc);

XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlParserInputBufferCreateFilename	(const char *URI,
                                                 xmlCharEncoding enc);
XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlParserInputBufferCreateFile		(FILE *file,
                                                 xmlCharEncoding enc);
XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlParserInputBufferCreateFd		(int fd,
	                                         xmlCharEncoding enc);
XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlParserInputBufferCreateMem		(const char *mem, int size,
	                                         xmlCharEncoding enc);
XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlParserInputBufferCreateStatic	(const char *mem, int size,
	                                         xmlCharEncoding enc);
XMLPUBFUN xmlParserInputBufferPtr XMLCALL
	xmlParserInputBufferCreateIO		(xmlInputReadCallback   ioread,
						 xmlInputCloseCallback  ioclose,
						 void *ioctx,
	                                         xmlCharEncoding enc);
XMLPUBFUN int XMLCALL	
	xmlParserInputBufferRead		(xmlParserInputBufferPtr in,
						 int len);
XMLPUBFUN int XMLCALL	
	xmlParserInputBufferGrow		(xmlParserInputBufferPtr in,
						 int len);
XMLPUBFUN int XMLCALL	
	xmlParserInputBufferPush		(xmlParserInputBufferPtr in,
						 int len,
						 const char *buf);
XMLPUBFUN void XMLCALL	
	xmlFreeParserInputBuffer		(xmlParserInputBufferPtr in);
XMLPUBFUN char * XMLCALL	
	xmlParserGetDirectory			(const char *filename);

XMLPUBFUN int XMLCALL     
	xmlRegisterInputCallbacks		(xmlInputMatchCallback matchFunc,
						 xmlInputOpenCallback openFunc,
						 xmlInputReadCallback readFunc,
						 xmlInputCloseCallback closeFunc);

xmlParserInputBufferPtr
	__xmlParserInputBufferCreateFilename(const char *URI,
										xmlCharEncoding enc);

XMLPUBFUN xmlParserInputPtr XMLCALL
	xmlCheckHTTPInput		(xmlParserCtxtPtr ctxt,
					 xmlParserInputPtr ret);

/*
 * A predefined entity loader disabling network accesses
 */
XMLPUBFUN xmlParserInputPtr XMLCALL 
	xmlNoNetExternalEntityLoader	(const char *URL,
					 const char *ID,
					 xmlParserCtxtPtr ctxt);

/* 
 * xmlNormalizeWindowsPath is obsolete, don't use it. 
 * Check xmlCanonicPath in uri.h for a better alternative.
 */
XMLPUBFUN xmlChar * XMLCALL 
	xmlNormalizeWindowsPath		(const xmlChar *path);

XMLPUBFUN int XMLCALL	
	xmlCheckFilename		(const char *path);
/**
 * Default 'file://' protocol callbacks 
 */
XMLPUBFUN int XMLCALL	
	xmlFileMatch 			(const char *filename);
XMLPUBFUN void * XMLCALL	
	xmlFileOpen 			(const char *filename);
XMLPUBFUN int XMLCALL	
	xmlFileRead 			(void * context, 
					 char * buffer, 
					 int len);
XMLPUBFUN int XMLCALL	
	xmlFileClose 			(void * context);

#ifdef __cplusplus
}
#endif

#endif /* __XML_IO_H__ */
