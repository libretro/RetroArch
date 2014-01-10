/* szlib.h -- internal interface and configuration of the compression library
 * Copyright (C) 1995-2002 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* This source as presented is a modified version of original zlib for use with
 * RetroArch, and must not be confused with the original software. */

#ifndef _RZLIB_H
#define _RZLIB_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>
#include "zconf.h"

/* constants */
#define ZLIB_VERSION "1.1.4"

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1 /* will be removed, use Z_SYNC_FLUSH instead */
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
/* Allowed flush values; see deflate() below for details */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)

/* Return codes for the compression/decompression functions. Negative
 * values are errors, positive values are used for special but normal events.
 */

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_DEFAULT_STRATEGY    0

#define Z_BINARY   0
#define Z_ASCII    1
#define Z_UNKNOWN  2
/* Possible values of the data_type field */

#define Z_DEFLATED   8
/* The deflate compression method (the only one supported in this version) */

#define Z_NULL  0  /* for initializing zalloc, zfree, opaque */

#define zlib_version zlibVersion()
/* for compatibility with versions < 1.0.2 */

typedef voidpf (*alloc_func) (voidpf opaque, unsigned int items, unsigned int size);
typedef void   (*free_func)  (voidpf opaque, voidpf address);

typedef struct z_stream_s {
    Bytef    *next_in;  /* next input byte */
    unsigned int     avail_in;  /* number of bytes available at next_in */
    unsigned long    total_in;  /* total nb of input bytes read so far */

    Bytef    *next_out; /* next output byte should be put there */
    unsigned int     avail_out; /* remaining free space at next_out */
    unsigned long    total_out; /* total nb of bytes output so far */

    char     *msg;      /* last error message, NULL if no error */
    void *state; /* not visible by applications */

    alloc_func zalloc;  /* used to allocate the internal state */
    free_func  zfree;   /* used to free the internal state */
    voidpf     opaque;  /* private data object passed to zalloc and zfree */

    int     data_type;  /* best guess about the data type: ascii or binary */
    unsigned long   adler;      /* adler32 value of the uncompressed data */
    unsigned long   reserved;   /* reserved for future use */
} z_stream;

typedef z_stream *z_streamp;

typedef unsigned char  uch;
typedef unsigned char uchf;
typedef unsigned short ush;
typedef unsigned short ushf;
typedef unsigned long  ulg;

extern const char *z_errmsg[10]; /* indexed by 2-zlib_error */
/* (size given to avoid silly warnings with Visual C++) */

#define ERR_MSG(err) z_errmsg[Z_NEED_DICT-(err)]

#define ERR_RETURN(strm,err) \
  return (strm->msg = (char*)ERR_MSG(err), (err))
/* To be used only when the state is known to be valid */

        /* common constants */

#ifndef DEF_WBITS
#  define DEF_WBITS MAX_WBITS
#endif
/* default windowBits for decompression. MAX_WBITS is for compression only */

#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif
/* default memLevel */

#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
/* The three kinds of block type */

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#define PRESET_DICT 0x20 /* preset dictionary flag in zlib header */

        /* target dependencies */

#ifdef WIN32 /* Window 95 & Windows NT */
#  define OS_CODE  0x0b
#endif

#if defined(ATARI) || defined(atarist)
#  define OS_CODE  0x05
#endif

#if defined(MACOS) || defined(TARGET_OS_MAC)
#  define OS_CODE  0x07
#  if defined(__MWERKS__) && __dest_os != __be_os && __dest_os != __win32_os
#    include <unix.h> /* for fdopen */
#  else
#    ifndef fdopen
#      define fdopen(fd,mode) NULL /* No fdopen() */
#    endif
#  endif
#endif

#ifdef __50SERIES /* Prime/PRIMOS */
#  define OS_CODE  0x0F
#endif

#ifdef TOPS20
#  define OS_CODE  0x0a
#endif

#if defined(_BEOS_) || defined(RISCOS)
#  define fdopen(fd,mode) NULL /* No fdopen() */
#endif

#if (defined(_MSC_VER) && (_MSC_VER > 600))
#  define fdopen(fd,type)  _fdopen(fd,type)
#endif

/* Common defaults */

#ifndef OS_CODE
#  define OS_CODE  0x03  /* assume Unix */
#endif

/* functions */

#ifdef HAVE_STRERROR
   extern char *strerror (int);
#  define zstrerror(errnum) strerror(errnum)
#else
#  define zstrerror(errnum) ""
#endif

