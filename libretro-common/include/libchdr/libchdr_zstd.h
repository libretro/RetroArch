/* license:BSD-3-Clause
 * copyright-holders:Aaron Giles
 ***************************************************************************

    libchr_zstd.h

    Zstd compression wrappers

***************************************************************************/

#pragma once

#ifndef __LIBCHDR_ZSTD_H__
#define __LIBCHDR_ZSTD_H__

#include <stdint.h>

#include <zstd.h>
#include "coretypes.h"
#include "chd.h"

typedef struct _zstd_codec_data zstd_codec_data;
struct _zstd_codec_data
{
	ZSTD_DStream *dstream;
};

typedef struct _cdzs_codec_data cdzs_codec_data;
struct _cdzs_codec_data
{
	zstd_codec_data base_decompressor;
#ifdef WANT_SUBCODE
	zstd_codec_data subcode_decompressor;
#endif
	uint8_t*				buffer;
};

#endif /* __LIBCHDR_ZSTD_H__ */
