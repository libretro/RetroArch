#ifndef FFT_H__
#define FFT_H__

#include <glsym/glsym.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct glfft glfft_t;

glfft_t *glfft_new(unsigned fft_steps, rglgen_proc_address_t proc);

void glfft_free(glfft_t *fft);

void glfft_init_multisample(glfft_t *fft, unsigned width, unsigned height, unsigned samples);

void glfft_step_fft(glfft_t *fft, const GLshort *buffer, unsigned frames);
void glfft_render(glfft_t *fft, GLuint backbuffer, unsigned width, unsigned height);

#ifdef __cplusplus
}
#endif

#endif

