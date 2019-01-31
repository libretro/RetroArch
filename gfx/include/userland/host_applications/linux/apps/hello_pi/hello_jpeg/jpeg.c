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

#include <stdio.h>
#include <assert.h>
#include "jpeg.h"

#define TIMEOUT_MS 2000

typedef struct _COMPONENT_DETAILS {
    COMPONENT_T    *component;
    OMX_HANDLETYPE  handle;
    int             inPort;
    int             outPort;
} COMPONENT_DETAILS;

struct _OPENMAX_JPEG_DECODER {
    ILCLIENT_T     *client;
    COMPONENT_DETAILS *imageDecoder;
    COMPONENT_DETAILS *imageResizer;
    OMX_BUFFERHEADERTYPE **ppInputBufferHeader;
    int             inputBufferHeaderCount;
    OMX_BUFFERHEADERTYPE *pOutputBufferHeader;
};

int             bufferIndex = 0;	// index to buffer array

int
portSettingsChanged(OPENMAX_JPEG_DECODER * decoder)
{
    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    // need to setup the input for the resizer with the output of the
    // decoder
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageDecoder->outPort;
    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    unsigned int    uWidth =
	(unsigned int) portdef.format.image.nFrameWidth;
    unsigned int    uHeight =
	(unsigned int) portdef.format.image.nFrameHeight;

    // tell resizer input what the decoder output will be providing
    portdef.nPortIndex = decoder->imageResizer->inPort;
    OMX_SetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // establish tunnel between decoder output and resizer input
    OMX_SetupTunnel(decoder->imageDecoder->handle,
		    decoder->imageDecoder->outPort,
		    decoder->imageResizer->handle,
		    decoder->imageResizer->inPort);

    // enable ports
    OMX_SendCommand(decoder->imageDecoder->handle,
		    OMX_CommandPortEnable,
		    decoder->imageDecoder->outPort, NULL);
    OMX_SendCommand(decoder->imageResizer->handle,
		    OMX_CommandPortEnable,
		    decoder->imageResizer->inPort, NULL);

    // put resizer in idle state (this allows the outport of the decoder
    // to become enabled)
    OMX_SendCommand(decoder->imageResizer->handle,
		    OMX_CommandStateSet, OMX_StateIdle, NULL);

    // wait for state change complete
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete,
			    OMX_CommandStateSet, 1,
			    OMX_StateIdle, 1, 0, TIMEOUT_MS);

    // once the state changes, both ports should become enabled and the
    // resizer
    // output should generate a settings changed event
    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete,
			    OMX_CommandPortEnable, 1,
			    decoder->imageDecoder->outPort, 1, 0,
			    TIMEOUT_MS);
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandPortEnable, 1,
			    decoder->imageResizer->inPort, 1, 0,
			    TIMEOUT_MS);
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventPortSettingsChanged,
			    decoder->imageResizer->outPort, 1, 0, 1, 0,
			    TIMEOUT_MS);

    ilclient_disable_port(decoder->imageResizer->component,
			  decoder->imageResizer->outPort);

    // query output buffer requirements for resizer
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageResizer->outPort;
    OMX_GetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // change output color format and dimensions to match input
    portdef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    portdef.format.image.eColorFormat = OMX_COLOR_Format32bitABGR8888;
    portdef.format.image.nFrameWidth = uWidth;
    portdef.format.image.nFrameHeight = uHeight;
    portdef.format.image.nStride = 0;
    portdef.format.image.nSliceHeight = 0;
    portdef.format.image.bFlagErrorConcealment = OMX_FALSE;

    OMX_SetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // grab output requirements again to get actual buffer size
    // requirement (and buffer count requirement!)
    OMX_GetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // move resizer into executing state
    ilclient_change_component_state(decoder->imageResizer->component,
				    OMX_StateExecuting);

    // show some logging so user knows it's working
    printf
	("Width: %u Height: %u Output Color Format: 0x%x Buffer Size: %u\n",
	 (unsigned int) portdef.format.image.nFrameWidth,
	 (unsigned int) portdef.format.image.nFrameHeight,
	 (unsigned int) portdef.format.image.eColorFormat,
	 (unsigned int) portdef.nBufferSize);
    fflush(stdout);

    // enable output port of resizer
    OMX_SendCommand(decoder->imageResizer->handle,
		    OMX_CommandPortEnable,
		    decoder->imageResizer->outPort, NULL);

    // allocate the buffer
    // void* outputBuffer = 0; 
    // if (posix_memalign(&outputBuffer, portdef.nBufferAlignment,
    // portdef.nBufferSize) != 0)
    // {
    // perror("Allocating output buffer");
    // return OMXJPEG_ERROR_MEMORY;
    // }

    // set the buffer
    // int ret = OMX_UseBuffer(decoder->imageResizer->handle,
    // &decoder->pOutputBufferHeader,
    // decoder->imageResizer->outPort, NULL,
    // portdef.nBufferSize,
    // (OMX_U8 *) outputBuffer);
    int             ret = OMX_AllocateBuffer(decoder->imageResizer->handle,
					     &decoder->pOutputBufferHeader,
					     decoder->imageResizer->
					     outPort,
					     NULL,
					     portdef.nBufferSize);
    if (ret != OMX_ErrorNone) {
	perror("Eror allocating buffer");
	fprintf(stderr, "OMX_AllocateBuffer returned 0x%x allocating buffer size 0x%x\n", ret, portdef.nBufferSize);
	return OMXJPEG_ERROR_MEMORY;
    }

    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete,
			    OMX_CommandPortEnable, 1,
			    decoder->imageResizer->outPort, 1, 0,
			    TIMEOUT_MS);

    return OMXJPEG_OK;
}