typedef unsigned long ( *check_func) (unsigned long check, const Bytef *buf,
				       unsigned int len);
voidpf zcalloc (voidpf opaque, unsigned items, unsigned size);
void   zcfree  (voidpf opaque, voidpf ptr);

#define ZALLOC(strm, items, size) \
           (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZFREE(strm, addr)  (*((strm)->zfree))((strm)->opaque, (voidpf)(addr))

/* Huffman code lookup table entry--this entry is four bytes for machines
   that have 16-bit pointers (e.g. PC's in the small or medium model). */

typedef struct inflate_huft_s inflate_huft;

struct inflate_huft_s {
  union {
    struct {
      Byte Exop;        /* number of extra bits or operation */
      Byte Bits;        /* number of bits in this code or subcode */
    } what;
    unsigned int pad;           /* pad structure to a power of 2 (4 bytes for */
  } word;               /*  16-bit, 8 bytes for 32-bit int's) */
  unsigned int base;            /* literal, length base, distance base,
                           or table offset */
};

/* Maximum size of dynamic tree.  The maximum found in a long but non-
   exhaustive search was 1004 huft structures (850 for length/literals
   and 154 for distances, the latter actually the result of an
   exhaustive search).  The actual maximum is not known, but the
   value below is more than safe. */
#define MANY 1440

extern int inflate_trees_bits (
    unsigned int *,                    /* 19 code lengths */
    unsigned int *,                    /* bits tree desired/actual depth */
    inflate_huft **,       /* bits tree result */
    inflate_huft *,             /* space for trees */
    z_streamp);                /* for messages */

extern int inflate_trees_dynamic (
    unsigned int,                       /* number of literal/length codes */
    unsigned int,                       /* number of distance codes */
    unsigned int *,                    /* that many (total) code lengths */
    unsigned int *,                    /* literal desired/actual bit depth */
    unsigned int *,                    /* distance desired/actual bit depth */
    inflate_huft **,       /* literal/length tree result */
    inflate_huft **,       /* distance tree result */
    inflate_huft *,             /* space for trees */
    z_streamp);                /* for messages */

extern int inflate_trees_fixed (
    unsigned int *,                    /* literal desired/actual bit depth */
    unsigned int *,                    /* distance desired/actual bit depth */
    inflate_huft **,       /* literal/length tree result */
    inflate_huft **,       /* distance tree result */
    z_streamp);                /* for memory allocation */

struct inflate_blocks_state;
typedef struct inflate_blocks_state inflate_blocks_statef;

extern inflate_blocks_statef * inflate_blocks_new (
    z_streamp z,
    check_func c,               /* check function */
    unsigned int w);                   /* window size */

extern int inflate_blocks (
    inflate_blocks_statef *,
    z_streamp ,
    int);                      /* initial return code */

extern void inflate_blocks_reset (
    inflate_blocks_statef *,
    z_streamp ,
    unsigned long *);                  /* check value on output */

extern int inflate_blocks_free (
    inflate_blocks_statef *,
    z_streamp);

struct inflate_codes_state;
typedef struct inflate_codes_state inflate_codes_statef;

extern inflate_codes_statef *inflate_codes_new (
    unsigned int, unsigned int,
    inflate_huft *, inflate_huft *,
    z_streamp );

extern int inflate_codes (
    inflate_blocks_statef *,
    z_streamp ,
    int);

typedef enum {
      TYPE,     /* get type bits (3, including end bit) */
      LENS,     /* get lengths for stored */
      STORED,   /* processing stored block */
      TABLE,    /* get table lengths */
      BTREE,    /* get bit lengths tree for a dynamic block */
      DTREE,    /* get length, distance trees for a dynamic block */
      CODES,    /* processing fixed or dynamic block */
      DRY,      /* output remaining window bytes */
      DONE,     /* finished last block, done */
      BAD}      /* got a data error--stuck here */
inflate_block_mode;

/* inflate blocks semi-private state */
struct inflate_blocks_state {

  /* mode */
  inflate_block_mode  mode;     /* current inflate_block mode */

  /* mode dependent information */
  union {
    unsigned int left;          /* if STORED, bytes left to copy */
    struct {
      unsigned int table;               /* table lengths (14 bits) */
      unsigned int index;               /* index into blens (or border) */
      unsigned int *blens;             /* bit lengths of codes */
      unsigned int bb;                  /* bit length tree depth */
      inflate_huft *tb;         /* bit length decoding tree */
    } trees;            /* if DTREE, decoding info for trees */
    struct {
      inflate_codes_statef 
         *codes;
    } decode;           /* if CODES, current state */
  } sub;                /* submode */
  unsigned int last;            /* true if this block is the last block */

  /* mode independent information */
  unsigned int bitk;            /* bits in bit buffer */
  unsigned long bitb;           /* bit buffer */
  inflate_huft *hufts;  /* single malloc for tree space */
  Bytef *window;        /* sliding window */
  Bytef *end;           /* one byte after sliding window */
  Bytef *read;          /* window read pointer */
  Bytef *write;         /* window write pointer */
  check_func checkfn;   /* check function */
  unsigned long check;          /* check on output */

};

extern int inflate(z_streamp z, int f);
extern int inflateEnd(z_streamp z);


/* defines for inflate input/output */
/*   update pointers and return */
#define UPDBITS {s->bitb=b;s->bitk=k;}
#define UPDIN {z->avail_in=n;z->total_in+=p-z->next_in;z->next_in=p;}
#define UPDOUT {s->write=q;}
#define UPDATE {UPDBITS UPDIN UPDOUT}
#define LEAVE {UPDATE return inflate_flush(s,z,r);}
/*   get bytes and bits */
#define LOADIN {p=z->next_in;n=z->avail_in;b=s->bitb;k=s->bitk;}
#define NEEDBYTE {if(n)r=Z_OK;else LEAVE}
#define NEXTBYTE (n--,*p++)
#define NEEDBITS(j) {while(k<(j)){NEEDBYTE;b|=((unsigned long)NEXTBYTE)<<k;k+=8;}}
#define DUMPBITS(j) {b>>=(j);k-=(j);}
/*   output bytes */
#define WAVAIL (unsigned int)(q<s->read?s->read-q-1:s->end-q)
#define LOADOUT {q=s->write;m=(unsigned int)WAVAIL;}
#define WRAP {if(q==s->end&&s->read!=s->window){q=s->window;m=(unsigned int)WAVAIL;}}
#define FLUSH {UPDOUT r=inflate_flush(s,z,r); LOADOUT}
#define NEEDOUT {if(m==0){WRAP if(m==0){FLUSH WRAP if(m==0) LEAVE}}r=Z_OK;}
#define OUTBYTE(a) {*q++=(Byte)(a);m--;}
/*   load local pointers */
#define LOAD {LOADIN LOADOUT}

/* masks for lower bits (size given to avoid silly warnings with Visual C++) */
extern unsigned int	inflate_mask[17];

/* copy as much as possible from the sliding window to the output area */
extern int	inflate_flush (inflate_blocks_statef *, z_streamp , int);

// utility functions

extern voidp	gzopen  (const char *path, const char *mode);
extern voidp	gzdopen  (int fd, const char *mode);
extern int	gzread  (voidp file, voidp buf, unsigned len);
extern int	gzwrite (voidp file, const voidp buf, unsigned len);
extern int	gzprintf (voidp file, const char *format, ...);
extern int	gzputs (voidp file, const char *s);
extern char *	gzgets (voidp file, char *buf, int len);
extern int	gzputc (voidp file, int c);
extern int	gzgetc (voidp file);
extern int	gzflush (voidp file, int flush);
extern z_off_t	gzseek (voidp file, z_off_t offset, int whence);
extern int	gzrewind (voidp file);
extern z_off_t	gztell (voidp file);
extern int	gzeof (voidp file);
extern int	gzclose (voidp file);

extern const char *  gzerror (voidp file, int *errnum);

/* checksum functions */

extern unsigned long	adler32 (unsigned long adler, const Bytef *buf, unsigned int len);
extern unsigned long	crc32   (unsigned long crc, const Bytef *buf, unsigned int len);

/* various hacks, don't look :) */

extern int	inflateInit_ (z_streamp strm, const char * version, int stream_size);
extern int	inflateInit2_ (z_streamp strm, int  windowBits, const char *version, int stream_size);

#define inflateInit(strm) \
        inflateInit_((strm),                ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) \
        inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

#if (!defined(_WIN32)) && (!defined(WIN32))

  // Linux needs this to support file operation on files larger then 4+GB
  // But might need better if/def to select just the platforms that needs them.

        #ifndef __USE_FILE_OFFSET64
                #define __USE_FILE_OFFSET64
        #endif
        #ifndef __USE_LARGEFILE64
                #define __USE_LARGEFILE64
        #endif
        #ifndef _LARGEFILE64_SOURCE
                #define _LARGEFILE64_SOURCE
        #endif
        #ifndef _FILE_OFFSET_BIT
                #define _FILE_OFFSET_BIT 64
        #endif
#endif

#if defined(USE_FILE32API)
#define fopen64 fopen
#define ftello64 ftell
#define fseeko64 fseek
#else
#ifdef _MSC_VER
 #define fopen64 fopen
 #if (_MSC_VER >= 1400) && (!(defined(NO_MSCVER_FILE64_FUNC)))
  #define ftello64 _ftelli64
  #define fseeko64 _fseeki64
 #else // old MSC
  #define ftello64 ftell
  #define fseeko64 fseek
 #endif
#endif
#endif

#ifdef HAVE_MINIZIP64_CONF_H
#include "mz64conf.h"
#endif

/* a type choosen by DEFINE */
#ifdef HAVE_64BIT_INT_CUSTOM
typedef  64BIT_INT_CUSTOM_TYPE ZPOS64_T;
#else
#ifdef HAS_STDINT_H
#include "stdint.h"
typedef uint64_t ZPOS64_T;
#else

#if defined(_MSC_VER) || defined(__BORLANDC__)
typedef unsigned __int64 ZPOS64_T;
#else
typedef unsigned long long int ZPOS64_T;
#endif
#endif
#endif

#define ZLIB_FILEFUNC_SEEK_CUR (1)
#define ZLIB_FILEFUNC_SEEK_END (2)
#define ZLIB_FILEFUNC_SEEK_SET (0)

#define ZLIB_FILEFUNC_MODE_READ      (1)
#define ZLIB_FILEFUNC_MODE_WRITE     (2)
#define ZLIB_FILEFUNC_MODE_READWRITEFILTER (3)

#define ZLIB_FILEFUNC_MODE_EXISTING (4)
#define ZLIB_FILEFUNC_MODE_CREATE   (8)

typedef voidpf   ( *open_file_func)      (voidpf opaque, const char* filename, int mode);
typedef unsigned long    ( *read_file_func)      (voidpf opaque, voidpf stream, void* buf, unsigned long size);
typedef unsigned long    ( *write_file_func)     (voidpf opaque, voidpf stream, const void* buf, unsigned long size);
typedef int      ( *close_file_func)     (voidpf opaque, voidpf stream);
typedef int      ( *testerror_file_func) (voidpf opaque, voidpf stream);

typedef long     ( *tell_file_func)      (voidpf opaque, voidpf stream);
typedef long     ( *seek_file_func)      (voidpf opaque, voidpf stream, unsigned long offset, int origin);


/* here is the "old" 32 bits structure structure */
typedef struct zlib_filefunc_def_s
{
    open_file_func      zopen_file;
    read_file_func      zread_file;
    write_file_func     zwrite_file;
    tell_file_func      ztell_file;
    seek_file_func      zseek_file;
    close_file_func     zclose_file;
    testerror_file_func zerror_file;
    voidpf              opaque;
} zlib_filefunc_def;

typedef ZPOS64_T ( *tell64_file_func)    (voidpf opaque, voidpf stream);
typedef long     ( *seek64_file_func)    (voidpf opaque, voidpf stream, ZPOS64_T offset, int origin);
typedef voidpf   ( *open64_file_func)    (voidpf opaque, const void* filename, int mode);

typedef struct zlib_filefunc64_def_s
{
    open64_file_func    zopen64_file;
    read_file_func      zread_file;
    write_file_func     zwrite_file;
    tell64_file_func    ztell64_file;
    seek64_file_func    zseek64_file;
    close_file_func     zclose_file;
    testerror_file_func zerror_file;
    voidpf              opaque;
} zlib_filefunc64_def;

void fill_fopen64_filefunc (zlib_filefunc64_def* pzlib_filefunc_def);
void fill_fopen_filefunc (zlib_filefunc_def* pzlib_filefunc_def);

/* now internal definition, only for zip.c and unzip.h */
typedef struct zlib_filefunc64_32_def_s
{
    zlib_filefunc64_def zfile_func64;
    open_file_func      zopen32_file;
    tell_file_func      ztell32_file;
    seek_file_func      zseek32_file;
} zlib_filefunc64_32_def;

#define ZREAD(filefunc,filestream,buf,size) ((*((filefunc).zread_file))((filefunc).opaque,filestream,buf,size))
#define ZWRITE(filefunc,filestream,buf,size) ((*((filefunc).zwrite_file))((filefunc).opaque,filestream,buf,size))
#define ZTELL(filefunc,filestream) ((*((filefunc).ztell_file))((filefunc).opaque,filestream))
#define ZSEEK(filefunc,filestream,pos,mode) ((*((filefunc).zseek_file))((filefunc).opaque,filestream,pos,mode))
#define ZCLOSE(filefunc,filestream) ((*((filefunc).zclose_file))((filefunc).opaque,filestream))
#define ZERROR(filefunc,filestream) ((*((filefunc).zerror_file))((filefunc).opaque,filestream))

voidpf call_zopen64 (const zlib_filefunc64_32_def* pfilefunc,const void*filename,int mode);
long    call_zseek64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream, ZPOS64_T offset, int origin);
ZPOS64_T call_ztell64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream);

