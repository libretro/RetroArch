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
#ifndef _XDELTA3_SECOND_H_
#define _XDELTA3_SECOND_H_

static inline void xd3_bit_state_encode_init (bit_state *bits)
{
  bits->cur_byte = 0;
  bits->cur_mask = 1;
}

static inline int xd3_decode_bits (xd3_stream     *stream,
				   bit_state      *bits,
				   const uint8_t **input,
				   const uint8_t  *input_max,
				   usize_t         nbits,
				   usize_t        *valuep)
{
  usize_t value = 0;
  usize_t vmask = 1 << nbits;

  if (bits->cur_mask == 0x100) { goto next_byte; }

  for (;;)
    {
      do
	{
	  vmask >>= 1;

	  if (bits->cur_byte & bits->cur_mask)
	    {
	      value |= vmask;
	    }

	  bits->cur_mask <<= 1;

	  if (vmask == 1) { goto done; }
	}
      while (bits->cur_mask != 0x100);

    next_byte:

      if (*input == input_max)
	{
	  stream->msg = "secondary decoder end of input";
	  return XD3_INTERNAL;
	}

      bits->cur_byte = *(*input)++;
      bits->cur_mask = 1;
    }

 done:

  IF_DEBUG2 (DP(RINT "(d) %"W"u ", value));

  (*valuep) = value;
  return 0;
}

#if REGRESSION_TEST
/* There may be extra bits at the end of secondary decompression, this macro
 * checks for non-zero bits.  This is overly strict, but helps pass the
 * single-bit-error regression test. */
static int
xd3_test_clean_bits (xd3_stream *stream, bit_state *bits)
{
  for (; bits->cur_mask != 0x100; bits->cur_mask <<= 1)
    {
      if (bits->cur_byte & bits->cur_mask)
	{
	  stream->msg = "secondary decoder garbage";
	  return XD3_INTERNAL;
	}
    }

  return 0;
}
#endif

static int
xd3_get_secondary (xd3_stream *stream, xd3_sec_stream **sec_streamp, 
		   int is_encode)
{
  if (*sec_streamp == NULL)
    {
      int ret;

      if ((*sec_streamp = stream->sec_type->alloc (stream)) == NULL)
	{
	  stream->msg = "error initializing secondary stream";
	  return XD3_INVALID;
	}

      if ((ret = stream->sec_type->init (stream, *sec_streamp, is_encode)) != 0)
	{
	  return ret;
	}
    }

  return 0;
}

static int
xd3_decode_secondary (xd3_stream      *stream,
		      xd3_desect      *sect,
		      xd3_sec_stream **sec_streamp)
{
  usize_t dec_size;
  uint8_t *out_used;
  int ret;

  if ((ret = xd3_get_secondary (stream, sec_streamp, 0)) != 0)
    {
      return ret;
    }

  /* Decode the size, allocate the buffer. */
  if ((ret = xd3_read_size (stream, & sect->buf,
			    sect->buf_max, & dec_size)) ||
      (ret = xd3_decode_allocate (stream, dec_size,
				  & sect->copied2, & sect->alloc2)))
    {
      return ret;
    }

  if (dec_size == 0)
    {
      stream->msg = "secondary decoder invalid output size";
      return XD3_INVALID_INPUT;
    }

  out_used = sect->copied2;

  if ((ret = stream->sec_type->decode (stream, *sec_streamp,
				       & sect->buf, sect->buf_max,
				       & out_used, out_used + dec_size)))
    {
      return ret;
    }

  if (sect->buf != sect->buf_max)
    {
      stream->msg = "secondary decoder finished with unused input";
      return XD3_INTERNAL;
    }

  if (out_used != sect->copied2 + dec_size)
    {
      stream->msg = "secondary decoder short output";
      return XD3_INTERNAL;
    }

  sect->buf = sect->copied2;
  sect->buf_max = sect->copied2 + dec_size;
  sect->size = dec_size;

  return 0;
}

