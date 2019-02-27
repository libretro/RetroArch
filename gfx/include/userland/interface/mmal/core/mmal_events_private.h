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

#ifndef MMAL_EVENTS_PRIVATE_H
#define MMAL_EVENTS_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mmal_events.h"

/** Send an error event through the component's control port.
 * The error event data will be the MMAL_STATUS_T passed in.
 *
 * @param component component to receive the error event.
 * @param status the error status to be sent.
 * @return MMAL_SUCCESS or an error if the event could not be sent.
 */
MMAL_STATUS_T mmal_event_error_send(MMAL_COMPONENT_T *component, MMAL_STATUS_T status);

/** Send an eos event through a specific port.
 *
 * @param port port to receive the error event.
 * @return MMAL_SUCCESS or an error if the event could not be sent.
 */
MMAL_STATUS_T mmal_event_eos_send(MMAL_PORT_T *port);

/** Forward an event onto an output port.
 * This will allocate a new event buffer on the output port, make a copy
 * of the event buffer which will then be forwarded.
 *
 * @event event to forward.
 * @param port port to forward event to.
 * @return MMAL_SUCCESS or an error if the event could not be forwarded.
 */
MMAL_STATUS_T mmal_event_forward(MMAL_BUFFER_HEADER_T *event, MMAL_PORT_T *port);

#ifdef __cplusplus
}
#endif

#endif /* MMAL_EVENTS_PRIVATE_H */