void    fill_zlib_filefunc64_32_def_from_filefunc32(zlib_filefunc64_32_def* p_filefunc64_32,const zlib_filefunc_def* p_filefunc32);

#define ZOPEN64(filefunc,filename,mode)         (call_zopen64((&(filefunc)),(filename),(mode)))
#define ZTELL64(filefunc,filestream)            (call_ztell64((&(filefunc)),(filestream)))
#define ZSEEK64(filefunc,filestream,pos,mode)   (call_zseek64((&(filefunc)),(filestream),(pos),(mode)))

#define Z_BZIP2ED 12

typedef voidp unzFile;

#define UNZ_OK                          (0)
#define UNZ_END_OF_LIST_OF_FILE         (-100)
#define UNZ_ERRNO                       (Z_ERRNO)
#define UNZ_EOF                         (0)
#define UNZ_PARAMERROR                  (-102)
#define UNZ_PARAMERROR_UL               (-102UL)
#define UNZ_BADZIPFILE                  (-103)
#define UNZ_INTERNALERROR               (-104)
#define UNZ_CRCERROR                    (-105)

/* tm_unz contain date/time info */
typedef struct tm_unz_s
{
    unsigned int tm_sec;            /* seconds after the minute - [0,59] */
    unsigned int tm_min;            /* minutes after the hour - [0,59] */
    unsigned int tm_hour;           /* hours since midnight - [0,23] */
    unsigned int tm_mday;           /* day of the month - [1,31] */
    unsigned int tm_mon;            /* months since January - [0,11] */
    unsigned int tm_year;           /* years - [1980..2044] */
} tm_unz;

