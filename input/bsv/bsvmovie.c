#include "bsvmovie.h"
#include <retro_endianness.h>
#include "../../retroarch.h"
#include "../../state_manager.h"
#include "../../verbosity.h"
#include "../input_driver.h"
#include "../../tasks/task_content.h"
#include "../../libretro-db/rmsgpack.h"
#include "../../libretro-db/rmsgpack_dom.h"
#ifdef HAVE_STATESTREAM
#include "input/bsv/uint32s_index.h"
#endif
#include "libretro.h"
#include "streams/interface_stream.h"
#ifdef HAVE_CHEEVOS
#include "../../cheevos/cheevos.h"
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

#define BSV_IFRAME_START_TOKEN 0x00
/* after START:
   frame counter uint
   state size (uncompressed) uint
   new block and new superblock data (see below)
   superblock seq (see below)
 */
#define BSV_IFRAME_NEW_BLOCK_TOKEN 0x01
/* after NEW_BLOCK:
   index uint
   binary
 */
#define BSV_IFRAME_NEW_SUPERBLOCK_TOKEN 0x02
/* after NEW_SUPERBLOCK:
   index uint
   array of uints
*/
#define BSV_IFRAME_SUPERBLOCK_SEQ_TOKEN 0x03
/* after SUPERBLOCK_SEQ:
   array of uints
   */

/* Later, tokens for pframes */

/* Forward declarations */
void bsv_movie_free(bsv_movie_t*);

#ifdef HAVE_STATESTREAM
int64_t bsv_movie_write_deduped_state(bsv_movie_t *movie, uint8_t *state, size_t state_size, uint8_t *output, size_t output_capacity);
bool bsv_movie_read_deduped_state(bsv_movie_t *movie,
      uint8_t *encoded, size_t encoded_size, bool output);
#endif

bool bsv_movie_reset_recording(bsv_movie_t *handle)
{
   size_t state_size, state_size_;
   uint8_t compression = handle->checkpoint_compression;
#if HAVE_STATESTREAM
   uint8_t encoding    = REPLAY_CHECKPOINT2_ENCODING_STATESTREAM;
   /* If recording, we simply reset
    * the starting point. Nice and easy. */
   uint32s_index_clear(handle->superblocks);
   uint32s_index_clear(handle->blocks);
#else
   uint8_t encoding    = REPLAY_CHECKPOINT2_ENCODING_RAW;
#endif

   intfstream_seek(handle->file, REPLAY_HEADER_LEN_BYTES, SEEK_SET);
   intfstream_truncate(handle->file, REPLAY_HEADER_LEN_BYTES);

   intfstream_write(handle->file, &compression, 1);
   intfstream_write(handle->file, &encoding, 1);
   handle->frame_counter = 0;
   state_size = 2 + bsv_movie_write_checkpoint(handle, compression, encoding);
   handle->min_file_pos = intfstream_tell(handle->file);
   /* Have to write initial state size header too */
   state_size_ = swap_if_big32(state_size);
   intfstream_seek(handle->file, 3*sizeof(uint32_t), SEEK_SET);
   intfstream_write(handle->file, &state_size_, sizeof(uint32_t));
   intfstream_seek(handle->file, handle->min_file_pos, SEEK_SET);
   return true;
}

void bsv_movie_enqueue(input_driver_state_t *input_st,
      bsv_movie_t * state, enum bsv_flags flags)
{
   if (input_st->bsv_movie_state_next_handle)
      bsv_movie_free(input_st->bsv_movie_state_next_handle);
   input_st->bsv_movie_state_next_handle    = state;
   input_st->bsv_movie_state.flags          = flags;
}

void bsv_movie_deinit(input_driver_state_t *input_st)
{
   if (input_st->bsv_movie_state_handle)
      bsv_movie_free(input_st->bsv_movie_state_handle);
   input_st->bsv_movie_state_handle = NULL;
}

void bsv_movie_deinit_full(input_driver_state_t *input_st)
{
   bsv_movie_deinit(input_st);
   if (input_st->bsv_movie_state_next_handle)
      bsv_movie_free(input_st->bsv_movie_state_next_handle);
   input_st->bsv_movie_state_next_handle = NULL;
}

void bsv_movie_frame_rewind()
{
   input_driver_state_t *input_st = input_state_get_ptr();
   bsv_movie_t          *handle   = input_st->bsv_movie_state_handle;
   bool recording = (input_st->bsv_movie_state.flags
         & BSV_FLAG_MOVIE_RECORDING) ? true : false;

   if (!handle)
      return;

   handle->did_rewind = true;
   handle->cur_save_valid = false;
   if (((handle->frame_counter & handle->frame_mask) <= 1)
         && (handle->frame_pos[0] == handle->min_file_pos))
   {
      /* If we're at the beginning... */
      RARCH_LOG("[REPLAY] rewound to beginning\n");
      handle->frame_counter = 0;
      intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
      /* clear incremental checkpoint table data.  We do this both on recording and playback for simplicity. */
#ifdef HAVE_STATESTREAM
      uint32s_index_remove_after(handle->superblocks, 0);
      uint32s_index_remove_after(handle->blocks, 0);
#endif
      if (recording)
         intfstream_truncate(handle->file, (int)handle->min_file_pos);
      else
         bsv_movie_read_next_events(handle, false);
   }
   else
   {
      /* First time rewind is performed, the old frame is simply replayed.
       * However, playing back that frame caused us to read data, and push
       * data to the ring buffer.
       *
       * Successively rewinding frames, we need to rewind past the read data,
       * plus another. */
      uint8_t delta = handle->first_rewind ? 1 : 2;
      if (handle->frame_counter >= delta)
         handle->frame_counter -= delta;
      else
         handle->frame_counter = 0;
#ifdef HAVE_STATESTREAM
      uint32s_index_remove_after(handle->superblocks, handle->frame_counter);
      uint32s_index_remove_after(handle->blocks, handle->frame_counter);
#endif
      RARCH_LOG("[REPLAY] rewound to %d\n", handle->frame_counter);
      intfstream_seek(handle->file, (int)handle->frame_pos[handle->frame_counter & handle->frame_mask], SEEK_SET);
      if (recording)
         intfstream_truncate(handle->file, (int)handle->frame_pos[handle->frame_counter & handle->frame_mask]);
      else
         bsv_movie_read_next_events(handle, false);
   }

   if (intfstream_tell(handle->file) <= (long)handle->min_file_pos)
   {
      RARCH_LOG("[Replay] rewound past beginning\n");
      /* We rewound past the beginning. */
      if (handle->playback)
      {
         intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
#ifdef HAVE_STATESTREAM
         uint32s_index_remove_after(handle->superblocks, 0);
         uint32s_index_remove_after(handle->blocks, 0);
#endif
         bsv_movie_read_next_events(handle, false);
      }
      else
      {
         bsv_movie_reset_recording(handle);
      }
   }
}

