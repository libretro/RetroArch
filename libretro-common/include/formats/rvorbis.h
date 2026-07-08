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

extern int rvorbis_get_samples_float_interleaved(rvorbis *f, int channels, float *buffer, int num_floats);

extern int rvorbis_get_samples_s16_interleaved(rvorbis *f, int channels, int16_t *buffer, int num_shorts);
/* gets num_shorts samples as native signed 16-bit, interleaved. Vorbis
 * decodes in float; the quantisation to s16 (round half away from
 * zero, clamped) happens once, during the interleave copy. */

#ifdef __cplusplus
}
#endif

#endif /* RVORBIS_INCLUDE_RVORBIS_H */
