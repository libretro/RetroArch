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

#ifndef _RTP_BASE64_H_
#define _RTP_BASE64_H_

#include "containers/containers.h"

/** Returns the number of bytes encoded by the given Base64 encoded string.
 *
 * \param str The Base64 encoded string.
 * \param str_len The number of characters in the string.
 * \return The number of bytes that can be decoded. */
uint32_t rtp_base64_byte_length(const char *str, uint32_t str_len);

/** Decodes a Base64 encoded string into a byte buffer.
 *
 * \param str The Base64 encoded string.
 * \param str_len The number of characters in the string.
 * \param buffer The buffer to receive the decoded output.
 * \param buffer_len The maximum number of bytes to put in the buffer.
 * \return Pointer to byte after the last one converted, or NULL on error. */
uint8_t *rtp_base64_decode(const char *str, uint32_t str_len, uint8_t *buffer, uint32_t buffer_len);

#endif /* _RTP_BASE64_H_ */
