/***************************************************************************

    chd.c

    MAME Compressed Hunks of Data file format

****************************************************************************

    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libchdr/chd.h>
#include <libchdr/minmax.h>
#include <libchdr/cdrom.h>
#include <libchdr/huffman.h>

#ifdef HAVE_FLAC
#include <libchdr/flac.h>
#endif

#ifdef HAVE_7ZIP
#include <libchdr/lzma.h>
#endif

#ifdef HAVE_ZLIB
#include <libchdr/libchdr_zlib.h>
#endif

#include <retro_inline.h>
#include <streams/file_stream.h>

#define TRUE 1
#define FALSE 0

#define CHD_MAKE_TAG(a,b,c,d)       (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

/***************************************************************************
    DEBUGGING
***************************************************************************/

#define PRINTF_MAX_HUNK				(0)

/***************************************************************************
    CONSTANTS
***************************************************************************/

#define MAP_STACK_ENTRIES			512			/* max number of entries to use on the stack */
#define MAP_ENTRY_SIZE				16			/* V3 and later */
#define OLD_MAP_ENTRY_SIZE			8			/* V1-V2 */
#define METADATA_HEADER_SIZE		16			/* metadata header size */

#define CRCMAP_HASH_SIZE			4095		/* number of CRC hashtable entries */

#define MAP_ENTRY_FLAG_TYPE_MASK	0x0f		/* what type of hunk */
#define MAP_ENTRY_FLAG_NO_CRC		0x10		/* no CRC is present */

#define CHD_V1_SECTOR_SIZE			512			/* size of a "sector" in the V1 header */

#define COOKIE_VALUE				0xbaadf00d

#define END_OF_LIST_COOKIE			"EndOfListCookie"

#define NO_MATCH					(~0)

#define MAP_ENTRY_TYPE_INVALID		0x0000		/* invalid type */
#define MAP_ENTRY_TYPE_COMPRESSED	0x0001		/* standard compression */
#define MAP_ENTRY_TYPE_UNCOMPRESSED	0x0002		/* uncompressed data */
#define MAP_ENTRY_TYPE_MINI			0x0003		/* mini: use offset as raw data */
#define MAP_ENTRY_TYPE_SELF_HUNK	   0x0004		/* same as another hunk in this file */
#define MAP_ENTRY_TYPE_PARENT_HUNK	0x0005		/* same as a hunk in the parent file */
#define MAP_ENTRY_TYPE_2ND_COMPRESSED 0x0006    /* compressed with secondary algorithm (usually FLAC CDDA) */

#ifdef WANT_RAW_DATA_SECTOR
const uint8_t s_cd_sync_header[12] = { 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00 };
#endif

/* V5 compression types */
enum
{
	/* codec #0
	 * these types are live when running */
	COMPRESSION_TYPE_0 = 0,
	/* codec #1 */
	COMPRESSION_TYPE_1 = 1,
	/* codec #2 */
	COMPRESSION_TYPE_2 = 2,
	/* codec #3 */
	COMPRESSION_TYPE_3 = 3,
	/* no compression; implicit length = hunkbytes */
	COMPRESSION_NONE = 4,
	/* same as another block in this chd */
	COMPRESSION_SELF = 5,
	/* same as a hunk's worth of units in the parent chd */
	COMPRESSION_PARENT = 6,

	/* start of small RLE run (4-bit length)
	 * these additional pseudo-types are used for compressed encodings: */
	COMPRESSION_RLE_SMALL,
	/* start of large RLE run (8-bit length) */
	COMPRESSION_RLE_LARGE,
	/* same as the last COMPRESSION_SELF block */
	COMPRESSION_SELF_0,
	/* same as the last COMPRESSION_SELF block + 1 */
	COMPRESSION_SELF_1,
	/* same block in the parent */
	COMPRESSION_PARENT_SELF,
	/* same as the last COMPRESSION_PARENT block */
	COMPRESSION_PARENT_0,
	/* same as the last COMPRESSION_PARENT block + 1 */
	COMPRESSION_PARENT_1
};

/***************************************************************************
    MACROS
***************************************************************************/

#define SET_ERROR_AND_CLEANUP(err) do { last_error = (err); goto cleanup; } while (0)

#define EARLY_EXIT(x)				do { (void)(x); goto cleanup; } while (0)

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

/* interface to a codec */
typedef struct _codec_interface codec_interface;
struct _codec_interface
{
	UINT32		compression;								/* type of compression */
	const char *compname;									/* name of the algorithm */
	UINT8		lossy;										/* is this a lossy algorithm? */
	chd_error	(*init)(void *codec, UINT32 hunkbytes);		/* codec initialize */
	void		(*free)(void *codec);						/* codec free */
	chd_error	(*decompress)(void *codec, const uint8_t *src, uint32_t complen, uint8_t *dest, uint32_t destlen); /* decompress data */
	chd_error	(*config)(void *codec, int param, void *config); /* configure */
};

/* a single map entry */
typedef struct _map_entry map_entry;
struct _map_entry
{
	UINT64					offset;			/* offset within the file of the data */
	UINT32					crc;			/* 32-bit CRC of the data */
	UINT32					length;			/* length of the data */
	UINT8					flags;			/* misc flags */
};

/* a single metadata entry */
typedef struct _metadata_entry metadata_entry;
struct _metadata_entry
{
	UINT64					offset;			/* offset within the file of the header */
	UINT64					next;			/* offset within the file of the next header */
	UINT64					prev;			/* offset within the file of the previous header */
	UINT32					length;			/* length of the metadata */
	UINT32					metatag;		/* metadata tag */
	UINT8					flags;			/* flag bits */
};

/* internal representation of an open CHD file */
struct _chd_file
{
	UINT32					cookie;			/* cookie, should equal COOKIE_VALUE */

	RFILE *				file;			/* handle to the open core file */
	UINT8					owns_file;		/* flag indicating if this file should be closed on chd_close() */
	chd_header				header;			/* header, extracted from file */

	chd_file *				parent;			/* pointer to parent file, or NULL */

	map_entry *				map;			/* array of map entries */

#ifdef NEED_CACHE_HUNK
	UINT8 *					cache;			/* hunk cache pointer */
	UINT32					cachehunk;		/* index of currently cached hunk */

	UINT8 *					compare;		/* hunk compare pointer */
	UINT32					comparehunk;	/* index of current compare data */
#endif

	UINT8 *					compressed;		/* pointer to buffer for compressed data */
	const codec_interface *	codecintf[4];	/* interface to the codec */

#ifdef HAVE_ZLIB
	zlib_codec_data			zlib_codec_data;		/* zlib codec data */
	cdzl_codec_data			cdzl_codec_data;		/* cdzl codec data */
#endif
#ifdef HAVE_7ZIP
	cdlz_codec_data			cdlz_codec_data;		/* cdlz codec data */
#endif
#ifdef HAVE_FLAC
	cdfl_codec_data			cdfl_codec_data;		/* cdfl codec data */
#endif

#ifdef NEED_CACHE_HUNK
	UINT32					maxhunk;		/* maximum hunk accessed */
#endif
   UINT8 *              file_cache; /* cache of underlying file */
};

/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

static const UINT8 nullmd5[CHD_MD5_BYTES] = { 0 };
static const UINT8 nullsha1[CHD_SHA1_BYTES] = { 0 };

/***************************************************************************
    PROTOTYPES
***************************************************************************/

/* internal header operations */
static chd_error header_validate(const chd_header *header);
static chd_error header_read(chd_file *chd, chd_header *header);

/* internal hunk read/write */
#ifdef NEED_CACHE_HUNK
static chd_error hunk_read_into_cache(chd_file *chd, UINT32 hunknum);
#endif
static chd_error hunk_read_into_memory(chd_file *chd, UINT32 hunknum, UINT8 *dest);

