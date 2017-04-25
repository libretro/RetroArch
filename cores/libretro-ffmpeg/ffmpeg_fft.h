#ifndef FFMPEG_FFT_H_
#define FFMPEG_FFT_H_

#include <glsym/glsym.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct GLFFT glfft_t;

glfft_t *glfft_new(unsigned fft_steps, rglgen_proc_address_t proc);

void glfft_free(glfft_t *fft);

void glfft_init_multisample(glfft_t *fft, unsigned width, unsigned height, unsigned samples);

void glfft_step_fft(glfft_t *fft, const GLshort *buffer, unsigned frames);

void glfft_render(glfft_t *fft, GLuint backbuffer, unsigned width, unsigned height);

RETRO_END_DECLS

#endif
