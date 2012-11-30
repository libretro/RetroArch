/*
 * xmlmemory.c:  libxml memory allocator wrapper.
 *
 * daniel@veillard.com
 */

#define IN_LIBXML
#include "libxml.h"

#include <string.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#endif

#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include <libxml/globals.h>	/* must come before xmlmemory.h */
#include <libxml/xmlmemory.h>
#include <libxml/xmlerror.h>
#include <libxml/threads.h>

static int xmlMemInitialized = 0;
static unsigned long  debugMemSize = 0;
static unsigned long  debugMemBlocks = 0;
static unsigned long  debugMaxMemSize = 0;

void xmlMallocBreakpoint(void);

/************************************************************************
 *									*
 * 		Macros, variables and associated types			*
 *									*
 ************************************************************************/

#ifdef xmlMalloc
#undef xmlMalloc
#endif
#ifdef xmlRealloc
#undef xmlRealloc
#endif
#ifdef xmlMemStrdup
#undef xmlMemStrdup
#endif

/*
 * Each of the blocks allocated begin with a header containing informations
 */

#define MEMTAG 0x5aa5

#define MALLOC_TYPE 1
#define REALLOC_TYPE 2
#define STRDUP_TYPE 3
#define MALLOC_ATOMIC_TYPE 4
#define REALLOC_ATOMIC_TYPE 5

typedef struct memnod {
    unsigned int   mh_tag;
    unsigned int   mh_type;
    unsigned long  mh_number;
    size_t         mh_size;
   const char    *mh_file;
   unsigned int   mh_line;
}  MEMHDR;


#ifdef SUN4
#define ALIGN_SIZE  16
#else
#define ALIGN_SIZE  sizeof(double)
#endif
#define HDR_SIZE    sizeof(MEMHDR)
#define RESERVE_SIZE (((HDR_SIZE + (ALIGN_SIZE-1)) \
		      / ALIGN_SIZE ) * ALIGN_SIZE)


#define CLIENT_2_HDR(a) ((MEMHDR *) (((char *) (a)) - RESERVE_SIZE))
#define HDR_2_CLIENT(a)    ((void *) (((char *) (a)) + RESERVE_SIZE))


static unsigned int block=0;
static unsigned int xmlMemStopAtBlock = 0;
static void *xmlMemTraceBlockAt = NULL;

static void debugmem_tag_error(void *addr);
#define Mem_Tag_Err(a) debugmem_tag_error(a);

#ifndef TEST_POINT
#define TEST_POINT
#endif

/**
 * xmlMallocBreakpoint:
 *
 * Breakpoint to use in conjunction with xmlMemStopAtBlock. When the block
 * number reaches the specified value this function is called. One need to add a breakpoint
 * to it to get the context in which the given block is allocated.
 */

void
xmlMallocBreakpoint(void) {
    xmlGenericError(xmlGenericErrorContext,
	    "xmlMallocBreakpoint reached on block %d\n", xmlMemStopAtBlock);
}

