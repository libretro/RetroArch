/* JACK's ringbuffer API with JACK's own prototypes; see jack.h. */
#ifndef JACK_RUNTIME_SHIM_RINGBUFFER_H
#define JACK_RUNTIME_SHIM_RINGBUFFER_H

#include <stddef.h>

typedef struct
{
   char  *buf;
   size_t len;
} jack_ringbuffer_data_t;

typedef struct jack_ringbuffer jack_ringbuffer_t;

jack_ringbuffer_t *jack_ringbuffer_create(size_t sz);
void jack_ringbuffer_free(jack_ringbuffer_t *rb);
void jack_ringbuffer_get_read_vector(const jack_ringbuffer_t *rb,
      jack_ringbuffer_data_t *vec);
int jack_ringbuffer_mlock(jack_ringbuffer_t *rb);
void jack_ringbuffer_read_advance(jack_ringbuffer_t *rb, size_t cnt);
size_t jack_ringbuffer_write(jack_ringbuffer_t *rb, const char *src,
      size_t cnt);
size_t jack_ringbuffer_write_space(const jack_ringbuffer_t *rb);
void jack_ringbuffer_get_write_vector(const jack_ringbuffer_t *rb,
      jack_ringbuffer_data_t *vec);
void jack_ringbuffer_write_advance(jack_ringbuffer_t *rb, size_t cnt);

#endif