void bsv_movie_push_key_event(bsv_movie_t *movie,
      uint8_t down, uint16_t mod, uint32_t code, uint32_t character)
{
   bsv_key_data_t data;
   data.down                                 = down;
   data._padding                             = 0;
   data.mod                                  = swap_if_big16(mod);
   data.code                                 = swap_if_big32(code);
   data.character                            = swap_if_big32(character);
   movie->key_events[movie->key_event_count] = data;
   movie->key_event_count++;
}

void bsv_movie_push_input_event(bsv_movie_t *movie,
     uint8_t port, uint8_t dev, uint8_t idx, uint16_t id, int16_t val)
{
   bsv_input_data_t data;
   data.port                          = port;
   data.device                        = dev;
   data.idx                           = idx;
   data._padding                      = 0;
   data.id                            = swap_if_big16(id);
   data.value                         = swap_if_big16(val);
   movie->input_events[movie->input_event_count] = data;
   movie->input_event_count++;
}

bool bsv_movie_handle_read_input_event(bsv_movie_t *movie,
     uint8_t port, uint8_t dev, uint8_t idx, uint16_t id, int16_t* val)
{
   int i;
   /* if movie is old, just read two bytes and hope for the best */
   if (movie->version == 0)
   {
      int64_t read = intfstream_read(movie->file, val, 2);
      *val         = swap_if_big16(*val);
      return (read == 2);
   }
   for (i = 0; i < movie->input_event_count; i++)
   {
      bsv_input_data_t evt = movie->input_events[i];
      if (   (evt.port   == port)
          && (evt.device == dev)
          && (evt.idx    == idx)
          && (evt.id     == id))
      {
         *val = swap_if_big16(evt.value);
         return true;
      }
   }
   return false;
}

void bsv_movie_finish_rewind(input_driver_state_t *input_st)
{
   bsv_movie_t *handle    = input_st->bsv_movie_state_handle;
   if (!handle)
      return;
   handle->frame_counter += 1;
   handle->first_rewind   = !handle->did_rewind;
   handle->did_rewind     = false;
}

bool bsv_movie_load_checkpoint(bsv_movie_t *handle, uint8_t compression, uint8_t encoding, bool just_update_structures)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   uint32_t compressed_encoded_size, encoded_size, size;
   uint8_t *compressed_data = NULL, *encoded_data = NULL;
   retro_ctx_serialize_info_t serial_info;
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
   size = swap_if_big32(size);
   encoded_size = swap_if_big32(encoded_size);
   compressed_encoded_size = swap_if_big32(compressed_encoded_size);
   if (just_update_structures && encoding == REPLAY_CHECKPOINT2_ENCODING_RAW)
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
      handle->cur_save_size = size;
      handle->cur_save = malloc(size);
      handle->cur_save_valid = false;
   }

   if (compression == REPLAY_CHECKPOINT2_COMPRESSION_NONE && encoding == REPLAY_CHECKPOINT2_ENCODING_RAW)
      compressed_data = handle->cur_save;
   else
      compressed_data = malloc(compressed_encoded_size);
   if (intfstream_read(handle->file, compressed_data, compressed_encoded_size) != (int64_t)compressed_encoded_size)
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
#ifdef EMSCRIPTEN
            uLongf uncompressed_size_zlib = encoded_size;
#else
            uint32_t uncompressed_size_zlib = encoded_size;
#endif
            encoded_data = calloc(encoded_size, sizeof(uint8_t));
            if (uncompress(encoded_data, &uncompressed_size_zlib, compressed_data, compressed_encoded_size) != Z_OK)
            {
               ret = false;
               goto exit;
            }
            break;
         }
#endif
#ifdef HAVE_ZSTD
      case REPLAY_CHECKPOINT2_COMPRESSION_ZSTD:
         {
            size_t uncompressed_size_big;
            /* TODO: figure out how to support in-place decompression to
               avoid allocating a second buffer; would need to allocate
               the compressed_data buffer to be decompressed size +
               margin.  but, how could the margin be known without
               calling the function that takes the compressed frames as
               an input?  */
            encoded_data = calloc(encoded_size, sizeof(uint8_t));
            uncompressed_size_big = ZSTD_decompress(encoded_data, encoded_size, compressed_data, compressed_encoded_size);
            if (ZSTD_isError(uncompressed_size_big))
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
         if(!bsv_movie_read_deduped_state(handle, encoded_data, encoded_size, !just_update_structures))
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
   if (just_update_structures)
      goto exit;
   serial_info.data_const = handle->cur_save;
   serial_info.size       = size;
   /* TODO: should this happen at the end of the current frame, or at the beginning before inputs have been polled/etc?  FCEUMM and PPSSPP have some jankiness here */
   if (!core_unserialize(&serial_info))
   {
      abort();
      ret = false;
      goto exit;
   }
 exit:
   handle->cur_save_size = size;
   handle->last_save_size = handle->cur_save_size;

   if (compressed_data)
      free(compressed_data);
   if (encoded_data)
      free(encoded_data);
   return ret;
}