/* internal map access */
static chd_error map_read(chd_file *chd);

/* metadata management */
static chd_error metadata_find_entry(chd_file *chd, UINT32 metatag, UINT32 metaindex, metadata_entry *metaentry);

/***************************************************************************
    CODEC INTERFACES
***************************************************************************/

#define CHD_MAKE_TAG(a,b,c,d)       (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

/* general codecs with CD frontend */
#define CHD_CODEC_CD_ZLIB CHD_MAKE_TAG('c','d','z','l')
#define CHD_CODEC_CD_LZMA CHD_MAKE_TAG('c','d','l','z')
#define CHD_CODEC_CD_FLAC CHD_MAKE_TAG('c','d','f','l')

static const codec_interface codec_interfaces[] =
{
	/* "none" or no compression */
	{
		CHDCOMPRESSION_NONE,
		"none",
		FALSE,
		NULL,
		NULL,
		NULL,
		NULL
	},

#ifdef HAVE_ZLIB
	/* standard zlib compression */
	{
		CHDCOMPRESSION_ZLIB,
		"zlib",
		FALSE,
		zlib_codec_init,
		zlib_codec_free,
		zlib_codec_decompress,
		NULL
	},

	/* zlib+ compression */
	{
		CHDCOMPRESSION_ZLIB_PLUS,
		"zlib+",
		FALSE,
		zlib_codec_init,
		zlib_codec_free,
		zlib_codec_decompress,
		NULL
	},

	/* V5 CD zlib compression */
	{
		CHD_CODEC_CD_ZLIB,
		"cdzl (CD Deflate)",
		FALSE,
		cdzl_codec_init,
		cdzl_codec_free,
		cdzl_codec_decompress,
		NULL
	},
#endif

#ifdef HAVE_7ZIP
	/* V5 CD lzma compression */
	{
		CHD_CODEC_CD_LZMA,
		"cdlz (CD LZMA)",
		FALSE,
		cdlz_codec_init,
		cdlz_codec_free,
		cdlz_codec_decompress,
		NULL
	},
#endif

#ifdef HAVE_FLAC
	/* V5 CD flac compression */
	{
		CHD_CODEC_CD_FLAC,
		"cdfl (CD FLAC)",
		FALSE,
		cdfl_codec_init,
		cdfl_codec_free,
		cdfl_codec_decompress,
		NULL
	},
#endif
};

/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    get_bigendian_uint64 - fetch a UINT64 from
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE UINT64 get_bigendian_uint64(const UINT8 *base)
{
	return ((UINT64)base[0] << 56) | ((UINT64)base[1] << 48) | ((UINT64)base[2] << 40) | ((UINT64)base[3] << 32) |
			((UINT64)base[4] << 24) | ((UINT64)base[5] << 16) | ((UINT64)base[6] << 8) | (UINT64)base[7];
}

/*-------------------------------------------------
    put_bigendian_uint64 - write a UINT64 to
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE void put_bigendian_uint64(UINT8 *base, UINT64 value)
{
	base[0] = value >> 56;
	base[1] = value >> 48;
	base[2] = value >> 40;
	base[3] = value >> 32;
	base[4] = value >> 24;
	base[5] = value >> 16;
	base[6] = value >> 8;
	base[7] = value;
}

/*-------------------------------------------------
    get_bigendian_uint48 - fetch a UINT48 from
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE UINT64 get_bigendian_uint48(const UINT8 *base)
{
	return  ((UINT64)base[0] << 40) | ((UINT64)base[1] << 32) |
			((UINT64)base[2] << 24) | ((UINT64)base[3] << 16) | ((UINT64)base[4] << 8) | (UINT64)base[5];
}

/*-------------------------------------------------
    put_bigendian_uint48 - write a UINT48 to
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE void put_bigendian_uint48(UINT8 *base, UINT64 value)
{
	value &= 0xffffffffffff;
	base[0] = value >> 40;
	base[1] = value >> 32;
	base[2] = value >> 24;
	base[3] = value >> 16;
	base[4] = value >> 8;
	base[5] = value;
}
/*-------------------------------------------------
    get_bigendian_uint32 - fetch a UINT32 from
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE UINT32 get_bigendian_uint32(const UINT8 *base)
{
	return (base[0] << 24) | (base[1] << 16) | (base[2] << 8) | base[3];
}

/*-------------------------------------------------
    put_bigendian_uint32 - write a UINT32 to
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE void put_bigendian_uint24(UINT8 *base, UINT32 value)
{
	value &= 0xffffff;
	base[0] = value >> 16;
	base[1] = value >> 8;
	base[2] = value;
}

/*-------------------------------------------------
    put_bigendian_uint24 - write a UINT24 to
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE void put_bigendian_uint32(UINT8 *base, UINT32 value)
{
	value &= 0xffffff;
	base[0] = value >> 16;
	base[1] = value >> 8;
	base[2] = value;
}

/*-------------------------------------------------
    get_bigendian_uint24 - fetch a UINT24 from
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE UINT32 get_bigendian_uint24(const UINT8 *base)
{
	return (base[0] << 16) | (base[1] << 8) | base[2];
}

/*-------------------------------------------------
    get_bigendian_uint16 - fetch a UINT16 from
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE UINT16 get_bigendian_uint16(const UINT8 *base)
{
	return (base[0] << 8) | base[1];
}

/*-------------------------------------------------
    put_bigendian_uint16 - write a UINT16 to
    the data stream in bigendian order
-------------------------------------------------*/

static INLINE void put_bigendian_uint16(UINT8 *base, UINT16 value)
{
	base[0] = value >> 8;
	base[1] = value;
}

/*-------------------------------------------------
    map_extract - extract a single map
    entry from the datastream
-------------------------------------------------*/

static INLINE void map_extract(const UINT8 *base, map_entry *entry)
{
	entry->offset = get_bigendian_uint64(&base[0]);
	entry->crc = get_bigendian_uint32(&base[8]);
	entry->length = get_bigendian_uint16(&base[12]) | (base[14] << 16);
	entry->flags = base[15];
}

/*-------------------------------------------------
    map_assemble - write a single map
    entry to the datastream
-------------------------------------------------*/

static INLINE void map_assemble(UINT8 *base, map_entry *entry)
{
	put_bigendian_uint64(&base[0], entry->offset);
	put_bigendian_uint32(&base[8], entry->crc);
	put_bigendian_uint16(&base[12], entry->length);
	base[14] = entry->length >> 16;
	base[15] = entry->flags;
}

/*-------------------------------------------------
    map_size_v5 - calculate CHDv5 map size
-------------------------------------------------*/
static INLINE int map_size_v5(chd_header* header)
{
	return header->hunkcount * header->mapentrybytes;
}

/*-------------------------------------------------
    crc16 - calculate CRC16 (from hashing.cpp)
-------------------------------------------------*/
uint16_t crc16(const void *data, uint32_t length)
{
	uint16_t crc = 0xffff;

	static const uint16_t s_table[256] =
	{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
		0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
		0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
		0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
		0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
		0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
		0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
		0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
		0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
		0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
		0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
		0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
		0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
		0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
		0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
		0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
		0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
		0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
		0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
		0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
		0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
		0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
		0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
		0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
		0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
		0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
	};

	const uint8_t *src = (uint8_t*)data;

	/* fetch the current value into a local and rip through the source data */
	while (length-- != 0)
		crc = (crc << 8) ^ s_table[(crc >> 8) ^ *src++];
	return crc;
}

/*-------------------------------------------------
	decompress_v5_map - decompress the v5 map
-------------------------------------------------*/

