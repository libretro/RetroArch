/*
Copyright (c) 2012, Matt Ownby
                    Anthong Sale

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

#ifndef _OPTION_H_
#define _OPTION_H_

/*
Defines the methods for interacting with openmax il and ilclient to decode
jpeg images from the camera
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "bcm_host.h"
#include "ilclient.h"

#define OMXJPEG_OK                  0
#define OMXJPEG_ERROR_ILCLIENT_INIT    -1024
#define OMXJPEG_ERROR_OMX_INIT         -1025
#define OMXJPEG_ERROR_MEMORY         -1026
#define OMXJPEG_ERROR_CREATING_COMP    -1027
#define OMXJPEG_ERROR_WRONG_NO_PORTS   -1028
#define OMXJPEG_ERROR_EXECUTING         -1029
#define OMXJPEG_ERROR_NOSETTINGS   -1030

typedef struct _OPENMAX_JPEG_DECODER OPENMAX_JPEG_DECODER;

//this function run the boilerplate to setup the openmax components;
int setupOpenMaxJpegDecoder(OPENMAX_JPEG_DECODER** decoder);

//this function passed the jpeg image buffer in, and returns the decoded image
int decodeImage(OPENMAX_JPEG_DECODER* decoder,
              char* sourceImage, size_t imageSize);

//this function cleans up the decoder.
void cleanup(OPENMAX_JPEG_DECODER* decoder);

#endif