int64_t bsv_movie_write_checkpoint(bsv_movie_t *handle, uint8_t compression, uint8_t encoding)
{
   int64_t ret = -1;
   uint32_t encoded_size, compressed_encoded_size, size_;
   uint8_t *swap;
   size_t size_swap;
   uint8_t *encoded_data = NULL, *compressed_encoded_data = NULL;
   bool owns_encoded = false, owns_compressed_encoded = false;
   retro_ctx_serialize_info_t serial_info;
   serial_info.size = core_serialize_size();
   if (handle->cur_save_size < serial_info.size)
   {
      free(handle->cur_save);
      handle->cur_save = NULL;
   }
   if (!handle->cur_save)
   {
      handle->cur_save_size = serial_info.size;
      handle->cur_save = malloc(serial_info.size);
      handle->cur_save_valid = false;
   }
   serial_info.data = handle->cur_save;
   core_serialize(&serial_info);
   switch (encoding)
   {
      case REPLAY_CHECKPOINT2_ENCODING_RAW:
         encoded_size = serial_info.size;
         encoded_data = serial_info.data;
         break;
#ifdef HAVE_STATESTREAM
      case REPLAY_CHECKPOINT2_ENCODING_STATESTREAM:
         encoded_size = serial_info.size + serial_info.size / 2;
         encoded_data = malloc(encoded_size);
         owns_encoded = true;
         encoded_size = bsv_movie_write_deduped_state(handle, serial_info.data, serial_info.size, encoded_data, encoded_size);
         break;
#endif
      default:
         RARCH_ERR("[Replay] Unrecognized encoding scheme %d\n", encoding);
         ret = -1;
         goto exit;
   }
   switch (compression)
   {
      case REPLAY_CHECKPOINT2_COMPRESSION_NONE:
         compressed_encoded_size = encoded_size;
         compressed_encoded_data = encoded_data;
         break;
#ifdef HAVE_ZLIB
      case REPLAY_CHECKPOINT2_COMPRESSION_ZLIB:
      {
         uLongf zlib_compressed_encoded_size = compressBound(encoded_size);
         compressed_encoded_data = calloc(zlib_compressed_encoded_size, sizeof(uint8_t));
         owns_compressed_encoded = true;
         if (compress2(compressed_encoded_data, &zlib_compressed_encoded_size, encoded_data, encoded_size, 6) != Z_OK)
         {
            ret = -1;
            goto exit;
         }
         compressed_encoded_size = zlib_compressed_encoded_size;
         break;
      }
#endif
#ifdef HAVE_ZSTD
      case REPLAY_CHECKPOINT2_COMPRESSION_ZSTD:
      {
         size_t compressed_encoded_size_big = ZSTD_compressBound(encoded_size);
         compressed_encoded_data = calloc(compressed_encoded_size_big, sizeof(uint8_t));
         owns_compressed_encoded = true;
         compressed_encoded_size_big = ZSTD_compress(compressed_encoded_data, compressed_encoded_size_big, encoded_data, encoded_size, 3);
         if (ZSTD_isError(compressed_encoded_size_big))
         {
            ret = -1;
            goto exit;
         }
         compressed_encoded_size = compressed_encoded_size_big;
         break;
      }
#endif
      default:
         RARCH_WARN("[Replay] Unrecognized compression scheme %d\n", compression);
         ret = -1;
         goto exit;
   }
   /* uncompressed, unencoded size */
   size_ = swap_if_big32(serial_info.size);
   if (intfstream_write(handle->file, &size_, sizeof(uint32_t)) < (int64_t)sizeof(uint32_t))
   {
      ret = -1;
      goto exit;
   }
   /* uncompressed, encoded size */
   size_ = swap_if_big32(encoded_size);
   if (intfstream_write(handle->file, &size_, sizeof(uint32_t)) < (int64_t)sizeof(uint32_t))
   {
      ret = -1;
      goto exit;
   }
   /* compressed, encoded size */
   size_ = swap_if_big32(compressed_encoded_size);
   if (intfstream_write(handle->file, &size_, sizeof(uint32_t)) < (int64_t)sizeof(uint32_t))
   {
      ret = -1;
      goto exit;
   }
   /* data */
   if (intfstream_write(handle->file, compressed_encoded_data, compressed_encoded_size) < compressed_encoded_size)
   {
      ret = -1;
      goto exit;
   }
   ret = 3 * sizeof(uint32_t) + compressed_encoded_size;
 exit:
   size_swap = handle->cur_save_size;
   handle->cur_save_size = handle->last_save_size;
   handle->last_save_size = size_swap;
   swap = handle->cur_save;
   handle->cur_save = handle->last_save;
   handle->last_save = swap;
   handle->cur_save_valid = handle->cur_save != NULL;
   if (encoded_data && owns_encoded)
      free(encoded_data);
   if (compressed_encoded_data && owns_compressed_encoded)
      free(compressed_encoded_data);
   return ret;
}

bool bsv_movie_read_next_events(bsv_movie_t *handle, bool skip_checkpoints)
{
   input_driver_state_t *input_st  = input_state_get_ptr();
   /* Skip over backref */
   if (handle->version > 1)
      intfstream_seek(handle->file, sizeof(uint32_t), SEEK_CUR);
   /* Start by reading key event */
   if (intfstream_read(handle->file, &(handle->key_event_count), 1) == 1)
   {
      int i;
      for (i = 0; i < handle->key_event_count; i++)
      {
         if (intfstream_read(handle->file, &(handle->key_events[i]),
                  sizeof(bsv_key_data_t)) != sizeof(bsv_key_data_t))
         {
            /* Unnatural EOF */
            RARCH_ERR("[Replay] Keyboard replay ran out of keyboard inputs too early\n");
            input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
            return false;
         }
      }
   }
   else
   {
      RARCH_LOG("[Replay] EOF after buttons\n");
      /* Natural(?) EOF */
      input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
      return false;
   }
   if (handle->version > 0)
   {
      if (intfstream_read(handle->file, &(handle->input_event_count), 2) == 2)
      {
         int i;
         handle->input_event_count = swap_if_big16(handle->input_event_count);
         for (i = 0; i < handle->input_event_count; i++)
         {
            if (intfstream_read(handle->file, &(handle->input_events[i]),
                     sizeof(bsv_input_data_t)) != sizeof(bsv_input_data_t))
            {
               /* Unnatural EOF */
               RARCH_ERR("[Replay] Input replay ran out of inputs too early\n");
               input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
               return false;
            }
         }
      }
      else
      {
         RARCH_LOG("[Replay] EOF after inputs\n");
         /* Natural(?) EOF */
         input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
         return false;
      }
   }

   {
      uint8_t next_frame_type=REPLAY_TOKEN_INVALID;
      if (intfstream_read(handle->file, (uint8_t *)(&next_frame_type),
               sizeof(uint8_t)) != sizeof(uint8_t))
      {
         /* Unnatural EOF */
         RARCH_ERR("[Replay] Replay ran out of frames\n");
         input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
         return false;
      }
      else if (next_frame_type == REPLAY_TOKEN_CHECKPOINT_FRAME)
      {
         uint64_t size;
         uint8_t *state;
         retro_ctx_serialize_info_t serial_info;
         if (intfstream_read(handle->file, &(size), sizeof(uint64_t)) != sizeof(uint64_t))
         {
            RARCH_ERR("[Replay] Replay ran out of frames\n");
            input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
            return false;
         }
         size = swap_if_big64(size);
         if(skip_checkpoints)
            intfstream_seek(handle->file, size, SEEK_CUR);
         else
         {
            state = (uint8_t*)malloc(size);
            if (intfstream_read(handle->file, state, size) != (int64_t)size)
            {
               RARCH_ERR("[Replay] Replay checkpoint truncated\n");
               input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
               free(state);
               return false;
            }
            serial_info.data_const = state;
            serial_info.size       = size;
            if (!core_unserialize(&serial_info))
            {
               RARCH_ERR("[Replay] Failed to load movie checkpoint, failing\n");
               input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
            }
            free(state);
         }
      }
      else if (next_frame_type == REPLAY_TOKEN_CHECKPOINT2_FRAME)
      {
         uint8_t compression, encoding;
         if (intfstream_read(handle->file, &(compression), sizeof(uint8_t)) != sizeof(uint8_t) ||
             intfstream_read(handle->file, &(encoding), sizeof(uint8_t)) != sizeof(uint8_t))
         {
            /* Unexpected EOF */
            RARCH_ERR("[Replay] Replay checkpoint truncated.\n");
            input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
            return false;
         }
         if (!bsv_movie_load_checkpoint(handle, compression, encoding, skip_checkpoints))
            RARCH_WARN("[Replay] Failed to load movie checkpoint\n");
      }
   }
   return true;
}