static chd_error decompress_v5_map(chd_file* chd, chd_header* header)
{
	uint8_t rawbuf[16];
   uint16_t mapcrc;
   uint32_t mapbytes;
   uint64_t firstoffs;
	uint32_t last_self = 0;
	uint64_t last_parent = 0;
	uint8_t lastcomp = 0;
	int hunknum, repcount = 0;
   enum huffman_error err;
   uint8_t lengthbits, selfbits, parentbits;
   uint8_t* compressed;
   struct bitstream* bitbuf;
   struct huffman_decoder* decoder;
   uint64_t curoffset;
	if (header->mapoffset == 0)
	{
#if 0
		memset(header->rawmap, 0xff,map_size_v5(header));
#endif
		return CHDERR_READ_ERROR;
	}

	/* read the reader */
	filestream_seek(chd->file, header->mapoffset, SEEK_SET);
	filestream_read(chd->file, rawbuf, sizeof(rawbuf));
	mapbytes = get_bigendian_uint32(&rawbuf[0]);
	firstoffs = get_bigendian_uint48(&rawbuf[4]);
	mapcrc = get_bigendian_uint16(&rawbuf[10]);
	lengthbits = rawbuf[12];
	selfbits = rawbuf[13];
	parentbits = rawbuf[14];

	/* now read the map */
	compressed = (uint8_t*)malloc(sizeof(uint8_t) * mapbytes);
	if (compressed == NULL)
		return CHDERR_OUT_OF_MEMORY;

	filestream_seek(chd->file, header->mapoffset + 16, SEEK_SET);
	filestream_read(chd->file, compressed, mapbytes);
	bitbuf = create_bitstream(compressed, sizeof(uint8_t) * mapbytes);
	if (bitbuf == NULL)
	{
		free(compressed);
		return CHDERR_OUT_OF_MEMORY;
	}

	header->rawmap = (uint8_t*)malloc(sizeof(uint8_t) * map_size_v5(header));
	if (header->rawmap == NULL)
	{
		free(compressed);
		free(bitbuf);
		return CHDERR_OUT_OF_MEMORY;
	}

	/* first decode the compression types */
	decoder = create_huffman_decoder(16, 8);
	if (decoder == NULL)
	{
		free(compressed);
		free(bitbuf);
		return CHDERR_OUT_OF_MEMORY;
	}

	err = huffman_import_tree_rle(decoder, bitbuf);
	if (err != HUFFERR_NONE)
	{
		free(compressed);
		free(bitbuf);
		delete_huffman_decoder(decoder);
		return CHDERR_DECOMPRESSION_ERROR;
	}

	for (hunknum = 0; hunknum < header->hunkcount; hunknum++)
	{
		uint8_t *rawmap = header->rawmap + (hunknum * 12);
		if (repcount > 0)
        {
            rawmap[0] = lastcomp;
            repcount--;
        }
		else
		{
			uint8_t val = huffman_decode_one(decoder, bitbuf);
			if (val == COMPRESSION_RLE_SMALL)
            {
                rawmap[0] = lastcomp;
                repcount = 2 + huffman_decode_one(decoder, bitbuf);
            }
			else if (val == COMPRESSION_RLE_LARGE)
            {
                rawmap[0] = lastcomp;
                repcount = 2 + 16 + (huffman_decode_one(decoder, bitbuf) << 4);
                repcount += huffman_decode_one(decoder, bitbuf);
            }
			else
				rawmap[0] = lastcomp = val;
		}
	}

	/* then iterate through the hunks and extract the needed data */
	curoffset = firstoffs;
	for (hunknum = 0; hunknum < header->hunkcount; hunknum++)
	{
		uint8_t *rawmap = header->rawmap + (hunknum * 12);
		uint64_t offset = curoffset;
		uint32_t length = 0;
		uint16_t crc = 0;
		switch (rawmap[0])
		{
			/* base types */
			case COMPRESSION_TYPE_0:
			case COMPRESSION_TYPE_1:
			case COMPRESSION_TYPE_2:
			case COMPRESSION_TYPE_3:
				curoffset += length = bitstream_read(bitbuf, lengthbits);
				crc = bitstream_read(bitbuf, 16);
				break;

			case COMPRESSION_NONE:
				curoffset += length = header->hunkbytes;
				crc = bitstream_read(bitbuf, 16);
				break;

			case COMPRESSION_SELF:
				last_self = offset = bitstream_read(bitbuf, selfbits);
				break;

			case COMPRESSION_PARENT:
				offset = bitstream_read(bitbuf, parentbits);
				last_parent = offset;
				break;

			/* pseudo-types; convert into base types */
			case COMPRESSION_SELF_1:
				last_self++;
			case COMPRESSION_SELF_0:
				rawmap[0] = COMPRESSION_SELF;
				offset = last_self;
				break;

			case COMPRESSION_PARENT_SELF:
				rawmap[0] = COMPRESSION_PARENT;
				last_parent = offset = ( ((uint64_t)hunknum) * ((uint64_t)header->hunkbytes) ) / header->unitbytes;
				break;

			case COMPRESSION_PARENT_1:
				last_parent += header->hunkbytes / header->unitbytes;
			case COMPRESSION_PARENT_0:
				rawmap[0] = COMPRESSION_PARENT;
				offset = last_parent;
				break;
		}
		/* UINT24 length */
		put_bigendian_uint24(&rawmap[1], length);

		/* UINT48 offset */
		put_bigendian_uint48(&rawmap[4], offset);

		/* crc16 */
		put_bigendian_uint16(&rawmap[10], crc);
	}

	/* free memory */
	free(compressed);
	free(bitbuf);
	delete_huffman_decoder(decoder);

	/* verify the final CRC */
	if (crc16(&header->rawmap[0], header->hunkcount * 12) != mapcrc)
		return CHDERR_DECOMPRESSION_ERROR;

	return CHDERR_NONE;
}

/*-------------------------------------------------
    map_extract_old - extract a single map
    entry in old format from the datastream
-------------------------------------------------*/

static INLINE void map_extract_old(const UINT8 *base, map_entry *entry, UINT32 hunkbytes)
{
	entry->offset = get_bigendian_uint64(&base[0]);
	entry->crc = 0;
	entry->length = entry->offset >> 44;
	entry->flags = MAP_ENTRY_FLAG_NO_CRC | ((entry->length == hunkbytes) ? MAP_ENTRY_TYPE_UNCOMPRESSED : MAP_ENTRY_TYPE_COMPRESSED);
#ifdef __MWERKS__
	entry->offset = entry->offset & 0x00000FFFFFFFFFFFLL;
#else
	entry->offset = (entry->offset << 20) >> 20;
#endif
}

/***************************************************************************
    CHD FILE MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    chd_open_file - open a CHD file for access
-------------------------------------------------*/