/* unz_global_info structure contain global data about the ZIPfile
   These data comes from the end of central dir */
typedef struct unz_global_info64_s
{
    ZPOS64_T number_entry;         /* total number of entries in
                                     the central dir on this disk */
    unsigned long size_comment;         /* size of the global comment of the zipfile */
} unz_global_info64;

typedef struct unz_global_info_s
{
    unsigned long number_entry;         /* total number of entries in
                                     the central dir on this disk */
    unsigned long size_comment;         /* size of the global comment of the zipfile */
} unz_global_info;

/* unz_file_info contain information about a file in the zipfile */
typedef struct unz_file_info64_s
{
    unsigned long version;              /* version made by                 2 bytes */
    unsigned long version_needed;       /* version needed to extract       2 bytes */
    unsigned long flag;                 /* general purpose bit flag        2 bytes */
    unsigned long compression_method;   /* compression method              2 bytes */
    unsigned long dosDate;              /* last mod file date in Dos fmt   4 bytes */
    unsigned long crc;                  /* crc-32                          4 bytes */
    ZPOS64_T compressed_size;   /* compressed size                 8 bytes */
    ZPOS64_T uncompressed_size; /* uncompressed size               8 bytes */
    unsigned long size_filename;        /* filename length                 2 bytes */
    unsigned long size_file_extra;      /* extra field length              2 bytes */
    unsigned long size_file_comment;    /* file comment length             2 bytes */

    unsigned long disk_num_start;       /* disk number start               2 bytes */
    unsigned long internal_fa;          /* internal file attributes        2 bytes */
    unsigned long external_fa;          /* external file attributes        4 bytes */

    tm_unz tmu_date;
} unz_file_info64;

