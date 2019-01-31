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

/** \file
 * Multi-Media Abstraction Layer API
 */

#ifndef MMAL_H
#define MMAL_H

/**
  *
  * \mainpage Multi-Media Abstraction Layer (MMAL). Draft Version 0.1.
  *
  * \par Contents
  * - \ref intro_sec
  * - \ref features
  * - \ref concepts
  * - \ref comp
  * - \ref create
  * - \ref port
  * - \ref buf
  * - \ref queue
  * - \ref pool
  * - \ref param
  * - \ref events
  * - \ref version
  * - \ref example
  *
  * \section intro_sec Introduction
  *
  * MMAL (Multi-Media Abstraction Layer) is a framework which is used to provide a host-side,
  * simple and relatively low-level interface to multimedia components running on VideoCore.
  * It also provides a component interface so that new components can be easily created and
  * integrated into the framework.
  *
  * There is no requirement that all the components be running on VideoCore as MMAL doesn't
  * put any restriction on where components live. The current implementation for instance
  * provides some components which can be run on both host-side or VideoCore (e.g. the splitter
  * component).
  *
  * \section features Features
  *
  * The MMAL API has been designed to support all the following features:
  * - Sufficiently generic to support different kinds of multimedia component.
  * - Simple to use from client side (mostly synchronous except where it matters).
  * - Straightforward API for designing components (e.g. avoids multiple data paths, as found in RIL).
  * - Allows for fully-optimised implementation of components (e.g. zero-copy buffer passing).
  * - Portability (API is self-contained).
  * - Supports multiple instances (e.g. of VideoCore).
  * - Extensible without breaking source or binary backward compatibility.
  *
  * \section concepts API concepts
  *
  * The MMAL API is based on the concept of components, ports and buffer headers.
  * Clients create MMAL components which expose ports for each individual
  * elementary stream of data they support (e.g. audio/video). Components expose
  * input ports to receive data from the client, and expose output ports
  * to return data to the client.
  *
  * Data sent to or received from the component needs to be attached to a buffer header.
  * Buffer headers are necessary because they contain buffer specific ancillary data which is
  * necessary for the component and client to do their processing (e.g timestamps).
  *
  * \section comp Components
  *
  * MMAL lets clients create multi-media components (video encoders,
  * video decoders, camera, and so-on) using a common API. Clients exchange
  * data with components using buffer headers. A buffer header
  * has a pointer to the payload data.
  * Buffer headers are sent to and received from ports that are provided by components.
  *
  * A typical decoder component would have a single input port and a
  * single output port, but the same architecture could also be used
  * for components with different layouts (e.g. a camera with a
  * capture and preview port, or a debugging component with just an input port).
  *
  * \subsection create Component Creation
  *
  * Each component is identified by a unique name. To create a specific component
  * the client needs to call \ref mmal_component_create with the desired component's
  * name as an argument.
  * This call will return a context (\ref MMAL_COMPONENT_T) to the component. This
  * context will expose the input and output ports (\ref MMAL_PORT_T) supported
  * by this specific component.
  *
  * \note All VideoCore components have a name starting with the "vc." prefix (this prefix
  * is used to distinguish when a creation request needs to be forwarded to VideoCore).
  *
  * \section port Component Ports
  *
  * A port (\ref MMAL_PORT_T) is the entity which exposes an elementary stream
  * (\ref MMAL_ES_FORMAT_T) on a component. It is also the entity to which buffer headers
  * (\ref MMAL_BUFFER_HEADER_T) are sent or from which they are received.
  *
  * Clients do not need to create ports. They are created automatically by
  * the component when this one is created but the format of a port might need to
  * be set by the client depending on the type of component the client is using.
  *
  * For example, for a video decoding component, one input port and one output port
  * will be available. The format of the input port must be set by the
  * client (using \ref mmal_port_format_commit) while the format of the output port
  * will be automatically set by the component once the component has enough information
  * to find out what its format should be.
  *
  * If the input port format contains enough information for the component to determine
  * the format of the output port straight away, then the output port will be set to the proper
  * format when \ref mmal_port_format_commit returns. Otherwise the output format will be set to
  * \ref MMAL_ENCODING_UNKNOWN until the component is fed enough data to determine the format
  * of the output port.
  * When this happens, the client will receive an event on the output port, signalling
  * that its format has changed.
  *
  * \section buf Buffer Headers
  *
  * Buffer headers (\ref MMAL_BUFFER_HEADER_T) are used to exchange data with components.
  * They do not contain the data directly but instead contain a pointer to the data being
  * transferred.
  *
  * Separating the buffer headers from the payload means that the memory for the data can
  * be allocated outside of MMAL (e.g. if it is supplied by an external library) while still
  * providing a consistent way to exchange data between clients and components.
  *
  * Buffer headers are allocated from pools and are reference counted. The refcount
  * will drop when \ref mmal_buffer_header_release is called and the buffer header
  * will be recycled to its pool when it reaches zero.
  * The client can be notified when the buffer header is recycled so that it can recycle the
  * associated payload memory as well.
  *
  * A pool of buffer headers should be created after committing the format of the port. When
  * the format is changed, the minimum and recommended size and number of buffers may change.
  *
  * \note The current number of buffers and their size (\ref MMAL_PORT_T::buffer_num and \ref
  * MMAL_PORT_T::buffer_size) are not modified by MMAL, and must be updated by the client as
  * required after changes to a port's format.
  *
  * \subsection queue Queues of Buffer Headers
  *
  * Queues (\ref MMAL_QUEUE_T) are a facility that allows thread-safe processing of buffer headers
  * from the client. Callbacks triggered by a MMAL component when it sends a buffer header to the
  * client can simply put the buffer in a queue and let the main processing thread of the client
  * get its data from the queue.
  *
  * \subsection pool Pools of Buffer Headers
  *
  * Pools (\ref MMAL_POOL_T) let clients allocate a fixed number of buffer headers, and 
  * a queue (\ref MMAL_QUEUE_T). They are used for buffer header allocation.
  * Optionally a pool can also allocate the payload memory for the client.
  *
  * Pools can also be resized after creation, for example, if the port format is changed leading
  * to a new number or size of buffers being required.
  *
  * \section param Port Parameters
  *
  * Components support setting and getting component specific parameters using
  * \ref mmal_port_parameter_set and \ref mmal_port_parameter_get. Parameters
  * are identified using an integer index; parameter data is binary. See the \ref MMAL_PARAMETER_IDS
  * "Pre-defined MMAL parameter IDs" page for more information on the pre-defined parameters.
  *
  * \section events Port Events
  *
  * Components can generate events on their ports. Events are sent to clients
  * as buffer headers and thus when the client receives a buffer header on one
  * of the component's port it should check if the buffer header is an event
  * and in which case process it and then release it (\ref mmal_buffer_header_release).
  * The reason for transmitting events in-band with the actual data is that it
  * is often very valuable to know exactly when the event happens relative to the
  * the actual data (e.g. with a focus event, from which video frame are we in focus).\n
  * Buffer headers used to transmit events are allocated internally by the framework
  * so it is important to release the buffer headers with \ref mmal_buffer_header_release
  * so the buffer headers make it back to their actual owner.
  *
  * Event buffer headers are allocated when the component is created, based on the
  * minimum number and size of control port buffers set by the component. Component
  * wide events (not port specific) are sent to the control port callback when that
  * port is enabled. Port events are sent to the port callback, the same as data
  * buffers, but the 'cmd' field is non-zero.
  *
  * \section version Versioning
  *
  * The API requires that the MMAL core be the same or more recent version
  * as the components and clients. Clients and components can be older and
  * the API will still work both at compile-time and run-time.
  *
  * \section example Example Code
  *
  * The following code is a simple example on how to do video decoding using MMAL. Note that
  * the code is intended to be clear and illustrate how to use MMAL at its most fundamental
  * level, not necessarily the most efficient way to achieve the same result. Use of opaque
  * images, tunneling and zero-copy inter-processor buffers can all improve the performance
  * or reduce the load.
  *
  * The \ref MmalConnectionUtility "Port Connection Utility" functions can also be used to
  * replace much of the common "boilerplate" code, especially when a pipeline of several
  * components needs to be processed.
  *
  * \code
  * #include <mmal.h>
  * ...
  * static void input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
  * {
  *    // The decoder is done with the data, just recycle the buffer header into its pool
  *    mmal_buffer_header_release(buffer);
  * }
  * static void output_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
  * {
  *    MMAL_QUEUE_T *queue = (MMAL_QUEUE_T *)port->userdata;
  *    mmal_queue_put(queue, buffer); // Queue the decoded video frame
  * }
  * ...
  *
  * MMAL_COMPONENT_T *decoder = 0;
  * MMAL_STATUS_T status;
  *
  * // Create the video decoder component on VideoCore
  * status = mmal_component_create("vc.ril.video_decoder", &decoder);
  * ABORT_IF_ERROR(status);
  *
  * // Set format of video decoder input port
  * MMAL_ES_FORMAT_T *format_in = decoder->input[0]->format;
  * format_in->type = MMAL_ES_TYPE_VIDEO;
  * format_in->encoding = MMAL_ENCODING_H264;
  * format_in->es->video.width = 1280;
  * format_in->es->video.height = 720;
  * format_in->es->video.frame_rate.num = 30;
  * format_in->es->video.frame_rate.den = 1;
  * format_in->es->video.par.num = 1;
  * format_in->es->video.par.den = 1;
  * format_in->flags = MMAL_ES_FORMAT_FLAG_FRAMED;
  * status = mmal_format_extradata_alloc(format_in, YOUR_H264_CODEC_HEADER_BYTES_SIZE);
  * ABORT_IF_ERROR(status);
  * format_in->extradata_size = YOUR_H264_CODEC_HEADER_BYTES_SIZE;
  * memcpy(format_in->extradata, YOUR_H264_CODEC_HEADER_BYTES, format_in->extradata_size);
  *
  * status = mmal_port_format_commit(decoder->input[0]);
  * ABORT_IF_ERROR(status);
  *
  * // Once the call to mmal_port_format_commit() on the input port returns, the decoder will
  * // have set the format of the output port.
  * // If the decoder still doesn t have enough information to determine the format of the
  * // output port, the encoding will be set to unknown. As soon as the decoder receives
  * // enough stream data to determine the format of the output port it will send an event
  * // to the client to signal that the format of the port has changed.
  * // However, for the sake of simplicity this example assumes that the decoder was given
  * // all the necessary information right at the start (i.e. video format and codec header bytes)
  * MMAL_FORMAT_T *format_out = decoder->output[0]->format;
  * if (format_out->encoding == MMAL_ENCODING_UNKNOWN)
  *    ABORT();
  *
  * // Now we know the format of both ports and the requirements of the decoder, we can create
  * // our buffer headers and their associated memory buffers. We use the buffer pool API for this.
  * decoder->input[0]->buffer_num = decoder->input[0]->buffer_num_min;
  * decoder->input[0]->buffer_size = decoder->input[0]->buffer_size_min;
  * MMAL_POOL_T *pool_in = mmal_pool_create(decoder->input[0]->buffer_num,
  *                                         decoder->input[0]->buffer_size);
  * decoder->output[0]->buffer_num = decoder->output[0]->buffer_num_min;
  * decoder->output[0]->buffer_size = decoder->output[0]->buffer_size_min;
  * MMAL_POOL_T *pool_out = mmal_pool_create(decoder->output[0]->buffer_num,
  *                                          decoder->output[0]->buffer_size);
  *
  * // Create a queue to store our decoded video frames. The callback we will get when
  * // a frame has been decoded will put the frame into this queue.
  * MMAL_QUEUE_T *queue_decoded_frames = mmal_queue_create();
  * decoder->output[0]->userdata = (void)queue_decoded_frames;
  *
  * // Enable all the input port and the output port.
  * // The callback specified here is the function which will be called when the buffer header
  * // we sent to the component has been processed.
  * status = mmal_port_enable(decoder->input[0], input_callback);
  * ABORT_IF_ERROR(status);
  * status = mmal_port_enable(decoder->output[0], output_callback);
  * ABORT_IF_ERROR(status);
  *
  * // Enable the component. Components will only process data when they are enabled.
  * status = mmal_component_enable(decoder);
  * ABORT_IF_ERROR(status);
  *
  * // Data processing loop
  * while (1)
  * {
  *    MMAL_BUFFER_HEADER_T *buffer;
  *
  *    // The client needs to implement its own blocking code.
  *    // (e.g. a semaphore which is posted when a buffer header is put in one of the queues)
  *    WAIT_FOR_QUEUES_TO_HAVE_BUFFERS();
  *
  *    // Send empty buffers to the output port of the decoder to allow the decoder to start
  *    // producing frames as soon as it gets input data
  *    while ((buffer = mmal_queue_get(pool_out->queue)) != NULL)
  *    {
  *       status = mmal_port_send_buffer(decoder->output[0], buffer);
  *       ABORT_IF_ERROR(status);
  *    }
  *
  *    // Send data to decode to the input port of the video decoder
  *    if ((buffer = mmal_queue_get(pool_in->queue)) != NULL)
  *    {
  *       READ_DATA_INTO_BUFFER(buffer);
  *
  *       status = mmal_port_send_buffer(decoder->input[0], buffer);
  *       ABORT_IF_ERROR(status);
  *    }
  *
  *    // Get our decoded frames. We also need to cope with events
  *    // generated from the component here.
  *    while ((buffer = mmal_queue_get(queue_decoded_frames)) != NULL)
  *    {
  *       if (buffer->cmd)
  *       {
  *          // This is an event. Do something with it and release the buffer.
  *          mmal_buffer_header_release(buffer);
  *          continue;
  *       }
  *
  *       // We have a frame, do something with it (why not display it for instance?).
  *       // Once we're done with it, we release it. It will magically go back
  *       // to its original pool so it can be reused for a new video frame.
  *       mmal_buffer_header_release(buffer);
  *    }
  * }
  *
  * // Cleanup everything
  * mmal_component_destroy(decoder);
  * mmal_pool_destroy(pool_in);
  * mmal_pool_destroy(pool_out);
  * mmal_queue_destroy(queue_decode_frames);
  *
  * \endcode
  */

#include "mmal_common.h"
#include "mmal_types.h"
#include "mmal_port.h"
#include "mmal_component.h"
#include "mmal_parameters.h"
#include "mmal_queue.h"
#include "mmal_pool.h"
#include "mmal_events.h"

/**/
/** \name API Version
 * The following define the version number of the API */
/* @{ */
/** Major version number.
 * This changes when the API breaks in a way which is not backward compatible. */
#define MMAL_VERSION_MAJOR 0
/** Minor version number.
 * This changes each time the API is extended in a way which is still source and
 * binary compatible. */
#define MMAL_VERSION_MINOR 1

#define MMAL_VERSION (MMAL_VERSION_MAJOR << 16 | MMAL_VERSION_MINOR)
#define MMAL_VERSION_TO_MAJOR(a) (a >> 16)
#define MMAL_VERSION_TO_MINOR(a) (a & 0xFFFF)
/* @} */

#endif /* MMAL_H */
