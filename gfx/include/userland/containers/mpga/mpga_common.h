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
#define MPGA_HEADER_SIZE 6

#define MPGA_MODE_STEREO  0
#define MPGA_MODE_JSTEREO 1
#define MPGA_MODE_DUAL    2
#define MPGA_MODE_MONO    3

/*****************************************************************************/
static VC_CONTAINER_STATUS_T mpga_read_header( uint8_t frame_header[MPGA_HEADER_SIZE],
   uint32_t *p_frame_size, unsigned int *p_frame_bitrate, unsigned int *p_version,
   unsigned int *p_layer, unsigned int *p_sample_rate, unsigned int *p_channels,
   unsigned int *p_frame_size_samples, unsigned int *p_offset )
{
   static const uint16_t mpga_bitrate[2][3][15] =
   {{{0, 32,  64,  96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448}, /* MPEG1, Layer 1 */
     {0, 32,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384}, /* MPEG1, Layer 2 */
     {0, 32,  40,  48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320}},/* MPEG1, Layer 3 */
    {{0, 32,  48,  56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256}, /* MPEG2 and MPEG2.5, Layer 1 */
     {0,  8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}, /* MPEG2 and MPEG2.5, Layer 2 */
     {0,  8,  16,  24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}} /* MPEG2 and MPEG2.5, Layer 3 */};
   static const uint16_t mpga_sample_rate[] = {44100, 48000, 32000};
   static const uint16_t mpga_frame_size[] = {384, 1152, 576};

   unsigned int version, layer, br_id, sr_id, emphasis;
   unsigned int bitrate, sample_rate, padding, mode;

   /* Check frame sync, 11 bits as we want to allow for MPEG2.5 */
   if (frame_header[0] != 0xff || (frame_header[1] & 0xe0) != 0xe0)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   version = 4 - ((frame_header[1] >> 3) & 3);
   layer = 4 - ((frame_header[1] >> 1) & 3 );
   br_id = (frame_header[2] >> 4) & 0xf;
   sr_id = (frame_header[2] >> 2) & 3;
   padding = (frame_header[2] >> 1) & 1;
   mode = (frame_header[3] >> 6) & 3;
   emphasis = (frame_header[3]) & 3;

   /* Check for invalid values */
   if (version == 3 || layer == 4 || br_id == 15 || sr_id == 3 || emphasis == 2)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   if (version == 4) version = 3;

   bitrate = mpga_bitrate[version == 1 ? 0 : 1][layer-1][br_id];
   bitrate *= 1000;

   sample_rate = mpga_sample_rate[sr_id];
   sample_rate >>= (version - 1);

   if (p_version) *p_version = version;
   if (p_layer) *p_layer = layer;
   if (p_sample_rate) *p_sample_rate = sample_rate;
   if (p_channels) *p_channels = mode == MPGA_MODE_MONO ? 1 : 2;
   if (p_frame_bitrate) *p_frame_bitrate = bitrate;
   if (p_offset) *p_offset = 0;

   if (p_frame_size_samples)
   {
      *p_frame_size_samples = mpga_frame_size[layer - 1];
      if (version == 1 && layer == 3) *p_frame_size_samples <<= 1;
   }

   if (!p_frame_size)
      return VC_CONTAINER_SUCCESS;

   if (!bitrate)
      *p_frame_size = 0;
   else if (layer == 1)
      *p_frame_size = (padding + bitrate * 12 / sample_rate) * 4;
   else if (layer == 2)
      *p_frame_size = padding + bitrate * 144 / sample_rate;
   else
      *p_frame_size = padding + bitrate * (version == 1 ? 144 : 72) / sample_rate;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T adts_read_header( uint8_t frame_header[MPGA_HEADER_SIZE],
   uint32_t *p_frame_size, unsigned int *p_frame_bitrate, unsigned int *p_version,
   unsigned int *p_layer, unsigned int *p_sample_rate, unsigned int *p_channels,
   unsigned int *p_frame_size_samples, unsigned int *p_offset )
{
   static const unsigned int adts_sample_rate[16] =
   {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350};
   unsigned int profile, sr_id, bitrate, sample_rate, frame_size, channels, crc;
   unsigned int frame_size_samples = 1024;

   /* Check frame sync (12 bits) */
   if (frame_header[0] != 0xff || (frame_header[1] & 0xf0) != 0xf0)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   /* Layer must be 0 */
   if ((frame_header[1] >> 1) & 3)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   crc = !(frame_header[1] & 0x1);
   profile = (frame_header[2] >> 6) + 1; /* MPEG-4 Audio Object Type */

   sr_id = (frame_header[2] >> 2) & 0xf;
   sample_rate = adts_sample_rate[sr_id];
   channels = ((frame_header[2] & 0x1) << 2) | ((frame_header[3] >> 6) & 0x3);
   frame_size = ((frame_header[3] & 0x03) << 11) | (frame_header[4] << 3) | (frame_header[5] >> 5);

   if (!sample_rate || !channels || !frame_size)
      return VC_CONTAINER_ERROR_FORMAT_INVALID;

   bitrate = frame_size * 8 * sample_rate / frame_size_samples;

   if (p_version) *p_version = profile;
   if (p_layer) *p_layer = 0;
   if (p_sample_rate) *p_sample_rate = sample_rate;
   if (p_channels) *p_channels = channels;
   if (p_frame_bitrate) *p_frame_bitrate = bitrate;
   if (p_frame_size) *p_frame_size = frame_size;
   if (p_frame_size_samples) *p_frame_size_samples = frame_size_samples;
   if (p_offset) *p_offset = crc ? 9 : 7;

   return VC_CONTAINER_SUCCESS;
}