void bsv_movie_scan_from_start(input_driver_state_t *input_st, int32_t len)
{
   bsv_movie_t *movie = input_st->bsv_movie_state_handle;
   if (movie->version == 0)
     return; /* Old movies don't store enough information to fixup the frame counters. */
   intfstream_seek(movie->file, movie->min_file_pos, SEEK_SET);
   movie->frame_counter = 0;
   movie->frame_pos[0] = intfstream_tell(movie->file);
   while(intfstream_tell(movie->file) < len && bsv_movie_read_next_events(movie, true))
   {
      movie->frame_counter += 1;
      movie->frame_pos[movie->frame_counter & movie->frame_mask] = intfstream_tell(movie->file);
   }
}

void bsv_movie_next_frame(input_driver_state_t *input_st)
{
   unsigned checkpoint_interval   = config_get_ptr()->uints.replay_checkpoint_interval;
   unsigned checkpoint_deserialize= config_get_ptr()->bools.replay_checkpoint_deserialize;
   /* if bsv_movie_state_next_handle is not null, deinit and set
      bsv_movie_state_handle to bsv_movie_state_next_handle and clear
      next_handle */
   bsv_movie_t         *handle    = input_st->bsv_movie_state_handle;
   if (input_st->bsv_movie_state_next_handle)
   {
      if (handle)
         bsv_movie_deinit(input_st);
      handle = input_st->bsv_movie_state_next_handle;
      input_st->bsv_movie_state_handle = handle;
      input_st->bsv_movie_state_next_handle = NULL;
   }

   if (!handle)
      return;
#ifdef HAVE_REWIND
   if (state_manager_frame_is_reversed())
      return;
#endif

   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
   {
      int i;
      uint16_t evt_count = swap_if_big16(handle->input_event_count);
      size_t last_pos = handle->frame_pos[(MAX(handle->frame_counter,2)-2) & handle->frame_mask], cur_pos = intfstream_tell(handle->file);
      uint32_t back_distance = swap_if_big32((uint32_t)(cur_pos-last_pos));
      /* write backref */
      intfstream_write(handle->file, &back_distance, sizeof(uint32_t));
      /* write key events, frame is over */
      intfstream_write(handle->file, &(handle->key_event_count), 1);
      for (i = 0; i < handle->key_event_count; i++)
         intfstream_write(handle->file, &(handle->key_events[i]),
               sizeof(bsv_key_data_t));
      /* Zero out key events when playing back or recording */
      handle->key_event_count = 0;
      /* write input events, frame is over */
      intfstream_write(handle->file, &evt_count, 2);
      for (i = 0; i < handle->input_event_count; i++)
         intfstream_write(handle->file, &(handle->input_events[i]),
               sizeof(bsv_input_data_t));
      /* Zero out input events when playing back or recording */
      handle->input_event_count = 0;

      /* Maybe record checkpoint */
      if (     (checkpoint_interval != 0)
            && (handle->frame_counter > 0)
            && (handle->frame_counter % (checkpoint_interval*60) == 0))
      {
         uint8_t frame_tok   = REPLAY_TOKEN_CHECKPOINT2_FRAME;
         uint8_t compression = handle->checkpoint_compression;
#if HAVE_STATESTREAM
         uint8_t encoding    = REPLAY_CHECKPOINT2_ENCODING_STATESTREAM;
#else
         uint8_t encoding    = REPLAY_CHECKPOINT2_ENCODING_RAW;
#endif
         /* "next frame is a checkpoint" */
         intfstream_write(handle->file, (uint8_t *)(&frame_tok), sizeof(uint8_t));
         /* compression and encoding schemes */
         intfstream_write(handle->file, (uint8_t *)(&compression), sizeof(uint8_t));
         intfstream_write(handle->file, (uint8_t *)(&encoding), sizeof(uint8_t));
         if (bsv_movie_write_checkpoint(handle, compression, encoding) < 0)
         {
            RARCH_ERR("[Replay] failed to write checkpoint, exiting record\n");
            input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
         }
      }
      else
      {
         uint8_t frame_tok = REPLAY_TOKEN_REGULAR_FRAME;
         /* write "next frame is not a checkpoint" */
         intfstream_write(handle->file, (uint8_t *)(&frame_tok), sizeof(uint8_t));
      }
   }

   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
      bsv_movie_read_next_events(handle, !checkpoint_deserialize);
   handle->frame_pos[handle->frame_counter & handle->frame_mask] = intfstream_tell(handle->file);
}

size_t replay_get_serialize_size(void)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   if (input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_RECORDING | BSV_FLAG_MOVIE_PLAYBACK))
      return sizeof(int32_t)+intfstream_tell(input_st->bsv_movie_state_handle->file);
   return 0;
}

bool replay_get_serialized_data(void* buffer)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   bsv_movie_t *handle            = input_st->bsv_movie_state_handle;

   if (input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_RECORDING | BSV_FLAG_MOVIE_PLAYBACK))
   {
      int32_t file_end        = (uint32_t)intfstream_tell(handle->file);
      int64_t read_amt        = 0;
      int32_t file_end_       = swap_if_big32(file_end);
      uint8_t *buf;
      ((uint32_t *)buffer)[0] = file_end_;
      buf                     = ((uint8_t *)buffer) + sizeof(uint32_t);
      intfstream_rewind(handle->file);
      read_amt                = intfstream_read(handle->file, buf, file_end);
      ((uint32_t *)buffer)[1+REPLAY_HEADER_FRAME_COUNT_INDEX] = swap_if_big32(handle->frame_counter);
      if (read_amt != file_end)
         RARCH_ERR("[Replay] Failed to write correct number of replay bytes into state file: %d / %d.\n",
               read_amt, file_end);
   }
   return true;
}

