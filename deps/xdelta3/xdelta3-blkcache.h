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

#include "xdelta3-internal.h"

typedef struct _main_blklru      main_blklru;
typedef struct _main_blklru_list main_blklru_list;


#define XD3_INVALID_OFFSET XOFF_T_MAX

struct _main_blklru_list
{
  main_blklru_list  *next;
  main_blklru_list  *prev;
};

struct _main_blklru
{
  uint8_t          *blk;
  xoff_t            blkno;
  usize_t           size;
  main_blklru_list  link;
};

XD3_MAKELIST(main_blklru_list,main_blklru,link);

static usize_t           lru_size = 0;
static main_blklru      *lru = NULL;  /* array of lru_size elts */
static main_blklru_list  lru_list;
static int               do_src_fifo = 0;  /* set to avoid lru */

static int lru_hits   = 0;
static int lru_misses = 0;
static int lru_filled = 0;

static void main_lru_reset (void)
{
  lru_size = 0;
  lru = NULL;
  do_src_fifo = 0;
  lru_hits   = 0;
  lru_misses = 0;
  lru_filled = 0;
}

static void main_lru_cleanup (void)
{
  if (lru != NULL)
    {
      main_buffree (lru[0].blk);
    }

  main_free (lru);
  lru = NULL;

  lru_hits = 0;
  lru_misses = 0;
  lru_filled = 0;
}

/* This is called at different times for encoding and decoding.  The
 * encoder calls it immediately, the decoder delays until the
 * application header is received.  */
