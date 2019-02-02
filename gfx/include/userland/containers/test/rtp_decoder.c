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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "containers/containers.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_io.h"

#include "nb_io.h"

#define MAXIMUM_BUFFER_SIZE   65000
#define MINIMUM_BUFFER_SPACE  1500

#define INITIAL_READ_BUFFER_SIZE 8000
#define MAXIMUM_READ_BUFFER_SIZE 64000

#define BYTES_PER_ROW   32

#define HAS_PADDING        0x20
#define HAS_EXTENSION      0x10
#define CSRC_COUNT_MASK    0x0F

#define HAS_MARKER         0x80
#define PAYLOAD_TYPE_MASK  0x7F

#define EXTENSION_LENGTH_MASK 0x0000FFFF
#define EXTENSION_ID_SHIFT 16

#define LOWEST_VERBOSITY         1
#define BASIC_HEADER_VERBOSITY   2
#define FULL_HEADER_VERBOSITY    3
#define FULL_PACKET_VERBOSITY    4

#define ESCAPE_CHARACTER   0x1B

static bool seen_first_packet;
static uint16_t expected_next_seq_num;

static bool do_print_usage;
static uint32_t verbosity;
static const char *read_uri;
static const char *packet_save_file;
static bool packet_save_is_pktfile;

static uint16_t network_to_host_16(const uint8_t *buffer)
{
   return (buffer[0] << 8) | buffer[1];
}