/**
 * xmlMallocLoc:
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a malloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlMallocLoc(size_t size, const char * file, int line)
{
    MEMHDR *p;
    void *ret;

    if (!xmlMemInitialized) xmlInitMemory();

    TEST_POINT

    p = (MEMHDR *) malloc(RESERVE_SIZE+size);

    if (!p) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMallocLoc : Out of free space\n");
	xmlMemoryDump();
	return(NULL);
    }
    p->mh_tag = MEMTAG;
    p->mh_size = size;
    p->mh_type = MALLOC_TYPE;
    p->mh_file = file;
    p->mh_line = line;
    p->mh_number = ++block;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;


    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();

    ret = HDR_2_CLIENT(p);

    if (xmlMemTraceBlockAt == ret) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Malloc(%lu) Ok\n", xmlMemTraceBlockAt,
			(long unsigned)size);
	xmlMallocBreakpoint();
    }

    TEST_POINT

    return(ret);
}

/**
 * xmlMallocAtomicLoc:
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a malloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlMallocAtomicLoc(size_t size, const char * file, int line)
{
    MEMHDR *p;
    void *ret;

    if (!xmlMemInitialized) xmlInitMemory();

    TEST_POINT

    p = (MEMHDR *) malloc(RESERVE_SIZE+size);

    if (!p) {
	xmlGenericError(xmlGenericErrorContext,
		"xmlMallocLoc : Out of free space\n");
	xmlMemoryDump();
	return(NULL);
    }
    p->mh_tag = MEMTAG;
    p->mh_size = size;
    p->mh_type = MALLOC_ATOMIC_TYPE;
    p->mh_file = file;
    p->mh_line = line;
    p->mh_number = ++block;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;


    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();

    ret = HDR_2_CLIENT(p);

    if (xmlMemTraceBlockAt == ret) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Malloc(%lu) Ok\n", xmlMemTraceBlockAt,
			(long unsigned)size);
	xmlMallocBreakpoint();
    }

    TEST_POINT

    return(ret);
}
/**
 * xmlMemMalloc:
 * @size:  an int specifying the size in byte to allocate.
 *
 * a malloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlMemMalloc(size_t size)
{
    return(xmlMallocLoc(size, "none", 0));
}

/**
 * xmlReallocLoc:
 * @ptr:  the initial memory block pointer
 * @size:  an int specifying the size in byte to allocate.
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a realloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlReallocLoc(void *ptr,size_t size, const char * file, int line)
{
    MEMHDR *p;
    unsigned long number;

    if (ptr == NULL)
        return(xmlMallocLoc(size, file, line));

    if (!xmlMemInitialized) xmlInitMemory();
    TEST_POINT

    p = CLIENT_2_HDR(ptr);
    number = p->mh_number;
    if (xmlMemStopAtBlock == number) xmlMallocBreakpoint();
    if (p->mh_tag != MEMTAG) {
       Mem_Tag_Err(p);
	 goto error;
    }
    p->mh_tag = ~MEMTAG;
    debugMemSize -= p->mh_size;
    debugMemBlocks--;

    p = (MEMHDR *) realloc(p,RESERVE_SIZE+size);
    if (!p) {
	 goto error;
    }
    if (xmlMemTraceBlockAt == ptr) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Realloced(%lu -> %lu) Ok\n",
			xmlMemTraceBlockAt, (long unsigned)p->mh_size,
			(long unsigned)size);
	xmlMallocBreakpoint();
    }
    p->mh_tag = MEMTAG;
    p->mh_number = number;
    p->mh_type = REALLOC_TYPE;
    p->mh_size = size;
    p->mh_file = file;
    p->mh_line = line;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;

    TEST_POINT

    return(HDR_2_CLIENT(p));

error:
    return(NULL);
}

/**
 * xmlMemRealloc:
 * @ptr:  the initial memory block pointer
 * @size:  an int specifying the size in byte to allocate.
 *
 * a realloc() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the allocated area or NULL in case of lack of memory.
 */

void *
xmlMemRealloc(void *ptr,size_t size) {
    return(xmlReallocLoc(ptr, size, "none", 0));
}

/**
 * xmlMemFree:
 * @ptr:  the memory block pointer
 *
 * a free() equivalent, with error checking.
 */
void
xmlMemFree(void *ptr)
{
    MEMHDR *p;
    char *target;

    if (ptr == NULL)
	return;

    if (ptr == (void *) -1) {
	xmlGenericError(xmlGenericErrorContext,
	    "trying to free pointer from freed area\n");
        goto error;
    }

    if (xmlMemTraceBlockAt == ptr) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Freed()\n", xmlMemTraceBlockAt);
	xmlMallocBreakpoint();
    }

    TEST_POINT

    target = (char *) ptr;

    p = CLIENT_2_HDR(ptr);
    if (p->mh_tag != MEMTAG) {
        Mem_Tag_Err(p);
        goto error;
    }
    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();
    p->mh_tag = ~MEMTAG;
    memset(target, -1, p->mh_size);
    debugMemSize -= p->mh_size;
    debugMemBlocks--;

    free(p);

    TEST_POINT


    return;

error:
    xmlGenericError(xmlGenericErrorContext,
	    "xmlMemFree(%lX) error\n", (unsigned long) ptr);
    xmlMallocBreakpoint();
    return;
}

/**
 * xmlMemStrdupLoc:
 * @str:  the initial string pointer
 * @file:  the file name or NULL
 * @line:  the line number
 *
 * a strdup() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the new string or NULL if allocation error occurred.
 */

char *
xmlMemStrdupLoc(const char *str, const char *file, int line)
{
    char *s;
    size_t size = strlen(str) + 1;
    MEMHDR *p;

    if (!xmlMemInitialized) xmlInitMemory();
    TEST_POINT

    p = (MEMHDR *) malloc(RESERVE_SIZE+size);
    if (!p) {
      goto error;
    }
    p->mh_tag = MEMTAG;
    p->mh_size = size;
    p->mh_type = STRDUP_TYPE;
    p->mh_file = file;
    p->mh_line = line;
    p->mh_number = ++block;
    debugMemSize += size;
    debugMemBlocks++;
    if (debugMemSize > debugMaxMemSize) debugMaxMemSize = debugMemSize;

    s = (char *) HDR_2_CLIENT(p);

    if (xmlMemStopAtBlock == p->mh_number) xmlMallocBreakpoint();

    if (s != NULL)
      strcpy(s,str);
    else
      goto error;

    TEST_POINT

    if (xmlMemTraceBlockAt == s) {
	xmlGenericError(xmlGenericErrorContext,
			"%p : Strdup() Ok\n", xmlMemTraceBlockAt);
	xmlMallocBreakpoint();
    }

    return(s);

error:
    return(NULL);
}

