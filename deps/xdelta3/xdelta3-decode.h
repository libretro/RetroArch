/* xdelta3 - delta compression tools and library
   Copyright 2016 Joshua MacDonald

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef _XDELTA3_DECODE_H_
#define _XDELTA3_DECODE_H_

#include "xdelta3-internal.h"

#define SRCORTGT(x) ((((x) & VCD_SRCORTGT) == VCD_SOURCE) ? \
                     VCD_SOURCE : ((((x) & VCD_SRCORTGT) == \
                                    VCD_TARGET) ? VCD_TARGET : 0))

static inline int
xd3_decode_byte (xd3_stream *stream, usize_t *val)
{
  if (stream->avail_in == 0)
    {
      stream->msg = "further input required";
      return XD3_INPUT;
    }

  (*val) = stream->next_in[0];

  DECODE_INPUT (1);
  return 0;
}

static inline int
xd3_decode_bytes (xd3_stream *stream, uint8_t *buf, usize_t *pos, usize_t size)
{
  usize_t want;
  usize_t take;

  /* Note: The case where (*pos == size) happens when a zero-length
   * appheader or code table is transmitted, but there is nothing in
   * the standard against that. */
  while (*pos < size)
    {
      if (stream->avail_in == 0)
	{
	  stream->msg = "further input required";
	  return XD3_INPUT;
	}

      want = size - *pos;
      take = xd3_min (want, stream->avail_in);

      memcpy (buf + *pos, stream->next_in, (size_t) take);

      DECODE_INPUT (take);
      (*pos) += take;
    }

  return 0;
}

/* Initialize the decoder for a new window.  The dec_tgtlen value is
 * preserved across successive window decodings, and the update to
 * dec_winstart is delayed until a new window actually starts.  This
 * is to avoid throwing an error due to overflow until the last
 * possible moment.  This makes it possible to encode exactly 4GB
 * through a 32-bit encoder. */
static int
xd3_decode_init_window (xd3_stream *stream)
{
  stream->dec_cpylen = 0;
  stream->dec_cpyoff = 0;
  stream->dec_cksumbytes = 0;

  xd3_init_cache (& stream->acache);

  return 0;
}

/* Allocates buffer space for the target window and possibly the
 * VCD_TARGET copy-window.  Also sets the base of the two copy
 * segments. */
static int
xd3_decode_setup_buffers (xd3_stream *stream)
{
  /* If VCD_TARGET is set then the previous buffer may be reused. */
  if (stream->dec_win_ind & VCD_TARGET)
    {
      /* Note: this implementation is untested, since Xdelta3 itself
       * does not implement an encoder for VCD_TARGET mode. Thus, mark
       * unimplemented until needed. */
      if (1)
	{
	  stream->msg = "VCD_TARGET not implemented";
	  return XD3_UNIMPLEMENTED;
	}

      /* But this implementation only supports copying from the last
       * target window.  If the offset is outside that range, it can't
       * be done. */
      if (stream->dec_cpyoff < stream->dec_laststart)
	{
	  stream->msg = "unsupported VCD_TARGET offset";
	  return XD3_INVALID_INPUT;
	}

      /* See if the two windows are the same.  This indicates the
       * first time VCD_TARGET is used.  This causes a second buffer
       * to be allocated, after that the two are swapped in the
       * DEC_FINISH case. */
      if (stream->dec_lastwin == stream->next_out)
	{
	  stream->next_out  = NULL;
	  stream->space_out = 0;
	}

      /* TODO: (See note above, this looks incorrect) */
      stream->dec_cpyaddrbase = stream->dec_lastwin +
	(usize_t) (stream->dec_cpyoff - stream->dec_laststart);
    }

  /* See if the current output window is large enough. */
  if (stream->space_out < stream->dec_tgtlen)
    {
      xd3_free (stream, stream->dec_buffer);

      stream->space_out =
	xd3_round_blksize (stream->dec_tgtlen, XD3_ALLOCSIZE);

      if ((stream->dec_buffer =
	   (uint8_t*) xd3_alloc (stream, stream->space_out, 1)) == NULL)
	{
	  return ENOMEM;
	}

      stream->next_out = stream->dec_buffer;
    }

  /* dec_tgtaddrbase refers to an invalid base address, but it is
   * always used with a sufficiently large instruction offset (i.e.,
   * beyond the copy window).  This condition is enforced by
   * xd3_decode_output_halfinst. */
  stream->dec_tgtaddrbase = stream->next_out - stream->dec_cpylen;

  return 0;
}