int
portSettingsChangedAgain(OPENMAX_JPEG_DECODER * decoder)
{
    ilclient_disable_port(decoder->imageDecoder->component,
			  decoder->imageDecoder->outPort);
    ilclient_disable_port(decoder->imageResizer->component,
			  decoder->imageResizer->inPort);

    OMX_PARAM_PORTDEFINITIONTYPE portdef;

    // need to setup the input for the resizer with the output of the
    // decoder
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageDecoder->outPort;
    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // tell resizer input what the decoder output will be providing
    portdef.nPortIndex = decoder->imageResizer->inPort;
    OMX_SetParameter(decoder->imageResizer->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // enable output of decoder and input of resizer (ie enable tunnel)
    ilclient_enable_port(decoder->imageDecoder->component,
			 decoder->imageDecoder->outPort);
    ilclient_enable_port(decoder->imageResizer->component,
			 decoder->imageResizer->inPort);

    // need to wait for this event
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventPortSettingsChanged,
			    decoder->imageResizer->outPort, 1,
			    0, 0, 0, TIMEOUT_MS);

    return OMXJPEG_OK;
}

int
prepareResizer(OPENMAX_JPEG_DECODER * decoder)
{
    decoder->imageResizer = malloc(sizeof(COMPONENT_DETAILS));
    if (decoder->imageResizer == NULL) {
	perror("malloc image resizer");
	return OMXJPEG_ERROR_MEMORY;
    }

    int             ret = ilclient_create_component(decoder->client,
						    &decoder->
						    imageResizer->
						    component,
						    "resize",
						    ILCLIENT_DISABLE_ALL_PORTS
						    |
						    ILCLIENT_ENABLE_INPUT_BUFFERS
						    |
						    ILCLIENT_ENABLE_OUTPUT_BUFFERS);
    if (ret != 0) {
	perror("image resizer");
	return OMXJPEG_ERROR_CREATING_COMP;
    }
    // grab the handle for later use
    decoder->imageResizer->handle =
	ILC_GET_HANDLE(decoder->imageResizer->component);

    // get and store the ports
    OMX_PORT_PARAM_TYPE port;
    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    OMX_GetParameter(ILC_GET_HANDLE(decoder->imageResizer->component),
		     OMX_IndexParamImageInit, &port);
    if (port.nPorts != 2) {
	return OMXJPEG_ERROR_WRONG_NO_PORTS;
    }
    decoder->imageResizer->inPort = port.nStartPortNumber;
    decoder->imageResizer->outPort = port.nStartPortNumber + 1;

    decoder->pOutputBufferHeader = NULL;

    return OMXJPEG_OK;
}

int
prepareImageDecoder(OPENMAX_JPEG_DECODER * decoder)
{
    decoder->imageDecoder = malloc(sizeof(COMPONENT_DETAILS));
    if (decoder->imageDecoder == NULL) {
	perror("malloc image decoder");
	return OMXJPEG_ERROR_MEMORY;
    }

    int             ret = ilclient_create_component(decoder->client,
						    &decoder->
						    imageDecoder->
						    component,
						    "image_decode",
						    ILCLIENT_DISABLE_ALL_PORTS
						    |
						    ILCLIENT_ENABLE_INPUT_BUFFERS);

    if (ret != 0) {
	perror("image decode");
	return OMXJPEG_ERROR_CREATING_COMP;
    }
    // grab the handle for later use in OMX calls directly
    decoder->imageDecoder->handle =
	ILC_GET_HANDLE(decoder->imageDecoder->component);

    // get and store the ports
    OMX_PORT_PARAM_TYPE port;
    port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;

    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamImageInit, &port);
    if (port.nPorts != 2) {
	return OMXJPEG_ERROR_WRONG_NO_PORTS;
    }
    decoder->imageDecoder->inPort = port.nStartPortNumber;
    decoder->imageDecoder->outPort = port.nStartPortNumber + 1;

    return OMXJPEG_OK;
}