/**
 * xmlMemoryStrdup:
 * @str:  the initial string pointer
 *
 * a strdup() equivalent, with logging of the allocation info.
 *
 * Returns a pointer to the new string or NULL if allocation error occurred.
 */

char *
xmlMemoryStrdup(const char *str) {
    return(xmlMemStrdupLoc(str, "none", 0));
}

/**
 * xmlMemUsed:
 *
 * Provides the amount of memory currently allocated
 *
 * Returns an int representing the amount of memory allocated.
 */

int
xmlMemUsed(void) {
     return(debugMemSize);
}

/**
 * xmlMemBlocks:
 *
 * Provides the number of memory areas currently allocated
 *
 * Returns an int representing the number of blocks
 */

int
xmlMemBlocks(void) {
     return(debugMemBlocks);
}

/**
 * xmlMemDisplayLast:
 * @fp:  a FILE descriptor used as the output file, if NULL, the result is
 *       written to the file .memorylist
 * @nbBytes: the amount of memory to dump
 *
 * the last nbBytes of memory allocated and not freed, useful for dumping
 * the memory left allocated between two places at runtime.
 */

void
xmlMemDisplayLast(FILE *fp, long nbBytes)
{
    FILE *old_fp = fp;

    if (nbBytes <= 0)
        return;

    if (fp == NULL) {
	fp = fopen(".memorylist", "w");
	if (fp == NULL)
	    return;
    }

    fprintf(fp,"Memory list not compiled (MEM_LIST not defined !)\n");
    if (old_fp == NULL)
	fclose(fp);
}

/**
 * xmlMemDisplay:
 * @fp:  a FILE descriptor used as the output file, if NULL, the result is
 *       written to the file .memorylist
 *
 * show in-extenso the memory blocks allocated
 */

void
xmlMemDisplay(FILE *fp)
{
    FILE *old_fp = fp;

    if (fp == NULL) {
	fp = fopen(".memorylist", "w");
	if (fp == NULL)
	    return;
    }

    fprintf(fp,"Memory list not compiled (MEM_LIST not defined !)\n");
    if (old_fp == NULL)
	fclose(fp);
}

/*
 * debugmem_tag_error:
 *
 * internal error function.
 */

static void debugmem_tag_error(void *p)
{
     xmlGenericError(xmlGenericErrorContext,
	     "Memory tag error occurs :%p \n\t bye\n", p);
}


/**
 * xmlMemShow:
 * @fp:  a FILE descriptor used as the output file
 * @nr:  number of entries to dump
 *
 * show a show display of the memory allocated, and dump
 * the @nr last allocated areas which were not freed
 */

void
xmlMemShow(FILE *fp, int nr ATTRIBUTE_UNUSED)
{

    if (fp != NULL)
	fprintf(fp,"      MEMORY ALLOCATED : %lu, MAX was %lu\n",
		debugMemSize, debugMaxMemSize);
}

/**
 * xmlMemoryDump:
 *
 * Dump in-extenso the memory blocks allocated to the file .memorylist
 */

void
xmlMemoryDump(void)
{
}


/****************************************************************
 *								*
 *		Initialization Routines				*
 *								*
 ****************************************************************/

/**
 * xmlInitMemory:
 *
 * Initialize the memory layer.
 *
 * Returns 0 on success
 */
int
xmlInitMemory(void)
{
#ifdef HAVE_STDLIB_H
     char *breakpoint;
#endif
    /*
     This is really not good code (see Bug 130419).  Suggestions for
     improvement will be welcome!
    */
     if (xmlMemInitialized) return(-1);
     xmlMemInitialized = 1;

#ifdef HAVE_STDLIB_H
     breakpoint = getenv("XML_MEM_BREAKPOINT");
     if (breakpoint != NULL) {
         sscanf(breakpoint, "%ud", &xmlMemStopAtBlock);
     }
#endif
#ifdef HAVE_STDLIB_H
     breakpoint = getenv("XML_MEM_TRACE");
     if (breakpoint != NULL) {
         sscanf(breakpoint, "%p", &xmlMemTraceBlockAt);
     }
#endif

     return(0);
}

/**
 * xmlCleanupMemory:
 *
 * Free up all the memory allocated by the library for its own
 * use. This should not be called by user level code.
 */
void
xmlCleanupMemory(void) {
    if (xmlMemInitialized == 0)
        return;

    xmlMemInitialized = 0;
}