static int
xd3_decode_allocate (xd3_stream  *stream,
		     usize_t       size,
		     uint8_t    **buf_ptr,
		     usize_t      *buf_alloc)
{
  IF_DEBUG2 (DP(RINT "[xd3_decode_allocate] size %"W"u alloc %"W"u\n",
		size, *buf_alloc));
  
  if (*buf_ptr != NULL && *buf_alloc < size)
    {
      xd3_free (stream, *buf_ptr);
      *buf_ptr = NULL;
    }

  if (*buf_ptr == NULL)
    {
      *buf_alloc = xd3_round_blksize (size, XD3_ALLOCSIZE);

      if ((*buf_ptr = (uint8_t*) xd3_alloc (stream, *buf_alloc, 1)) == NULL)
	{
	  return ENOMEM;
	}
    }

  return 0;
}

static int
xd3_decode_section (xd3_stream *stream,
		    xd3_desect *section,
		    xd3_decode_state nstate,
		    int copy)
{
  XD3_ASSERT (section->pos <= section->size);
  XD3_ASSERT (stream->dec_state != nstate);

  if (section->pos < section->size)
    {
      usize_t sect_take;

      if (stream->avail_in == 0)
	{
	  return XD3_INPUT;
	}

      if ((copy == 0) && (section->pos == 0))
	{
	  /* No allocation/copy needed */
	  section->buf = stream->next_in;
	  sect_take    = section->size;
	  IF_DEBUG1 (DP(RINT "[xd3_decode_section] zerocopy %"W"u @ %"W"u avail %"W"u\n",
			sect_take, section->pos, stream->avail_in));
	}
      else
	{
	  usize_t sect_need = section->size - section->pos;

	  /* Allocate and copy */
	  sect_take = xd3_min (sect_need, stream->avail_in);

	  if (section->pos == 0)
	    {
	      int ret;

	      if ((ret = xd3_decode_allocate (stream,
					      section->size,
					      & section->copied1,
					      & section->alloc1)))
		{
		  return ret;
		}

	      section->buf = section->copied1;
	    }

	  IF_DEBUG2 (DP(RINT "[xd3_decode_section] take %"W"u @ %"W"u [need %"W"u] avail %"W"u\n",
			sect_take, section->pos, sect_need, stream->avail_in));
	  XD3_ASSERT (section->pos + sect_take <= section->alloc1);

	  memcpy (section->copied1 + section->pos,
		  stream->next_in,
		  sect_take);
	}

      section->pos += sect_take;

      stream->dec_winbytes += sect_take;

      DECODE_INPUT (sect_take);
    }

  if (section->pos < section->size)
    {
      IF_DEBUG1 (DP(RINT "[xd3_decode_section] further input required %"W"u\n",
		    section->size - section->pos));
      stream->msg = "further input required";
      return XD3_INPUT;
    }

  XD3_ASSERT (section->pos == section->size);

  stream->dec_state = nstate;
  section->buf_max  = section->buf + section->size;
  section->pos      = 0;
  return 0;
}

/* Decode the size and address for half of an instruction (i.e., a
 * single opcode).  This updates the stream->dec_position, which are
 * bytes already output prior to processing this instruction.  Perform
 * bounds checking for sizes and copy addresses, which uses the
 * dec_position (which is why these checks are done here). */
static int
xd3_decode_parse_halfinst (xd3_stream *stream, xd3_hinst *inst)
{
  int ret;

  /* If the size from the instruction table is zero then read a size value. */
  if ((inst->size == 0) &&
      (ret = xd3_read_size (stream,
 			    & stream->inst_sect.buf,
			      stream->inst_sect.buf_max,
			    & inst->size)))
    {
      return XD3_INVALID_INPUT;
    }

  /* For copy instructions, read address. */
  if (inst->type >= XD3_CPY)
    {
      IF_DEBUG2 ({
	static int cnt = 0;
	XPR(NT "DECODE:%u: COPY at %"Q"u (winoffset %"W"u) "
	    "size %"W"u winaddr %"W"u\n",
	    cnt++,
	    stream->total_out + (stream->dec_position -
				 stream->dec_cpylen),
	    (stream->dec_position - stream->dec_cpylen),
	    inst->size,
	    inst->addr);
      });

      if ((ret = xd3_decode_address (stream,
				     stream->dec_position,
				     inst->type - XD3_CPY,
				     & stream->addr_sect.buf,
				     stream->addr_sect.buf_max,
				     & inst->addr)))
	{
	  return ret;
	}

      /* Cannot copy an address before it is filled-in. */
      if (inst->addr >= stream->dec_position)
	{
	  stream->msg = "address too large";
	  return XD3_INVALID_INPUT;
	}

      /* Check: a VCD_TARGET or VCD_SOURCE copy cannot exceed the remaining
       * buffer space in its own segment. */
      if (inst->addr < stream->dec_cpylen &&
	  inst->addr + inst->size > stream->dec_cpylen)
	{
	  stream->msg = "size too large";
	  return XD3_INVALID_INPUT;
	}
    }
  else
    {
      IF_DEBUG2 ({
	if (inst->type == XD3_ADD)
	  {
	    static int cnt;
	    XPR(NT "DECODE:%d: ADD at %"Q"u (winoffset %"W"u) size %"W"u\n",
	       cnt++,
	       (stream->total_out + stream->dec_position - stream->dec_cpylen),
	       stream->dec_position - stream->dec_cpylen,
	       inst->size);
	  }
	else
	  {
	    static int cnt;
	    XD3_ASSERT (inst->type == XD3_RUN);
	    XPR(NT "DECODE:%d: RUN at %"Q"u (winoffset %"W"u) size %"W"u\n",
	       cnt++,
	       stream->total_out + stream->dec_position - stream->dec_cpylen,
	       stream->dec_position - stream->dec_cpylen,
	       inst->size);
	  }
      });
    }

  /* Check: The instruction will not overflow the output buffer. */
  if (stream->dec_position + inst->size > stream->dec_maxpos)
    {
      stream->msg = "size too large";
      return XD3_INVALID_INPUT;
    }

  stream->dec_position += inst->size;
  return 0;
}