int
startupImageDecoder(OPENMAX_JPEG_DECODER * decoder)
{
    // move to idle
    ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateIdle);

    // set input image format
    OMX_IMAGE_PARAM_PORTFORMATTYPE imagePortFormat;
    memset(&imagePortFormat, 0, sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE));
    imagePortFormat.nSize = sizeof(OMX_IMAGE_PARAM_PORTFORMATTYPE);
    imagePortFormat.nVersion.nVersion = OMX_VERSION;
    imagePortFormat.nPortIndex = decoder->imageDecoder->inPort;
    imagePortFormat.eCompressionFormat = OMX_IMAGE_CodingJPEG;
    OMX_SetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamImagePortFormat, &imagePortFormat);

    // get buffer requirements
    OMX_PARAM_PORTDEFINITIONTYPE portdef;
    portdef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    portdef.nVersion.nVersion = OMX_VERSION;
    portdef.nPortIndex = decoder->imageDecoder->inPort;
    OMX_GetParameter(decoder->imageDecoder->handle,
		     OMX_IndexParamPortDefinition, &portdef);

    // enable the port and setup the buffers
    OMX_SendCommand(decoder->imageDecoder->handle,
		    OMX_CommandPortEnable,
		    decoder->imageDecoder->inPort, NULL);
    decoder->inputBufferHeaderCount = portdef.nBufferCountActual;
    // allocate pointer array
    decoder->ppInputBufferHeader =
	(OMX_BUFFERHEADERTYPE **) malloc(sizeof(void) *
					 decoder->inputBufferHeaderCount);
    // allocate each buffer
    int             i;
    for (i = 0; i < decoder->inputBufferHeaderCount; i++) {
	if (OMX_AllocateBuffer(decoder->imageDecoder->handle,
			       &decoder->ppInputBufferHeader[i],
			       decoder->imageDecoder->inPort,
			       (void *) i,
			       portdef.nBufferSize) != OMX_ErrorNone) {
	    perror("Allocate decode buffer");
	    return OMXJPEG_ERROR_MEMORY;
	}
    }
    // wait for port enable to complete - which it should once buffers are 
    // assigned
    int             ret =
	ilclient_wait_for_event(decoder->imageDecoder->component,
				OMX_EventCmdComplete,
				OMX_CommandPortEnable, 0,
				decoder->imageDecoder->inPort, 0,
				0, TIMEOUT_MS);
    if (ret != 0) {
	fprintf(stderr, "Did not get port enable %d\n", ret);
	return OMXJPEG_ERROR_EXECUTING;
    }
    // start executing the decoder 
    ret = OMX_SendCommand(decoder->imageDecoder->handle,
			  OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if (ret != 0) {
	fprintf(stderr, "Error starting image decoder %x\n", ret);
	return OMXJPEG_ERROR_EXECUTING;
    }
    ret = ilclient_wait_for_event(decoder->imageDecoder->component,
				  OMX_EventCmdComplete,
				  OMX_StateExecuting, 0, 0, 1, 0,
				  TIMEOUT_MS);
    if (ret != 0) {
	fprintf(stderr, "Did not receive executing stat %d\n", ret);
	// return OMXJPEG_ERROR_EXECUTING;
    }

    return OMXJPEG_OK;
}

// this function run the boilerplate to setup the openmax components;
int
setupOpenMaxJpegDecoder(OPENMAX_JPEG_DECODER ** pDecoder)
{
    *pDecoder = malloc(sizeof(OPENMAX_JPEG_DECODER));
    if (pDecoder[0] == NULL) {
	perror("malloc decoder");
	return OMXJPEG_ERROR_MEMORY;
    }
    memset(*pDecoder, 0, sizeof(OPENMAX_JPEG_DECODER));

    if ((pDecoder[0]->client = ilclient_init()) == NULL) {
	perror("ilclient_init");
	return OMXJPEG_ERROR_ILCLIENT_INIT;
    }

    if (OMX_Init() != OMX_ErrorNone) {
	ilclient_destroy(pDecoder[0]->client);
	perror("OMX_Init");
	return OMXJPEG_ERROR_OMX_INIT;
    }
    // prepare the image decoder
    int             ret = prepareImageDecoder(pDecoder[0]);
    if (ret != OMXJPEG_OK)
	return ret;

    ret = prepareResizer(pDecoder[0]);
    if (ret != OMXJPEG_OK)
	return ret;

    ret = startupImageDecoder(pDecoder[0]);
    if (ret != OMXJPEG_OK)
	return ret;

    return OMXJPEG_OK;
}