typedef struct unz_file_info_s
{
    unsigned long version;              /* version made by                 2 bytes */
    unsigned long version_needed;       /* version needed to extract       2 bytes */
    unsigned long flag;                 /* general purpose bit flag        2 bytes */
    unsigned long compression_method;   /* compression method              2 bytes */
    unsigned long dosDate;              /* last mod file date in Dos fmt   4 bytes */
    unsigned long crc;                  /* crc-32                          4 bytes */
    unsigned long compressed_size;      /* compressed size                 4 bytes */
    unsigned long uncompressed_size;    /* uncompressed size               4 bytes */
    unsigned long size_filename;        /* filename length                 2 bytes */
    unsigned long size_file_extra;      /* extra field length              2 bytes */
    unsigned long size_file_comment;    /* file comment length             2 bytes */

    unsigned long disk_num_start;       /* disk number start               2 bytes */
    unsigned long internal_fa;          /* internal file attributes        2 bytes */
    unsigned long external_fa;          /* external file attributes        4 bytes */

    tm_unz tmu_date;
} unz_file_info;

extern int  unzStringFileNameCompare (const char* fileName1, const char* fileName2, int iCaseSensitivity);
extern unzFile  unzOpen (const char *path);
extern unzFile  unzOpen64 (const void *path);
extern unzFile  unzOpen2 (const char *path, zlib_filefunc_def* pzlib_filefunc_def);

