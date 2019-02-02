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

#include <stdio.h>

#define BITS_LOG_INDENT(ctx) indent_level
#include "containers/containers.h"
#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_bits.h"

uint32_t indent_level;

/** Bit stream containing the values 0 to 10, with each value in that many bits.
 * At the end there is one further zero bit before the end of the stream. */
static uint8_t bits_0_to_10[] = {
   0xCD, 0x0A, 0x30, 0x70, 0x80, 0x48, 0x14
};

/** Bit stream containing the values 0 to 10, encoded using Exp-Golomb.
 * At the end there is one further one bit before the end of the stream. */
static uint8_t exp_golomb_0_to_10[] = {
   0xA6, 0x42, 0x98, 0xE2, 0x04, 0x8A, 0x17
};

/** Array of signed values for the Exp-Golomb encoding of each index. */
static int32_t exp_golomb_values[] = {
   0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5
};

/** Bit stream containing two large 32-bit values, encoded using Exp-Golomb. */
static uint8_t exp_golomb_large[] = {
   0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFE, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
};

/** Bit stream containing a 33-bit value, encoded using Exp-Golomb. */
static uint8_t exp_golomb_oversize[] = {
   0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80
};


static const char *plural_ext(uint32_t val)
{
   return (val == 1) ? "" : "s";
}