// this function passed the jpeg image buffer in, and returns the decoded
// image
int
decodeImage(OPENMAX_JPEG_DECODER * decoder, char *sourceImage,
	    size_t imageSize)
{
    char           *sourceOffset = sourceImage;	// we store a separate
						// buffer ot image so we
						// can offset it
    size_t          toread = 0;	// bytes left to read from buffer
    toread += imageSize;
    int             bFilled = 0;	// have we filled our output
					// buffer
    bufferIndex = 0;

    while (toread > 0) {
	// get next buffer from array
	OMX_BUFFERHEADERTYPE *pBufHeader =
	    decoder->ppInputBufferHeader[bufferIndex];

	// step index and reset to 0 if required
	bufferIndex++;
	if (bufferIndex >= decoder->inputBufferHeaderCount)
	    bufferIndex = 0;

	// work out the next chunk to load into the decoder
	if (toread > pBufHeader->nAllocLen)
	    pBufHeader->nFilledLen = pBufHeader->nAllocLen;
	else
	    pBufHeader->nFilledLen = toread;

	toread = toread - pBufHeader->nFilledLen;

	// pass the bytes to the buffer
	memcpy(pBufHeader->pBuffer, sourceOffset, pBufHeader->nFilledLen);

	// update the buffer pointer and set the input flags

	sourceOffset = sourceOffset + pBufHeader->nFilledLen;
	pBufHeader->nOffset = 0;
	pBufHeader->nFlags = 0;
	if (toread <= 0) {
	    pBufHeader->nFlags = OMX_BUFFERFLAG_EOS;
	}
	// empty the current buffer
	int             ret =
	    OMX_EmptyThisBuffer(decoder->imageDecoder->handle,
				pBufHeader);

	if (ret != OMX_ErrorNone) {
	    perror("Empty input buffer");
	    fprintf(stderr, "return code %x\n", ret);
	    return OMXJPEG_ERROR_MEMORY;
	}
	// wait for buffer to empty or port changed event
	int             done = 0;
	while ((done == 0) && (decoder->pOutputBufferHeader == NULL)) {
	    if (decoder->pOutputBufferHeader == NULL) {
		ret =
		    ilclient_wait_for_event
		    (decoder->imageDecoder->component,
		     OMX_EventPortSettingsChanged,
		     decoder->imageDecoder->outPort, 0, 0, 1, 0, 5);

		if (ret == 0) {
		    ret = portSettingsChanged(decoder);
		    if (ret != OMXJPEG_OK)
			return ret;
		}
	    } else {
		ret =
		    ilclient_remove_event(decoder->imageDecoder->component,
					  OMX_EventPortSettingsChanged,
					  decoder->imageDecoder->outPort,
					  0, 0, 1);
		if (ret == 0)
		    portSettingsChangedAgain(decoder);

	    }

	    // check to see if buffer is now empty
	    if (pBufHeader->nFilledLen == 0)
		done = 1;

	    if ((done == 0)
		|| (decoder->pOutputBufferHeader == NULL))
		sleep(1);
	}

	// fill the buffer if we have created the buffer
	if ((bFilled == 0) && (decoder->pOutputBufferHeader != NULL)) {
	    ret = OMX_FillThisBuffer(decoder->imageResizer->handle,
				     decoder->pOutputBufferHeader);
	    if (ret != OMX_ErrorNone) {
		perror("Filling output buffer");
		fprintf(stderr, "Error code %x\n", ret);
		return OMXJPEG_ERROR_MEMORY;
	    }

	    bFilled = 1;
	}
    }

    // wait for buffer to fill
    /*
     * while(pBufHeader->nFilledLen == 0) { sleep(5); } 
     */

    // wait for end of stream events
    int             ret =
	ilclient_wait_for_event(decoder->imageDecoder->component,
				OMX_EventBufferFlag,
				decoder->imageDecoder->outPort, 1,
				OMX_BUFFERFLAG_EOS, 1,
				0, 2);
    if (ret != 0) {
	fprintf(stderr, "No EOS event on image decoder %d\n", ret);
    }
    ret = ilclient_wait_for_event(decoder->imageResizer->component,
				  OMX_EventBufferFlag,
				  decoder->imageResizer->outPort, 1,
				  OMX_BUFFERFLAG_EOS, 1, 0, 2);
    if (ret != 0) {
	fprintf(stderr, "No EOS event on image resizer %d\n", ret);
    }
    return OMXJPEG_OK;
}