bool replay_check_same_timeline(bsv_movie_t *movie, uint8_t *other_movie, int64_t other_len)
{
   int64_t check_limit = MIN(other_len, intfstream_tell(movie->file));
   intfstream_t *check_stream = intfstream_open_memory(other_movie, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE, other_len);
   bool ret = true;
   int64_t check_cap = MAX(128 << 10, MAX(128*sizeof(bsv_key_data_t), 512*sizeof(bsv_input_data_t)));
   uint8_t *buf1 = calloc(check_cap,1), *buf2 = calloc(check_cap,1);
   size_t movie_pos = intfstream_tell(movie->file);
   uint8_t keycount1, keycount2, frametok1, frametok2;
   uint16_t btncount1, btncount2;
   uint64_t size1, size2;
   intfstream_rewind(movie->file);
   intfstream_read(movie->file, buf1, REPLAY_HEADER_LEN_BYTES);
   intfstream_read(check_stream, buf2, REPLAY_HEADER_LEN_BYTES);
   /* Don't want to compare frame counts */
   ((uint32_t *)buf1)[REPLAY_HEADER_FRAME_COUNT_INDEX] = 0;
   ((uint32_t *)buf2)[REPLAY_HEADER_FRAME_COUNT_INDEX] = 0;
   if (memcmp(buf1, buf2, REPLAY_HEADER_LEN_BYTES) != 0)
   {
      RARCH_ERR("[Replay] Headers of two movies differ, not same timeline\n");
      ret = false;
      goto exit;
   }
   intfstream_seek(movie->file, movie->min_file_pos, SEEK_SET);
   /* assumption: both headers have the same state size */
   intfstream_seek(check_stream, movie->min_file_pos, SEEK_SET);
   if (movie->version == 0)
   {
      int64_t i;
      /* no choice but to memcmp the whole stream against the other */
      for (i = 0; ret && i < check_limit; i+=check_cap)
      {
         int64_t read_end = MIN(check_limit - i, check_cap);
         int64_t read1 = intfstream_read(movie->file, buf1, read_end);
         int64_t read2 = intfstream_read(check_stream, buf2, read_end);
         if (read1 != read_end || read2 != read_end || memcmp(buf1, buf2, read_end) != 0)
         {
            RARCH_ERR("[Replay] One or the other replay checkpoint has different byte values\n");
            ret = false;
            goto exit;
         }
      }
      goto exit;
   }
   while(intfstream_tell(movie->file) < check_limit && intfstream_tell(check_stream) < check_limit)
   {
      if (intfstream_tell(movie->file) < 0 || intfstream_tell(check_stream) < 0)
      {
         RARCH_ERR("[Replay] One or the other replay checkpoint has ended prematurely\n");
         ret = false;
         goto exit;
      }
      /* skip past backref */
      if (movie->version > 1)
      {
         intfstream_seek(movie->file, sizeof(uint32_t), SEEK_CUR);
         intfstream_seek(check_stream, sizeof(uint32_t), SEEK_CUR);
      }
      if (intfstream_read(movie->file, &keycount1, 1) < 1 ||
            intfstream_read(check_stream, &keycount2, 1) < 1 ||
            keycount1 != keycount2)
      {
         RARCH_ERR("[Replay] Replay checkpoints disagree on key count, %d vs %d\n", keycount1, keycount2);
         ret = false;
         goto exit;
      }
      if ((uint64_t)intfstream_read(movie->file, buf1, keycount1*sizeof(bsv_key_data_t)) < keycount1*sizeof(bsv_key_data_t) ||
            (uint64_t)intfstream_read(check_stream, buf2, keycount2*sizeof(bsv_key_data_t)) < keycount2*sizeof(bsv_key_data_t) ||
            memcmp(buf1, buf2, keycount1*sizeof(bsv_key_data_t)) != 0)
      {
         RARCH_ERR("[Replay] Replay checkpoints disagree on key data\n");
         ret = false;
         goto exit;
      }
      if (intfstream_read(movie->file, &btncount1, 2) < 2 ||
            intfstream_read(check_stream, &btncount2, 2) < 2 ||
            btncount1 != btncount2)
      {
         RARCH_ERR("[Replay] Replay checkpoints disagree on input count\n");
         ret = false;
         goto exit;
      }
      btncount1 = swap_if_big16(btncount1);
      btncount2 = swap_if_big16(btncount2);
      if ((uint64_t)intfstream_read(movie->file, buf1, btncount1*sizeof(bsv_input_data_t)) < btncount1*sizeof(bsv_input_data_t) ||
            (uint64_t)intfstream_read(check_stream, buf2, btncount2*sizeof(bsv_input_data_t)) < btncount2*sizeof(bsv_input_data_t) ||
            memcmp(buf1, buf2, btncount1*sizeof(bsv_input_data_t)) != 0)
      {
         RARCH_ERR("[Replay] Replay checkpoints disagree on input data\n");
         ret = false;
         goto exit;
      }
      if (intfstream_read(movie->file, &frametok1, 1) < 1 ||
            intfstream_read(check_stream, &frametok2, 1) < 1 ||
            frametok1 != frametok2)
      {
         RARCH_ERR("[Replay] Replay checkpoints disagree on frame token\n");
         ret = false;
         goto exit;
      }
      switch (frametok1)
      {
         case REPLAY_TOKEN_INVALID:
            RARCH_ERR("[Replay] Both replays are somehow invalid\n");
            ret = false;
            goto exit;
         case REPLAY_TOKEN_REGULAR_FRAME:
            break;
         case REPLAY_TOKEN_CHECKPOINT_FRAME:
            if ((uint64_t)intfstream_read(movie->file, &size1, sizeof(uint64_t)) < sizeof(uint64_t) ||
                  (uint64_t)intfstream_read(check_stream, &size2, sizeof(uint64_t)) < sizeof(uint64_t) ||
                  size1 != size2)
            {
               RARCH_ERR("[Replay] Replay checkpoints disagree on size or scheme\n");
               ret = false;
               goto exit;
            }
            size1 = swap_if_big64(size1);
            intfstream_seek(movie->file, size1, SEEK_CUR);
            intfstream_seek(check_stream, size1, SEEK_CUR);
            break;
         case REPLAY_TOKEN_CHECKPOINT2_FRAME:
         {
            uint32_t cpsize1, cpsize2;
            /* read cp2 header:
               - one byte compression codec, one byte encoding scheme
               - 4 byte uncompressed unencoded size, 4 byte uncompressed encoded size
               - 4 byte compressed, encoded size
               - the data will follow
            */
            if (intfstream_read(movie->file, buf1, 2+sizeof(uint32_t)*3) != 2+sizeof(uint32_t)*3 ||
                  intfstream_read(check_stream, buf2, 2+sizeof(uint32_t)*3) != 2+sizeof(uint32_t)*3 ||
                  memcmp(buf1, buf2, 2+sizeof(uint32_t)*3) != 0
                )
            {
               ret = false;
               goto exit;
            }
            memcpy(&cpsize1, buf1+10, sizeof(uint32_t));
            memcpy(&cpsize2, buf2+10, sizeof(uint32_t));
            cpsize1 = swap_if_big32(cpsize1);
            cpsize2 = swap_if_big32(cpsize2);
            intfstream_seek(movie->file, cpsize1, SEEK_CUR);
            intfstream_seek(check_stream, cpsize2, SEEK_CUR);
            break;
         }
         default:
            RARCH_ERR("[Replay] Unrecognized frame token in both replays\n");
            ret = false;
            goto exit;
      }
   }
 exit:
   free(buf1);
   free(buf2);
   intfstream_close(check_stream);
   intfstream_seek(movie->file, movie_pos, SEEK_SET);
   return ret;
}