static int test_reset_and_available(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   int error_count = 0;

   LOG_DEBUG(NULL, "Testing vc_container_bits_reset and vc_container_bits_available");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   if (!BITS_AVAILABLE(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Expected initialised stream to contain bits");
      error_count++;
   }

   BITS_RESET(NULL, &bit_stream);

   if (BITS_AVAILABLE(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Expected reset stream not to contain bits");
      error_count++;
   }

   return error_count;
}

static int test_read_u32(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii, value;
   int error_count = 0;

   LOG_DEBUG(NULL, "Testing vc_container_bits_get_u32");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   for (ii = 0; ii < 11; ii++)
   {
      value = BITS_READ_U32(NULL, &bit_stream, ii, "test_read_u32");
      if (value != ii)
      {
         LOG_ERROR(NULL, "Expected %u, got %u", ii, value);
         error_count++;
      }
   }

   value = BITS_READ_U32(NULL, &bit_stream, 1, "Final bit");
   if (!BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Failed to get final bit");
      error_count++;
   }
   value = BITS_READ_U32(NULL, &bit_stream, 1, "Beyond final bit");
   if (BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded reading beyond expected end of stream");
      error_count++;
   }

   return error_count;
}

static int test_skip(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii;
   int error_count = 0;
   uint32_t last_bits_left, bits_left;

   LOG_DEBUG(NULL, "Testing vc_container_bits_skip");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   last_bits_left = BITS_AVAILABLE(NULL, &bit_stream);
   for (ii = 0; ii < 11; ii++)
   {
      BITS_SKIP(NULL, &bit_stream, ii, "test_skip");
      bits_left = BITS_AVAILABLE(NULL, &bit_stream);
      if (bits_left + ii != last_bits_left)
      {
         int32_t actual = last_bits_left - bits_left;
         LOG_ERROR(NULL, "Tried to skip %u bit%s, actually skipped %d bit%s",
               ii, plural_ext(ii), actual, plural_ext(actual));
         error_count++;
      }
      last_bits_left = bits_left;
   }

   BITS_SKIP(NULL, &bit_stream, 1, "Final bit");
   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Failed to skip final bit");
      error_count++;
   }
   if (BITS_AVAILABLE(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "End of stream not reached by skipping");
      error_count++;
   }

   BITS_SKIP(NULL, &bit_stream, 1, "Beyond final bit");
   if (BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded skipping beyond expected end of stream");
      error_count++;
   }
   return error_count;
}

static int test_ptr_and_skip_bytes(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii;
   const uint8_t *expected_ptr;
   int error_count = 0;
   uint32_t last_bytes_left, bytes_left;

   LOG_DEBUG(NULL, "Testing vc_container_bits_current_pointer, vc_container_bits_skip_bytes and vc_container_bits_bytes_available");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   last_bytes_left = BITS_BYTES_AVAILABLE(NULL, &bit_stream);
   if (last_bytes_left != countof(bits_0_to_10))
   {
      LOG_ERROR(NULL, "Expected bytes available to initially match given size");
      error_count++;
   }

   if (BITS_CURRENT_POINTER(NULL, &bit_stream) != bits_0_to_10)
   {
      LOG_ERROR(NULL, "Expected initial current pointer to match original buffer");
      error_count++;
   }

   expected_ptr = bits_0_to_10;
   for (ii = 0; ii < 4; ii++)
   {
      BITS_SKIP_BYTES(NULL, &bit_stream, ii, "test_ptr_and_skip_bytes");

      expected_ptr += ii;
      if (BITS_CURRENT_POINTER(NULL, &bit_stream) != expected_ptr)
      {
         LOG_ERROR(NULL, "Expected current pointer to have moved by %u byte%s", ii, plural_ext(ii));
         error_count++;
      }

      bytes_left = BITS_BYTES_AVAILABLE(NULL, &bit_stream);
      if (bytes_left + ii != last_bytes_left)
      {
         int32_t actual = last_bytes_left - bytes_left;
         LOG_ERROR(NULL, "Tried to skip %u byte%s, actually skipped %d byte%s",
               ii, plural_ext(ii), actual, plural_ext(actual));
         error_count++;
      }

      last_bytes_left = bytes_left;
   }

   if (!bytes_left)
   {
      LOG_ERROR(NULL, "Reached end of stream too soon");
      error_count++;
   }
   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Expected stream to be valid");
      error_count++;
   }

   BITS_SKIP_BYTES(NULL, &bit_stream, bytes_left + 1, "Beyond end of stream");
   if (BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded skipping bytes beyond end of stream");
      error_count++;
   }
   if (BITS_BYTES_AVAILABLE(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Expected stream to have been reset");
      error_count++;
   }

   return error_count;
}

static int test_reduce_bytes(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii;
   int error_count = 0;
   uint32_t last_bytes_left, bytes_left;

   LOG_DEBUG(NULL, "Testing vc_container_bits_reduce_bytes");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   last_bytes_left = BITS_BYTES_AVAILABLE(NULL, &bit_stream);
   if (last_bytes_left != countof(bits_0_to_10))
   {
      LOG_ERROR(NULL, "Expected bytes available to initially match given size");
      error_count++;
   }

   if (BITS_CURRENT_POINTER(NULL, &bit_stream) != bits_0_to_10)
   {
      LOG_ERROR(NULL, "Expected initial current pointer to match original buffer");
      error_count++;
   }

   for (ii = 0; ii < 4; ii++)
   {
      BITS_REDUCE_BYTES(NULL, &bit_stream, ii, "test_reduce_bytes");

      if (BITS_CURRENT_POINTER(NULL, &bit_stream) != bits_0_to_10)
      {
         LOG_ERROR(NULL, "Did not expect current pointer to have moved");
         error_count++;
      }

      bytes_left = BITS_BYTES_AVAILABLE(NULL, &bit_stream);
      if (bytes_left + ii != last_bytes_left)
      {
         int32_t actual = last_bytes_left - bytes_left;
         LOG_ERROR(NULL, "Tried to reduce by %u byte%s, actually reduced by %d byte%s",
               ii, plural_ext(ii), actual, plural_ext(actual));
         error_count++;
      }

      last_bytes_left = bytes_left;
   }

   if (!bytes_left)
   {
      LOG_ERROR(NULL, "Reached end of stream too soon");
      error_count++;
   }
   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Expected stream to be valid");
      error_count++;
   }

   BITS_REDUCE_BYTES(NULL, &bit_stream, bytes_left + 1, "Reducing an empty stream");
   if (BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded reducing by too many bytes");
      error_count++;
   }
   if (BITS_AVAILABLE(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Expected stream to have been reset");
      error_count++;
   }

   return error_count;
}

static int test_copy_bytes(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   int error_count = 0;
   uint8_t buffer[countof(bits_0_to_10)];
   uint32_t ii;

   LOG_DEBUG(NULL, "Testing vc_container_bits_copy_bytes");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));
   memset(buffer, 0, sizeof(buffer));

   /* Copy whole buffer in one go */
   BITS_COPY_BYTES(NULL, &bit_stream, countof(buffer), buffer, "Copy whole buffer");

   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Failed to copy the whole buffer");
      error_count++;
   }

   if (memcmp(buffer, bits_0_to_10, countof(bits_0_to_10)) != 0)
   {
      LOG_ERROR(NULL, "Single copy doesn't match original");
      error_count++;
   }

   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));
   memset(buffer, 0, sizeof(buffer));

   /* Copy whole buffer one byte at a time */
   for (ii = 0; ii < countof(bits_0_to_10); ii++)
   {
      BITS_COPY_BYTES(NULL, &bit_stream, 1, buffer + ii, "Copy buffer piecemeal");
   }

   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Failed to copy the buffer piecemeal");
      error_count++;
   }

   if (memcmp(buffer, bits_0_to_10, countof(bits_0_to_10)) != 0)
   {
      LOG_ERROR(NULL, "Multiple copy doesn't match original");
      error_count++;
   }

   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));
   memset(buffer, 0, sizeof(buffer));

   /* Copy part of buffer */
   BITS_SKIP_BYTES(NULL, &bit_stream, 1, "Copy part of buffer");
   BITS_REDUCE_BYTES(NULL, &bit_stream, 1, "Copy part of buffer");
   BITS_COPY_BYTES(NULL, &bit_stream, countof(buffer) - 2, buffer, "Copy part of buffer");

   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Failed to copy part of buffer");
      error_count++;
   }

   if (memcmp(buffer, bits_0_to_10 + 1, countof(bits_0_to_10) - 2) != 0)
   {
      LOG_ERROR(NULL, "Partial copy doesn't match original");
      error_count++;
   }

   return error_count;
}