// this function cleans up the decoder.
void
cleanup(OPENMAX_JPEG_DECODER * decoder)
{
    // flush everything through
    OMX_SendCommand(decoder->imageDecoder->handle,
		    OMX_CommandFlush, decoder->imageDecoder->outPort,
		    NULL);
    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandFlush, 0,
			    decoder->imageDecoder->outPort, 0, 0,
			    TIMEOUT_MS);
    OMX_SendCommand(decoder->imageResizer->handle, OMX_CommandFlush,
		    decoder->imageResizer->inPort, NULL);
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandFlush, 0,
			    decoder->imageResizer->inPort, 1, 0,
			    TIMEOUT_MS);

    OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandPortDisable,
		    decoder->imageDecoder->inPort, NULL);

    int             i = 0;
    for (i = 0; i < decoder->inputBufferHeaderCount; i++) {
	OMX_BUFFERHEADERTYPE *vpBufHeader =
	    decoder->ppInputBufferHeader[i];

	OMX_FreeBuffer(decoder->imageDecoder->handle,
		       decoder->imageDecoder->inPort, vpBufHeader);
    }

    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageDecoder->inPort, 0, 0,
			    TIMEOUT_MS);

    OMX_SendCommand(decoder->imageResizer->handle, OMX_CommandPortDisable,
		    decoder->imageResizer->outPort, NULL);

    OMX_FreeBuffer(decoder->imageResizer->handle,
		   decoder->imageResizer->outPort,
		   decoder->pOutputBufferHeader);

    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageResizer->outPort, 0, 0,
			    TIMEOUT_MS);

    OMX_SendCommand(decoder->imageDecoder->handle, OMX_CommandPortDisable,
		    decoder->imageDecoder->outPort, NULL);
    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageDecoder->outPort, 0, 0,
			    TIMEOUT_MS);

    OMX_SendCommand(decoder->imageResizer->handle, OMX_CommandPortDisable,
		    decoder->imageResizer->inPort, NULL);
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandPortDisable,
			    0, decoder->imageResizer->inPort, 0, 0,
			    TIMEOUT_MS);

    OMX_SetupTunnel(decoder->imageDecoder->handle,
		    decoder->imageDecoder->outPort, NULL, 0);
    OMX_SetupTunnel(decoder->imageResizer->handle,
		    decoder->imageResizer->inPort, NULL, 0);

    ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateIdle);
    ilclient_change_component_state(decoder->imageResizer->component,
				    OMX_StateIdle);

    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, 0, TIMEOUT_MS);
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateIdle, 0, 0, TIMEOUT_MS);

    ilclient_change_component_state(decoder->imageDecoder->component,
				    OMX_StateLoaded);
    ilclient_change_component_state(decoder->imageResizer->component,
				    OMX_StateLoaded);

    ilclient_wait_for_event(decoder->imageDecoder->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateLoaded, 0, 0, TIMEOUT_MS);
    ilclient_wait_for_event(decoder->imageResizer->component,
			    OMX_EventCmdComplete, OMX_CommandStateSet, 0,
			    OMX_StateLoaded, 0, 0, TIMEOUT_MS);

    OMX_Deinit();

    if (decoder->client != NULL) {
	ilclient_destroy(decoder->client);
    }
}

int
main(int argc, char *argv[])
{
    OPENMAX_JPEG_DECODER *pDecoder;
    char           *sourceImage;
    size_t          imageSize;
    int             s;
    if (argc < 2) {
	printf("Usage: %s <filename>\n", argv[0]);
	return -1;
    }
    FILE           *fp = fopen(argv[1], "rb");
    if (!fp) {
	printf("File %s not found.\n", argv[1]);
    }
    fseek(fp, 0L, SEEK_END);
    imageSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    sourceImage = malloc(imageSize);
    assert(sourceImage != NULL);
    s = fread(sourceImage, 1, imageSize, fp);
    assert(s == imageSize);
    fclose(fp);
    bcm_host_init();
    s = setupOpenMaxJpegDecoder(&pDecoder);
    assert(s == 0);
    s = decodeImage(pDecoder, sourceImage, imageSize);
    assert(s == 0);
    cleanup(pDecoder);
    free(sourceImage);
    return 0;
}