#if XD3_ENCODER
static inline int xd3_encode_bit (xd3_stream      *stream,
				  xd3_output     **output,
				  bit_state       *bits,
				  usize_t          bit)
{
  int ret;

  if (bit)
    {
      bits->cur_byte |= bits->cur_mask;
    }

  /* OPT: Might help to buffer more than 8 bits at once. */
  if (bits->cur_mask == 0x80)
    {
      if ((ret = xd3_emit_byte (stream, output, bits->cur_byte)) != 0)
	{
	  return ret;
	}

      bits->cur_mask = 1;
      bits->cur_byte = 0;
    }
  else
    {
      bits->cur_mask <<= 1;
    }

  return 0;
}

static inline int xd3_flush_bits (xd3_stream      *stream,
				  xd3_output     **output,
				  bit_state       *bits)
{
  return (bits->cur_mask == 1) ? 0 :
    xd3_emit_byte (stream, output, bits->cur_byte);
}

static inline int xd3_encode_bits (xd3_stream      *stream,
				   xd3_output     **output,
				   bit_state       *bits,
				   usize_t           nbits,
				   usize_t           value)
{
  int ret;
  usize_t mask = 1 << nbits;

  XD3_ASSERT (nbits > 0);
  XD3_ASSERT (nbits < sizeof (usize_t) * 8);
  XD3_ASSERT (value < mask);

  do
    {
      mask >>= 1;

      if ((ret = xd3_encode_bit (stream, output, bits, value & mask)))
	{
	  return ret;
	}
    }
  while (mask != 1);

  IF_DEBUG2 (DP(RINT "(e) %"W"u ", value));

  return 0;
}

static int
xd3_encode_secondary (xd3_stream      *stream,
		      xd3_output     **head,
		      xd3_output     **tail,
		      xd3_sec_stream **sec_streamp,
		      xd3_sec_cfg     *cfg,
		      int             *did_it)
{
  xd3_output     *tmp_head;
  xd3_output     *tmp_tail;

  usize_t comp_size;
  usize_t orig_size;

  int ret;

  orig_size = xd3_sizeof_output (*head);

  if (orig_size < SECONDARY_MIN_INPUT) { return 0; }

  if ((ret = xd3_get_secondary (stream, sec_streamp, 1)) != 0)
    {
      return ret;
    }

  tmp_head = xd3_alloc_output (stream, NULL);

  /* Encode the size, encode the data.  Encoding the size makes it
   * simpler, but is a little gross.  Should not need the entire
   * section in contiguous memory, but it is much easier this way. */
  if ((ret = xd3_emit_size (stream, & tmp_head, orig_size)) ||
      (ret = stream->sec_type->encode (stream, *sec_streamp, *head,
				       tmp_head, cfg)))
    {
      goto getout;
    }

  /* If the secondary compressor determines it's no good, it returns
   * XD3_NOSECOND. */

  /* Setup tmp_tail, comp_size */
  tmp_tail  = tmp_head;
  comp_size = tmp_head->next;

  while (tmp_tail->next_page != NULL)
    {
      tmp_tail = tmp_tail->next_page;
      comp_size += tmp_tail->next;
    }

  XD3_ASSERT (comp_size == xd3_sizeof_output (tmp_head));
  XD3_ASSERT (tmp_tail != NULL);

  if (comp_size < (orig_size - SECONDARY_MIN_SAVINGS) || cfg->inefficient)
    {
      if (comp_size < orig_size)
	{
	  IF_DEBUG1(DP(RINT "[encode_secondary] saved %"W"u bytes: %"W"u -> %"W"u (%0.2f%%)\n",
		       orig_size - comp_size, orig_size, comp_size,
		       100.0 * (double) comp_size / (double) orig_size));
	}

      xd3_free_output (stream, *head);

      *head = tmp_head;
      *tail = tmp_tail;
      *did_it = 1;
    }
  else
    {
    getout:
      if (ret == XD3_NOSECOND) { ret = 0; }
      xd3_free_output (stream, tmp_head);
    }

  return ret;
}
#endif /* XD3_ENCODER */
#endif /* _XDELTA3_SECOND_H_ */