static uint32_t network_to_host_32(const uint8_t *buffer)
{
   return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/** Avoid alignment problems when writing a word value to the buffer */
static void store_u32(uint8_t *buffer, uint32_t value)
{
   buffer[0] = (uint8_t)value;
   buffer[1] = (uint8_t)(value >> 8);
   buffer[2] = (uint8_t)(value >> 16);
   buffer[3] = (uint8_t)(value >> 24);
}

/** Avoid alignment problems when reading a word value from the buffer */
static uint32_t fetch_u32(uint8_t *buffer)
{
   return (buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0];
}

static bool marker_bit_set(const uint8_t *buffer, size_t buffer_len)
{
   if (buffer_len < 2)
      return false;

   return (buffer[1] & HAS_MARKER);
}

static void dump_bytes(const uint8_t *buffer, size_t buffer_len)
{
   char dump_str[3 * BYTES_PER_ROW + 1];
   int in_row = 0;

   while (buffer_len--)
   {
      sprintf(dump_str + 3 * in_row, "%2.2X ", *buffer++);
      if (++in_row == BYTES_PER_ROW)
      {
         LOG_INFO(NULL, dump_str);
         in_row = 0;
      }
   }
   
   if (in_row)
   {
      LOG_INFO(NULL, dump_str);
   }
}

static bool decode_packet(const uint8_t *buffer, size_t buffer_len)
{
   uint8_t flags;
   uint8_t payload_type;
   uint16_t seq_num;
   uint32_t timestamp;
   uint32_t ssrc;
   uint32_t csrc_count;
   
   if (buffer_len < 12)
   {
      LOG_ERROR(NULL, "Packet too small: basic header missing");
      return false;
   }
   
   flags = buffer[0];
   payload_type = buffer[1];
   seq_num = network_to_host_16(buffer + 2);
   timestamp = network_to_host_32(buffer + 4);
   ssrc = network_to_host_32(buffer + 8);

   if (seen_first_packet && seq_num != expected_next_seq_num)
   {
      int16_t missing_packets = seq_num - expected_next_seq_num;

      LOG_INFO(NULL, "*** Sequence break, expected %hu, got %hu ***", expected_next_seq_num, seq_num);
      if (missing_packets > 0)
         LOG_INFO(NULL, "*** Jumped forward %hd packets ***", missing_packets);
      else
         LOG_INFO(NULL, "*** Jumped backward %hd packets ***", -missing_packets);
   }
   seen_first_packet = true;
   expected_next_seq_num = seq_num + 1;

   /* Dump the basic header information */
   if (verbosity >= BASIC_HEADER_VERBOSITY)
   {
      LOG_INFO(NULL, "Version: %d\nPayload type: %d%s\nSequence: %d\nTimestamp: %u\nSSRC: 0x%8.8X",
            flags >> 6, payload_type & PAYLOAD_TYPE_MASK,
            (const char *)((payload_type & HAS_MARKER) ? " (M)" : ""),
            seq_num, timestamp, ssrc);
   }

   buffer += 12;
   buffer_len -= 12;

   if (verbosity >= FULL_HEADER_VERBOSITY)
   {
      /* Dump the CSRCs, if any */
      csrc_count = flags & CSRC_COUNT_MASK;
      if (csrc_count)
      {
         uint32_t ii;
      
         if (buffer_len < (csrc_count * 4))
         {
            LOG_ERROR(NULL, "Packet too small: CSRCs missing");
            return false;
         }

         LOG_INFO(NULL, "CSRCs:");
         for (ii = 0; ii < csrc_count; ii++)
         {
            LOG_INFO(NULL, " 0x%8.8X", network_to_host_32(buffer));
            buffer += 4;
            buffer_len -= 4;
         }
      }

      /* Dump any extension, if present */
      if (flags & HAS_EXTENSION)
      {
         uint32_t extension_hdr;
         uint32_t extension_id;
         size_t extension_len;

         if (buffer_len < 4)
         {
            LOG_ERROR(NULL, "Packet too small: extension header missing");
            return false;
         }
      
         extension_hdr = network_to_host_32(buffer);
         buffer += 4;
         buffer_len -= 4;
      
         extension_len = (size_t)(extension_hdr & EXTENSION_LENGTH_MASK);
         extension_id = extension_hdr >> EXTENSION_ID_SHIFT;
      
         if (buffer_len < extension_len)
         {
            LOG_ERROR(NULL, "Packet too small: extension content missing");
            return false;
         }
      
         LOG_INFO(NULL, "Extension: 0x%4.4X (%u bytes)", extension_id, (unsigned)extension_len);
         dump_bytes(buffer, extension_len);
         buffer += extension_len;
         buffer_len -= extension_len;
      }
   }

   /* And finally the payload data */
   if (verbosity >= FULL_PACKET_VERBOSITY)
   {
      LOG_INFO(NULL, "Data: (%u bytes)", (unsigned)buffer_len);
      dump_bytes(buffer, buffer_len);
   }

   return true;
}

static void increase_read_buffer_size(VC_CONTAINER_IO_T *p_ctx)
{
   uint32_t buffer_size = INITIAL_READ_BUFFER_SIZE;

   /* Iteratively enlarge read buffer until either operation fails or maximum is reached. */
   while (vc_container_io_control(p_ctx, VC_CONTAINER_CONTROL_IO_SET_READ_BUFFER_SIZE, buffer_size) == VC_CONTAINER_SUCCESS)
   {
      buffer_size <<= 1;   /* Double and try again */
      if (buffer_size > MAXIMUM_READ_BUFFER_SIZE)
         break;
   }
}

static void parse_command_line(int argc, char **argv)
{
   int arg = 1;

   while (arg < argc)
   {
      if (*argv[arg] != '-')  /* End of options, next should be URI */
         break;

      switch (argv[arg][1])
      {
      case 'h':
         do_print_usage = true;
         break;
      case 's':
         arg++;
         if (arg >= argc)
            break;
         packet_save_file = argv[arg];
         packet_save_is_pktfile = (strncmp(packet_save_file, "pktfile:", 8) == 0);
         break;
      case 'v':
         {
            const char *ptr = &argv[arg][2];

            verbosity = 1;
            while (*ptr++ == 'v')
               verbosity++;
         }
         break;
      default: LOG_ERROR(NULL, "Unrecognised option: %s", argv[arg]); return;
      }

      arg++;
   }

   if (arg < argc)
      read_uri = argv[arg];
}

static void print_usage(char *program_name)
{
   LOG_INFO(NULL, "Usage:");
   LOG_INFO(NULL, "  %s [opts] <uri>", program_name);
   LOG_INFO(NULL, "Reads RTP packets from <uri>, decodes to standard output.");
   LOG_INFO(NULL, "Press the escape key to terminate the program.");
   LOG_INFO(NULL, "Options:");
   LOG_INFO(NULL, "  -h   Print this information");
   LOG_INFO(NULL, "  -s x Save packets to URI x");
   LOG_INFO(NULL, "  -v   Dump standard packet header");
   LOG_INFO(NULL, "  -vv  Dump entire header");
   LOG_INFO(NULL, "  -vvv Dump entire header and data");
}

int main(int argc, char **argv)
{
   int result = 0;
   uint8_t *buffer = NULL;
   VC_CONTAINER_IO_T *read_io = NULL;
   VC_CONTAINER_IO_T *write_io = NULL;
   VC_CONTAINER_STATUS_T status;
   size_t received_bytes;
   bool ready = true;
   uint32_t available_bytes;
   uint8_t *packet_ptr;

   parse_command_line(argc, argv);

   if (do_print_usage || !read_uri)
   {
      print_usage(argv[0]);
      result = 1; goto tidyup;
   }

   buffer = (uint8_t *)malloc(MAXIMUM_BUFFER_SIZE);
   if (!buffer)
   {
      LOG_ERROR(NULL, "Allocating %d bytes for the buffer failed", MAXIMUM_BUFFER_SIZE);
      result = 2; goto tidyup;
   }

   read_io = vc_container_io_open(read_uri, VC_CONTAINER_IO_MODE_READ, &status);
   if (!read_io)
   {
      LOG_ERROR(NULL, "Opening <%s> for read failed: %d", read_uri, status);
      result = 3; goto tidyup;
   }

   increase_read_buffer_size(read_io);

   if (packet_save_file)
   {
      write_io = vc_container_io_open(packet_save_file, VC_CONTAINER_IO_MODE_WRITE, &status);
      if (!write_io)
      {
         LOG_ERROR(NULL, "Opening <%s> for write failed: %d", packet_save_file, status);
         result = 4; goto tidyup;
      }
      if (!packet_save_is_pktfile)
      {
         store_u32(buffer, 0x50415753);
         vc_container_io_write(write_io, buffer, sizeof(uint32_t));
      }
   }

   /* Use non-blocking I/O for both network and console */
   vc_container_io_control(read_io, VC_CONTAINER_CONTROL_IO_SET_READ_TIMEOUT_MS, 20);
   nb_set_nonblocking_input(1);

   packet_ptr = buffer;
   available_bytes = MAXIMUM_BUFFER_SIZE - sizeof(uint32_t);
   while (ready)
   {
      /* Read a packet and store its length in the word before it */
      received_bytes = vc_container_io_read(read_io, packet_ptr + sizeof(uint32_t), available_bytes);
      if (received_bytes)
      {
         bool packet_has_marker;

         store_u32(packet_ptr, received_bytes);
         packet_ptr += sizeof(uint32_t);
         packet_has_marker = marker_bit_set(packet_ptr, received_bytes);
         packet_ptr += received_bytes;
         available_bytes -= received_bytes + sizeof(uint32_t);

         if (packet_has_marker || (available_bytes < MINIMUM_BUFFER_SPACE))
         {
            uint8_t *decode_ptr;

            if (write_io && !packet_save_is_pktfile)
            {
               uint32_t total_bytes = packet_ptr - buffer;
               if (vc_container_io_write(write_io, buffer, total_bytes) != total_bytes)
               {
                  LOG_ERROR(NULL, "Error saving packets to file");
                  break;
               }
               if (verbosity >= LOWEST_VERBOSITY)
                  LOG_INFO(NULL, "Written %u bytes to file", total_bytes);
            }

            for (decode_ptr = buffer; decode_ptr < packet_ptr;)
            {
               received_bytes = fetch_u32(decode_ptr);
               decode_ptr += sizeof(uint32_t);

               if (write_io && packet_save_is_pktfile)
               {
                  if (vc_container_io_write(write_io, buffer, received_bytes) != received_bytes)
                  {
                     LOG_ERROR(NULL, "Error saving packets to file");
                     break;
                  }
                  if (verbosity >= LOWEST_VERBOSITY)
                     LOG_INFO(NULL, "Written %u bytes to file", received_bytes);
               }

               if (!decode_packet(decode_ptr, received_bytes))
               {
                  LOG_ERROR(NULL, "Failed to decode packet");
                  break;
               }
               decode_ptr += received_bytes;
            }

            /* Reset to start of buffer */
            packet_ptr = buffer;
            available_bytes = MAXIMUM_BUFFER_SIZE - sizeof(uint32_t);
         }
      }

      if (nb_char_available())
      {
         if (nb_get_char() == ESCAPE_CHARACTER) 
            ready = false;
      }

      switch (read_io->status)
      {
      case VC_CONTAINER_SUCCESS:
      case VC_CONTAINER_ERROR_CONTINUE:
         break;
      default:
         ready = false;
      }
   }

   nb_set_nonblocking_input(0);

tidyup:
   if (write_io)
      vc_container_io_close(write_io);
   if (read_io)
      vc_container_io_close(read_io);
   if (buffer)
      free(buffer);

   return result;
}