static int test_skip_exp_golomb(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii;
   int error_count = 0;

   LOG_DEBUG(NULL, "Testing vc_container_bits_skip_exp_golomb");
   BITS_INIT(NULL, &bit_stream, exp_golomb_0_to_10, countof(exp_golomb_0_to_10));

   for (ii = 0; ii < 12; ii++)
   {
      BITS_SKIP_EXP(NULL, &bit_stream, "test_skip_exp_golomb");
   }

   if (!BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Failed to skip through buffer");
      error_count++;
   }

   BITS_SKIP_EXP(NULL, &bit_stream, "Skip beyond end of stream");
   if (BITS_VALID(NULL, &bit_stream))
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded skipping beyond expected end of stream");
      error_count++;
   }

   return error_count;
}

static int test_read_u32_exp_golomb(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii, value;
   int error_count = 0;

   LOG_DEBUG(NULL, "Testing vc_container_bits_get_u32_exp_golomb");
   BITS_INIT(NULL, &bit_stream, exp_golomb_0_to_10, countof(exp_golomb_0_to_10));

   for (ii = 0; ii < 11; ii++)
   {
      value = BITS_READ_U32_EXP(NULL, &bit_stream, "test_read_u32_exp_golomb");
      if (value != ii)
      {
         LOG_ERROR(NULL, "Expected %u, got %u", ii, value);
         error_count++;
      }
   }

   value = BITS_READ_U32(NULL, &bit_stream, 1, "Final bit");
   if (!BITS_VALID(NULL, &bit_stream) || !value)
   {
      LOG_ERROR(NULL, "Failed to get final bit (expected a 1)");
      error_count++;
   }
   value = BITS_READ_U32_EXP(NULL, &bit_stream, "Beyond end of stream");
   if (BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded reading beyond expected end of stream");
      error_count++;
   }

   /* Test getting two large (32 bit) Exp-Golomb values */
   BITS_INIT(NULL, &bit_stream, exp_golomb_large, countof(exp_golomb_large));

   value = BITS_READ_U32_EXP(NULL, &bit_stream, "Second largest 32-bit value");
   if (value != 0xFFFFFFFE)
   {
      LOG_ERROR(NULL, "Failed to get second largest 32-bit value");
      error_count++;
   }

   value = BITS_READ_U32_EXP(NULL, &bit_stream, "Largest 32-bit value");
   if (value != 0xFFFFFFFF)
   {
      LOG_ERROR(NULL, "Failed to get largest 32-bit value");
      error_count++;
   }

   /* Test getting an oversize (33 bit) Exp-Golomb value */
   BITS_INIT(NULL, &bit_stream, exp_golomb_oversize, countof(exp_golomb_oversize));

   value = BITS_READ_U32_EXP(NULL, &bit_stream, "Unsigned 33-bit value");
   if (BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Unexpectedly got 33-bit value: %u", value);
      error_count++;
   }

   return error_count;
}