/* Decode a single opcode and then decode the two half-instructions. */
static int
xd3_decode_instruction (xd3_stream *stream)
{
  int ret;
  const xd3_dinst *inst;

  if (stream->inst_sect.buf == stream->inst_sect.buf_max)
    {
      stream->msg = "instruction underflow";
      return XD3_INVALID_INPUT;
    }

  inst = &stream->code_table[*stream->inst_sect.buf++];

  stream->dec_current1.type = inst->type1;
  stream->dec_current2.type = inst->type2;
  stream->dec_current1.size = inst->size1;
  stream->dec_current2.size = inst->size2;

  /* For each instruction with a real operation, decode the
   * corresponding size and addresses if necessary.  Assume a
   * code-table may have NOOP in either position, although this is
   * unlikely. */
  if (inst->type1 != XD3_NOOP &&
      (ret = xd3_decode_parse_halfinst (stream, & stream->dec_current1)))
    {
      return ret;
    }
  if (inst->type2 != XD3_NOOP &&
      (ret = xd3_decode_parse_halfinst (stream, & stream->dec_current2)))
    {
      return ret;
    }
  return 0;
}

/* Output the result of a single half-instruction. OPT: This the
   decoder hotspot.  Modifies "hinst", see below.  */
static int
xd3_decode_output_halfinst (xd3_stream *stream, xd3_hinst *inst)
{
  /* This method is reentrant for copy instructions which may return
   * XD3_GETSRCBLK to the caller.  Each time through a copy takes the
   * minimum of inst->size and the available space on whichever block
   * supplies the data */
  usize_t take = inst->size;

  if (USIZE_T_OVERFLOW (stream->avail_out, take) ||
      stream->avail_out + take > stream->space_out)
    {
      stream->msg = "overflow while decoding";
      return XD3_INVALID_INPUT;
    }

  XD3_ASSERT (inst->type != XD3_NOOP);

  switch (inst->type)
    {
    case XD3_RUN:
      {
	/* Only require a single data byte. */
	if (stream->data_sect.buf == stream->data_sect.buf_max)
	  {
	    stream->msg = "data underflow";
	    return XD3_INVALID_INPUT;
	  }

	memset (stream->next_out + stream->avail_out,
		stream->data_sect.buf[0],
		take);

	stream->data_sect.buf += 1;
	stream->avail_out += take;
	inst->type = XD3_NOOP;
	break;
      }
    case XD3_ADD:
      {
	/* Require at least TAKE data bytes. */
	if (stream->data_sect.buf + take > stream->data_sect.buf_max)
	  {
	    stream->msg = "data underflow";
	    return XD3_INVALID_INPUT;
	  }

	memcpy (stream->next_out + stream->avail_out,
		stream->data_sect.buf,
		take);

	stream->data_sect.buf += take;
	stream->avail_out += take;
	inst->type = XD3_NOOP;
	break;
      }
    default:
      {
	usize_t i;
	const uint8_t *src;
	uint8_t *dst;
	int overlap;

	/* See if it copies from the VCD_TARGET/VCD_SOURCE window or
	 * the target window.  Out-of-bounds checks for the addresses
	 * and sizes are performed in xd3_decode_parse_halfinst.  This
	 * if/else must set "overlap", "src", and "dst". */
	if (inst->addr < stream->dec_cpylen)
	  {
	    /* In both branches we are copying from outside the
	     * current decoder window, the first (VCD_TARGET) is
	     * unimplemented. */
	    overlap = 0;
	    
	    /* This branch sets "src".  As a side-effect, we modify
	     * "inst" so that if we reenter this method after a
	     * XD3_GETSRCBLK response the state is correct.  So if the
	     * instruction can be fulfilled by a contiguous block of
	     * memory then we will set:
	     *
	     *  inst->type = XD3_NOOP;
	     *  inst->size = 0;
	     */
	    if (stream->dec_win_ind & VCD_TARGET)
	      {
		/* TODO: Users have requested long-distance copies of
		 * similar material within a target (e.g., for dup
		 * supression in backups). This code path is probably
		 * dead due to XD3_UNIMPLEMENTED in xd3_decode_setup_buffers */
		inst->size = 0;
		inst->type = XD3_NOOP;
		stream->msg = "VCD_TARGET not implemented";
		return XD3_UNIMPLEMENTED;
	      }
	    else
	      {
		/* In this case we have to read a source block, which
		 * could return control to the caller.  We need to
		 * know the first block number needed for this
		 * copy. */
		xd3_source *source = stream->src;
		xoff_t block = source->cpyoff_blocks;
		usize_t blkoff = source->cpyoff_blkoff;
		const usize_t blksize = source->blksize;
		int ret;

		xd3_blksize_add (&block, &blkoff, source, inst->addr);
		XD3_ASSERT (blkoff < blksize);

		if ((ret = xd3_getblk (stream, block)))
		  {
		    /* could be a XD3_GETSRCBLK failure. */
		    if (ret == XD3_TOOFARBACK)
		      {
			stream->msg = "non-seekable source in decode";
			ret = XD3_INTERNAL;
		      }
		    return ret;
		  }

		src = source->curblk + blkoff;

		/* This block is either full, or a partial block that
		 * must contain enough bytes. */
		if ((source->onblk != blksize) &&
		    (blkoff + take > source->onblk))
		  {
		    IF_DEBUG1 (XPR(NT "[srcfile] short at blkno %"Q"u onblk "
				   "%"W"u blksize %"W"u blkoff %"W"u take %"W"u\n",
				   block,
				   source->onblk,
				   blksize,
				   blkoff,
				   take));
		    stream->msg = "source file too short";
		    return XD3_INVALID_INPUT;
		  }

		XD3_ASSERT (blkoff != blksize);

		/* Check if we have enough data on this block to
		 * finish the instruction. */
		if (blkoff + take <= blksize)
		  {
		    inst->type = XD3_NOOP;
		    inst->size = 0;
		  }
		else
		  {
		    take = blksize - blkoff;
		    inst->size -= take;
		    inst->addr += take;

		    /* because (blkoff + take > blksize), above */
		    XD3_ASSERT (inst->size != 0);
		  }
	      }
	  }
	else
	  {
	    /* TODO: the memcpy/overlap optimization, etc.  Overlap
	     * here could be more specific, it's whether (inst->addr -
	     * srclen) + inst->size > input_pos ?  And is the system
	     * memcpy really any good? */
	    overlap = 1;

	    /* For a target-window copy, we know the entire range is
	     * in-memory.  The dec_tgtaddrbase is negatively offset by
	     * dec_cpylen because the addresses start beyond that
	     * point. */
	    src = stream->dec_tgtaddrbase + inst->addr;
	    inst->type = XD3_NOOP;
	    inst->size = 0;
	  }

 	dst = stream->next_out + stream->avail_out;

	stream->avail_out += take;

	if (overlap)
	  {
	    /* Can't just memcpy here due to possible overlap. */
	    for (i = take; i != 0; i -= 1)
	      {
		*dst++ = *src++;
	      }
	  }
	else
	  {
	    memcpy (dst, src, take);
	  }
      }
    }

  return 0;
}