bool replay_set_serialized_data(void* buf)
{
   uint8_t *buffer                = buf;
   input_driver_state_t *input_st = input_state_get_ptr();
   bsv_movie_t *handle            = input_st->bsv_movie_state_handle;
   bool playback                  = (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)  ? true : false;
   bool recording                 = (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING) ? true : false;
   /* If there is no current replay, ignore this entirely.
      TODO/FIXME: Later, consider loading up the replay
      and allow the user to continue it?
      Or would that be better done from the replay hotkeys?
    */
   if (!(playback || recording))
      return true;

   if (!handle)
      return false;
   handle->cur_save_valid = false;
   if (!buffer)
   {
      if (recording)
      {
         const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         RARCH_ERR("[Replay] %s.\n", _msg);
         return false;
      }

      if (playback)
      {
         const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT);
         runloop_msg_queue_push(_msg, sizeof(_msg), 1, 180, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
         RARCH_WARN("[Replay] %s.\n", _msg);
         movie_stop(input_st);
      }
   }
   else
   {
      /* TODO: should factor the next few lines away, magic numbers ahoy */
      uint32_t *header         = (uint32_t *)(buffer + sizeof(uint32_t));
      int64_t *ident_spot      = (int64_t *)(header + REPLAY_HEADER_IDENTIFIER_INDEX);
      int64_t ident;
      /* avoid unaligned 8-byte read */
      memcpy(&ident, ident_spot, sizeof(int64_t));
      ident = swap_if_big64(ident);

      if (ident == handle->identifier) /* is compatible? */
      {
         int32_t loaded_len    = swap_if_big32(((int32_t *)buffer)[0]);
         int64_t handle_idx    = intfstream_tell(handle->file);
         bool same_timeline    = replay_check_same_timeline(handle, (uint8_t *)header, loaded_len);
         /* If the state is part of this replay, go back to that state
            and fast forward/rewind the replay.

            If the savestate movie is after the current replay
            length we can replace the current replay data with it,
            but if it's earlier we can rewind the replay to the
            savestate movie time point.

            This can truncate the current replay if we're in recording mode.
          */
         if (playback && loaded_len > handle_idx)
         {
            const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
            RARCH_ERR("[Replay] %s.\n", _msg);
            return false;
         }
         else if (playback && !same_timeline)
         {
            const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
            RARCH_ERR("[Replay] %s.\n", _msg);
            return false;
         }
         else if (recording && (loaded_len > handle_idx || !same_timeline))
         {
            if (!same_timeline)
            {
               const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY);
               runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
               RARCH_WARN("[Replay] %s.\n", _msg);
            }
#ifdef HAVE_STATESTREAM
            uint32s_index_remove_after(handle->superblocks, 0);
            uint32s_index_remove_after(handle->blocks, 0);
#endif
            intfstream_rewind(handle->file);
            intfstream_write(handle->file, header, loaded_len);
            /* also need to update/reinit frame_pos,
               frame_counter--rewind won't work properly unless we do. */
            /* TODO: in the future, if same_timeline, don't clear
               indices above and only scan forward from handle_idx. */
            /* TODO use backrefs to help here */
            bsv_movie_scan_from_start(input_st, loaded_len);
         }
         else
         {
#ifdef HAVE_STATESTREAM
            uint32s_index_remove_after(handle->superblocks, 0);
            uint32s_index_remove_after(handle->blocks, 0);
#endif
            intfstream_seek(handle->file, loaded_len, SEEK_SET);
            /* TODO: in the future, don't clear indices above and only
               update frame counter and remove index entries after the
               loaded movie's frame counter */
            /* TODO use backrefs to help here */
            bsv_movie_scan_from_start(input_st, loaded_len);
            if (recording)
               intfstream_truncate(handle->file, loaded_len);
         }
      }
      else
      {
         /* otherwise, if recording do not allow the load */
         if (recording)
         {
            const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
            RARCH_ERR("[Replay] %s.\n", _msg);
            return false;
         }
         /* if in playback, halt playback and go to that state normally */
         if (playback)
         {
            const char *_msg = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
            RARCH_WARN("[Replay] %s.\n", _msg);
            movie_stop(input_st);
         }
      }
   }
   return true;
}


void bsv_movie_poll(input_driver_state_t *input_st) {
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   retro_keyboard_event_t *key_event = &runloop_st->key_event;
   bsv_movie_t *handle = input_st->bsv_movie_state_handle;
   if (*key_event && *key_event == runloop_st->frontend_key_event)
   {
      int i;
      bsv_key_data_t k;
      for (i = 0; i < handle->key_event_count; i++)
      {
#ifdef HAVE_CHEEVOS
         rcheevos_pause_hardcore();
#endif
         k = handle->key_events[i];
         input_keyboard_event(k.down, swap_if_big32(k.code),
                              swap_if_big32(k.character), swap_if_big16(k.mod),
                              RETRO_DEVICE_KEYBOARD);
      }
      /* Have to clear here so we don't double-apply key events */
      /* Zero out key events when playing back or recording */
      handle->key_event_count = 0;
   }
}

int16_t bsv_movie_read_state(input_driver_state_t *input_st,
                             unsigned port, unsigned device,
                             unsigned idx, unsigned id) {
   int16_t bsv_result = 0;
   bsv_movie_t *movie = input_st->bsv_movie_state_handle;
   if (bsv_movie_handle_read_input_event(movie, port, device, idx, id, &bsv_result))
   {
#ifdef HAVE_CHEEVOS
      rcheevos_pause_hardcore();
#endif
      return bsv_result;
   }
   input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
   return 0;
}

