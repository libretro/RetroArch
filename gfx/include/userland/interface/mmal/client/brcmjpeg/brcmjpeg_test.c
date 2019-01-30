/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "brcmjpeg.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <user-vcsm.h>

#define MAX_WIDTH   5000
#define MAX_HEIGHT  5000
#define MAX_ENCODED (15*1024*1024)
#define MAX_DECODED (MAX_WIDTH*MAX_HEIGHT*2)

static uint8_t encodedInBuf[MAX_ENCODED];
static uint8_t encodedOutBuf[MAX_ENCODED];
static uint8_t decodedBuf[MAX_DECODED];
static char outFileName[2048];

int64_t get_time_microsec(void)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000LL + now.tv_usec;
}

int main(int argc, char **argv)
{
    BRCMJPEG_STATUS_T status;
    BRCMJPEG_REQUEST_T enc_request, dec_request;
    BRCMJPEG_T *enc = 0, *dec = 0;
    int64_t start, stop, time_dec = 0, time_enc = 0;
    unsigned int count = 1, format = PIXEL_FORMAT_YUYV;
    unsigned int use_vcsm = 0, handle = 0, vc_handle = 0;
    int i, arg = 1, help = 0;

    // Parse command line arguments
    while (arg < argc && argv[arg][0] == '-')
    {
        if (!strcmp(argv[arg], "-n"))
        {
            if (++arg >= argc || sscanf(argv[arg++], "%u", &count) != 1)
                arg = argc;
        }
        else if (!strcmp(argv[arg], "-f"))
        {
            if (++arg >= argc || sscanf(argv[arg++], "%u", &format) != 1)
                arg = argc;
        }
        else if (!strcmp(argv[arg], "-s"))
        {
            use_vcsm = 1;
            arg++;
        }
        else if (!strcmp(argv[arg], "-h"))
        {
            help = 1;
            break;
        }
        else
        {
            arg = argc;
        }
    }

    if (arg == argc || help)
    {
        if (!help) fprintf(stderr, "invalid arguments\n");
        fprintf(stderr, "usage: %s [options] file1 ... fileN\n", argv[0]);
        fprintf(stderr, "options list:\n");
        fprintf(stderr, " -h     : help\n");
        fprintf(stderr, " -n <N> : process each file N times\n");
        fprintf(stderr, " -f <N> : force color format\n");
        fprintf(stderr, " -s     : use shared-memory for intermediate buffer\n");
        return !help;
    }

    if (use_vcsm)
    {
        if (vcsm_init() < 0)
        {
            fprintf(stderr, "failed to initialise vcsm\n");
            return 1;
        }

        handle = vcsm_malloc_cache(MAX_DECODED, VCSM_CACHE_TYPE_HOST, "brcmjpeg-test");
        if (!handle)
        {
            fprintf(stderr, "failed to alloc vcsm buffer\n");
            vcsm_exit();
            return 1;
        }

        vc_handle = vcsm_vc_hdl_from_hdl(handle);

        fprintf(stderr, "decodedBuf handle %u vc_handle %u\n", handle, vc_handle);
    }

    // Setup of the dec / enc requests
    memset(&enc_request, 0, sizeof(enc_request));
    memset(&dec_request, 0, sizeof(dec_request));
    dec_request.input = encodedInBuf;
    dec_request.output = use_vcsm ? NULL : decodedBuf;
    dec_request.output_handle = use_vcsm ? vc_handle : 0;
    dec_request.output_alloc_size = MAX_DECODED;
    enc_request.input = dec_request.output;
    enc_request.input_handle = dec_request.output_handle;
    enc_request.output = encodedOutBuf;
    enc_request.output_alloc_size = sizeof(encodedOutBuf);
    enc_request.quality = 75;
    enc_request.pixel_format = dec_request.pixel_format = format;

    status = brcmjpeg_create(BRCMJPEG_TYPE_ENCODER, &enc);
    if (status != BRCMJPEG_SUCCESS)
    {
        fprintf(stderr, "could not create encoder\n");
        return 1;
    }
    status = brcmjpeg_create(BRCMJPEG_TYPE_DECODER, &dec);
    if (status != BRCMJPEG_SUCCESS)
    {
        fprintf(stderr, "could not create decoder\n");
        brcmjpeg_release(enc);
        return 1;
    }

    for (i = arg; i < argc; i++)
    {
        unsigned int j;
        fprintf(stderr, "processing %s\n", argv[i]);

        FILE *file_in = fopen(argv[i], "rb");
        if (!file_in) {
            fprintf(stderr, "could not open file %s\n", argv[i]);
            continue;
        }
        snprintf(outFileName, sizeof(outFileName), "%s.out", argv[i]);
        FILE *file_out = fopen(outFileName, "wb+");
        if (!file_out) {
            fprintf(stderr, "could not open file %s\n", outFileName);
            fclose(file_in);
            continue;
        }
        dec_request.input_size = fread(encodedInBuf, 1, sizeof(encodedInBuf), file_in);

        for (j = 0; j < count; j++)
        {
            dec_request.buffer_width = 0;
            dec_request.buffer_height = 0;

            start = get_time_microsec();
            status = brcmjpeg_process(dec, &dec_request);
            stop = get_time_microsec();
            if (status != BRCMJPEG_SUCCESS) {
                fprintf(stderr, "could not decode %s\n", argv[i]);
                break;
            }

            fprintf(stderr, "decoded %ix%i(%ix%i), %i bytes in %lldus\n",
                    dec_request.width, dec_request.height,
                    dec_request.buffer_width, dec_request.buffer_height,
                    dec_request.input_size, stop - start);
            time_dec += stop - start;

            enc_request.input_size = dec_request.output_size;
            enc_request.width = dec_request.width;
            enc_request.height = dec_request.height;
            enc_request.buffer_width = dec_request.buffer_width;
            enc_request.buffer_height = dec_request.buffer_height;

            start = get_time_microsec();
            status = brcmjpeg_process(enc, &enc_request);
            stop = get_time_microsec();
            if (status != BRCMJPEG_SUCCESS) {
                fprintf(stderr, "could not encode %s\n", outFileName);
                break;
            }

            fprintf(stderr, "encoded %ix%i(%ix%i), %i bytes in %lldus\n",
                    enc_request.width, enc_request.height,
                    enc_request.buffer_width, enc_request.buffer_height,
                    enc_request.output_size, stop - start);
            time_enc += stop - start;
        }

        if (status != BRCMJPEG_SUCCESS)
            continue;

        fwrite(enc_request.output, 1, enc_request.output_size, file_out);
        fclose(file_out);
        fclose(file_in);

        fprintf(stderr, "decode times %lldus (%lldus per run)\n",
                time_dec, time_dec / count);
        fprintf(stderr, "encode times %lldus (%lldus per run)\n",
                time_enc, time_enc / count);
    }

    brcmjpeg_release(dec);
    brcmjpeg_release(enc);

    if (use_vcsm)
    {
       vcsm_free(handle);
       vcsm_exit();
    }

    return 0;
}
