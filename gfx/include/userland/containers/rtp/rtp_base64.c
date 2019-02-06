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

#include "rtp_base64.h"

/******************************************************************************
Defines and constants.
******************************************************************************/

#define LOWEST_BASE64_CHAR '+'
#define HIGHEST_BASE64_CHAR 'z'
#define IN_BASE64_RANGE(C) ((C) >= LOWEST_BASE64_CHAR && (C) <= HIGHEST_BASE64_CHAR)

/** Used as a marker in the lookup table to indicate an invalid Base64 character */
#define INVALID 0xFF

/* Reduced lookup table for translating a character to a 6-bit value. The
 * table starts at the lowest Base64 character, '+' */
uint8_t base64_decode_lookup[] = {
   62, INVALID, 62, INVALID, 63,                                              /* '+' to '/' */
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,                                    /* '0' to '9' */
   INVALID, INVALID, INVALID, INVALID, INVALID, INVALID, INVALID,             /* ':' to '@' */
   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,      /* 'A' to 'T' */
   20, 21, 22, 23, 24, 25,                                                    /* 'U' to 'Z' */
   INVALID, INVALID, INVALID, INVALID, 63, INVALID,                           /* '[' to '`' */
   26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,    /* 'a' to 'r' */
   44, 45, 46, 47, 48, 49, 50, 51                                             /* 's' to 'z' */
};

/******************************************************************************
Type definitions
******************************************************************************/

/******************************************************************************
Function prototypes
******************************************************************************/

/******************************************************************************
Local Functions
******************************************************************************/

/*****************************************************************************
Functions exported as part of the Base64 API
 *****************************************************************************/

/*****************************************************************************/
uint32_t rtp_base64_byte_length(const char *str, uint32_t str_len)
{
   uint32_t character_count = 0;
   uint32_t ii;
   char cc;

   /* Scan through string until either a pad ('=') character or the end is
    * reached. Ignore characters that are not part of the Base64 alphabet.
    * Number of bytes should then be 3/4 of the character count. */

   for (ii = 0; ii < str_len; ii++)
   {
      cc = *str++;
      if (cc == '=')
         break;         /* Found a pad character: stop */

      if (!IN_BASE64_RANGE(cc))
         continue;      /* Ignore invalid character */

      if (base64_decode_lookup[cc - LOWEST_BASE64_CHAR] != INVALID)
         character_count++;
   }

   return (character_count * 3) >> 2;
}

/*****************************************************************************/
uint8_t *rtp_base64_decode(const char *str, uint32_t str_len, uint8_t *buffer, uint32_t buffer_len)
{
   uint32_t character_count = 0;
   uint32_t value = 0;
   uint32_t ii;
   char cc;
   uint8_t lookup;

   /* Build up sets of four characters (ignoring invalid ones) to generate
    * triplets of bytes, until either the end of the string or the pad ('=')
    * characters are reached. */

   for (ii = 0; ii < str_len; ii++)
   {
      cc = *str++;
      if (cc == '=')
         break;         /* Found a pad character: stop */

      if (!IN_BASE64_RANGE(cc))
         continue;      /* Ignore invalid character */

      lookup = base64_decode_lookup[cc - LOWEST_BASE64_CHAR];
      if (lookup == INVALID)
         continue;      /* Ignore invalid character */

      value = (value << 6) | lookup;
      character_count++;

      if (character_count == 4)
      {
         if (buffer_len < 3)
            return NULL;   /* Not enough room in the output buffer */

         *buffer++ = (uint8_t)(value >> 16);
         *buffer++ = (uint8_t)(value >>  8);
         *buffer++ = (uint8_t)(value      );
         buffer_len -= 3;

         character_count = 0;
         value = 0;
      }
   }

   /* If there were extra characters on the end, these need to be handled to get
    * the last one or two bytes. */

   switch (character_count)
   {
   case 0:  /* Nothing more to do, the final bytes were converted in the loop */
      break;
   case 2:  /* One additional byte, padded with four zero bits */
      if (!buffer_len)
         return NULL;
      *buffer++ = (uint8_t)(value >> 4);
      break;
   case 3:  /* Two additional bytes, padded with two zero bits */
      if (buffer_len < 2)
         return NULL;
      *buffer++ = (uint8_t)(value >> 10);
      *buffer++ = (uint8_t)(value >> 2);
      break;
   default: /* This is an invalid Base64 encoding */
      return NULL;
   }

   /* Return number of bytes written to the buffer */
   return buffer;
}