#ifdef HAVE_STATESTREAM
int64_t bsv_movie_write_deduped_state(bsv_movie_t *movie, uint8_t *state, size_t state_size, uint8_t *output, size_t output_capacity)
{
   static uint32_t skipped_blocks = 0;
   static uint32_t memcmps = 0, hashes = 0;
   static uint32_t reused_blocks = 0;
   static uint32_t reused_superblocks = 0;
   static uint32_t total_blocks = 0;
   static uint32_t total_superblocks = 0;
   static uint32_t total_checkpoints = 0;
   static uint32_t total_kbs_input = 0;
   static uint32_t total_kbs_written = 0;
   static retro_perf_tick_t total_encode_micros = 0;
   retro_perf_tick_t start = cpu_features_get_time_usec();
   size_t block_byte_size = movie->blocks->object_size*4;
   size_t superblock_size = movie->superblocks->object_size;
   size_t superblock_byte_size = superblock_size*block_byte_size;
   size_t superblock_count = state_size / superblock_byte_size + (state_size % superblock_byte_size != 0);
   uint32_t *superblock_buf = calloc(superblock_size, sizeof(uint32_t));
   uint8_t *padded_block = NULL;
   intfstream_t *out_stream = intfstream_open_writable_memory(output, RETRO_VFS_FILE_ACCESS_READ_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE, output_capacity);
   int64_t encoded_size;
   size_t superblock, block;
   uint32_t i;
   bool can_compare_saves = movie->cur_save_valid && movie->last_save && movie->last_save_size >= state_size;
   if (movie->last_save_size < state_size)
   {
      free(movie->superblock_seq);
      movie->superblock_seq = NULL;
   }
   if (!movie->superblock_seq)
   {
      movie->cur_save_valid = false;
      movie->superblock_seq = calloc(superblock_count, sizeof(uint32_t));
   }
   rmsgpack_write_int(out_stream, BSV_IFRAME_START_TOKEN);
   rmsgpack_write_int(out_stream, movie->frame_counter);
   for(superblock = 0; superblock < superblock_count; superblock++)
   {
      uint32s_insert_result_t found_block;
      total_superblocks++;
      for(block = 0; block < superblock_size; block++)
      {
         size_t block_start = superblock*superblock_byte_size+block*block_byte_size;
         size_t block_end = MIN(block_start + block_byte_size, state_size);
         if(block_start > state_size)
         {
            /* pad superblocks with zero blocks */
            found_block.index = 0;
            found_block.is_new = false;
         }
         else if (can_compare_saves &&
                  (++memcmps) &&
                  memcmp(movie->last_save + block_start,
                         state + block_start,
                         block_end-block_start) == 0)
         {
            skipped_blocks++;
            found_block.index = uint32s_index_get(movie->superblocks,
                                                  movie->superblock_seq[superblock])[block];
            found_block.is_new = false;
            /* bump usage count */
            uint32s_index_bump_count(movie->blocks, found_block.index);
         }
         else if(block_start + block_byte_size > state_size)
         {
            if(!padded_block)
               padded_block = calloc(block_byte_size, sizeof(uint8_t));
            else
               memset(padded_block+(state_size-block_start),
                     0,
                     block_byte_size-(state_size-block_start));
            memcpy(padded_block, state+block_start, state_size - block_start);
            found_block = uint32s_index_insert(movie->blocks,
                                               (uint32_t*)padded_block,
                                               movie->frame_counter);
            hashes++;
         }
         else
         {
            hashes++;
            found_block = uint32s_index_insert(movie->blocks,
                                               (uint32_t*)(state+block_start),
                                               movie->frame_counter);
         }
         total_blocks++;
         if(found_block.is_new)
         {
            /* write "here is a new block" and new block to file */
            rmsgpack_write_int(out_stream, BSV_IFRAME_NEW_BLOCK_TOKEN);
            rmsgpack_write_int(out_stream, found_block.index);
            rmsgpack_write_bin(out_stream, state+block_start, block_byte_size);
         }
         else
            reused_blocks++;
         superblock_buf[block] = found_block.index;
      }
      found_block = uint32s_index_insert(movie->superblocks, superblock_buf, movie->frame_counter);
      if(found_block.is_new)
      {
         /* write "here is a new superblock" and new superblock to file */
         rmsgpack_write_int(out_stream, BSV_IFRAME_NEW_SUPERBLOCK_TOKEN);
         rmsgpack_write_int(out_stream, found_block.index);
         rmsgpack_write_array_header(out_stream, superblock_size);
         for(i = 0; i < superblock_size; i++)
            rmsgpack_write_int(out_stream, superblock_buf[i]);
      }
      else
         reused_superblocks++;
      movie->superblock_seq[superblock] = found_block.index;
   }
   uint32s_index_commit(movie->blocks);
   /* Superblocks are small enough that there's no real benefit to garbage collecting them */
   /* uint32s_index_commit(movie->superblocks); */
   /* write "here is the superblock seq" and superblock seq to file */
   rmsgpack_write_int(out_stream, BSV_IFRAME_SUPERBLOCK_SEQ_TOKEN);
   rmsgpack_write_array_header(out_stream, superblock_count);
   for(i = 0; i < superblock_count; i++)
       rmsgpack_write_int(out_stream, movie->superblock_seq[i]);
   free(superblock_buf);
   if(padded_block)
     free(padded_block);
   movie->cur_save_valid = true;
   total_checkpoints++;
   total_encode_micros += cpu_features_get_time_usec() - start;
   total_kbs_input += state_size/1024;
   encoded_size = intfstream_tell(out_stream);
   total_kbs_written += encoded_size/1024;
   RARCH_DBG("[STATESTREAM] Encode stats at checkpoint %d: %d blocks (%d reused, %d skipped [%d checks], %d distinct [%d hashes])\n", total_checkpoints, total_blocks, reused_blocks, skipped_blocks, memcmps, uint32s_index_count(movie->blocks), hashes);
   RARCH_DBG("[STATESTREAM] %d superblocks (%d reused, %d distinct); unencoded size (KB) %d, encoded size (KB) %d; net time (secs) %f\n", total_superblocks, reused_superblocks, uint32s_index_count(movie->superblocks), total_kbs_input, total_kbs_written, ((float)total_encode_micros) / (float)1000000.0);
   intfstream_close(out_stream);
   return encoded_size;
}