extern unzFile  unzOpen2_64 (const void *path, zlib_filefunc64_def* pzlib_filefunc_def);
extern int  unzClose (unzFile file);
extern int  unzGetGlobalInfo (unzFile file, unz_global_info *pglobal_info);
extern int  unzGetGlobalInfo64 (unzFile file, unz_global_info64 *pglobal_info);
extern int  unzGetGlobalComment (unzFile file, char *szComment, unsigned long uSizeBuf);
extern int  unzGoToFirstFile (unzFile file);
extern int  unzGoToNextFile (unzFile file);
extern int  unzLocateFile (unzFile file, const char *szFileName, int iCaseSensitivity);

typedef struct unz_file_pos_s
{
    unsigned long pos_in_zip_directory;   /* offset in zip file directory */
    unsigned long num_of_file;            /* # of file */
} unz_file_pos;

extern int  unzGetFilePos(unzFile file, unz_file_pos* file_pos);
extern int  unzGoToFilePos(unzFile file, unz_file_pos* file_pos);

typedef struct unz64_file_pos_s
{
    ZPOS64_T pos_in_zip_directory;   /* offset in zip file directory */
    ZPOS64_T num_of_file;            /* # of file */
} unz64_file_pos;

extern int  unzGetFilePos64(unzFile file, unz64_file_pos* file_pos);
extern int  unzGoToFilePos64(unzFile file, const unz64_file_pos* file_pos);

extern int  unzGetCurrentFileInfo64 (unzFile file,
                         unz_file_info64 *pfile_info,
                         char *szFileName,
                         unsigned long fileNameBufferSize,
                         void *extraField,
                         unsigned long extraFieldBufferSize,
                         char *szComment,
                         unsigned long commentBufferSize);

extern int  unzGetCurrentFileInfo (unzFile file,
                         unz_file_info *pfile_info,
                         char *szFileName,
                         unsigned long fileNameBufferSize,
                         void *extraField,
                         unsigned long extraFieldBufferSize,
                         char *szComment,
                         unsigned long commentBufferSize);

extern ZPOS64_T  unzGetCurrentFileZStreamPos64 (unzFile file);
extern int  unzOpenCurrentFile (unzFile file);
extern int  unzOpenCurrentFilePassword (unzFile file, const char* password);
extern int  unzOpenCurrentFile2 (unzFile file, int* method, int* level, int raw);
extern int  unzOpenCurrentFile3 (unzFile file, int* method, int* level, int raw, const char* password);
extern int  unzCloseCurrentFile (unzFile file);
extern int  unzReadCurrentFile (unzFile file, voidp buf, unsigned len);
extern z_off_t  unztell (unzFile file);
extern ZPOS64_T  unztell64 (unzFile file);
extern int  unzeof (unzFile file);
extern int  unzGetLocalExtrafield (unzFile file, voidp buf, unsigned len);

/* Get the current file offset */
extern ZPOS64_T  unzGetOffset64 (unzFile file);
extern unsigned long  unzGetOffset (unzFile file);

/* Set the current file offset */
extern int  unzSetOffset64 (unzFile file, ZPOS64_T pos);
extern int  unzSetOffset (unzFile file, unsigned long pos);

#if defined(__cplusplus)
}
#endif

#endif /* _RZLIB_H */