static int
xd3_decode_finish_window (xd3_stream *stream)
{
  stream->dec_winbytes  = 0;
  stream->dec_state     = DEC_FINISH;

  stream->data_sect.pos = 0;
  stream->inst_sect.pos = 0;
  stream->addr_sect.pos = 0;

  return XD3_OUTPUT;
}

static int
xd3_decode_secondary_sections (xd3_stream *secondary_stream)
{
#if SECONDARY_ANY
  int ret;
#define DECODE_SECONDARY_SECTION(UPPER,LOWER) \
  ((secondary_stream->dec_del_ind & VCD_ ## UPPER ## COMP) && \
   (ret = xd3_decode_secondary (secondary_stream, \
				& secondary_stream-> LOWER ## _sect,	\
				& xd3_sec_ ## LOWER (secondary_stream))))

  if (DECODE_SECONDARY_SECTION (DATA, data) ||
      DECODE_SECONDARY_SECTION (INST, inst) ||
      DECODE_SECONDARY_SECTION (ADDR, addr))
    {
      return ret;
    }
#undef DECODE_SECONDARY_SECTION
#endif
  return 0;
}

static int
xd3_decode_sections (xd3_stream *stream)
{
  usize_t need, more, take;
  int copy, ret;

  if ((stream->flags & XD3_JUST_HDR) != 0)
    {
      /* Nothing left to do. */
      return xd3_decode_finish_window (stream);
    }

  /* To avoid extra copying, allocate three sections at once (but
   * check for overflow). */
  need = stream->inst_sect.size;

  if (USIZE_T_OVERFLOW (need, stream->addr_sect.size))
    {
      stream->msg = "decoder section size overflow";
      return XD3_INTERNAL;
    }
  need += stream->addr_sect.size;

  if (USIZE_T_OVERFLOW (need, stream->data_sect.size))
    {
      stream->msg = "decoder section size overflow";
      return XD3_INTERNAL;
    }
  need += stream->data_sect.size;

  /* The window may be entirely processed. */
  XD3_ASSERT (stream->dec_winbytes <= need);

  /* Compute how much more input is needed. */
  more = (need - stream->dec_winbytes);

  /* How much to consume. */
  take = xd3_min (more, stream->avail_in);

  /* See if the input is completely available, to avoid copy. */
  copy = (take != more);

  /* If the window is skipped... */
  if ((stream->flags & XD3_SKIP_WINDOW) != 0)
    {
      /* Skip the available input. */
      DECODE_INPUT (take);

      stream->dec_winbytes += take;

      if (copy)
	{
	  stream->msg = "further input required";
	  return XD3_INPUT;
	}

      return xd3_decode_finish_window (stream);
    }

  /* Process all but the DATA section. */
  switch (stream->dec_state)
    {
    default:
      stream->msg = "internal error";
      return XD3_INVALID_INPUT;

    case DEC_DATA:
      if ((ret = xd3_decode_section (stream, & stream->data_sect,
				     DEC_INST, copy))) { return ret; }
    case DEC_INST:
      if ((ret = xd3_decode_section (stream, & stream->inst_sect,
				     DEC_ADDR, copy))) { return ret; }
    case DEC_ADDR:
      if ((ret = xd3_decode_section (stream, & stream->addr_sect,
				     DEC_EMIT, copy))) { return ret; }
    }

  XD3_ASSERT (stream->dec_winbytes == need);

  if ((ret = xd3_decode_secondary_sections (stream))) { return ret; }

  if (stream->flags & XD3_SKIP_EMIT)
    {
      return xd3_decode_finish_window (stream);
    }

  /* OPT: A possible optimization is to avoid allocating memory in
   * decode_setup_buffers and to avoid a large memcpy when the window
   * consists of a single VCD_SOURCE copy instruction. */
  if ((ret = xd3_decode_setup_buffers (stream))) { return ret; }

  return 0;
}

static int
xd3_decode_emit (xd3_stream *stream)
{
  int ret;

  /* Produce output: originally structured to allow reentrant code
   * that fills as much of the output buffer as possible, but VCDIFF
   * semantics allows to copy from anywhere from the target window, so
   * instead allocate a sufficiently sized buffer after the target
   * window length is decoded.
   *
   * This code still needs to be reentrant to allow XD3_GETSRCBLK to
   * return control.  This is handled by setting the
   * stream->dec_currentN instruction types to XD3_NOOP after they
   * have been processed. */
  XD3_ASSERT (! (stream->flags & XD3_SKIP_EMIT));
  XD3_ASSERT (stream->dec_tgtlen <= stream->space_out);

  while (stream->inst_sect.buf != stream->inst_sect.buf_max ||
	 stream->dec_current1.type != XD3_NOOP ||
	 stream->dec_current2.type != XD3_NOOP)
    {
      /* Decode next instruction pair. */
      if ((stream->dec_current1.type == XD3_NOOP) &&
	  (stream->dec_current2.type == XD3_NOOP) &&
	  (ret = xd3_decode_instruction (stream))) { return ret; }

      /* Output dec_current1 */
      while ((stream->dec_current1.type != XD3_NOOP))
	{
	  if ((ret = xd3_decode_output_halfinst (stream, & stream->dec_current1)))
	    {
	      return ret;
	    }
	}
      /* Output dec_current2 */
      while (stream->dec_current2.type != XD3_NOOP)
	{
	  if ((ret = xd3_decode_output_halfinst (stream, & stream->dec_current2)))
	    {
	      return ret;
	    }
	}
    }

  if (stream->avail_out != stream->dec_tgtlen)
    {
      IF_DEBUG2 (DP(RINT "AVAIL_OUT(%"W"u) != DEC_TGTLEN(%"W"u)\n",
		    stream->avail_out, stream->dec_tgtlen));
      stream->msg = "wrong window length";
      return XD3_INVALID_INPUT;
    }

  if (stream->data_sect.buf != stream->data_sect.buf_max)
    {
      stream->msg = "extra data section";
      return XD3_INVALID_INPUT;
    }

  if (stream->addr_sect.buf != stream->addr_sect.buf_max)
    {
      stream->msg = "extra address section";
      return XD3_INVALID_INPUT;
    }

  /* OPT: Should cksum computation be combined with the above loop? */
  if ((stream->dec_win_ind & VCD_ADLER32) != 0 &&
      (stream->flags & XD3_ADLER32_NOVER) == 0)
    {
      uint32_t a32 = adler32 (1L, stream->next_out, stream->avail_out);

      if (a32 != stream->dec_adler32)
	{
	  stream->msg = "target window checksum mismatch";
	  return XD3_INVALID_INPUT;
	}
    }

  /* Finished with a window. */
  return xd3_decode_finish_window (stream);
}

int
xd3_decode_input (xd3_stream *stream)
{
  int ret;

  if (stream->enc_state != 0)
    {
      stream->msg = "encoder/decoder transition";
      return XD3_INVALID_INPUT;
    }

#define BYTE_CASE(expr,x,nstate) \
      do { \
      if ( (expr) && \
           ((ret = xd3_decode_byte (stream, & (x))) != 0) ) { return ret; } \
      stream->dec_state = (nstate); \
      } while (0)

#define OFFSET_CASE(expr,x,nstate) \
      do { \
      if ( (expr) && \
           ((ret = xd3_decode_offset (stream, & (x))) != 0) ) { return ret; } \
      stream->dec_state = (nstate); \
      } while (0)

#define SIZE_CASE(expr,x,nstate) \
      do { \
      if ( (expr) && \
           ((ret = xd3_decode_size (stream, & (x))) != 0) ) { return ret; } \
      stream->dec_state = (nstate); \
      } while (0)

  switch (stream->dec_state)
    {
    case DEC_VCHEAD:
      {
	if ((ret = xd3_decode_bytes (stream, stream->dec_magic,
				     & stream->dec_magicbytes, 4)))
	  {
	    return ret;
	  }

	if (stream->dec_magic[0] != VCDIFF_MAGIC1 ||
	    stream->dec_magic[1] != VCDIFF_MAGIC2 ||
	    stream->dec_magic[2] != VCDIFF_MAGIC3)
	  {
	    stream->msg = "not a VCDIFF input";
	    return XD3_INVALID_INPUT;
	  }

	if (stream->dec_magic[3] != 0)
	  {
	    stream->msg = "VCDIFF input version > 0 is not supported";
	    return XD3_INVALID_INPUT;
	  }

	stream->dec_state = DEC_HDRIND;
      }
    case DEC_HDRIND:
      {
	if ((ret = xd3_decode_byte (stream, & stream->dec_hdr_ind)))
	  {
	    return ret;
	  }

	if ((stream->dec_hdr_ind & VCD_INVHDR) != 0)
	  {
	    stream->msg = "unrecognized header indicator bits set";
	    return XD3_INVALID_INPUT;
	  }

	stream->dec_state = DEC_SECONDID;
      }

    case DEC_SECONDID:
      /* Secondary compressor ID: only if VCD_SECONDARY is set */
      if ((stream->dec_hdr_ind & VCD_SECONDARY) != 0)
	{
	  BYTE_CASE (1, stream->dec_secondid, DEC_TABLEN);

	  switch (stream->dec_secondid)
	    {
	    case VCD_FGK_ID:
	      FGK_CASE (stream);
	    case VCD_DJW_ID:
	      DJW_CASE (stream);
	    case VCD_LZMA_ID:
	      LZMA_CASE (stream);
	    default:
	      stream->msg = "unknown secondary compressor ID";
	      return XD3_INVALID_INPUT;
	    }
	}

    case DEC_TABLEN:
      /* Length of code table data: only if VCD_CODETABLE is set */
      SIZE_CASE ((stream->dec_hdr_ind & VCD_CODETABLE) != 0,
		 stream->dec_codetblsz, DEC_NEAR);

      /* The codetblsz counts the two NEAR/SAME bytes */
      if ((stream->dec_hdr_ind & VCD_CODETABLE) != 0) {
	if (stream->dec_codetblsz <= 2) {
	  stream->msg = "invalid code table size";
	  return ENOMEM;
	}
	stream->dec_codetblsz -= 2;
      }
    case DEC_NEAR:
      /* Near modes: only if VCD_CODETABLE is set */
      BYTE_CASE((stream->dec_hdr_ind & VCD_CODETABLE) != 0,
		stream->acache.s_near, DEC_SAME);
    case DEC_SAME:
      /* Same modes: only if VCD_CODETABLE is set */
      BYTE_CASE((stream->dec_hdr_ind & VCD_CODETABLE) != 0,
		stream->acache.s_same, DEC_TABDAT);
    case DEC_TABDAT:
      /* Compressed code table data */

      if ((stream->dec_hdr_ind & VCD_CODETABLE) != 0)
	{
	  stream->msg = "VCD_CODETABLE support was removed";
	  return XD3_UNIMPLEMENTED;
	}
      else
	{
	  /* Use the default table. */
	  stream->acache.s_near = __rfc3284_code_table_desc.near_modes;
	  stream->acache.s_same = __rfc3284_code_table_desc.same_modes;
	  stream->code_table    = xd3_rfc3284_code_table ();
	}

      if ((ret = xd3_alloc_cache (stream))) { return ret; }

      stream->dec_state = DEC_APPLEN;

    case DEC_APPLEN:
      /* Length of application data */
      SIZE_CASE((stream->dec_hdr_ind & VCD_APPHEADER) != 0,
		stream->dec_appheadsz, DEC_APPDAT);

    case DEC_APPDAT:
      /* Application data */
      if (stream->dec_hdr_ind & VCD_APPHEADER)
	{
	  /* Note: we add an additional byte for padding, to allow
	     0-termination. Check for overflow: */
	  if (USIZE_T_OVERFLOW(stream->dec_appheadsz, 1))
	    {
	      stream->msg = "exceptional appheader size";
	      return XD3_INVALID_INPUT;
	    }

	  if ((stream->dec_appheader == NULL) &&
	      (stream->dec_appheader =
	       (uint8_t*) xd3_alloc (stream,
				     stream->dec_appheadsz+1, 1)) == NULL)
	    {
	      return ENOMEM;
	    }

	  stream->dec_appheader[stream->dec_appheadsz] = 0;

	  if ((ret = xd3_decode_bytes (stream, stream->dec_appheader,
				       & stream->dec_appheadbytes,
				       stream->dec_appheadsz)))
	    {
	      return ret;
	    }
	}

      /* xoff_t -> usize_t is safe because this is the first block. */
      stream->dec_hdrsize = (usize_t) stream->total_in;
      stream->dec_state = DEC_WININD;

    case DEC_WININD:
      {
	/* Start of a window: the window indicator */
	if ((ret = xd3_decode_byte (stream, & stream->dec_win_ind)))
	  {
	    return ret;
	  }

	stream->current_window = stream->dec_window_count;

	if (XOFF_T_OVERFLOW (stream->dec_winstart, stream->dec_tgtlen))
	  {
	    stream->msg = "decoder file offset overflow";
	    return XD3_INVALID_INPUT;
	  }

	stream->dec_winstart += stream->dec_tgtlen;

	if ((stream->dec_win_ind & VCD_INVWIN) != 0)
	  {
	    stream->msg = "unrecognized window indicator bits set";
	    return XD3_INVALID_INPUT;
	  }

	if ((ret = xd3_decode_init_window (stream))) { return ret; }

	stream->dec_state = DEC_CPYLEN;

	IF_DEBUG2 (DP(RINT "--------- TARGET WINDOW %"Q"u -----------\n",
		      stream->current_window));
      }

    case DEC_CPYLEN:
      /* Copy window length: only if VCD_SOURCE or VCD_TARGET is set */
      SIZE_CASE(SRCORTGT (stream->dec_win_ind), stream->dec_cpylen,
		DEC_CPYOFF);

      /* Set the initial, logical decoder position (HERE address) in
       * dec_position.  This is set to just after the source/copy
       * window, as we are just about to output the first byte of
       * target window. */
      stream->dec_position = stream->dec_cpylen;

    case DEC_CPYOFF:
      /* Copy window offset: only if VCD_SOURCE or VCD_TARGET is set */
      OFFSET_CASE(SRCORTGT (stream->dec_win_ind), stream->dec_cpyoff,
		  DEC_ENCLEN);

      /* Copy offset and copy length may not overflow. */
      if (XOFF_T_OVERFLOW (stream->dec_cpyoff, stream->dec_cpylen))
	{
	  stream->msg = "decoder copy window overflows a file offset";
	  return XD3_INVALID_INPUT;
	}

      /* Check copy window bounds: VCD_TARGET window may not exceed
	 current position. */
      if ((stream->dec_win_ind & VCD_TARGET) &&
	  (stream->dec_cpyoff + stream->dec_cpylen >
	   stream->dec_winstart))
	{
	  stream->msg = "VCD_TARGET window out of bounds";
	  return XD3_INVALID_INPUT;
	}

    case DEC_ENCLEN:
      /* Length of the delta encoding */
      SIZE_CASE(1, stream->dec_enclen, DEC_TGTLEN);
    case DEC_TGTLEN:
      /* Length of target window */
      SIZE_CASE(1, stream->dec_tgtlen, DEC_DELIND);

      /* Set the maximum decoder position, beyond which we should not
       * decode any data.  This is the maximum value for dec_position.
       * This may not exceed the size of a usize_t. */
      if (USIZE_T_OVERFLOW (stream->dec_cpylen, stream->dec_tgtlen))
	{
	  stream->msg = "decoder target window overflows a usize_t";
	  return XD3_INVALID_INPUT;
	}

      /* Check for malicious files. */
      if (stream->dec_tgtlen > XD3_HARDMAXWINSIZE)
	{
	  stream->msg = "hard window size exceeded";
	  return XD3_INVALID_INPUT;
	}

      stream->dec_maxpos = stream->dec_cpylen + stream->dec_tgtlen;

    case DEC_DELIND:
      /* Delta indicator */
      BYTE_CASE(1, stream->dec_del_ind, DEC_DATALEN);

      if ((stream->dec_del_ind & VCD_INVDEL) != 0)
	{
	  stream->msg = "unrecognized delta indicator bits set";
	  return XD3_INVALID_INPUT;
	}

      /* Delta indicator is only used with secondary compression. */
      if ((stream->dec_del_ind != 0) && (stream->sec_type == NULL))
	{
	  stream->msg = "invalid delta indicator bits set";
	  return XD3_INVALID_INPUT;
	}

      /* Section lengths */
    case DEC_DATALEN:
      SIZE_CASE(1, stream->data_sect.size, DEC_INSTLEN);
    case DEC_INSTLEN:
      SIZE_CASE(1, stream->inst_sect.size, DEC_ADDRLEN);
    case DEC_ADDRLEN:
      SIZE_CASE(1, stream->addr_sect.size, DEC_CKSUM);

    case DEC_CKSUM:
      /* Window checksum. */
      if ((stream->dec_win_ind & VCD_ADLER32) != 0)
	{
	  int i;

	  if ((ret = xd3_decode_bytes (stream, stream->dec_cksum,
				       & stream->dec_cksumbytes, 4)))
	    {
	      return ret;
	    }

	  for (i = 0; i < 4; i += 1)
	    {
	      stream->dec_adler32 =
		(stream->dec_adler32 << 8) | stream->dec_cksum[i];
	    }
	}

      stream->dec_state = DEC_DATA;

      /* Check dec_enclen for redundency, otherwise it is not really used. */
      {
	usize_t enclen_check =
	  (1 + (xd3_sizeof_size (stream->dec_tgtlen) +
		xd3_sizeof_size (stream->data_sect.size) +
		xd3_sizeof_size (stream->inst_sect.size) +
		xd3_sizeof_size (stream->addr_sect.size)) +
	   stream->data_sect.size +
	   stream->inst_sect.size +
	   stream->addr_sect.size +
	   ((stream->dec_win_ind & VCD_ADLER32) ? 4 : 0));

	if (stream->dec_enclen != enclen_check)
	  {
	    stream->msg = "incorrect encoding length (redundent)";
	    return XD3_INVALID_INPUT;
	  }
      }

      /* Returning here gives the application a chance to inspect the
       * header, skip the window, etc. */
      if (stream->current_window == 0) { return XD3_GOTHEADER; }
      else                             { return XD3_WINSTART; }

    case DEC_DATA:
    case DEC_INST:
    case DEC_ADDR:
      /* Next read the three sections. */
     if ((ret = xd3_decode_sections (stream))) { return ret; }

    case DEC_EMIT:

      /* To speed VCD_SOURCE block-address calculations, the source
       * cpyoff_blocks and cpyoff_blkoff are pre-computed. */
      if (stream->dec_win_ind & VCD_SOURCE)
	{
	  xd3_source *src = stream->src;

	  if (src == NULL)
	    {
	      stream->msg = "source input required";
	      return XD3_INVALID_INPUT;
	    }

	  xd3_blksize_div(stream->dec_cpyoff, src,
			  &src->cpyoff_blocks,
			  &src->cpyoff_blkoff);
	  
	  IF_DEBUG2(DP(RINT
		       "[decode_cpyoff] %"Q"u "
		       "cpyblkno %"Q"u "
		       "cpyblkoff %"W"u "
		       "blksize %"W"u\n",
		       stream->dec_cpyoff,
		       src->cpyoff_blocks,
		       src->cpyoff_blkoff,
		       src->blksize));
	}

      /* xd3_decode_emit returns XD3_OUTPUT on every success. */
      if ((ret = xd3_decode_emit (stream)) == XD3_OUTPUT)
	{
	  stream->total_out += stream->avail_out;
	}

      return ret;

    case DEC_FINISH:
      {
	if (stream->dec_win_ind & VCD_TARGET)
	  {
	    if (stream->dec_lastwin == NULL)
	      {
		stream->dec_lastwin   = stream->next_out;
		stream->dec_lastspace = stream->space_out;
	      }
	    else
	      {
		xd3_swap_uint8p (& stream->dec_lastwin,
				 & stream->next_out);
		xd3_swap_usize_t (& stream->dec_lastspace,
				  & stream->space_out);
	      }
	  }

	stream->dec_lastlen   = stream->dec_tgtlen;
	stream->dec_laststart = stream->dec_winstart;
	stream->dec_window_count += 1;

	/* Note: the updates to dec_winstart & current_window are
	 * deferred until after the next DEC_WININD byte is read. */
	stream->dec_state = DEC_WININD;
	return XD3_WINFINISH;
      }

    default:
      stream->msg = "invalid state";
      return XD3_INVALID_INPUT;
    }
}

#endif // _XDELTA3_DECODE_H_