bool bsv_movie_read_deduped_state(bsv_movie_t *movie,
      uint8_t *encoded, size_t encoded_size, bool output)
{
   static retro_perf_tick_t total_decode_micros = 0;
   static retro_perf_tick_t total_decode_count = 0;
   retro_perf_tick_t start = cpu_features_get_time_usec();
   bool ret = false;
   size_t state_size = movie->cur_save_size;
   /*uint32_t frame_counter = 0;*/
   struct rmsgpack_dom_value item;
   struct rmsgpack_dom_reader_state *reader_state = rmsgpack_dom_reader_state_new();
   size_t block_byte_size = movie->blocks->object_size*4;
   size_t superblock_byte_size = movie->superblocks->object_size*block_byte_size;
   intfstream_t *read_mem = intfstream_open_memory(encoded, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE, encoded_size);
   size_t i;
   if (state_size > movie->last_save_size && movie->superblock_seq)
   {
      free(movie->superblock_seq);
      movie->superblock_seq = NULL;
   }
   total_decode_count++;
   rmsgpack_dom_read_with(read_mem, &item, reader_state);
   if(item.type != RDT_INT)
   {
      RARCH_ERR("[STATESTREAM] start token type is wrong\n");
      goto exit;
   }
   if(item.val.int_ != BSV_IFRAME_START_TOKEN)
   {
      RARCH_ERR("[STATESTREAM] start token value is wrong\n");
      goto exit;
   }
   rmsgpack_dom_read_with(read_mem, &item, reader_state);
   if(item.type != RDT_INT)
   {
      RARCH_ERR("[STATESTREAM] frame counter type is wrong\n");
      goto exit;
   }
   /*frame_counter = item.val.int_;*/
   while(rmsgpack_dom_read_with(read_mem, &item, reader_state) >= 0)
   {
      uint32_t index, *superblock;
      size_t len;
      if(item.type != RDT_INT)
      {
         RARCH_ERR("[STATESTREAM] state update chunk token type is wrong\n");
         goto exit;
      }
      switch(item.val.int_) {
      case BSV_IFRAME_NEW_BLOCK_TOKEN:
         rmsgpack_dom_read_with(read_mem, &item, reader_state);
         if(item.type != RDT_INT)
         {
            RARCH_ERR("[STATESTREAM] new block index type is wrong\n");
            goto exit;
         }
         index = item.val.int_;
         rmsgpack_dom_read_with(read_mem, &item, reader_state);
         if(item.type != RDT_BINARY)
         {
            RARCH_ERR("[STATESTREAM] new block value type is wrong\n");
            rmsgpack_dom_value_free(&item);
            goto exit;
         }
         if(item.val.binary.len != block_byte_size)
         {
            RARCH_ERR("[STATESTREAM] new block binary length is wrong: %d vs %d\n", item.val.binary.len, block_byte_size);
            rmsgpack_dom_value_free(&item);
            goto exit;
         }
         if(!uint32s_index_insert_exact(movie->blocks, index, (uint32_t *)item.val.binary.buff, movie->frame_counter))
         {
            RARCH_ERR("[STATESTREAM] couldn't insert new block at right index %d\n", index);
            rmsgpack_dom_value_free(&item);
            goto exit;
         }
         /* do not free binary rmsgpack item since insert_exact takes over its allocation */
         break;
      case BSV_IFRAME_NEW_SUPERBLOCK_TOKEN:
         rmsgpack_dom_read_with(read_mem, &item, reader_state);
         if(item.type != RDT_INT)
         {
            RARCH_ERR("[STATESTREAM] new superblock index type is wrong\n");
            goto exit;
         }
         index = item.val.int_;
         if(rmsgpack_dom_read_with(read_mem, &item, reader_state) < 0)
         {
            RARCH_ERR("[STATESTREAM] array read failed\n");
            goto exit;
         }
         if(item.type != RDT_ARRAY)
         {
            RARCH_ERR("[STATESTREAM] new superblock contents type is wrong\n");
            goto exit;
         }
         if(item.val.array.len != movie->superblocks->object_size)
         {
            RARCH_ERR("[STATESTREAM] new superblock contents length is wrong\n");
            goto exit;
         }
         len = movie->superblocks->object_size;
         superblock = calloc(len, sizeof(uint32_t));
         for(i = 0; i < len; i++)
         {
            struct rmsgpack_dom_value inner_item = item.val.array.items[i];
            /* assert(inner_item.type == RDT_INT); */
            superblock[i] = inner_item.val.int_;
         }
         if(!uint32s_index_insert_exact(movie->superblocks, index, superblock, movie->frame_counter))
         {
            RARCH_ERR("[STATESTREAM] new superblock couldn't be inserted at right index\n");
            rmsgpack_dom_value_free(&item);
            free(superblock);
            goto exit;
         }
         /* Do not free superblock since insert_exact takes over its allocation */
         rmsgpack_dom_value_free(&item);
         break;
      case BSV_IFRAME_SUPERBLOCK_SEQ_TOKEN:
         rmsgpack_dom_read_with(read_mem, &item, reader_state);
         if(item.type != RDT_ARRAY)
         {
            RARCH_ERR("[STATESTREAM] superblock seq type is wrong\n");
            goto exit;
         }
         len = item.val.array.len;
         if (output)
         {
            if (!movie->superblock_seq)
               movie->superblock_seq = calloc(len,sizeof(uint32_t));
            for(i = 0; i < len; i++)
            {
               struct rmsgpack_dom_value inner_item = item.val.array.items[i];
               /* assert(inner_item.type == RDT_INT); */
               uint32_t superblock_idx = inner_item.val.int_;
               uint32_t *superblock;
               size_t j;
               /* if this superblock is the same as last time, no need to scan the blocks. */
               if (movie->cur_save_valid && movie->cur_save && superblock_idx == movie->superblock_seq[i])
               {
                  superblock = uint32s_index_get(movie->superblocks, movie->superblock_seq[i]);
                  uint32s_index_bump_count(movie->superblocks, movie->superblock_seq[i]);
                  /* We do need to increment all the involved block counts though */
                  for (j = 0; j < movie->superblocks->object_size; j++)
                     uint32s_index_bump_count(movie->blocks, superblock[j]);
                  continue;
               }
               movie->superblock_seq[i] = superblock_idx;
               superblock = uint32s_index_get(movie->superblocks, superblock_idx);
               uint32s_index_bump_count(movie->superblocks, superblock_idx);
               for(j = 0; j < movie->superblocks->object_size; j++)
               {
                  uint32_t block_idx = superblock[j];
                  size_t block_start = MIN(i*superblock_byte_size+j*block_byte_size, state_size);
                  size_t block_end = MIN(block_start+block_byte_size, state_size);
                  uint8_t *block;
                  /* This (==) can only happen in the last superblock, if it was padded with extra blocks. */
                  if(block_end <= block_start) { break; }
                  block = (uint8_t *)uint32s_index_get(movie->blocks, block_idx);
                  uint32s_index_bump_count(movie->blocks, block_idx);
                  memcpy(movie->cur_save+block_start, (uint8_t*)block, block_end-block_start);
               }
            }

         }
         rmsgpack_dom_value_free(&item);
         ret = true;
         goto exit;
      default:
         RARCH_ERR("[STATESTREAM] state update chunk token value is invalid: %d @ %x\n", item.val.int_, intfstream_tell(read_mem));
         goto exit;
     }
   }
exit:
   uint32s_index_commit(movie->blocks);
   /* Superblocks are small enough that there's no real benefit to garbage collecting them */
   /* uint32s_index_commit(movie->superblocks); */
   rmsgpack_dom_reader_state_free(reader_state);
   intfstream_close(read_mem);
   if(!ret)
   {
      RARCH_ERR("[STATESTREAM] made it to end without superblock seq\n");
      abort();
   }
   total_decode_micros += cpu_features_get_time_usec() - start;
   RARCH_DBG("[STATESTREAM] Total statestream decodes %d ; net time (secs): %f\n", total_decode_count, (double)total_decode_micros / (1000000.0));
   return ret;
}
#endif

bool movie_commit_checkpoint(input_driver_state_t *input_st)
{
   return false;
}
bool movie_skip_to_next_checkpoint(input_driver_state_t *input_st)
{
   return false;
}
bool movie_skip_to_prev_checkpoint(input_driver_state_t *input_st)
{
   return false;
}