static int
main_set_source (xd3_stream *stream, xd3_cmd cmd,
		 main_file *sfile, xd3_source *source)
{
  int ret = 0;
  usize_t i;
  xoff_t source_size = 0;
  usize_t blksize;

  XD3_ASSERT (lru == NULL);
  XD3_ASSERT (stream->src == NULL);
  XD3_ASSERT (option_srcwinsz >= XD3_MINSRCWINSZ);

  /* TODO: this code needs refactoring into FIFO, LRU, FAKE.  Yuck!
   * This is simplified from 3.0z which had issues with sizing the
   * source buffer memory allocation and the source blocksize. */

  /* LRU-specific */
  main_blklru_list_init (& lru_list);

  if (allow_fake_source)
    {
      /* TODO: refactor
       * TOOLS/recode-specific: Check "allow_fake_source" mode looks
       * broken now. */
      sfile->mode = XO_READ;
      sfile->realname = sfile->filename;
      sfile->nread = 0;
    }
  else
    {
      /* Either a regular file (possibly compressed) or a FIFO
       * (possibly compressed). */
      if ((ret = main_file_open (sfile, sfile->filename, XO_READ)))
	{
	  return ret;
	}

      /* If the file is regular we know it's size.  If the file turns
       * out to be externally compressed, size_known may change. */
      sfile->size_known = (main_file_stat (sfile, &source_size) == 0);
    }

  /* Note: The API requires a power-of-two blocksize and srcwinsz
   * (-B).  The logic here will use a single block if the entire file
   * is known to fit into srcwinsz. */
  option_srcwinsz = xd3_xoff_roundup (option_srcwinsz);

  /* Though called "lru", it is not LRU-specific.  We always allocate
   * a maximum number of source block buffers.  If the entire file
   * fits into srcwinsz, this buffer will stay as the only
   * (lru_size==1) source block.  Otherwise, we know that at least
   * option_srcwinsz bytes are available.  Split the source window
   * into buffers. */
  if ((lru = (main_blklru*) main_malloc (MAX_LRU_SIZE *
					 sizeof (main_blklru))) == NULL)
    {
      ret = ENOMEM;
      return ret;
    }

  memset (lru, 0, sizeof(lru[0]) * MAX_LRU_SIZE);

  /* Allocate the entire buffer. */
  if ((lru[0].blk = (uint8_t*) main_bufalloc (option_srcwinsz)) == NULL)
    {
      ret = ENOMEM;
      return ret;
    }

  /* Main calls main_getblk_func() once before xd3_set_source().  This
   * is the point at which external decompression may begin.  Set the
   * system for a single block. */
  lru_size = 1;
  lru[0].blkno = XD3_INVALID_OFFSET;
  blksize = option_srcwinsz;
  main_blklru_list_push_back (& lru_list, & lru[0]);
  XD3_ASSERT (blksize != 0);

  /* Initialize xd3_source. */
  source->blksize  = blksize;
  source->name     = sfile->filename;
  source->ioh      = sfile;
  source->curblkno = XD3_INVALID_OFFSET;
  source->curblk   = NULL;
  source->max_winsize = option_srcwinsz;

  if ((ret = main_getblk_func (stream, source, 0)) != 0)
    {
      XPR(NT "error reading source: %s: %s\n",
	  sfile->filename,
	  xd3_mainerror (ret));
      return ret;
    }

  source->onblk = lru[0].size;  /* xd3 sets onblk */

  /* If the file is smaller than a block, size is known. */
  if (!sfile->size_known && source->onblk < blksize)
    {
      source_size = source->onblk;
      source->onlastblk = source_size;
      sfile->size_known = 1;
    }

  /* If the size is not known or is greater than the buffer size, we
   * split the buffer across MAX_LRU_SIZE blocks (already allocated in
   * "lru"). */
  if (!sfile->size_known || source_size > option_srcwinsz)
    {
      /* Modify block 0, change blocksize. */
      blksize = option_srcwinsz / MAX_LRU_SIZE;
      source->blksize = blksize;
      source->onblk = blksize;
      source->onlastblk = blksize;
      source->max_blkno = MAX_LRU_SIZE - 1;

      lru[0].size = blksize;
      lru_size = MAX_LRU_SIZE;

      /* Setup rest of blocks. */
      for (i = 1; i < lru_size; i += 1)
	{
	  lru[i].blk = lru[0].blk + (blksize * i);
	  lru[i].blkno = i;
	  lru[i].size = blksize;
	  main_blklru_list_push_back (& lru_list, & lru[i]);
	}
    }

  if (! sfile->size_known)
    {
      /* If the size is not know, we must use FIFO discipline. */
      do_src_fifo = 1;
    }

  /* Call the appropriate set_source method, handle errors, print
   * verbose message, etc. */
  if (sfile->size_known)
    {
      ret = xd3_set_source_and_size (stream, source, source_size);
    }
  else
    {
      ret = xd3_set_source (stream, source);
    }

  if (ret)
    {
      XPR(NT XD3_LIB_ERRMSG (stream, ret));
      return ret;
    }

  XD3_ASSERT (stream->src == source);
  XD3_ASSERT (source->blksize == blksize);

  if (option_verbose)
    {
      static shortbuf srcszbuf;
      static shortbuf srccntbuf;
      static shortbuf winszbuf;
      static shortbuf blkszbuf;
      static shortbuf nbufs;

      if (sfile->size_known)
	{
	  short_sprintf (srcszbuf, "source size %s [%"Q"u]",
			 main_format_bcnt (source_size, &srccntbuf),
			 source_size);
	}
      else
	{
	  short_sprintf (srcszbuf, "%s", "source size unknown");
	}

      nbufs.buf[0] = 0;

      if (option_verbose > 1)
	{
	  short_sprintf (nbufs, " #bufs %"W"u", lru_size);
	}

      XPR(NT "source %s %s blksize %s window %s%s%s\n",
	  sfile->filename,
	  srcszbuf.buf,
	  main_format_bcnt (blksize, &blkszbuf),
	  main_format_bcnt (option_srcwinsz, &winszbuf),
	  nbufs.buf,
	  do_src_fifo ? " (FIFO)" : "");
    }

  return 0;
}