chd_error chd_open_file(RFILE *file, int mode, chd_file *parent, chd_file **chd)
{
	chd_file *newchd = NULL;
	chd_error err;
	int intfnum;

	/* verify parameters */
	if (file == NULL)
		EARLY_EXIT(err = CHDERR_INVALID_PARAMETER);

	/* punt if invalid parent */
	if (parent != NULL && parent->cookie != COOKIE_VALUE)
		EARLY_EXIT(err = CHDERR_INVALID_PARAMETER);

	/* allocate memory for the final result */
	newchd = (chd_file *)malloc(sizeof(**chd));
	if (newchd == NULL)
		EARLY_EXIT(err = CHDERR_OUT_OF_MEMORY);
	memset(newchd, 0, sizeof(*newchd));
	newchd->cookie = COOKIE_VALUE;
	newchd->parent = parent;
	newchd->file = file;

	/* now attempt to read the header */
	err = header_read(newchd, &newchd->header);
	if (err != CHDERR_NONE)
		EARLY_EXIT(err);

	/* validate the header */
	err = header_validate(&newchd->header);
	if (err != CHDERR_NONE)
		EARLY_EXIT(err);

	/* make sure we don't open a read-only file writeable */
	if (mode == CHD_OPEN_READWRITE && !(newchd->header.flags & CHDFLAGS_IS_WRITEABLE))
		EARLY_EXIT(err = CHDERR_FILE_NOT_WRITEABLE);

	/* also, never open an older version writeable */
	if (mode == CHD_OPEN_READWRITE && newchd->header.version < CHD_HEADER_VERSION)
		EARLY_EXIT(err = CHDERR_UNSUPPORTED_VERSION);

	/* if we need a parent, make sure we have one */
	if (parent == NULL && (newchd->header.flags & CHDFLAGS_HAS_PARENT))
		EARLY_EXIT(err = CHDERR_REQUIRES_PARENT);

	/* make sure we have a valid parent */
	if (parent != NULL)
	{
		/* check MD5 if it isn't empty */
		if (memcmp(nullmd5, newchd->header.parentmd5, sizeof(newchd->header.parentmd5)) != 0 &&
			memcmp(nullmd5, newchd->parent->header.md5, sizeof(newchd->parent->header.md5)) != 0 &&
			memcmp(newchd->parent->header.md5, newchd->header.parentmd5, sizeof(newchd->header.parentmd5)) != 0)
			EARLY_EXIT(err = CHDERR_INVALID_PARENT);

		/* check SHA1 if it isn't empty */
		if (memcmp(nullsha1, newchd->header.parentsha1, sizeof(newchd->header.parentsha1)) != 0 &&
			memcmp(nullsha1, newchd->parent->header.sha1, sizeof(newchd->parent->header.sha1)) != 0 &&
			memcmp(newchd->parent->header.sha1, newchd->header.parentsha1, sizeof(newchd->header.parentsha1)) != 0)
			EARLY_EXIT(err = CHDERR_INVALID_PARENT);
	}

	/* now read the hunk map */
	if (newchd->header.version < 5)
	{
		err = map_read(newchd);
		if (err != CHDERR_NONE)
			EARLY_EXIT(err);
	}
	else
	{
		err = decompress_v5_map(newchd, &(newchd->header));
        (void)err;
	}

#ifdef NEED_CACHE_HUNK
	/* allocate and init the hunk cache */
	newchd->cache = (UINT8 *)malloc(newchd->header.hunkbytes);
	newchd->compare = (UINT8 *)malloc(newchd->header.hunkbytes);
	if (newchd->cache == NULL || newchd->compare == NULL)
		EARLY_EXIT(err = CHDERR_OUT_OF_MEMORY);
	newchd->cachehunk = ~0;
	newchd->comparehunk = ~0;
#endif

	/* allocate the temporary compressed buffer */
	newchd->compressed = (UINT8 *)malloc(newchd->header.hunkbytes);
	if (newchd->compressed == NULL)
		EARLY_EXIT(err = CHDERR_OUT_OF_MEMORY);

	/* find the codec interface */
	if (newchd->header.version < 5)
	{
		for (intfnum = 0; intfnum < ARRAY_SIZE(codec_interfaces); intfnum++)
			if (codec_interfaces[intfnum].compression == newchd->header.compression[0])
			{
				newchd->codecintf[0] = &codec_interfaces[intfnum];
				break;
			}
		if (intfnum == ARRAY_SIZE(codec_interfaces))
			EARLY_EXIT(err = CHDERR_UNSUPPORTED_FORMAT);

#ifdef HAVE_ZLIB
		/* initialize the codec */
		if (newchd->codecintf[0]->init != NULL)
      {
         err = (*newchd->codecintf[0]->init)(&newchd->zlib_codec_data, newchd->header.hunkbytes);
         (void)err;
      }
#endif
	}
	else
	{
		int i, decompnum;
		/* verify the compression types and initialize the codecs */
		for (decompnum = 0; decompnum < ARRAY_SIZE(newchd->header.compression); decompnum++)
		{
			for (i = 0 ; i < ARRAY_SIZE(codec_interfaces) ; i++)
			{
				if (codec_interfaces[i].compression == newchd->header.compression[decompnum])
				{
					newchd->codecintf[decompnum] = &codec_interfaces[i];
					if (newchd->codecintf[decompnum] == NULL && newchd->header.compression[decompnum] != 0)
                    {
						err = CHDERR_UNSUPPORTED_FORMAT;
                        (void)err;
                    }

					/* initialize the codec */
					if (newchd->codecintf[decompnum]->init != NULL)
					{
						void* codec = NULL;
						switch (newchd->header.compression[decompnum])
						{
							case CHD_CODEC_CD_ZLIB:
#ifdef HAVE_ZLIB
								codec = &newchd->cdzl_codec_data;
#endif
								break;

							case CHD_CODEC_CD_LZMA:
#ifdef HAVE_7ZIP
								codec = &newchd->cdlz_codec_data;
#endif
								break;

							case CHD_CODEC_CD_FLAC:
#ifdef HAVE_FLAC
								codec = &newchd->cdfl_codec_data;
#endif
								break;
						}
						if (codec != NULL)
                        {
							err = (*newchd->codecintf[decompnum]->init)(codec, newchd->header.hunkbytes);
                            (void)err;
                        }
					}

				}
			}
		}
	}

#if 0
	/* HACK */
	if (err != CHDERR_NONE)
		EARLY_EXIT(err);
#endif

	/* all done */
	*chd = newchd;
	return CHDERR_NONE;

cleanup:
	if (newchd != NULL)
		chd_close(newchd);
	return err;
}

/*-------------------------------------------------
    chd_open - open a CHD file by
    filename
-------------------------------------------------*/