static int test_read_s32_exp_golomb(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii;
   int32_t value;
   int error_count = 0;

   LOG_DEBUG(NULL, "Testing vc_container_bits_get_s32_exp_golomb");
   BITS_INIT(NULL, &bit_stream, exp_golomb_0_to_10, countof(exp_golomb_0_to_10));

   for (ii = 0; ii < 11; ii++)
   {
      value = BITS_READ_S32_EXP(NULL, &bit_stream, "test_read_s32_exp_golomb");
      if (value != exp_golomb_values[ii])
      {
         LOG_ERROR(NULL, "Expected %u, got %u", ii, value);
         error_count++;
      }
   }

   value = BITS_READ_S32_EXP(NULL, &bit_stream, "Final bit");
   if (!BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Failed to get final Exp-Golomb value (expected a zero)");
      error_count++;
   }
   value = BITS_READ_S32_EXP(NULL, &bit_stream, "Beyond final bit");
   if (BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Unexpectedly succeeded reading beyond expected end of stream");
      error_count++;
   }

   /* Test getting two large (32 bit) Exp-Golomb values */
   BITS_INIT(NULL, &bit_stream, exp_golomb_large, countof(exp_golomb_large));

   value = BITS_READ_S32_EXP(NULL, &bit_stream, "Largest signed 32-bit value");
   if (!BITS_VALID(NULL, &bit_stream) || value != -0x7FFFFFFF)
   {
      LOG_ERROR(NULL, "Failed to get largest signed 32-bit value: %d", value);
      error_count++;
   }

   value = BITS_READ_S32_EXP(NULL, &bit_stream, "Just too large signed 33-bit value");
   if (BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Unexpectedly got slightly too large signed 32-bit value: %d", value);
      error_count++;
   }

   /* Test getting an oversize (33 bit) Exp-Golomb value */
   BITS_INIT(NULL, &bit_stream, exp_golomb_oversize, countof(exp_golomb_oversize));

   value = BITS_READ_S32_EXP(NULL, &bit_stream, "Larger signed 33-bit value");
   if (BITS_VALID(NULL, &bit_stream) || value)
   {
      LOG_ERROR(NULL, "Unexpectedly got signed 33-bit value: %d", value);
      error_count++;
   }

   return error_count;
}

#ifdef ENABLE_CONTAINERS_LOG_FORMAT
static int test_indentation(void)
{
   VC_CONTAINER_BITS_T bit_stream;
   uint32_t ii;

   LOG_DEBUG(NULL, "Testing logging indentation");
   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   for (ii = 0; ii < 11; ii++)
   {
      indent_level = ii;
      BITS_READ_U32(NULL, &bit_stream, ii, "test_indentation (unit)");
   }

   BITS_INIT(NULL, &bit_stream, bits_0_to_10, countof(bits_0_to_10));

   for (ii = 0; ii < 11; ii++)
   {
      indent_level = ii * 10;
      BITS_READ_U32(NULL, &bit_stream, ii, "test_indentation (tens)");
   }
   return 0;
}
#endif

int main(int argc, char **argv)
{
   int error_count = 0;

   VC_CONTAINER_PARAM_UNUSED(argc);
   VC_CONTAINER_PARAM_UNUSED(argv);

   error_count += test_reset_and_available();
   error_count += test_read_u32();
   error_count += test_skip();
   error_count += test_ptr_and_skip_bytes();
   error_count += test_reduce_bytes();
   error_count += test_copy_bytes();
   error_count += test_skip_exp_golomb();
   error_count += test_read_u32_exp_golomb();
   error_count += test_read_s32_exp_golomb();
#ifdef ENABLE_CONTAINERS_LOG_FORMAT
   error_count += test_indentation();
#endif

   if (error_count)
   {
      LOG_ERROR(NULL, "*** %d errors reported", error_count);
      getchar();
   }

   return error_count;
}