static int
main_getblk_lru (xd3_source *source, xoff_t blkno,
		 main_blklru** blrup, int *is_new)
{
  main_blklru *blru = NULL;
  usize_t i;

  (*is_new) = 0;

  if (do_src_fifo)
    {
      /* Direct lookup assumes sequential scan w/o skipping blocks. */
      int idx = blkno % lru_size;
      blru = & lru[idx];
      if (blru->blkno == blkno)
	{
	  (*blrup) = blru;
	  return 0;
	}
      /* No going backwards in a sequential scan. */
      if (blru->blkno != XD3_INVALID_OFFSET && blru->blkno > blkno)
	{
	  return XD3_TOOFARBACK;
	}
    }
  else
    {
      /* Sequential search through LRU. */
      for (i = 0; i < lru_size; i += 1)
	{
	  blru = & lru[i];
	  if (blru->blkno == blkno)
	    {
	      main_blklru_list_remove (blru);
	      main_blklru_list_push_back (& lru_list, blru);
	      (*blrup) = blru;
	      IF_DEBUG1 (DP(RINT "[getblk_lru] HIT blkno = %"Q"u lru_size=%"W"u\n",
		    blkno, lru_size));
	      return 0;
	    }
	}
      IF_DEBUG1 (DP(RINT "[getblk_lru] MISS blkno = %"Q"u lru_size=%"W"u\n",
		    blkno, lru_size));
    }

  if (do_src_fifo)
    {
      int idx = blkno % lru_size;
      blru = & lru[idx];
    }
  else
    {
      XD3_ASSERT (! main_blklru_list_empty (& lru_list));
      blru = main_blklru_list_pop_front (& lru_list);
      main_blklru_list_push_back (& lru_list, blru);
    }

  lru_filled += 1;
  (*is_new) = 1;
  (*blrup) = blru;
  blru->blkno = XD3_INVALID_OFFSET;
  return 0;
}

static int
main_read_seek_source (xd3_stream *stream,
		       xd3_source *source,
		       xoff_t      blkno) {
  xoff_t pos = blkno * source->blksize;
  main_file *sfile = (main_file*) source->ioh;
  main_blklru *blru;
  int is_new;
  size_t nread = 0;
  int ret = 0;

  if (!sfile->seek_failed)
    {
      ret = main_file_seek (sfile, pos);

      if (ret == 0)
	{
	  sfile->source_position = pos;
	}
    }

  if (sfile->seek_failed || ret != 0)
    {
      /* For an unseekable file (or other seek error, does it
       * matter?) */
      if (sfile->source_position > pos)
	{
	  /* Could assert !IS_ENCODE(), this shouldn't happen
	   * because of do_src_fifo during encode. */
	  if (!option_quiet)
	    {
	      XPR(NT "source can't seek backwards; requested block offset "
		  "%"Q"u source position is %"Q"u\n",
		  pos, sfile->source_position);
	    }

	  sfile->seek_failed = 1;
	  stream->msg = "non-seekable source: "
	    "copy is too far back (try raising -B)";
	  return XD3_TOOFARBACK;
	}

      /* There's a chance here, that an genuine lseek error will cause
       * xdelta3 to shift into non-seekable mode, entering a degraded
       * condition.  */
      if (!sfile->seek_failed && option_verbose)
	{
	  XPR(NT "source can't seek, will use FIFO for %s\n",
	      sfile->filename);

	  if (option_verbose > 1)
	    {
	      XPR(NT "seek error at offset %"Q"u: %s\n",
		  pos, xd3_mainerror (ret));
	    }
	}

      sfile->seek_failed = 1;

      if (option_verbose > 1 && pos != sfile->source_position)
	{
	  XPR(NT "non-seekable source skipping %"Q"u bytes @ %"Q"u\n",
	      pos - sfile->source_position,
	      sfile->source_position);
	}

      while (sfile->source_position < pos)
	{
	  xoff_t skip_blkno;
	  usize_t skip_offset;

	  xd3_blksize_div (sfile->source_position, source,
			   &skip_blkno, &skip_offset);

	  /* Read past unused data */
	  XD3_ASSERT (pos - sfile->source_position >= source->blksize);
	  XD3_ASSERT (skip_offset == 0);

	  if ((ret = main_getblk_lru (source, skip_blkno,
				      & blru, & is_new)))
	    {
	      return ret;
	    }

	  XD3_ASSERT (is_new);
	  blru->blkno = skip_blkno;

	  if ((ret = main_read_primary_input (sfile,
					      (uint8_t*) blru->blk,
					      source->blksize,
					      & nread)))
	    {
	      return ret;
	    }

	  if (nread != source->blksize)
	    {
	      IF_DEBUG1 (DP(RINT "[getblk] short skip block nread = %"Z"u\n",
			    nread));
	      stream->msg = "non-seekable input is short";
	      return XD3_INVALID_INPUT;
	    }

	  sfile->source_position += nread;
	  blru->size = nread;

	  IF_DEBUG1 (DP(RINT "[getblk] skip blkno %"Q"u size %"W"u\n",
			skip_blkno, blru->size));

	  XD3_ASSERT (sfile->source_position <= pos);
	}
    }

  return 0;
}