/**
 * xmlMemSetup:
 * @freeFunc: the free() function to use
 * @mallocFunc: the malloc() function to use
 * @reallocFunc: the realloc() function to use
 * @strdupFunc: the strdup() function to use
 *
 * Override the default memory access functions with a new set
 * This has to be called before any other libxml routines !
 *
 * Should this be blocked if there was already some allocations
 * done ?
 *
 * Returns 0 on success
 */
int
xmlMemSetup(xmlFreeFunc freeFunc, xmlMallocFunc mallocFunc,
            xmlReallocFunc reallocFunc, xmlStrdupFunc strdupFunc) {
    if (freeFunc == NULL)
	return(-1);
    if (mallocFunc == NULL)
	return(-1);
    if (reallocFunc == NULL)
	return(-1);
    if (strdupFunc == NULL)
	return(-1);
    xmlFree = freeFunc;
    xmlMalloc = mallocFunc;
    xmlMallocAtomic = mallocFunc;
    xmlRealloc = reallocFunc;
    xmlMemStrdup = strdupFunc;
    return(0);
}

/**
 * xmlMemGet:
 * @freeFunc: place to save the free() function in use
 * @mallocFunc: place to save the malloc() function in use
 * @reallocFunc: place to save the realloc() function in use
 * @strdupFunc: place to save the strdup() function in use
 *
 * Provides the memory access functions set currently in use
 *
 * Returns 0 on success
 */
int
xmlMemGet(xmlFreeFunc *freeFunc, xmlMallocFunc *mallocFunc,
	  xmlReallocFunc *reallocFunc, xmlStrdupFunc *strdupFunc) {
    if (freeFunc != NULL) *freeFunc = xmlFree;
    if (mallocFunc != NULL) *mallocFunc = xmlMalloc;
    if (reallocFunc != NULL) *reallocFunc = xmlRealloc;
    if (strdupFunc != NULL) *strdupFunc = xmlMemStrdup;
    return(0);
}

/**
 * xmlGcMemSetup:
 * @freeFunc: the free() function to use
 * @mallocFunc: the malloc() function to use
 * @mallocAtomicFunc: the malloc() function to use for atomic allocations
 * @reallocFunc: the realloc() function to use
 * @strdupFunc: the strdup() function to use
 *
 * Override the default memory access functions with a new set
 * This has to be called before any other libxml routines !
 * The mallocAtomicFunc is specialized for atomic block
 * allocations (i.e. of areas  useful for garbage collected memory allocators
 *
 * Should this be blocked if there was already some allocations
 * done ?
 *
 * Returns 0 on success
 */
int
xmlGcMemSetup(xmlFreeFunc freeFunc, xmlMallocFunc mallocFunc,
              xmlMallocFunc mallocAtomicFunc, xmlReallocFunc reallocFunc,
	      xmlStrdupFunc strdupFunc) {
    if (freeFunc == NULL)
	return(-1);
    if (mallocFunc == NULL)
	return(-1);
    if (mallocAtomicFunc == NULL)
	return(-1);
    if (reallocFunc == NULL)
	return(-1);
    if (strdupFunc == NULL)
	return(-1);
    xmlFree = freeFunc;
    xmlMalloc = mallocFunc;
    xmlMallocAtomic = mallocAtomicFunc;
    xmlRealloc = reallocFunc;
    xmlMemStrdup = strdupFunc;
    return(0);
}

/**
 * xmlGcMemGet:
 * @freeFunc: place to save the free() function in use
 * @mallocFunc: place to save the malloc() function in use
 * @mallocAtomicFunc: place to save the atomic malloc() function in use
 * @reallocFunc: place to save the realloc() function in use
 * @strdupFunc: place to save the strdup() function in use
 *
 * Provides the memory access functions set currently in use
 * The mallocAtomicFunc is specialized for atomic block
 * allocations (i.e. of areas  useful for garbage collected memory allocators
 *
 * Returns 0 on success
 */
int
xmlGcMemGet(xmlFreeFunc *freeFunc, xmlMallocFunc *mallocFunc,
            xmlMallocFunc *mallocAtomicFunc, xmlReallocFunc *reallocFunc,
	    xmlStrdupFunc *strdupFunc) {
    if (freeFunc != NULL) *freeFunc = xmlFree;
    if (mallocFunc != NULL) *mallocFunc = xmlMalloc;
    if (mallocAtomicFunc != NULL) *mallocAtomicFunc = xmlMallocAtomic;
    if (reallocFunc != NULL) *reallocFunc = xmlRealloc;
    if (strdupFunc != NULL) *strdupFunc = xmlMemStrdup;
    return(0);
}

#define bottom_xmlmemory
#include "elfgcchack.h"