chd_error chd_open(const char *filename, int mode, chd_file *parent, chd_file **chd)
{
	chd_error err;
	RFILE *file = NULL;

	/* choose the proper mode */
	switch(mode)
	{
		case CHD_OPEN_READ:
			break;

		default:
			err = CHDERR_INVALID_PARAMETER;
			goto cleanup;
	}

	/* open the file */
	file = filestream_open(filename,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (!file)
	{
		err = CHDERR_FILE_NOT_FOUND;
		goto cleanup;
	}

	/* now open the CHD */
	err = chd_open_file(file, mode, parent, chd);
	if (err != CHDERR_NONE)
		goto cleanup;

	/* we now own this file */
	(*chd)->owns_file = TRUE;

cleanup:
	if ((err != CHDERR_NONE) && (file != NULL))
		filestream_close(file);
	return err;
}

chd_error chd_precache(chd_file *chd)
{
	int64_t size, count;

	if (!chd->file_cache)
	{
		filestream_seek(chd->file, 0, SEEK_END);
		size = filestream_tell(chd->file);
		if (size <= 0)
			return CHDERR_INVALID_DATA;
		chd->file_cache = (UINT8*)malloc(size);
		if (chd->file_cache == NULL)
			return CHDERR_OUT_OF_MEMORY;
		filestream_seek(chd->file, 0, SEEK_SET);
		count = filestream_read(chd->file, chd->file_cache, size);
		if (count != size)
		{
			free(chd->file_cache);
			chd->file_cache = NULL;
			return CHDERR_READ_ERROR;
		}
	}

	return CHDERR_NONE;
}

/*-------------------------------------------------
    chd_close - close a CHD file for access
-------------------------------------------------*/

void chd_close(chd_file *chd)
{
	/* punt if NULL or invalid */
	if (chd == NULL || chd->cookie != COOKIE_VALUE)
		return;

	/* deinit the codec */
	if (chd->header.version < 5)
	{
#ifdef HAVE_ZLIB
		if (chd->codecintf[0] != NULL && chd->codecintf[0]->free != NULL)
			(*chd->codecintf[0]->free)(&chd->zlib_codec_data);
#endif
	}
	else
	{
		int i;
		/* Free the codecs */
		for (i = 0 ; i < 4 ; i++)
      {
         void* codec = NULL;
         if (!chd->codecintf[i])
            continue;

         switch (chd->codecintf[i]->compression)
         {
            case CHD_CODEC_CD_LZMA:
#ifdef HAVE_7ZIP
               codec = &chd->cdlz_codec_data;
#endif
               break;

            case CHD_CODEC_CD_ZLIB:
#ifdef HAVE_ZLIB
               codec = &chd->cdzl_codec_data;
#endif
               break;

            case CHD_CODEC_CD_FLAC:
#ifdef HAVE_FLAC
               codec = &chd->cdfl_codec_data;
#endif
               break;
         }
         if (codec)
            (*chd->codecintf[i]->free)(codec);
      }

		/* Free the raw map */
		if (chd->header.rawmap != NULL)
			free(chd->header.rawmap);
	}

	/* free the compressed data buffer */
	if (chd->compressed != NULL)
		free(chd->compressed);

#ifdef NEED_CACHE_HUNK
	/* free the hunk cache and compare data */
	if (chd->compare != NULL)
		free(chd->compare);
	if (chd->cache != NULL)
		free(chd->cache);
#endif

	/* free the hunk map */
	if (chd->map != NULL)
		free(chd->map);

	/* close the file */
	if (chd->owns_file && chd->file != NULL)
		filestream_close(chd->file);

#ifdef NEED_CACHE_HUNK
	if (PRINTF_MAX_HUNK) printf("Max hunk = %d/%d\n", chd->maxhunk, chd->header.totalhunks);
#endif

   if (chd->file_cache)
      free(chd->file_cache);

	/* free our memory */
	free(chd);
}

/*-------------------------------------------------
    chd_core_file - return the associated
    core_file
-------------------------------------------------*/

RFILE *chd_core_file(chd_file *chd)
{
	return chd->file;
}

/*-------------------------------------------------
    chd_error_string - return an error string for
    the given CHD error
-------------------------------------------------*/

const char *chd_error_string(chd_error err)
{
	switch (err)
	{
		case CHDERR_NONE:						return "no error";
		case CHDERR_NO_INTERFACE:				return "no drive interface";
		case CHDERR_OUT_OF_MEMORY:				return "out of memory";
		case CHDERR_INVALID_FILE:				return "invalid file";
		case CHDERR_INVALID_PARAMETER:			return "invalid parameter";
		case CHDERR_INVALID_DATA:				return "invalid data";
		case CHDERR_FILE_NOT_FOUND:				return "file not found";
		case CHDERR_REQUIRES_PARENT:			return "requires parent";
		case CHDERR_FILE_NOT_WRITEABLE:			return "file not writeable";
		case CHDERR_READ_ERROR:					return "read error";
		case CHDERR_WRITE_ERROR:				return "write error";
		case CHDERR_CODEC_ERROR:				return "codec error";
		case CHDERR_INVALID_PARENT:				return "invalid parent";
		case CHDERR_HUNK_OUT_OF_RANGE:			return "hunk out of range";
		case CHDERR_DECOMPRESSION_ERROR:		return "decompression error";
		case CHDERR_COMPRESSION_ERROR:			return "compression error";
		case CHDERR_CANT_CREATE_FILE:			return "can't create file";
		case CHDERR_CANT_VERIFY:				return "can't verify file";
		case CHDERR_NOT_SUPPORTED:				return "operation not supported";
		case CHDERR_METADATA_NOT_FOUND:			return "can't find metadata";
		case CHDERR_INVALID_METADATA_SIZE:		return "invalid metadata size";
		case CHDERR_UNSUPPORTED_VERSION:		return "unsupported CHD version";
		case CHDERR_VERIFY_INCOMPLETE:			return "incomplete verify";
		case CHDERR_INVALID_METADATA:			return "invalid metadata";
		case CHDERR_INVALID_STATE:				return "invalid state";
		case CHDERR_OPERATION_PENDING:			return "operation pending";
		case CHDERR_NO_ASYNC_OPERATION:			return "no async operation in progress";
		case CHDERR_UNSUPPORTED_FORMAT:			return "unsupported format";
		default:								return "undocumented error";
	}
}

/***************************************************************************
    CHD HEADER MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    chd_get_header - return a pointer to the
    extracted header data
-------------------------------------------------*/

const chd_header *chd_get_header(chd_file *chd)
{
	/* punt if NULL or invalid */
	if (chd == NULL || chd->cookie != COOKIE_VALUE)
		return NULL;

	return &chd->header;
}

/***************************************************************************
    CORE DATA READ/WRITE
***************************************************************************/

/*-------------------------------------------------
    chd_read - read a single hunk from the CHD
    file
-------------------------------------------------*/

chd_error chd_read(chd_file *chd, UINT32 hunknum, void *buffer)
{
	/* punt if NULL or invalid */
	if (chd == NULL || chd->cookie != COOKIE_VALUE)
		return CHDERR_INVALID_PARAMETER;

	/* perform the read */
	return hunk_read_into_memory(chd, hunknum, (UINT8 *)buffer);
}

/***************************************************************************
    METADATA MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    chd_get_metadata - get the indexed metadata
    of the given type
-------------------------------------------------*/

chd_error chd_get_metadata(chd_file *chd, UINT32 searchtag, UINT32 searchindex, void *output, UINT32 outputlen, UINT32 *resultlen, UINT32 *resulttag, UINT8 *resultflags)
{
	metadata_entry metaentry;
	chd_error err;
	int64_t count;

	/* if we didn't find it, just return */
	err = metadata_find_entry(chd, searchtag, searchindex, &metaentry);
	if (err != CHDERR_NONE)
	{
		/* unless we're an old version and they are requesting hard disk metadata */
		if (chd->header.version < 3 && (searchtag == HARD_DISK_METADATA_TAG || searchtag == CHDMETATAG_WILDCARD) && searchindex == 0)
		{
			char faux_metadata[256];
			UINT32 faux_length;

			/* fill in the faux metadata */
			sprintf(faux_metadata, HARD_DISK_METADATA_FORMAT, chd->header.obsolete_cylinders, chd->header.obsolete_heads, chd->header.obsolete_sectors, chd->header.hunkbytes / chd->header.obsolete_hunksize);
			faux_length = (UINT32)strlen(faux_metadata) + 1;

			/* copy the metadata itself */
			memcpy(output, faux_metadata, MIN(outputlen, faux_length));

			/* return the length of the data and the tag */
			if (resultlen != NULL)
				*resultlen = faux_length;
			if (resulttag != NULL)
				*resulttag = HARD_DISK_METADATA_TAG;
			return CHDERR_NONE;
		}
		return err;
	}

	/* read the metadata */
	outputlen = MIN(outputlen, metaentry.length);
	filestream_seek(chd->file, metaentry.offset + METADATA_HEADER_SIZE, SEEK_SET);
	count = filestream_read(chd->file, output, outputlen);
	if (count != outputlen)
		return CHDERR_READ_ERROR;

	/* return the length of the data and the tag */
	if (resultlen != NULL)
		*resultlen = metaentry.length;
	if (resulttag != NULL)
		*resulttag = metaentry.metatag;
	if (resultflags != NULL)
		*resultflags = metaentry.flags;
	return CHDERR_NONE;
}

/***************************************************************************
    CODEC INTERFACES
***************************************************************************/

/*-------------------------------------------------
    chd_codec_config - set internal codec
    parameters
-------------------------------------------------*/

chd_error chd_codec_config(chd_file *chd, int param, void *config)
{
	return CHDERR_INVALID_PARAMETER;
}

/*-------------------------------------------------
    chd_get_codec_name - get the name of a
    particular codec
-------------------------------------------------*/

const char *chd_get_codec_name(UINT32 codec)
{
	return "Unknown";
}

/***************************************************************************
    INTERNAL HEADER OPERATIONS
***************************************************************************/

/*-------------------------------------------------
    header_validate - check the validity of a
    CHD header
-------------------------------------------------*/

static chd_error header_validate(const chd_header *header)
{
	int intfnum;

	/* require a valid version */
	if (header->version == 0 || header->version > CHD_HEADER_VERSION)
		return CHDERR_UNSUPPORTED_VERSION;

	/* require a valid length */
	if ((header->version == 1 && header->length != CHD_V1_HEADER_SIZE) ||
		(header->version == 2 && header->length != CHD_V2_HEADER_SIZE) ||
		(header->version == 3 && header->length != CHD_V3_HEADER_SIZE) ||
		(header->version == 4 && header->length != CHD_V4_HEADER_SIZE) ||
		(header->version == 5 && header->length != CHD_V5_HEADER_SIZE))
		return CHDERR_INVALID_PARAMETER;

	/* Do not validate v5 header */
	if (header->version <= 4)
	{
		/* require valid flags */
		if (header->flags & CHDFLAGS_UNDEFINED)
			return CHDERR_INVALID_PARAMETER;

		/* require a supported compression mechanism */
		for (intfnum = 0; intfnum < ARRAY_SIZE(codec_interfaces); intfnum++)
			if (codec_interfaces[intfnum].compression == header->compression[0])
				break;

		if (intfnum == ARRAY_SIZE(codec_interfaces))
			return CHDERR_INVALID_PARAMETER;

		/* require a valid hunksize */
		if (header->hunkbytes == 0 || header->hunkbytes >= 65536 * 256)
			return CHDERR_INVALID_PARAMETER;

		/* require a valid hunk count */
		if (header->totalhunks == 0)
			return CHDERR_INVALID_PARAMETER;

		/* require a valid MD5 and/or SHA1 if we're using a parent */
		if ((header->flags & CHDFLAGS_HAS_PARENT) && memcmp(header->parentmd5, nullmd5, sizeof(nullmd5)) == 0 && memcmp(header->parentsha1, nullsha1, sizeof(nullsha1)) == 0)
			return CHDERR_INVALID_PARAMETER;

		/* if we're V3 or later, the obsolete fields must be 0 */
		if (header->version >= 3 &&
			(header->obsolete_cylinders != 0 || header->obsolete_sectors != 0 ||
			 header->obsolete_heads != 0 || header->obsolete_hunksize != 0))
			return CHDERR_INVALID_PARAMETER;

		/* if we're pre-V3, the obsolete fields must NOT be 0 */
		if (header->version < 3 &&
			(header->obsolete_cylinders == 0 || header->obsolete_sectors == 0 ||
			 header->obsolete_heads == 0 || header->obsolete_hunksize == 0))
			return CHDERR_INVALID_PARAMETER;
	}

	return CHDERR_NONE;
}

/*-------------------------------------------------
    header_guess_unitbytes - for older CHD formats,
    guess at the bytes/unit based on metadata
-------------------------------------------------*/

static UINT32 header_guess_unitbytes(chd_file *chd)
{
	/* look for hard disk metadata; if found, then the unit size == sector size */
	char metadata[512];
	unsigned int i0, i1, i2, i3;
	if (chd_get_metadata(chd, HARD_DISK_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE &&
		sscanf(metadata, HARD_DISK_METADATA_FORMAT, &i0, &i1, &i2, &i3) == 4)
		return i3;

	/* look for CD-ROM metadata; if found, then the unit size == CD frame size */
	if (chd_get_metadata(chd, CDROM_OLD_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE ||
		chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, 0, metadata, sizeof(metadata), NULL, NULL, NULL) == CHDERR_NONE)
		return CD_FRAME_SIZE;

	/* otherwise, just map 1:1 with the hunk size */
	return chd->header.hunkbytes;
}

/*-------------------------------------------------
    header_read - read a CHD header into the
    internal data structure
-------------------------------------------------*/

static chd_error header_read(chd_file *chd, chd_header *header)
{
	UINT8 rawheader[CHD_MAX_HEADER_SIZE];
	int64_t count;

	/* punt if NULL */
	if (header == NULL)
		return CHDERR_INVALID_PARAMETER;

	/* punt if invalid file */
	if (chd->file == NULL)
		return CHDERR_INVALID_FILE;

	/* seek and read */
	filestream_seek(chd->file, 0, SEEK_SET);
	count = filestream_read(chd->file, rawheader, sizeof(rawheader));
	if (count != sizeof(rawheader))
		return CHDERR_READ_ERROR;

	/* verify the tag */
	if (strncmp((char *)rawheader, "MComprHD", 8) != 0)
		return CHDERR_INVALID_DATA;

	/* extract the direct data */
	memset(header, 0, sizeof(*header));
	header->length        = get_bigendian_uint32(&rawheader[8]);
	header->version       = get_bigendian_uint32(&rawheader[12]);

	/* make sure it's a version we understand */
	if (header->version == 0 || header->version > CHD_HEADER_VERSION)
		return CHDERR_UNSUPPORTED_VERSION;

	/* make sure the length is expected */
	if ((header->version == 1 && header->length != CHD_V1_HEADER_SIZE) ||
		(header->version == 2 && header->length != CHD_V2_HEADER_SIZE) ||
		(header->version == 3 && header->length != CHD_V3_HEADER_SIZE) ||
		(header->version == 4 && header->length != CHD_V4_HEADER_SIZE) ||
		(header->version == 5 && header->length != CHD_V5_HEADER_SIZE))

		return CHDERR_INVALID_DATA;

	/* extract the common data */
	header->flags         	= get_bigendian_uint32(&rawheader[16]);
	header->compression[0]	= get_bigendian_uint32(&rawheader[20]);

	/* extract the V1/V2-specific data */
	if (header->version < 3)
	{
		int seclen = (header->version == 1) ? CHD_V1_SECTOR_SIZE : get_bigendian_uint32(&rawheader[76]);
		header->obsolete_hunksize  = get_bigendian_uint32(&rawheader[24]);
		header->totalhunks         = get_bigendian_uint32(&rawheader[28]);
		header->obsolete_cylinders = get_bigendian_uint32(&rawheader[32]);
		header->obsolete_heads     = get_bigendian_uint32(&rawheader[36]);
		header->obsolete_sectors   = get_bigendian_uint32(&rawheader[40]);
		memcpy(header->md5, &rawheader[44], CHD_MD5_BYTES);
		memcpy(header->parentmd5, &rawheader[60], CHD_MD5_BYTES);
		header->logicalbytes = (UINT64)header->obsolete_cylinders * (UINT64)header->obsolete_heads * (UINT64)header->obsolete_sectors * (UINT64)seclen;
		header->hunkbytes = seclen * header->obsolete_hunksize;
		header->unitbytes          = header_guess_unitbytes(chd);
		header->unitcount          = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		header->metaoffset = 0;
	}

	/* extract the V3-specific data */
	else if (header->version == 3)
	{
		header->totalhunks   = get_bigendian_uint32(&rawheader[24]);
		header->logicalbytes = get_bigendian_uint64(&rawheader[28]);
		header->metaoffset   = get_bigendian_uint64(&rawheader[36]);
		memcpy(header->md5, &rawheader[44], CHD_MD5_BYTES);
		memcpy(header->parentmd5, &rawheader[60], CHD_MD5_BYTES);
		header->hunkbytes    = get_bigendian_uint32(&rawheader[76]);
		header->unitbytes    = header_guess_unitbytes(chd);
		header->unitcount    = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		memcpy(header->sha1, &rawheader[80], CHD_SHA1_BYTES);
		memcpy(header->parentsha1, &rawheader[100], CHD_SHA1_BYTES);
	}

	/* extract the V4-specific data */
	else if (header->version == 4)
	{
		header->totalhunks   = get_bigendian_uint32(&rawheader[24]);
		header->logicalbytes = get_bigendian_uint64(&rawheader[28]);
		header->metaoffset   = get_bigendian_uint64(&rawheader[36]);
		header->hunkbytes    = get_bigendian_uint32(&rawheader[44]);
		header->unitbytes    = header_guess_unitbytes(chd);
		header->unitcount    = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		memcpy(header->sha1, &rawheader[48], CHD_SHA1_BYTES);
		memcpy(header->parentsha1, &rawheader[68], CHD_SHA1_BYTES);
		memcpy(header->rawsha1, &rawheader[88], CHD_SHA1_BYTES);
	}

	/* extract the V5-specific data */
	else if (header->version == 5)
	{
		/* TODO */
		header->compression[0]  = get_bigendian_uint32(&rawheader[16]);
		header->compression[1]  = get_bigendian_uint32(&rawheader[20]);
		header->compression[2]  = get_bigendian_uint32(&rawheader[24]);
		header->compression[3]  = get_bigendian_uint32(&rawheader[28]);
		header->logicalbytes    = get_bigendian_uint64(&rawheader[32]);
		header->mapoffset       = get_bigendian_uint64(&rawheader[40]);
		header->metaoffset      = get_bigendian_uint64(&rawheader[48]);
		header->hunkbytes       = get_bigendian_uint32(&rawheader[56]);
		header->hunkcount       = (UINT32)((header->logicalbytes + header->hunkbytes - 1) / header->hunkbytes);
		header->unitbytes       = get_bigendian_uint32(&rawheader[60]);
		header->unitcount       = (header->logicalbytes + header->unitbytes - 1) / header->unitbytes;
		memcpy(header->sha1, &rawheader[84], CHD_SHA1_BYTES);
		memcpy(header->parentsha1, &rawheader[104], CHD_SHA1_BYTES);
		memcpy(header->rawsha1, &rawheader[64], CHD_SHA1_BYTES);

		/* determine properties of map entries */
		header->mapentrybytes = 12; /* TODO compressed() ? 12 : 4; */

		/* hack */
		header->totalhunks 		= header->hunkcount;
	}

	/* Unknown version */
	else
	{
		/* TODO */
	}

	/* guess it worked */
	return CHDERR_NONE;
}

/***************************************************************************
    INTERNAL HUNK READ/WRITE
***************************************************************************/

#ifdef NEED_CACHE_HUNK
/*-------------------------------------------------
    hunk_read_into_cache - read a hunk into
    the CHD's hunk cache
-------------------------------------------------*/

static chd_error hunk_read_into_cache(chd_file *chd, UINT32 hunknum)
{
	chd_error err;

	/* track the max */
	if (hunknum > chd->maxhunk)
		chd->maxhunk = hunknum;

	/* if we're already in the cache, we're done */
	if (chd->cachehunk == hunknum)
		return CHDERR_NONE;
	chd->cachehunk = ~0;

	/* otherwise, read the data */
	err = hunk_read_into_memory(chd, hunknum, chd->cache);
	if (err != CHDERR_NONE)
		return err;

	/* mark the hunk successfully cached in */
	chd->cachehunk = hunknum;
	return CHDERR_NONE;
}
#endif

static UINT8* read_compressed(chd_file *chd, UINT64 offset, size_t size)
{
   int64_t bytes;
   if (chd->file_cache)
      return chd->file_cache + offset;
   filestream_seek(chd->file, offset, SEEK_SET);
   bytes = filestream_read(chd->file, chd->compressed, size);
   if (bytes != size)
      return NULL;
   return chd->compressed;
}

static chd_error read_uncompressed(chd_file *chd, UINT64 offset, size_t size, UINT8 *dest)
{
   int64_t bytes;
   if (chd->file_cache)
   {
      memcpy(dest, chd->file_cache + offset, size);
      return CHDERR_NONE;
   }
   filestream_seek(chd->file, offset, SEEK_SET);
   bytes = filestream_read(chd->file, dest, size);
   if (bytes != size)
      return CHDERR_READ_ERROR;
   return CHDERR_NONE;
}

/*-------------------------------------------------
    hunk_read_into_memory - read a hunk into
    memory at the given location
-------------------------------------------------*/

static chd_error hunk_read_into_memory(chd_file *chd, UINT32 hunknum, UINT8 *dest)
{
	chd_error err;

	/* punt if no file */
	if (chd->file == NULL)
		return CHDERR_INVALID_FILE;

	/* return an error if out of range */
	if (hunknum >= chd->header.totalhunks)
		return CHDERR_HUNK_OUT_OF_RANGE;

	if (dest == NULL)
		return CHDERR_INVALID_PARAMETER;

	if (chd->header.version < 5)
	{
		map_entry *entry = &chd->map[hunknum];
		UINT32 bytes;

		/* switch off the entry type */
		switch (entry->flags & MAP_ENTRY_FLAG_TYPE_MASK)
		{
			/* compressed data */
			case MAP_ENTRY_TYPE_COMPRESSED:
            {
               void *codec;
               UINT8 *bytes = read_compressed(chd, entry->offset,
                     entry->length);
               if (bytes == NULL)
                  return CHDERR_READ_ERROR;

#ifdef HAVE_ZLIB
               /* now decompress using the codec */
               err   = CHDERR_NONE;
               codec = &chd->zlib_codec_data;
               if (chd->codecintf[0]->decompress != NULL)
                  err = (*chd->codecintf[0]->decompress)(codec, chd->compressed, entry->length, dest, chd->header.hunkbytes);
               if (err != CHDERR_NONE)
                  return err;
#endif
            }
				break;

			/* uncompressed data */
			case MAP_ENTRY_TYPE_UNCOMPRESSED:
            err = read_uncompressed(chd, entry->offset, chd->header.hunkbytes, dest);
            if (err != CHDERR_NONE)
               return err;
				break;

			/* mini-compressed data */
			case MAP_ENTRY_TYPE_MINI:
				put_bigendian_uint64(&dest[0], entry->offset);
				for (bytes = 8; bytes < chd->header.hunkbytes; bytes++)
					dest[bytes] = dest[bytes - 8];
				break;

			/* self-referenced data */
			case MAP_ENTRY_TYPE_SELF_HUNK:
#ifdef NEED_CACHE_HUNK
				if (chd->cachehunk == entry->offset && dest == chd->cache)
					break;
#endif
				return hunk_read_into_memory(chd, (UINT32)entry->offset, dest);

			/* parent-referenced data */
			case MAP_ENTRY_TYPE_PARENT_HUNK:
				err = hunk_read_into_memory(chd->parent, (UINT32)entry->offset, dest);
				if (err != CHDERR_NONE)
					return err;
				break;
		}
		return CHDERR_NONE;
	}
	else
	{
		void* codec = NULL;
		/* get a pointer to the map entry */
		uint64_t blockoffs;
		uint32_t blocklen;
#ifdef VERIFY_BLOCK_CRC
		uint16_t blockcrc;
#endif
		uint8_t *rawmap = &chd->header.rawmap[chd->header.mapentrybytes * hunknum];
      UINT8 *bytes;

#if 0
		/* uncompressed case - TODO */
		if (!compressed())
		{
			blockoffs = uint64_t(be_read(rawmap, 4)) * uint64_t(m_hunkbytes);
			if (blockoffs != 0)
				file_read(blockoffs, dest, m_hunkbytes);
			else if (m_parent_missing)
				throw CHDERR_REQUIRES_PARENT;
			else if (m_parent != nullptr)
				m_parent->read_hunk(hunknum, dest);
			else
				memset(dest, 0, m_hunkbytes);
			return CHDERR_NONE;
		}
#endif

		/* compressed case */
		blocklen  = get_bigendian_uint24(&rawmap[1]);
		blockoffs = get_bigendian_uint48(&rawmap[4]);
#ifdef VERIFY_BLOCK_CRC
		blockcrc  = get_bigendian_uint16(&rawmap[10]);
#endif
		switch (rawmap[0])
		{
			case COMPRESSION_TYPE_0:
			case COMPRESSION_TYPE_1:
			case COMPRESSION_TYPE_2:
			case COMPRESSION_TYPE_3:
            bytes = read_compressed(chd, blockoffs, blocklen);
            if (bytes == NULL)
               return CHDERR_READ_ERROR;
            if (!chd->codecintf[rawmap[0]])
               return CHDERR_UNSUPPORTED_FORMAT;
				switch (chd->codecintf[rawmap[0]]->compression)
				{
					case CHD_CODEC_CD_LZMA:
#ifdef HAVE_7ZIP
						codec = &chd->cdlz_codec_data;
#endif
						break;

					case CHD_CODEC_CD_ZLIB:
#ifdef HAVE_ZLIB
						codec = &chd->cdzl_codec_data;
#endif
						break;

					case CHD_CODEC_CD_FLAC:
#ifdef HAVE_FLAC
						codec = &chd->cdfl_codec_data;
#endif
						break;
				}
				if (codec==NULL)
					return CHDERR_CODEC_ERROR;
				err = (*chd->codecintf[rawmap[0]]->decompress)(codec, chd->compressed, blocklen, dest, chd->header.hunkbytes);
				if (err != CHDERR_NONE)
					return err;
#ifdef VERIFY_BLOCK_CRC
				if (crc16(dest, chd->header.hunkbytes) != blockcrc)
					return CHDERR_DECOMPRESSION_ERROR;
#endif
				return CHDERR_NONE;

			case COMPRESSION_NONE:
            err = read_uncompressed(chd, blockoffs, blocklen, dest);
            if (err != CHDERR_NONE)
               return err;
#ifdef VERIFY_BLOCK_CRC
            if (crc16(dest, chd->header.hunkbytes) != blockcrc)
               return CHDERR_DECOMPRESSION_ERROR;
#endif
				return CHDERR_NONE;

			case COMPRESSION_SELF:
				return hunk_read_into_memory(chd, (UINT32)blockoffs, dest);

			case COMPRESSION_PARENT:
#if 0
				/* TODO */
				if (m_parent_missing)
					return CHDERR_REQUIRES_PARENT;
				return m_parent->read_bytes(uint64_t(blockoffs) * uint64_t(m_parent->unit_bytes()), dest, m_hunkbytes);
#endif
				return CHDERR_DECOMPRESSION_ERROR;
		}
		return CHDERR_NONE;
	}

	/* We should not reach this code */
	return CHDERR_DECOMPRESSION_ERROR;
}

/***************************************************************************
    INTERNAL MAP ACCESS
***************************************************************************/

static size_t core_fsize(RFILE *f)
{
   int64_t rv, p = filestream_tell(f);
	filestream_seek(f, 0, SEEK_END);
	rv = filestream_tell(f);
	filestream_seek(f, p, SEEK_SET);
	return rv;
}

/*-------------------------------------------------
    map_read - read the initial sector map
-------------------------------------------------*/

static chd_error map_read(chd_file *chd)
{
	UINT32 entrysize = (chd->header.version < 3) ? OLD_MAP_ENTRY_SIZE : MAP_ENTRY_SIZE;
	UINT8 raw_map_entries[MAP_STACK_ENTRIES * MAP_ENTRY_SIZE];
	UINT64 fileoffset, maxoffset = 0;
	UINT8 cookie[MAP_ENTRY_SIZE];
	int64_t count;
	chd_error err;
	int i;

	/* first allocate memory */
	chd->map = (map_entry *)malloc(sizeof(chd->map[0]) * chd->header.totalhunks);
	if (!chd->map)
		return CHDERR_OUT_OF_MEMORY;

	/* read the map entries in in chunks and extract to the map list */
	fileoffset = chd->header.length;
	for (i = 0; i < chd->header.totalhunks; i += MAP_STACK_ENTRIES)
	{
		/* compute how many entries this time */
		int entries = chd->header.totalhunks - i, j;
		if (entries > MAP_STACK_ENTRIES)
			entries = MAP_STACK_ENTRIES;

		/* read that many */
		filestream_seek(chd->file, fileoffset, SEEK_SET);
		count = filestream_read(chd->file, raw_map_entries, entries * entrysize);
		if (count != entries * entrysize)
		{
			err = CHDERR_READ_ERROR;
			goto cleanup;
		}
		fileoffset += entries * entrysize;

		/* process that many */
		if (entrysize == MAP_ENTRY_SIZE)
		{
			for (j = 0; j < entries; j++)
				map_extract(&raw_map_entries[j * MAP_ENTRY_SIZE], &chd->map[i + j]);
		}
		else
		{
			for (j = 0; j < entries; j++)
				map_extract_old(&raw_map_entries[j * OLD_MAP_ENTRY_SIZE], &chd->map[i + j], chd->header.hunkbytes);
		}

		/* track the maximum offset */
		for (j = 0; j < entries; j++)
			if ((chd->map[i + j].flags & MAP_ENTRY_FLAG_TYPE_MASK) == MAP_ENTRY_TYPE_COMPRESSED ||
				(chd->map[i + j].flags & MAP_ENTRY_FLAG_TYPE_MASK) == MAP_ENTRY_TYPE_UNCOMPRESSED)
				maxoffset = MAX(maxoffset, chd->map[i + j].offset + chd->map[i + j].length);
	}

	/* verify the cookie */
	filestream_seek(chd->file, fileoffset, SEEK_SET);
	count = filestream_read(chd->file, &cookie, entrysize);
	if (count != entrysize || memcmp(&cookie, END_OF_LIST_COOKIE, entrysize))
	{
		err = CHDERR_INVALID_FILE;
		goto cleanup;
	}

	/* verify the length */
	if (maxoffset > core_fsize(chd->file))
	{
		err = CHDERR_INVALID_FILE;
		goto cleanup;
	}
	return CHDERR_NONE;

cleanup:
	if (chd->map)
		free(chd->map);
	chd->map = NULL;
	return err;
}

/***************************************************************************
    INTERNAL METADATA ACCESS
***************************************************************************/

/*-------------------------------------------------
    metadata_find_entry - find a metadata entry
-------------------------------------------------*/

static chd_error metadata_find_entry(chd_file *chd, UINT32 metatag, UINT32 metaindex, metadata_entry *metaentry)
{
	/* start at the beginning */
	metaentry->offset = chd->header.metaoffset;
	metaentry->prev = 0;

	/* loop until we run out of options */
	while (metaentry->offset != 0)
	{
		UINT8	raw_meta_header[METADATA_HEADER_SIZE];
		int64_t	count;

		/* read the raw header */
		filestream_seek(chd->file, metaentry->offset, SEEK_SET);
		count = filestream_read(chd->file, raw_meta_header, sizeof(raw_meta_header));
		if (count != sizeof(raw_meta_header))
			break;

		/* extract the data */
		metaentry->metatag = get_bigendian_uint32(&raw_meta_header[0]);
		metaentry->length = get_bigendian_uint32(&raw_meta_header[4]);
		metaentry->next = get_bigendian_uint64(&raw_meta_header[8]);

		/* flags are encoded in the high byte of length */
		metaentry->flags = metaentry->length >> 24;
		metaentry->length &= 0x00ffffff;

		/* if we got a match, proceed */
		if (metatag == CHDMETATAG_WILDCARD || metaentry->metatag == metatag)
			if (metaindex-- == 0)
				return CHDERR_NONE;

		/* no match, fetch the next link */
		metaentry->prev = metaentry->offset;
		metaentry->offset = metaentry->next;
	}

	/* if we get here, we didn't find it */
	return CHDERR_METADATA_NOT_FOUND;
}
