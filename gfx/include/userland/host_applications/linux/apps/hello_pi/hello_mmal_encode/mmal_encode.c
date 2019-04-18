/*
Copyright (C) 2016 RealVNC Limited. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Image encoding example using MMAL

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_format.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_component_wrapper.h>
#include <interface/mmal/util/mmal_util_params.h>

// Format for test image
const unsigned int WIDTH = 512;
const unsigned int HEIGHT = 512;
const MMAL_FOURCC_T INPUT_ENC = MMAL_ENCODING_RGBA;
const unsigned int BYTESPP = 4;
const uint32_t RED = 0xff;
const uint32_t GREEN = 0xff << 8;
const uint32_t BLUE = 0xff << 16;
const uint32_t ALPHA = 0xff << 24;

static MMAL_WRAPPER_T* encoder;
static VCOS_SEMAPHORE_T sem;

// This callback and the above semaphore is used when waiting for
// data to be returned.
static void mmalCallback(MMAL_WRAPPER_T* encoder)
{
   vcos_semaphore_post(&sem);
}

// Create a test input image in the supplied buffer consisting of 8 vertical bars of
// white | black | red | green | blue | cyan | magenta | yellow
void create_rgba_test_image(void* buf, unsigned int length, unsigned int stride)
{
   uint32_t* pixel = buf;
   int i;
   for (i=0; i<length/BYTESPP; ++i) {
      switch ((i % stride) / (WIDTH / 8)) {
      case 0: *pixel = RED | GREEN | BLUE; break;
      case 1: *pixel = 0; break;
      case 2: *pixel = RED; break;
      case 3: *pixel = GREEN; break;
      case 4: *pixel = BLUE; break;
      case 5: *pixel = GREEN | BLUE; break;
      case 6: *pixel = RED | BLUE; break;
      case 7: *pixel = RED | GREEN; break;
      }
      *pixel |= ALPHA; // Full alpha
      ++pixel;
   }
}

// mmal_encode_test - Encode a test image and write to file
void mmal_encode_test(MMAL_FOURCC_T encoding, // Encoding
                      const char* filename) // File name
{
   MMAL_PORT_T* portIn;
   MMAL_PORT_T* portOut;
   MMAL_BUFFER_HEADER_T* in;
   MMAL_BUFFER_HEADER_T* out;
   MMAL_STATUS_T status;
   int eos = 0;
   int sent = 0;
   int outputWritten = 0;
   FILE* outFile;
   int nw;

   printf("Encoding test image %s\n", filename);

   // Configure input

   portIn = encoder->input[0];
   encoder->status = MMAL_SUCCESS;

   if (portIn->is_enabled) {
      if (mmal_wrapper_port_disable(portIn) != MMAL_SUCCESS) {
         fprintf(stderr, "Failed to disable input port\n");
         exit(1);
      }
   }

   portIn->format->encoding = INPUT_ENC;
   portIn->format->es->video.width = VCOS_ALIGN_UP(WIDTH, 32);
   portIn->format->es->video.height = VCOS_ALIGN_UP(HEIGHT, 16);
   portIn->format->es->video.crop.x = 0;
   portIn->format->es->video.crop.y = 0;
   portIn->format->es->video.crop.width = WIDTH;
   portIn->format->es->video.crop.height = HEIGHT;
   if (mmal_port_format_commit(portIn) != MMAL_SUCCESS) {
      fprintf(stderr, "Failed to commit input port format\n");
      exit(1);
   }

   portIn->buffer_size = portIn->buffer_size_recommended;
   portIn->buffer_num = portIn->buffer_num_recommended;

   if (mmal_wrapper_port_enable(portIn, MMAL_WRAPPER_FLAG_PAYLOAD_ALLOCATE)
       != MMAL_SUCCESS) {
      fprintf(stderr, "Failed to enable input port\n");
      exit(1);
   }

   printf("- input %4.4s %ux%u\n",
          (char*)&portIn->format->encoding,
          portIn->format->es->video.width, portIn->format->es->video.height);

   // Configure output

   portOut = encoder->output[0];

   if (portOut->is_enabled) {
      if (mmal_wrapper_port_disable(portOut) != MMAL_SUCCESS) {
         fprintf(stderr, "Failed to disable output port\n");
         exit(1);
      }
   }

   portOut->format->encoding = encoding;
   if (mmal_port_format_commit(portOut) != MMAL_SUCCESS) {
      fprintf(stderr, "Failed to commit output port format\n");
      exit(1);
   }

   mmal_port_parameter_set_uint32(portOut, MMAL_PARAMETER_JPEG_Q_FACTOR, 100);

   portOut->buffer_size = portOut->buffer_size_recommended;
   portOut->buffer_num = portOut->buffer_num_recommended;

   if (mmal_wrapper_port_enable(portOut, MMAL_WRAPPER_FLAG_PAYLOAD_ALLOCATE)
       != MMAL_SUCCESS) {
      fprintf(stderr, "Failed to enable output port\n");
      exit(1);
   }

   printf("- output %4.4s\n", (char*)&encoding);

   // Perform the encoding

   outFile = fopen(filename, "w");
   if (!outFile) {
      fprintf(stderr, "Failed to open file %s (%s)\n", filename, strerror(errno));
      exit(1);
   }

   while (!eos) {

      // Send output buffers to be filled with encoded image.
      while (mmal_wrapper_buffer_get_empty(portOut, &out, 0) == MMAL_SUCCESS) {
         if (mmal_port_send_buffer(portOut, out) != MMAL_SUCCESS) {
            fprintf(stderr, "Failed to send buffer\n");
            break;
         }
      }

      // Send image to be encoded.
      if (!sent && mmal_wrapper_buffer_get_empty(portIn, &in, 0) == MMAL_SUCCESS) {
         printf("- sending %u bytes to encoder\n", in->alloc_size);
         create_rgba_test_image(in->data, in->alloc_size, portIn->format->es->video.width);
         in->length = in->alloc_size;
         in->flags = MMAL_BUFFER_HEADER_FLAG_EOS;
         if (mmal_port_send_buffer(portIn, in) != MMAL_SUCCESS) {
            fprintf(stderr, "Failed to send buffer\n");
            break;
         }
         sent = 1;
      }

      // Get filled output buffers.
      status = mmal_wrapper_buffer_get_full(portOut, &out, 0);
      if (status == MMAL_EAGAIN) {
         // No buffer available, wait for callback and loop.
         vcos_semaphore_wait(&sem);
         continue;
      } else if (status != MMAL_SUCCESS) {
         fprintf(stderr, "Failed to get full buffer\n");
         exit(1);
      }

      printf("- received %i bytes\n", out->length);
      eos = out->flags & MMAL_BUFFER_HEADER_FLAG_EOS;

      nw = fwrite(out->data, 1, out->length, outFile);
      if (nw != out->length) {
         fprintf(stderr, "Failed to write complete buffer\n");
         exit(1);
      }
      outputWritten += nw;

      mmal_buffer_header_release(out);
   }

   mmal_port_flush(portOut);

   fclose(outFile);
   printf("- written %u bytes to %s\n\n", outputWritten, filename);
}

int main(int argc, const char** argv)
{
   bcm_host_init();

   if (vcos_semaphore_create(&sem, "encoder sem", 0) != VCOS_SUCCESS) {
      fprintf(stderr, "Failed to create semaphore\n");
      exit(1);
   }

   if (mmal_wrapper_create(&encoder, MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER)
       != MMAL_SUCCESS) {
      fprintf(stderr, "Failed to create mmal component\n");
      exit(1);
   }
   encoder->callback = mmalCallback;

   // Perform test encodings in various formats
   mmal_encode_test(MMAL_ENCODING_PNG, "out.png");
   mmal_encode_test(MMAL_ENCODING_JPEG, "out.jpg");
   mmal_encode_test(MMAL_ENCODING_GIF, "out.gif");
   mmal_encode_test(MMAL_ENCODING_BMP, "out.bmp");

   mmal_wrapper_destroy(encoder);
   vcos_semaphore_delete(&sem);

   return 0;
}

