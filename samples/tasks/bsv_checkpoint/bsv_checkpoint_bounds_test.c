/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (bsv_checkpoint_bounds_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Bounds test for the checkpoints in a .bsv replay file, read by
 * bsv_movie_load_checkpoint() in input/bsv/bsvmovie.c.
 *
 * A checkpoint starts with three sizes from the file: the state
 * size, the encoded size and the compressed size.  cur_save holds
 * 'size' bytes, uncompressed data is read straight into its decode
 * buffer, and uncompressed raw data is read into cur_save itself,
 * which the movie keeps.  The test feeds the function checkpoints
 * whose sizes disagree, truncated checkpoints and skipped ones, with
 * buffers at their exact sizes so AddressSanitizer reports any
 * overflow, double free or leak, and checks that cur_save_size never
 * claims more than cur_save holds.
 *
 * The function below is a verbatim copy, built against the stand-ins
 * that follow; only the uncompressed path is compiled (no zlib, no
 * zstd, no statestream).  If bsv_movie_load_checkpoint changes, the
 * copy must follow. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RARCH_ERR(...)  ((void)0)
#define RARCH_WARN(...) ((void)0)
#define swap_if_big32(x) (x)
#define BSV_FLAG_MOVIE_END 1

/* Production values from input/input_driver.h. */
enum
{
   REPLAY_CHECKPOINT2_COMPRESSION_NONE = 0,
   REPLAY_CHECKPOINT2_COMPRESSION_ZLIB,
   REPLAY_CHECKPOINT2_COMPRESSION_ZSTD
};
enum
{
   REPLAY_CHECKPOINT2_ENCODING_RAW = 0,
   REPLAY_CHECKPOINT2_ENCODING_STATESTREAM
};
typedef enum
{
   REPLAY_CPBEHAVIOR_DESERIALIZE = 0,
   REPLAY_CPBEHAVIOR_UPDATE,
   REPLAY_CPBEHAVIOR_SKIP
} replay_checkpoint_behavior;

/* An in-memory stream: reads stop at the end of the data. */
typedef struct
{
   const uint8_t *data;
   size_t len;
   size_t pos;
} intfstream_t;

static int64_t intfstream_read(intfstream_t *f, void *dst, size_t len)
{
   size_t left = f->len - f->pos;
   if (left > len)
      left = len;
   memcpy(dst, f->data + f->pos, left);
   f->pos += left;
   return (int64_t)left;
}

static int64_t intfstream_seek(intfstream_t *f, int64_t off, int whence)
{
   (void)whence;
   f->pos = (f->pos + (size_t)off > f->len) ? f->len : f->pos + (size_t)off;
   return 0;
}

typedef struct
{
   intfstream_t *file;
   uint8_t *cur_save;
   size_t cur_save_size;
   bool cur_save_valid;
   bool checkpoint_ready;
   size_t last_save_size;
} bsv_movie_t;

typedef struct
{
   struct { unsigned flags; } bsv_movie_state;
} input_driver_state_t;

static input_driver_state_t g_input_st;
static input_driver_state_t *input_state_get_ptr(void) { return &g_input_st; }

/* === verbatim copy: input/bsv/bsvmovie.c === */
bool bsv_movie_load_checkpoint(bsv_movie_t *handle, uint8_t compression,
      uint8_t encoding,replay_checkpoint_behavior checkpoint_behavior)
{
   uint32_t compressed_encoded_size, encoded_size, size;
   input_driver_state_t *input_st = input_state_get_ptr();
   uint8_t *compressed_data = NULL, *encoded_data = NULL;
   bool ret = true;
   if (intfstream_read(handle->file, &(size),
               sizeof(uint32_t)) != sizeof(uint32_t))
   {
      RARCH_ERR("[Replay] Replay truncated before uncompressed unencoded size\n");
      ret = false;
      goto exit;
   }
   if (intfstream_read(handle->file, &(encoded_size),
               sizeof(uint32_t)) != sizeof(uint32_t))
   {
      RARCH_ERR("[Replay] Replay truncated before uncompressed encoded size\n");
      ret = false;
      goto exit;
   }
   if (intfstream_read(handle->file, &(compressed_encoded_size),
               sizeof(uint32_t)) != sizeof(uint32_t))
   {
      RARCH_ERR("[Replay] Replay truncated before compressed encoded size\n");
      ret = false;
      goto exit;
   }
   size         = swap_if_big32(size);
   encoded_size = swap_if_big32(encoded_size);
   compressed_encoded_size = swap_if_big32(compressed_encoded_size);
   /* The three sizes come from the file.  Uncompressed data is read
    * straight into its decode buffer, and a raw state is copied into
    * cur_save, which holds 'size' bytes, so both must match what the
    * writer produces. */
   if (     (compression == REPLAY_CHECKPOINT2_COMPRESSION_NONE
         && compressed_encoded_size != encoded_size)
         || (encoding == REPLAY_CHECKPOINT2_ENCODING_RAW
         && encoded_size != size))
   {
      RARCH_ERR("[Replay] Checkpoint sizes do not agree, terminating movie\n");
      input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
      ret = false;
      goto exit;
   }
   if (       checkpoint_behavior == REPLAY_CPBEHAVIOR_SKIP
         || ((checkpoint_behavior == REPLAY_CPBEHAVIOR_UPDATE)
         &&    encoding == REPLAY_CHECKPOINT2_ENCODING_RAW))
   {
      intfstream_seek(handle->file, compressed_encoded_size, SEEK_CUR);
      goto exit;
   }

   if (handle->cur_save_size < size)
   {
      free(handle->cur_save);
      handle->cur_save = NULL;
   }
   if (!handle->cur_save)
   {
      handle->cur_save_size  = size;
      handle->cur_save       = (uint8_t*)malloc(size);
      handle->cur_save_valid = false;
   }

   if (  compression == REPLAY_CHECKPOINT2_COMPRESSION_NONE
         && encoding == REPLAY_CHECKPOINT2_ENCODING_RAW)
      compressed_data = handle->cur_save;
   else
      compressed_data = (uint8_t*)malloc(compressed_encoded_size);
   if (!handle->cur_save || !compressed_data)
   {
      RARCH_ERR("[Replay] Out of memory for checkpoint, terminating movie\n");
      input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
      ret = false;
      goto exit;
   }
   if (intfstream_read(handle->file, compressed_data,
       compressed_encoded_size) != (int64_t)compressed_encoded_size)
   {
      RARCH_ERR("[Replay] Truncated checkpoint, terminating movie\n");
      input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
      ret = false;
      goto exit;
   }
   switch (compression)
   {
      case REPLAY_CHECKPOINT2_COMPRESSION_NONE:
         encoded_data = compressed_data;
         compressed_data = NULL;
         break;
#ifdef HAVE_ZLIB
      case REPLAY_CHECKPOINT2_COMPRESSION_ZLIB:
         {
            uLongf uncompressed_size_zlib = encoded_size;
            encoded_data = (uint8_t*)calloc(encoded_size, sizeof(uint8_t));
            if (!encoded_data || uncompress(encoded_data, &uncompressed_size_zlib,
                compressed_data, compressed_encoded_size) != Z_OK)
            {
               ret = false;
               goto exit;
            }
            break;
         }
#endif
#ifdef HAVE_RZSTD
      case REPLAY_CHECKPOINT2_COMPRESSION_ZSTD:
         {
            size_t uncompressed_size_big;
            /* TODO: figure out how to support in-place decompression to
               avoid allocating a second buffer; would need to allocate
               the compressed_data buffer to be decompressed size +
               margin.  but, how could the margin be known without
               calling the function that takes the compressed frames as
               an input?  */
            encoded_data          = (uint8_t*)calloc(encoded_size, sizeof(uint8_t));
            if (!encoded_data || rzstd_decode(encoded_data, encoded_size,
                     compressed_data, compressed_encoded_size,
                     &uncompressed_size_big) != RZSTD_PROCESS_END)
               {
                  ret = false;
                  goto exit;
               }
            break;
         }
#endif
      default:
         RARCH_WARN("[Replay] Unrecognized compression scheme %d\n", compression);
         ret = false;
         goto exit;
   }
   switch (encoding)
   {
      case REPLAY_CHECKPOINT2_ENCODING_RAW:
         size = encoded_size;
         /* If decompression wasn't zerocopy, need to copy here;
            otherwise decoding is also free */
         if (handle->cur_save != encoded_data)
            memcpy(handle->cur_save, encoded_data, size);
         else
            encoded_data = NULL;
         break;
#ifdef HAVE_STATESTREAM
      case REPLAY_CHECKPOINT2_ENCODING_STATESTREAM:
         if (!bsv_movie_read_deduped_state(handle, encoded_data, encoded_size))
         {
            RARCH_ERR("[STATESTREAM] Couldn't load incremental checkpoint");
            ret = false;
            goto exit;
         }
         break;
#endif
      default:
         RARCH_WARN("[Replay] Unrecognized encoding scheme %d\n", encoding);
         ret = false;
         goto exit;
   }
   /* cur_save now holds this checkpoint's state. */
   handle->cur_save_size  = size;
   handle->last_save_size = handle->cur_save_size;
   if (checkpoint_behavior != REPLAY_CPBEHAVIOR_DESERIALIZE)
      goto exit;
   handle->checkpoint_ready = true;
 exit:
   /* Uncompressed raw data is read into cur_save itself, which the
    * handle keeps. */
   if (compressed_data && compressed_data != handle->cur_save)
      free(compressed_data);
   if (encoded_data && encoded_data != handle->cur_save)
      free(encoded_data);
   return ret;
}
/* === end verbatim copy === */

static int failures = 0;

#define CHECK(cond, what) do { if (!(cond)) { \
   printf("[ERROR] %s: %s\n", (what), #cond); failures++; } } while (0)

/* One checkpoint: its three sizes, then 'avail' bytes of payload. */
static uint8_t *make_checkpoint(uint32_t size, uint32_t encoded,
      uint32_t compressed, size_t avail, size_t *out_len)
{
   uint8_t *buf = (uint8_t*)malloc(12 + avail);
   memcpy(buf,     &size,       4);
   memcpy(buf + 4, &encoded,    4);
   memcpy(buf + 8, &compressed, 4);
   memset(buf + 12, 0x5A, avail);
   *out_len = 12 + avail;
   return buf;
}

static bool load(bsv_movie_t *h, intfstream_t *f, uint8_t *data, size_t len,
      replay_checkpoint_behavior behavior)
{
   f->data = data;
   f->len  = len;
   f->pos  = 0;
   h->file = f;
   return bsv_movie_load_checkpoint(h, REPLAY_CHECKPOINT2_COMPRESSION_NONE,
         REPLAY_CHECKPOINT2_ENCODING_RAW, behavior);
}

static void reset(bsv_movie_t *h)
{
   free(h->cur_save);
   memset(h, 0, sizeof(*h));
   g_input_st.bsv_movie_state.flags = 0;
}

int main(void)
{
   bsv_movie_t  h;
   intfstream_t f;
   uint8_t     *data;
   size_t       len;
   memset(&h, 0, sizeof(h));

   /* A well-formed checkpoint loads into cur_save. */
   data = make_checkpoint(64, 64, 64, 64, &len);
   CHECK(load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "well-formed");
   CHECK(h.cur_save && h.cur_save_size == 64 && h.cur_save[63] == 0x5A, "well-formed");
   CHECK(h.checkpoint_ready, "well-formed");
   free(data);
   reset(&h);

   /* Compressed size larger than the state: refused, nothing read. */
   data = make_checkpoint(16, 16, 256, 256, &len);
   CHECK(!load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "compressed > state");
   CHECK(g_input_st.bsv_movie_state.flags & BSV_FLAG_MOVIE_END, "compressed > state");
   free(data);
   reset(&h);

   /* Raw encoded size larger than the state: refused. */
   data = make_checkpoint(16, 256, 256, 256, &len);
   CHECK(!load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "encoded > state");
   free(data);
   reset(&h);

   /* Truncated payload: refused, and cur_save stays the movie's. */
   data = make_checkpoint(64, 64, 64, 10, &len);
   CHECK(!load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "truncated");
   CHECK(h.cur_save != NULL, "truncated");
   free(data);
   reset(&h); /* frees cur_save once */

   /* A skipped checkpoint leaves the buffer and its size alone, so a
    * later, larger checkpoint still gets a buffer of its own size. */
   data = make_checkpoint(16, 16, 16, 16, &len);
   CHECK(load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "first");
   free(data);
   data = make_checkpoint(256, 256, 256, 256, &len);
   CHECK(load(&h, &f, data, len, REPLAY_CPBEHAVIOR_SKIP), "skip");
   CHECK(h.cur_save_size == 16, "skip keeps size");
   CHECK(load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "after skip");
   CHECK(h.cur_save_size == 256 && h.cur_save[255] == 0x5A, "after skip");
   free(data);

   /* A smaller checkpoint reuses the buffer and records its size. */
   data = make_checkpoint(32, 32, 32, 32, &len);
   CHECK(load(&h, &f, data, len, REPLAY_CPBEHAVIOR_DESERIALIZE), "smaller");
   CHECK(h.cur_save_size == 32, "smaller");
   free(data);
   reset(&h);

   if (failures)
   {
      printf("\n%d bsv_checkpoint_bounds check(s) failed\n", failures);
      return 1;
   }
   printf("All bsv_checkpoint_bounds checks passed.\n");
   return 0;
}