/* This is the callback for reading a block of source.  This function
 * is blocking and it implements a small LRU.
 *
 * Note that it is possible for main_input() to handle getblk requests
 * in a non-blocking manner.  If the callback is NULL then the caller
 * of xd3_*_input() must handle the XD3_GETSRCBLK return value and
 * fill the source in the same way.  See xd3_getblk for details.  To
 * see an example of non-blocking getblk, see xdelta-test.h. */
static int
main_getblk_func (xd3_stream *stream,
		  xd3_source *source,
		  xoff_t      blkno)
{
  int ret = 0;
  xoff_t pos = blkno * source->blksize;
  main_file *sfile = (main_file*) source->ioh;
  main_blklru *blru;
  int is_new;
  size_t nread = 0;

  if (allow_fake_source)
    {
      source->curblkno = blkno;
      source->onblk    = 0;
      source->curblk   = lru[0].blk;
      lru[0].size = 0;
      return 0;
    }

  if ((ret = main_getblk_lru (source, blkno, & blru, & is_new)))
    {
      return ret;
    }

  if (!is_new)
    {
      source->curblkno = blkno;
      source->onblk    = blru->size;
      source->curblk   = blru->blk;
      lru_hits++;
      return 0;
    }

  lru_misses += 1;

  if (pos != sfile->source_position)
    {
      /* Only try to seek when the position is wrong.  This means the
       * decoder will fail when the source buffer is too small, but
       * only when the input is non-seekable. */
      if ((ret = main_read_seek_source (stream, source, blkno)))
	{
	  return ret;
	}
    }

  XD3_ASSERT (sfile->source_position == pos);

  if ((ret = main_read_primary_input (sfile,
				      (uint8_t*) blru->blk,
				      source->blksize,
				      & nread)))
    {
      return ret;
    }

  /* Save the last block read, used to handle non-seekable files. */
  sfile->source_position = pos + nread;

  if (option_verbose > 3)
    {
      if (blru->blkno != XD3_INVALID_OFFSET)
	{
	  if (blru->blkno != blkno)
	    {
	      XPR(NT "source block %"Q"u read %"Z"u ejects %"Q"u (lru_hits=%u, "
		  "lru_misses=%u, lru_filled=%u)\n",
		  blkno, nread, blru->blkno, lru_hits, lru_misses, lru_filled);
	    }
	  else
	    {
	      XPR(NT "source block %"Q"u read %"Z"u (lru_hits=%u, "
		  "lru_misses=%u, lru_filled=%u)\n",
		  blkno, nread, lru_hits, lru_misses, lru_filled);
	    }
	}
      else
	{
	  XPR(NT "source block %"Q"u read %"Z"u (lru_hits=%u, lru_misses=%u, "
	      "lru_filled=%u)\n", blkno, nread, 
	      lru_hits, lru_misses, lru_filled);
	}
    }

  source->curblk   = blru->blk;
  source->curblkno = blkno;
  source->onblk    = nread;
  blru->size       = nread;
  blru->blkno      = blkno;

  IF_DEBUG1 (DP(RINT "[main_getblk] blkno %"Q"u onblk %"Z"u pos %"Q"u "
		"srcpos %"Q"u\n",
		blkno, nread, pos, sfile->source_position));

  return 0;
}
