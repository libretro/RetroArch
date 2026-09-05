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

/* Either backend serves. rzstd wins when both are configured: it is the
 * one the rest of the tree is moving to, and it keeps <zstd.h> - which
 * declares long long - out of this translation unit. */
#ifndef HAVE_RZSTD
#include <zstd.h>
#endif

#include "coretypes.h"
#include "chd.h"

typedef struct _zstd_codec_data zstd_codec_data;
struct _zstd_codec_data
{
#ifdef HAVE_RZSTD
	/* rzstd decodes a hunk in one call and carries nothing between
	 * hunks, so there is no decoder to hold on to. The member exists
	 * only because an empty struct is not C89. */
	uint8_t unused;
#else
	ZSTD_DStream *dstream;
#endif
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
