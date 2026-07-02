#ifndef RVORBIS_INCLUDE_RVORBIS_H
#define RVORBIS_INCLUDE_RVORBIS_H

#include <stdint.h> /* fixed-width types used throughout (self-contained header) */
#include <stddef.h> /* size_t */

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char *alloc_buffer;
   int   alloc_buffer_length_in_bytes;
} rvorbis_alloc;


/*   FUNCTIONS USEABLE WITH ALL INPUT MODES */

typedef struct rvorbis rvorbis;

typedef struct
{
   unsigned int sample_rate;
   int channels;

   unsigned int setup_memory_required;
   unsigned int setup_temp_memory_required;
   unsigned int temp_memory_required;

   int max_frame_size;
} rvorbis_info;

/* get general information about the file */
extern rvorbis_info rvorbis_get_info(rvorbis *f);

/* get the last error detected (clears it, too) */
extern int rvorbis_get_error(rvorbis *f);

/* close an ogg vorbis file and free all memory in use */
extern void rvorbis_close(rvorbis *f);

/* this function returns the offset (in samples) from the beginning of the
 * file that will be returned by the next decode, if it is known, or -1
 * otherwise. after a flush_pushdata() call, this may take a while before
 * it becomes valid again.
 * NOT WORKING YET after a seek with PULLDATA API */
extern int rvorbis_get_sample_offset(rvorbis *f);

/* returns the current seek point within the file, or offset from the beginning
 * of the memory buffer. In pushdata mode it returns 0. */
extern unsigned int rvorbis_get_file_offset(rvorbis *f);

/*   PULLING INPUT API */

/* This API assumes rvorbis is allowed to pull data from a source--
 * either a block of memory containing the _entire_ vorbis stream, or a
 * FILE * that you or it create, or possibly some other reading mechanism
 * if you go modify the source to replace the FILE * case with some kind
 * of callback to your code. (But if you don't support seeking, you may
 * just want to go ahead and use pushdata.) */

extern rvorbis * rvorbis_open_memory(const unsigned char *data, int len,
                                  int *error, rvorbis_alloc *alloc_buffer);
/* create an ogg vorbis decoder from an ogg vorbis stream in memory (note
 * this must be the entire stream!). on failure, returns NULL and sets *error */

extern int rvorbis_seek_frame(rvorbis *f, unsigned int sample_number);
extern int rvorbis_seek(rvorbis *f, unsigned int sample_number);
/* NOT WORKING YET
 * these functions seek in the Vorbis file to (approximately) 'sample_number'.
 * after calling seek_frame(), the next call to get_frame_*() will include
 * the specified sample. after calling rvorbis_seek(), the next call to
 * rvorbis_get_samples_* will start with the specified sample. If you
 * do not need to seek to EXACTLY the target sample when using get_samples_*,
 * you can also use seek_frame(). */

extern void rvorbis_seek_start(rvorbis *f);
/* this function is equivalent to rvorbis_seek(f,0), but it
 * actually works */

extern unsigned int rvorbis_stream_length_in_samples(rvorbis *f);
extern float        rvorbis_stream_length_in_seconds(rvorbis *f);
/* these functions return the total length of the vorbis stream */

extern int rvorbis_get_frame_float(rvorbis *f, int *channels, float ***output);
/* decode the next frame and return the number of samples. the number of
 * channels returned are stored in *channels (which can be NULL--it is always
 * the same as the number of channels reported by get_info). *output will
 * contain an array of float* buffers, one per channel. These outputs will
 * be overwritten on the next call to rvorbis_get_frame_*.
 *
 * You generally should not intermix calls to rvorbis_get_frame_*()
 * and rvorbis_get_samples_*(), since the latter calls the former.
 */

extern int rvorbis_get_samples_float_interleaved(rvorbis *f, int channels, float *buffer, int num_floats);
extern int rvorbis_get_samples_float(rvorbis *f, int channels, float **buffer, int num_samples);
/* gets num_samples samples, not necessarily on a frame boundary--this requires
 * buffering so you have to supply the buffers. DOES NOT APPLY THE COERCION RULES.
 * Returns the number of samples stored per channel; it may be less than requested
 * at the end of the file. If there are no more samples in the file, returns 0.
 */

/*   ERROR CODES */

enum RVorbisError
{
   RVORBIS__no_error,

   RVORBIS_need_more_data=1,             /* not a real error */

   RVORBIS_invalid_api_mixing,           /* can't mix API modes */
   RVORBIS_outofmem,                     /* not enough memory */
   RVORBIS_feature_not_supported,        /* uses floor 0 */
   RVORBIS_too_many_channels,            /* RVORBIS_MAX_CHANNELS is too small */
   RVORBIS_file_open_failure,            /* fopen() failed */
   RVORBIS_seek_without_length,          /* can't seek in unknown-length file */

   RVORBIS_unexpected_eof=10,            /* file is truncated? */
   RVORBIS_seek_invalid,                 /* seek past EOF */

   /* decoding errors (corrupt/invalid stream) -- you probably
    * don't care about the exact details of these */

   /* vorbis errors: */
   RVORBIS_invalid_setup=20,
   RVORBIS_invalid_stream,

   /* ogg errors: */
   RVORBIS_missing_capture_pattern=30,
   RVORBIS_invalid_stream_structure_version,
   RVORBIS_continued_packet_flag_invalid,
   RVORBIS_incorrect_stream_serial_number,
   RVORBIS_invalid_first_page,
   RVORBIS_bad_packet_type,
   RVORBIS_cant_find_last_page,
   RVORBIS_seek_failed
};


#ifdef __cplusplus
}
#endif

#endif /* RVORBIS_INCLUDE_RVORBIS_H */
