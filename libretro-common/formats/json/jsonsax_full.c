/*
  Copyright (c) 2012 John-Anthony Owens

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdlib.h>
#include <string.h>

/* Ensure uint32_t type (compiler-dependent). */
#if defined(_MSC_VER)
typedef unsigned __int32 uint32_t;
#else
#include <stdint.h>
#endif

/* Ensure SIZE_MAX defined. */
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/* Mark APIs for export (as opposed to import) when we build this file. */
#define JSON_BUILDING
#include <formats/jsonsax_full.h>

/* Default allocation constants. */
#define DEFAULT_TOKEN_BYTES_LENGTH 64 /* MUST be a power of 2 */
#define DEFAULT_SYMBOL_STACK_SIZE  32 /* MUST be a power of 2 */

/* Types for readability. */
typedef unsigned char byte;
typedef uint32_t Codepoint;

/* Especially-relevant Unicode codepoints. */
#define U_(x) ((Codepoint)(x))
#define NULL_CODEPOINT                  U_(0x0000)
#define BACKSPACE_CODEPOINT             U_(0x0008)
#define TAB_CODEPOINT                   U_(0x0009)
#define LINE_FEED_CODEPOINT             U_(0x000A)
#define FORM_FEED_CODEPOINT             U_(0x000C)
#define CARRIAGE_RETURN_CODEPOINT       U_(0x000D)
#define FIRST_NON_CONTROL_CODEPOINT     U_(0x0020)
#define DELETE_CODEPOINT                U_(0x007F)
#define FIRST_NON_ASCII_CODEPOINT       U_(0x0080)
#define FIRST_2_BYTE_UTF8_CODEPOINT     U_(0x0080)
#define FIRST_3_BYTE_UTF8_CODEPOINT     U_(0x0800)
#define LINE_SEPARATOR_CODEPOINT        U_(0x2028)
#define PARAGRAPH_SEPARATOR_CODEPOINT   U_(0x2029)
#define BOM_CODEPOINT                   U_(0xFEFF)
#define REPLACEMENT_CHARACTER_CODEPOINT U_(0xFFFD)
#define FIRST_NON_BMP_CODEPOINT         U_(0x10000)
#define FIRST_4_BYTE_UTF8_CODEPOINT     U_(0x10000)
#define MAX_CODEPOINT                   U_(0x10FFFF)
#define EOF_CODEPOINT                   U_(0xFFFFFFFF)

/* Bit-masking macros. */
#define BOTTOM_3_BITS(x) ((x) & 0x7)
#define BOTTOM_4_BITS(x) ((x) & 0xF)
#define BOTTOM_5_BITS(x) ((x) & 0x1F)
#define BOTTOM_6_BITS(x) ((x) & 0x3F)

/* Bit-flag macros. */
#define GET_FLAGS(x, f)                  ((x) & (f))
#define SET_FLAGS_ON(flagstype, x, f)    do { (x) |= (flagstype)(f); } while (0)
#define SET_FLAGS_OFF(flagstype, x, f)   do { (x) &= (flagstype)~(f); } while (0)
#define SET_FLAGS(flagstype, x, f, cond) do { if (cond) (x) |= (flagstype)(f); else (x) &= (flagstype)~(f); } while (0)

/* UTF-8 byte-related macros. */
#define IS_UTF8_SINGLE_BYTE(b)       (((b) & 0x80) == 0)
#define IS_UTF8_CONTINUATION_BYTE(b) (((b) & 0xC0) == 0x80)
#define IS_UTF8_FIRST_BYTE_OF_2(b)   (((b) & 0xE0) == 0xC0)
#define IS_UTF8_FIRST_BYTE_OF_3(b)   (((b) & 0xF0) == 0xE0)
#define IS_UTF8_FIRST_BYTE_OF_4(b)   (((b) & 0xF8) == 0xF0)

/* Unicode codepoint-related macros. */
#define IS_NONCHARACTER(c)               ((((c) & 0xFE) == 0xFE) || (((c) >= 0xFDD0) && ((c) <= 0xFDEF)))
#define IS_SURROGATE(c)                  (((c) & 0xFFFFF800) == 0xD800)
#define IS_LEADING_SURROGATE(c)          (((c) & 0xFFFFFC00) == 0xD800)
#define IS_TRAILING_SURROGATE(c)         (((c) & 0xFFFFFC00) == 0xDC00)
#define CODEPOINT_FROM_SURROGATES(hi_lo) ((((hi_lo) >> 16) << 10) + ((hi_lo) & 0xFFFF) + 0xFCA02400)
#define SURROGATES_FROM_CODEPOINT(c)     ((((c) << 6) & 0x7FF0000) + ((c) & 0x3FF) + 0xD7C0DC00)
#define SHORTEST_ENCODING_SEQUENCE(enc)  (1U << ((enc) >> 1))
#define LONGEST_ENCODING_SEQUENCE        4

/* Internal types that alias enum types in the public API.
   By using byte to represent these values internally,
   we can guarantee minimal storage size and avoid compiler
   warnings when using values of the type in switch statements
   that don't have (or need) a default case. */
typedef byte Encoding;
typedef byte Error;
typedef byte TokenAttributes;

/******************** Default Memory Suite ********************/

static void* JSON_CALL DefaultReallocHandler(void* userData, void* ptr, size_t size)
{
   (void)userData; /* unused */
   return realloc(ptr, size);
}

static void JSON_CALL DefaultFreeHandler(void* userData, void* ptr)
{
   (void)userData; /* unused */
   free(ptr);
}

static const JSON_MemorySuite defaultMemorySuite = { NULL, &DefaultReallocHandler, &DefaultFreeHandler };

static byte* DoubleBuffer(const JSON_MemorySuite* pMemorySuite, byte* pDefaultBuffer, byte* pBuffer, size_t length)
{
   size_t newLength = length * 2;
   if (newLength < length)
   {
      pBuffer = NULL;
   }
   else if (pBuffer == pDefaultBuffer)
   {
      pBuffer = (byte*)pMemorySuite->realloc(pMemorySuite->userData, NULL, newLength);
      if (pBuffer)
      {
         memcpy(pBuffer, pDefaultBuffer, length);
      }
   }
   else
   {
      pBuffer = (byte*)pMemorySuite->realloc(pMemorySuite->userData, pBuffer, newLength);
   }
   return pBuffer;
}

/******************** Unicode Decoder ********************/

/* Mutually-exclusive decoder states. */
/* The bits of DecoderState are layed out as follows:

   ---lllnn

   - = unused (3 bits)
   l = expected total sequence length (3 bits)
   d = number of bytes decoded so far (2 bits)
   */

#define DECODER_RESET  0x00
#define DECODED_1_OF_2 0x09 /* 00001001 */
#define DECODED_1_OF_3 0x0D /* 00001101 */
#define DECODED_2_OF_3 0x0E /* 00001110 */
#define DECODED_1_OF_4 0x11 /* 00010001 */
#define DECODED_2_OF_4 0x12 /* 00010010 */
#define DECODED_3_OF_4 0x13 /* 00010011 */
typedef byte DecoderState;

#define DECODER_STATE_BYTES(s) (size_t)((s) & 0x3)

/* Decoder data. */
typedef struct tag_DecoderData
{
   DecoderState state;
   uint32_t     bits;
} DecoderData;
typedef DecoderData* Decoder;

/* The bits of DecoderOutput are layed out as follows:

   ------rrlllccccccccccccccccccccc

   - = unused (6 bits)
   r = result code (2 bits)
   l = sequence length (3 bits)
   c = codepoint (21 bits)
   */
#define SEQUENCE_PENDING           0
#define SEQUENCE_COMPLETE          1
#define SEQUENCE_INVALID_INCLUSIVE 2
#define SEQUENCE_INVALID_EXCLUSIVE 3
typedef uint32_t DecoderResultCode;

#define DECODER_OUTPUT(r, l, c)    (DecoderOutput)(((r) << 24) | ((l) << 21) | (c))
#define DECODER_RESULT_CODE(o)     (DecoderResultCode)((DecoderOutput)(o) >> 24)
#define DECODER_SEQUENCE_LENGTH(o) (size_t)(((DecoderOutput)(o) >> 21) & 0x7)
#define DECODER_CODEPOINT(o)       (Codepoint)((DecoderOutput)(o) & 0x001FFFFF)
typedef uint32_t DecoderOutput;

/* Decoder functions. */

static void Decoder_Reset(Decoder decoder)
{
   decoder->state = DECODER_RESET;
   decoder->bits = 0;
}

static int Decoder_SequencePending(Decoder decoder)
{
   return decoder->state != DECODER_RESET;
}

static DecoderOutput Decoder_ProcessByte(Decoder decoder, Encoding encoding, byte b)
{
   DecoderOutput output = DECODER_OUTPUT(SEQUENCE_PENDING, 0, 0);
   switch (encoding)
   {
      case JSON_UTF8:
         /* When the input encoding is UTF-8, the decoded codepoint's bits are
            recorded in the bottom 3 bytes of bits as they are decoded.
            The top byte is not used. */
         switch (decoder->state)
         {
            case DECODER_RESET:
               if (IS_UTF8_SINGLE_BYTE(b))
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 1, b);
               }
               else if (IS_UTF8_FIRST_BYTE_OF_2(b))
               {
                  /* UTF-8 2-byte sequences that are overlong encodings can be
                     detected from just the first byte (C0 or C1). */
                  decoder->bits = (uint32_t)BOTTOM_5_BITS(b) << 6;
                  if (decoder->bits < FIRST_2_BYTE_UTF8_CODEPOINT)
                  {
                     output = DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 1, 0);
                  }
                  else
                  {
                     decoder->state = DECODED_1_OF_2;
                     goto noreset;
                  }
               }
               else if (IS_UTF8_FIRST_BYTE_OF_3(b))
               {
                  decoder->bits = (uint32_t)BOTTOM_4_BITS(b) << 12;
                  decoder->state = DECODED_1_OF_3;
                  goto noreset;
               }
               else if (IS_UTF8_FIRST_BYTE_OF_4(b))
               {
                  /* Some UTF-8 4-byte sequences that encode out-of-range
                     codepoints can be detected from the first byte (F5 - FF). */
                  decoder->bits = (uint32_t)BOTTOM_3_BITS(b) << 18;
                  if (decoder->bits > MAX_CODEPOINT)
                  {
                     output = DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 1, 0);
                  }
                  else
                  {
                     decoder->state = DECODED_1_OF_4;
                     goto noreset;
                  }
               }
               else
               {
                  /* The byte is of the form 11111xxx or 10xxxxxx, and is not
                     a valid first byte for a UTF-8 sequence. */
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 1, 0);
               }
               break;

            case DECODED_1_OF_2:
               if (IS_UTF8_CONTINUATION_BYTE(b))
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 2, decoder->bits | BOTTOM_6_BITS(b));
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 1, 0);

               }
               break;

            case DECODED_1_OF_3:
               if (IS_UTF8_CONTINUATION_BYTE(b))
               {
                  /* UTF-8 3-byte sequences that are overlong encodings or encode
                     surrogate codepoints can be detected after 2 bytes. */
                  decoder->bits |= (uint32_t)BOTTOM_6_BITS(b) << 6;
                  if ((decoder->bits < FIRST_3_BYTE_UTF8_CODEPOINT) || IS_SURROGATE(decoder->bits))
                  {
                     output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 1, 0);
                  }
                  else
                  {
                     decoder->state = DECODED_2_OF_3;
                     goto noreset;
                  }
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 1, 0);
               }
               break;

            case DECODED_2_OF_3:
               if (IS_UTF8_CONTINUATION_BYTE(b))
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 3, decoder->bits | BOTTOM_6_BITS(b));
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 2, 0);
               }
               break;

            case DECODED_1_OF_4:
               if (IS_UTF8_CONTINUATION_BYTE(b))
               {
                  /* UTF-8 4-byte sequences that are overlong encodings or encode
                     out-of-range codepoints can be detected after 2 bytes. */
                  decoder->bits |= (uint32_t)BOTTOM_6_BITS(b) << 12;
                  if ((decoder->bits < FIRST_4_BYTE_UTF8_CODEPOINT) || (decoder->bits > MAX_CODEPOINT))
                  {
                     output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 1, 0);
                  }
                  else
                  {
                     decoder->state = DECODED_2_OF_4;
                     goto noreset;
                  }
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 1, 0);
               }
               break;

            case DECODED_2_OF_4:
               if (IS_UTF8_CONTINUATION_BYTE(b))
               {
                  decoder->bits |= (uint32_t)BOTTOM_6_BITS(b) << 6;
                  decoder->state = DECODED_3_OF_4;
                  goto noreset;
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 2, 0);
               }
               break;

            case DECODED_3_OF_4:
               if (IS_UTF8_CONTINUATION_BYTE(b))
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 4, decoder->bits | BOTTOM_6_BITS(b));
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 3, 0);
               }
               break;
         }
         break;

      case JSON_UTF16LE:
         /* When the input encoding is UTF-16, the decoded codepoint's bits are
            recorded in the bottom 2 bytes of bits as they are decoded.
            If those 2 bytes form a leading surrogate, the decoder treats the
            surrogate pair as a single 4-byte sequence, shifts the leading
            surrogate into the high 2 bytes of bits, and decodes the
            trailing surrogate's bits in the bottom 2 bytes of bits. */
         switch (decoder->state)
         {
            case DECODER_RESET:
               decoder->bits = b;
               decoder->state = DECODED_1_OF_2;
               goto noreset;

            case DECODED_1_OF_2:
               decoder->bits |= (uint32_t)b << 8;
               if (IS_TRAILING_SURROGATE(decoder->bits))
               {
                  /* A trailing surrogate cannot appear on its own. */
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 2, 0);
               }
               else if (IS_LEADING_SURROGATE(decoder->bits))
               {
                  /* A leading surrogate implies a 4-byte surrogate pair. */
                  decoder->bits <<= 16;
                  decoder->state = DECODED_2_OF_4;
                  goto noreset;
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 2, decoder->bits);
               }
               break;

            case DECODED_2_OF_4:
               decoder->bits |= b;
               decoder->state = DECODED_3_OF_4;
               goto noreset;

            case DECODED_3_OF_4:
               decoder->bits |= (uint32_t)b << 8;
               if (!IS_TRAILING_SURROGATE(decoder->bits & 0xFFFF))
               {
                  /* A leading surrogate must be followed by a trailing one.
                     Treat the previous 3 bytes as an invalid 2-byte sequence
                     followed by the first byte of a new sequence. */
                  decoder->bits &= 0xFF;
                  decoder->state = DECODED_1_OF_2;
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 2, 0);
                  goto noreset;
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 4, CODEPOINT_FROM_SURROGATES(decoder->bits));
               }
               break;
         }
         break;

      case JSON_UTF16BE:
         /* When the input encoding is UTF-16, the decoded codepoint's bits are
            recorded in the bottom 2 bytes of bits as they are decoded.
            If those 2 bytes form a leading surrogate, the decoder treats the
            surrogate pair as a single 4-byte sequence, shifts the leading
            surrogate into the high 2 bytes of bits, and decodes the
            trailing surrogate's bits in the bottom 2 bytes of bits. */
         switch (decoder->state)
         {
            case DECODER_RESET:
               decoder->bits = (uint32_t)b << 8;
               decoder->state = DECODED_1_OF_2;
               goto noreset;

            case DECODED_1_OF_2:
               decoder->bits |= b;
               if (IS_TRAILING_SURROGATE(decoder->bits))
               {
                  /* A trailing surrogate cannot appear on its own. */
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 2, 0);
               }
               else if (IS_LEADING_SURROGATE(decoder->bits))
               {
                  /* A leading surrogate implies a 4-byte surrogate pair. */
                  decoder->bits <<= 16;
                  decoder->state = DECODED_2_OF_4;
                  goto noreset;
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 2, decoder->bits);
               }
               break;

            case DECODED_2_OF_4:
               decoder->bits |= (uint32_t)b << 8;
               decoder->state = DECODED_3_OF_4;
               goto noreset;

            case DECODED_3_OF_4:
               decoder->bits |= b;
               if (!IS_TRAILING_SURROGATE(decoder->bits & 0xFFFF))
               {
                  /* A leading surrogate must be followed by a trailing one.
                     Treat the previous 3 bytes as an invalid 2-byte sequence
                     followed by the first byte of a new sequence. */
                  decoder->bits &= 0xFF00;
                  decoder->state = DECODED_1_OF_2;
                  output = DECODER_OUTPUT(SEQUENCE_INVALID_EXCLUSIVE, 2, 0);
                  goto noreset;
               }
               else
               {
                  output = DECODER_OUTPUT(SEQUENCE_COMPLETE, 4, CODEPOINT_FROM_SURROGATES(decoder->bits));
               }
               break;
         }
         break;

      case JSON_UTF32LE:
         /* When the input encoding is UTF-32, the decoded codepoint's bits are
            recorded in bits as they are decoded. */
         switch (decoder->state)
         {
            case DECODER_RESET:
               decoder->state = DECODED_1_OF_4;
               decoder->bits = (uint32_t)b;
               goto noreset;

            case DECODED_1_OF_4:
               decoder->state = DECODED_2_OF_4;
               decoder->bits |= (uint32_t)b << 8;
               goto noreset;

            case DECODED_2_OF_4:
               decoder->state = DECODED_3_OF_4;
               decoder->bits |= (uint32_t)b << 16;
               goto noreset;

            case DECODED_3_OF_4:
               decoder->bits |= (uint32_t)b << 24;
               output = (IS_SURROGATE(decoder->bits) || (decoder->bits > MAX_CODEPOINT))
                  ? DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 4, 0)
                  : DECODER_OUTPUT(SEQUENCE_COMPLETE, 4, decoder->bits);
               break;
         }
         break;

      case JSON_UTF32BE:
         /* When the input encoding is UTF-32, the decoded codepoint's bits are
            recorded in bits as they are decoded. */
         switch (decoder->state)
         {
            case DECODER_RESET:
               decoder->state = DECODED_1_OF_4;
               decoder->bits = (uint32_t)b << 24;
               goto noreset;

            case DECODED_1_OF_4:
               decoder->state = DECODED_2_OF_4;
               decoder->bits |= (uint32_t)b << 16;
               goto noreset;

            case DECODED_2_OF_4:
               decoder->state = DECODED_3_OF_4;
               decoder->bits |= (uint32_t)b << 8;
               goto noreset;

            case DECODED_3_OF_4:
               decoder->bits |= b;
               output = (IS_SURROGATE(decoder->bits) || (decoder->bits > MAX_CODEPOINT))
                  ? DECODER_OUTPUT(SEQUENCE_INVALID_INCLUSIVE, 4, 0)
                  : DECODER_OUTPUT(SEQUENCE_COMPLETE, 4, decoder->bits);
               break;
         }
         break;
   }

   /* Reset the decoder for the next sequence. */
   Decoder_Reset(decoder);

noreset:
   return output;
}

/******************** Unicode Encoder ********************/

/* This function makes the following assumptions about its input:

   1. The c argument is a valid codepoint (U+0000 - U+10FFFF).
   2. The encoding argument is not JSON_UnknownEncoding.
   3. The pBytes argument points to an array of at least 4 bytes.
   */
static size_t EncodeCodepoint(Codepoint c, Encoding encoding, byte* pBytes)
{
   size_t length = 0;
   switch (encoding)
   {
      case JSON_UTF8:
         if (c < FIRST_2_BYTE_UTF8_CODEPOINT)
         {
            pBytes[0] = (byte)c;
            length = 1;
         }
         else if (c < FIRST_3_BYTE_UTF8_CODEPOINT)
         {
            pBytes[0] = (byte)(0xC0 | (c >> 6));
            pBytes[1] = (byte)(0x80 | BOTTOM_6_BITS(c));
            length = 2;
         }
         else if (c < FIRST_4_BYTE_UTF8_CODEPOINT)
         {
            pBytes[0] = (byte)(0xE0 | (c >> 12));
            pBytes[1] = (byte)(0x80 | BOTTOM_6_BITS(c >> 6));
            pBytes[2] = (byte)(0x80 | BOTTOM_6_BITS(c));
            length = 3;
         }
         else
         {
            pBytes[0] = (byte)(0xF0 | (c >> 18));
            pBytes[1] = (byte)(0x80 | BOTTOM_6_BITS(c >> 12));
            pBytes[2] = (byte)(0x80 | BOTTOM_6_BITS(c >> 6));
            pBytes[3] = (byte)(0x80 | BOTTOM_6_BITS(c));
            length = 4;
         }
         break;

      case JSON_UTF16LE:
         if (c < FIRST_NON_BMP_CODEPOINT)
         {
            pBytes[0] = (byte)(c);
            pBytes[1] = (byte)(c >> 8);
            length = 2;
         }
         else
         {
            uint32_t surrogates = SURROGATES_FROM_CODEPOINT(c);

            /* Leading surrogate. */
            pBytes[0] = (byte)(surrogates >> 16);
            pBytes[1] = (byte)(surrogates >> 24);

            /* Trailing surrogate. */
            pBytes[2] = (byte)(surrogates);
            pBytes[3] = (byte)(surrogates >> 8);
            length = 4;
         }
         break;

      case JSON_UTF16BE:
         if (c < FIRST_NON_BMP_CODEPOINT)
         {
            pBytes[1] = (byte)(c);
            pBytes[0] = (byte)(c >> 8);
            length = 2;
         }
         else
         {
            /* The codepoint requires a surrogate pair in UTF-16. */
            uint32_t surrogates = SURROGATES_FROM_CODEPOINT(c);

            /* Leading surrogate. */
            pBytes[1] = (byte)(surrogates >> 16);
            pBytes[0] = (byte)(surrogates >> 24);

            /* Trailing surrogate. */
            pBytes[3] = (byte)(surrogates);
            pBytes[2] = (byte)(surrogates >> 8);
            length = 4;
         }
         break;

      case JSON_UTF32LE:
         pBytes[0] = (byte)(c);
         pBytes[1] = (byte)(c >> 8);
         pBytes[2] = (byte)(c >> 16);
         pBytes[3] = (byte)(c >> 24);
         length = 4;
         break;

      case JSON_UTF32BE:
         pBytes[3] = (byte)(c);
         pBytes[2] = (byte)(c >> 8);
         pBytes[1] = (byte)(c >> 16);
         pBytes[0] = (byte)(c >> 24);
         length = 4;
         break;
   }
   return length;
}

/******************** JSON Lexer States ********************/

/* Mutually-exclusive lexer states. */
#define LEXING_WHITESPACE                                     0
#define LEXING_LITERAL                                        1
#define LEXING_STRING                                         2
#define LEXING_STRING_ESCAPE                                  3
#define LEXING_STRING_HEX_ESCAPE_BYTE_1                       4
#define LEXING_STRING_HEX_ESCAPE_BYTE_2                       5
#define LEXING_STRING_HEX_ESCAPE_BYTE_3                       6
#define LEXING_STRING_HEX_ESCAPE_BYTE_4                       7
#define LEXING_STRING_HEX_ESCAPE_BYTE_5                       8
#define LEXING_STRING_HEX_ESCAPE_BYTE_6                       9
#define LEXING_STRING_HEX_ESCAPE_BYTE_7                       10
#define LEXING_STRING_HEX_ESCAPE_BYTE_8                       11
#define LEXING_STRING_TRAILING_SURROGATE_HEX_ESCAPE_BACKSLASH 12
#define LEXING_STRING_TRAILING_SURROGATE_HEX_ESCAPE_U         13
#define LEXING_NUMBER_AFTER_MINUS                             14
#define LEXING_NUMBER_AFTER_LEADING_ZERO                      15
#define LEXING_NUMBER_AFTER_LEADING_NEGATIVE_ZERO             16
#define LEXING_NUMBER_AFTER_X                                 17
#define LEXING_NUMBER_HEX_DIGITS                              18
#define LEXING_NUMBER_DECIMAL_DIGITS                          19
#define LEXING_NUMBER_AFTER_DOT                               20
#define LEXING_NUMBER_FRACTIONAL_DIGITS                       21
#define LEXING_NUMBER_AFTER_E                                 22
#define LEXING_NUMBER_AFTER_EXPONENT_SIGN                     23
#define LEXING_NUMBER_EXPONENT_DIGITS                         24
#define LEXING_COMMENT_AFTER_SLASH                            25
#define LEXING_SINGLE_LINE_COMMENT                            26
#define LEXING_MULTI_LINE_COMMENT                             27
#define LEXING_MULTI_LINE_COMMENT_AFTER_STAR                  28
#define LEXER_ERROR                                           255
typedef byte LexerState;

/******************** JSON Grammarian ********************/

/* The JSON grammar comprises the following productions:

   1.  VALUE => null
   2.  VALUE => boolean
   3.  VALUE => string
   4.  VALUE => number
   5.  VALUE => specialnumber
   6.  VALUE => { MEMBERS }
   7.  VALUE => [ ITEMS ]
   8.  MEMBERS => MEMBER MORE_MEMBERS
   9.  MEMBERS => e
   10. MEMBER => string : VALUE
   11. MORE_MEMBERS => , MEMBER MORE_MEMBERS
   12. MORE_MEMBERS => e
   13. ITEMS => ITEM MORE_ITEMS
   14. ITEMS => e
   15. ITEM => VALUE
   16. MORE_ITEMS => , ITEM MORE_ITEMS
   17. MORE_ITEMS => e

   We implement a simple LL(1) parser based on this grammar, with events
   emitted when certain non-terminals are replaced.
   */

/* Mutually-exclusive grammar tokens and non-terminals. The values are defined
   so that the bottom 4 bits of a value can be used as an index into the
   grammar production rule table. */
#define T_NONE              0x00 /* tokens are in the form 0x0X */
#define T_NULL              0x01
#define T_TRUE              0x02
#define T_FALSE             0x03
#define T_STRING            0x04
#define T_NUMBER            0x05
#define T_NAN               0x06
#define T_INFINITY          0x07
#define T_NEGATIVE_INFINITY 0x08
#define T_LEFT_CURLY        0x09
#define T_RIGHT_CURLY       0x0A
#define T_LEFT_SQUARE       0x0B
#define T_RIGHT_SQUARE      0x0C
#define T_COLON             0x0D
#define T_COMMA             0x0E
#define NT_VALUE            0x10 /* non-terminals are in the form 0x1X */
#define NT_MEMBERS          0x11
#define NT_MEMBER           0x12
#define NT_MORE_MEMBERS     0x13
#define NT_ITEMS            0x14
#define NT_ITEM             0x15
#define NT_MORE_ITEMS       0x16
typedef byte Symbol;

#define IS_NONTERMINAL(s) ((s) & 0x10)
#define IS_TOKEN(s)       !IS_NONTERMINAL(s)

/* Grammarian data. */
typedef struct tag_GrammarianData
{
   Symbol* pStack; /* initially set to defaultStack */
   size_t  stackSize;
   size_t  stackUsed;
   Symbol  defaultStack[DEFAULT_SYMBOL_STACK_SIZE];
} GrammarianData;
typedef GrammarianData* Grammarian;

/* Mutually-exclusive result codes returned by the grammarian
   after processing a token. */
#define ACCEPTED_TOKEN    0
#define REJECTED_TOKEN    1
#define SYMBOL_STACK_FULL 2
typedef uint32_t GrammarianResultCode;

/* Events emitted by the grammarian as a result of processing a
   token. Note that EMIT_ARRAY_ITEM always appears bitwise OR-ed
   with one of the other values. */
#define EMIT_NOTHING        0x00
#define EMIT_NULL           0x01
#define EMIT_BOOLEAN        0x02
#define EMIT_STRING         0x03
#define EMIT_NUMBER         0x04
#define EMIT_SPECIAL_NUMBER 0x05
#define EMIT_START_OBJECT   0x06
#define EMIT_END_OBJECT     0x07
#define EMIT_OBJECT_MEMBER  0x08
#define EMIT_START_ARRAY    0x09
#define EMIT_END_ARRAY      0x0A
#define EMIT_ARRAY_ITEM     0x10 /* may be combined with other values */
typedef byte GrammarEvent;

/* The bits of GrammarianOutput are layed out as follows:

   -rreeeee

   - = unused (1 bit)
   r = result code (2 bits)
   e = event (5 bits)
   */
#define GRAMMARIAN_OUTPUT(r, e)   (GrammarianOutput)(((GrammarianResultCode)(r) << 5) | (GrammarEvent)(e))
#define GRAMMARIAN_RESULT_CODE(o) (GrammarianResultCode)((GrammarianOutput)(o) >> 5)
#define GRAMMARIAN_EVENT(o)       (GrammarEvent)((GrammarianOutput)(o) & 0x1F)
typedef byte GrammarianOutput;

/* Grammar rule used by the grammarian to process a token. */
typedef struct tag_GrammarRule
{
   Symbol       symbolToPush1;
   Symbol       symbolToPush2;
   byte         reprocess;
   GrammarEvent emit;
} GrammarRule;

/* Grammarian functions. */

static void Grammarian_Reset(Grammarian grammarian, int isInitialized)
{
   /* When we reset the grammarian, we keep the symbol stack that has
      already been allocated, if any. If the client wants to reclaim the
      memory used by the that buffer, he needs to free the grammarian
      and create a new one. */
   if (!isInitialized)
   {
      grammarian->pStack = grammarian->defaultStack;
      grammarian->stackSize = sizeof(grammarian->defaultStack);
   }

   /* The grammarian always starts with NT_VALUE on the symbol stack. */
   grammarian->pStack[0] = NT_VALUE;
   grammarian->stackUsed = 1;
}

static void Grammarian_FreeAllocations(Grammarian grammarian,
      const JSON_MemorySuite* pMemorySuite)
{
   if (grammarian->pStack != grammarian->defaultStack)
      pMemorySuite->free(pMemorySuite->userData, grammarian->pStack);
}

static int Grammarian_FinishedDocument(Grammarian grammarian)
{
   return !grammarian->stackUsed;
}

static GrammarianOutput Grammarian_ProcessToken(Grammarian grammarian,
      Symbol token, const JSON_MemorySuite* pMemorySuite)
{
   /* The order and number of the rows and columns in this table must
      match the defined token and non-terminal symbol values.

      The row index is the incoming token's Symbol value.

      The column index is the bottom 4 bits of Symbol value of
      the non-terminal at the top of the processing stack.
      Since non-terminal Symbol values start at 0x10, taking
      the bottom 4 bits yields a 0-based index. */
   static const byte ruleLookup[15][7] =
   {
      /*             V     MS    M     MM    IS    I     MI  */
      /*  ----  */ { 0,    0,    0,    0,    0,    0,    0  },
      /*  null  */ { 1,    0,    0,    0,    13,   15,   0  },
      /*  true  */ { 2,    0,    0,    0,    13,   15,   0  },
      /* false  */ { 2,    0,    0,    0,    13,   15,   0  },
      /* string */ { 3,    8,    10,   0,    13,   15,   0  },
      /* number */ { 4,    0,    0,    0,    13,   15,   0  },
      /*  NaN   */ { 5,    0,    0,    0,    13,   15,   0  },
      /*  Inf   */ { 5,    0,    0,    0,    13,   15,   0  },
      /* -Inf   */ { 5,    0,    0,    0,    13,   15,   0  },
      /*   {    */ { 6,    0,    0,    0,    13,   15,   0  },
      /*   }    */ { 0,    9,    0,    12,   0,    0,    0  },
      /*   [    */ { 7,    0,    0,    0,    13,   15,   0  },
      /*   ]    */ { 0,    0,    0,    0,    14,   0,    17 },
      /*   :    */ { 0,    0,    0,    0,    0,    0,    0  },
      /*   ,    */ { 0,    0,    0,    11,   0,    0,    16 }
   };

   static const GrammarRule rules[17] =
   {
      /* 1.  */ { T_NONE,          T_NONE,      0, EMIT_NULL           },
      /* 2.  */ { T_NONE,          T_NONE,      0, EMIT_BOOLEAN        },
      /* 3.  */ { T_NONE,          T_NONE,      0, EMIT_STRING         },
      /* 4.  */ { T_NONE,          T_NONE,      0, EMIT_NUMBER         },
      /* 5.  */ { T_NONE,          T_NONE,      0, EMIT_SPECIAL_NUMBER },
      /* 6.  */ { T_RIGHT_CURLY,   NT_MEMBERS,  0, EMIT_START_OBJECT   },
      /* 7.  */ { T_RIGHT_SQUARE,  NT_ITEMS,    0, EMIT_START_ARRAY    },
      /* 8.  */ { NT_MORE_MEMBERS, NT_MEMBER,   1, EMIT_NOTHING        },
      /* 9.  */ { T_NONE,          T_NONE,      1, EMIT_END_OBJECT     },
      /* 10. */ { NT_VALUE,        T_COLON,     0, EMIT_OBJECT_MEMBER  },
      /* 11. */ { NT_MORE_MEMBERS, NT_MEMBER,   0, EMIT_NOTHING        },
      /* 12. */ { T_NONE,          T_NONE,      1, EMIT_END_OBJECT     },
      /* 13. */ { NT_MORE_ITEMS,   NT_ITEM,     1, EMIT_NOTHING        },
      /* 14. */ { T_NONE,          T_NONE,      1, EMIT_END_ARRAY      },
      /* 15. */ { NT_VALUE,        T_NONE,      1, EMIT_ARRAY_ITEM     },
      /* 16. */ { NT_MORE_ITEMS,   NT_ITEM,     0, EMIT_NOTHING        },
      /* 17. */ { T_NONE,          T_NONE,      1, EMIT_END_ARRAY      }
   };

   GrammarEvent emit = EMIT_NOTHING;

   /* If the stack is empty, no more tokens were expected. */
   if (Grammarian_FinishedDocument(grammarian))
      return GRAMMARIAN_OUTPUT(REJECTED_TOKEN, EMIT_NOTHING);

   for (;;)
   {
      Symbol topSymbol = grammarian->pStack[grammarian->stackUsed - 1];
      if (IS_TOKEN(topSymbol))
      {
         if (topSymbol != token)
            return GRAMMARIAN_OUTPUT(REJECTED_TOKEN, EMIT_NOTHING);
         grammarian->stackUsed--;
         break;
      }
      else
      {
         const GrammarRule* pRule = NULL;
         byte ruleNumber          = ruleLookup[token][BOTTOM_4_BITS(topSymbol)];

         if (ruleNumber == 0)
            return GRAMMARIAN_OUTPUT(REJECTED_TOKEN, EMIT_NOTHING);

         pRule = &rules[ruleNumber - 1];

         /* The rule removes the top symbol and does not replace it. */
         if (pRule->symbolToPush1 == T_NONE)
            grammarian->stackUsed--;
         else
         {
            /* The rule replaces the top symbol with 1 or 2 symbols. */
            grammarian->pStack[grammarian->stackUsed - 1] = pRule->symbolToPush1;
            if (pRule->symbolToPush2 != T_NONE)
            {
               /* The rule replaces the top symbol with 2 symbols.
                  Make sure the stack has room for the second one. */
               if (grammarian->stackUsed == grammarian->stackSize)
               {
                  Symbol* pBiggerStack = DoubleBuffer(pMemorySuite,
                        grammarian->defaultStack, grammarian->pStack,
                        grammarian->stackSize);

                  if (!pBiggerStack)
                     return GRAMMARIAN_OUTPUT(SYMBOL_STACK_FULL, EMIT_NOTHING);

                  grammarian->pStack = pBiggerStack;
                  grammarian->stackSize *= 2;
               }
               grammarian->pStack[grammarian->stackUsed] = pRule->symbolToPush2;
               grammarian->stackUsed++;
            }
         }
         emit |= pRule->emit;
         if (!pRule->reprocess)
            break;
      }
   }

   return GRAMMARIAN_OUTPUT(ACCEPTED_TOKEN, emit);
}

/******************** JSON Parser ********************/

#ifndef JSON_NO_PARSER

/* Combinable parser state flags. */
#define PARSER_RESET                 0x00
#define PARSER_STARTED               0x01
#define PARSER_FINISHED              0x02
#define PARSER_IN_PROTECTED_API      0x04
#define PARSER_IN_TOKEN_HANDLER      0x08
#define PARSER_AFTER_CARRIAGE_RETURN 0x10
typedef byte ParserState;

/* Combinable parser settings flags. */
#define PARSER_DEFAULT_FLAGS         0x00
#define PARSER_ALLOW_BOM             0x01
#define PARSER_ALLOW_COMMENTS        0x02
#define PARSER_ALLOW_SPECIAL_NUMBERS 0x04
#define PARSER_ALLOW_HEX_NUMBERS     0x08
#define PARSER_REPLACE_INVALID       0x10
#define PARSER_TRACK_OBJECT_MEMBERS  0x20
#define PARSER_ALLOW_CONTROL_CHARS   0x40
#define PARSER_EMBEDDED_DOCUMENT     0x80
typedef byte ParserFlags;

/* Sentinel value for parser error location offset. */
#define ERROR_LOCATION_IS_TOKEN_START 0xFF

/* An object member name stored in an unordered, singly-linked-list, used for
   detecting duplicate member names. Note that the name string is not null-
   terminated. */
typedef struct tag_MemberName
{
   struct tag_MemberName* pNextName;
   size_t                 length;
   byte                   pBytes[1]; /* variable-size buffer */
} MemberName;

/* An object's list of member names, and a pointer to the object's
   nearest ancestor object, if any. This is used as a stack. Because arrays
   do not have named items, they do not need to be recorded in the stack. */
typedef struct tag_MemberNames
{
   struct tag_MemberNames* pAncestor;
   MemberName*             pFirstName;
} MemberNames;

/* A parser instance. */
struct JSON_Parser_Data
{
   JSON_MemorySuite                    memorySuite;
   void*                               userData;
   ParserState                         state;
   ParserFlags                         flags;
   Encoding                            inputEncoding;
   Encoding                            stringEncoding;
   Encoding                            numberEncoding;
   Symbol                              token;
   TokenAttributes                     tokenAttributes;
   Error                               error;
   byte                                errorOffset;
   LexerState                          lexerState;
   uint32_t                            lexerBits;
   size_t                              codepointLocationByte;
   size_t                              codepointLocationLine;
   size_t                              codepointLocationColumn;
   size_t                              tokenLocationByte;
   size_t                              tokenLocationLine;
   size_t                              tokenLocationColumn;
   size_t                              depth;
   byte*                               pTokenBytes;
   size_t                              tokenBytesLength;
   size_t                              tokenBytesUsed;
   size_t                              maxStringLength;
   size_t                              maxNumberLength;
   MemberNames*                        pMemberNames;
   DecoderData                         decoderData;
   GrammarianData                      grammarianData;
   JSON_Parser_EncodingDetectedHandler encodingDetectedHandler;
   JSON_Parser_NullHandler             nullHandler;
   JSON_Parser_BooleanHandler          booleanHandler;
   JSON_Parser_StringHandler           stringHandler;
   JSON_Parser_NumberHandler           numberHandler;
   JSON_Parser_SpecialNumberHandler    specialNumberHandler;
   JSON_Parser_StartObjectHandler      startObjectHandler;
   JSON_Parser_EndObjectHandler        endObjectHandler;
   JSON_Parser_ObjectMemberHandler     objectMemberHandler;
   JSON_Parser_StartArrayHandler       startArrayHandler;
   JSON_Parser_EndArrayHandler         endArrayHandler;
   JSON_Parser_ArrayItemHandler        arrayItemHandler;
   byte                                defaultTokenBytes[DEFAULT_TOKEN_BYTES_LENGTH];
};

/* Parser internal functions. */

static void JSON_Parser_SetErrorAtCodepoint(JSON_Parser parser, Error error)
{
   parser->error = error;
}

static void JSON_Parser_SetErrorAtStringEscapeSequenceStart(
      JSON_Parser parser, Error error, int codepointsAgo)
{
   /* Note that backtracking from the current codepoint requires us to make
      three assumptions, which are always valid in the context of a string
      escape sequence:

      1. The input encoding is not JSON_UnknownEncoding.

      2 The codepoints we are backing up across are all in the range
      U+0000 - U+007F, aka ASCII, so we can assume the number of
      bytes comprising them based on the input encoding.

      3. The codepoints we are backing up across do not include any
      line breaks, so we can assume that the line number stays the
      same and the column number can simply be decremented.
      */
   parser->error = error;
   parser->errorOffset = (byte)codepointsAgo;
}

static void JSON_Parser_SetErrorAtToken(JSON_Parser parser, Error error)
{
   parser->error = error;
   parser->errorOffset = ERROR_LOCATION_IS_TOKEN_START;
}

static JSON_Status JSON_Parser_PushMemberNameList(JSON_Parser parser)
{
   MemberNames* pNames = (MemberNames*)parser->memorySuite.realloc(
         parser->memorySuite.userData, NULL, sizeof(MemberNames));

   if (!pNames)
   {
      JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_OutOfMemory);
      return JSON_Failure;
   }

   pNames->pAncestor    = parser->pMemberNames;
   pNames->pFirstName   = NULL;
   parser->pMemberNames = pNames;
   return JSON_Success;
}

static void JSON_Parser_PopMemberNameList(JSON_Parser parser)
{
   MemberNames* pAncestor = parser->pMemberNames->pAncestor;
   while (parser->pMemberNames->pFirstName)
   {
      MemberName* pNextName = parser->pMemberNames->pFirstName->pNextName;
      parser->memorySuite.free(parser->memorySuite.userData, parser->pMemberNames->pFirstName);
      parser->pMemberNames->pFirstName = pNextName;
   }
   parser->memorySuite.free(parser->memorySuite.userData, parser->pMemberNames);
   parser->pMemberNames = pAncestor;
}

static JSON_Status JSON_Parser_StartContainer(JSON_Parser parser, int isObject)
{
   if (isObject && GET_FLAGS(parser->flags, PARSER_TRACK_OBJECT_MEMBERS) &&
         !JSON_Parser_PushMemberNameList(parser))
   {
      return JSON_Failure;
   }
   parser->depth++;
   return JSON_Success;
}

static void JSON_Parser_EndContainer(JSON_Parser parser, int isObject)
{
   parser->depth--;
   if (isObject && GET_FLAGS(parser->flags, PARSER_TRACK_OBJECT_MEMBERS))
   {
      JSON_Parser_PopMemberNameList(parser);
   }
}

static JSON_Status JSON_Parser_AddMemberNameToList(JSON_Parser parser)
{
   if (GET_FLAGS(parser->flags, PARSER_TRACK_OBJECT_MEMBERS))
   {
      MemberName* pName;
      for (pName = parser->pMemberNames->pFirstName; pName; pName = pName->pNextName)
      {
         if (pName->length == parser->tokenBytesUsed && !memcmp(pName->pBytes, parser->pTokenBytes, pName->length))
         {
            JSON_Parser_SetErrorAtToken(parser, JSON_Error_DuplicateObjectMember);
            return JSON_Failure;
         }
      }
      pName = (MemberName*)parser->memorySuite.realloc(parser->memorySuite.userData, NULL, sizeof(MemberName) + parser->tokenBytesUsed - 1);
      if (!pName)
      {
         JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_OutOfMemory);
         return JSON_Failure;
      }
      pName->pNextName = parser->pMemberNames->pFirstName;
      pName->length = parser->tokenBytesUsed;
      memcpy(pName->pBytes, parser->pTokenBytes, parser->tokenBytesUsed);
      parser->pMemberNames->pFirstName = pName;
   }
   return JSON_Success;
}

static void JSON_Parser_ResetData(JSON_Parser parser, int isInitialized)
{
   parser->userData                 = NULL;
   parser->flags                    = PARSER_DEFAULT_FLAGS;
   parser->inputEncoding            = JSON_UnknownEncoding;
   parser->stringEncoding           = JSON_UTF8;
   parser->numberEncoding           = JSON_UTF8;
   parser->token                    = T_NONE;
   parser->tokenAttributes          = 0;
   parser->error                    = JSON_Error_None;
   parser->errorOffset              = 0;
   parser->lexerState               = LEXING_WHITESPACE;
   parser->lexerBits                = 0;
   parser->codepointLocationByte    = 0;
   parser->codepointLocationLine    = 0;
   parser->codepointLocationColumn  = 0;
   parser->tokenLocationByte        = 0;
   parser->tokenLocationLine        = 0;
   parser->tokenLocationColumn      = 0;
   parser->depth                    = 0;

   if (!isInitialized)
   {
      parser->pTokenBytes      = parser->defaultTokenBytes;
      parser->tokenBytesLength = sizeof(parser->defaultTokenBytes);
   }
   else
   {
      /* When we reset the parser, we keep the output buffer and the symbol
         stack that have already been allocated, if any. If the client wants
         to reclaim the memory used by the those buffers, he needs to free
         the parser and create a new one. */
   }
   parser->tokenBytesUsed  = 0;
   parser->maxStringLength = SIZE_MAX;
   parser->maxNumberLength = SIZE_MAX;
   if (!isInitialized)
      parser->pMemberNames = NULL;
   else
   {
      while (parser->pMemberNames)
         JSON_Parser_PopMemberNameList(parser);
   }
   Decoder_Reset(&parser->decoderData);
   Grammarian_Reset(&parser->grammarianData, isInitialized);
   parser->encodingDetectedHandler = NULL;
   parser->nullHandler = NULL;
   parser->booleanHandler = NULL;
   parser->stringHandler = NULL;
   parser->numberHandler = NULL;
   parser->specialNumberHandler = NULL;
   parser->startObjectHandler = NULL;
   parser->endObjectHandler = NULL;
   parser->objectMemberHandler = NULL;
   parser->startArrayHandler = NULL;
   parser->endArrayHandler = NULL;
   parser->arrayItemHandler = NULL;
   parser->state = PARSER_RESET; /* do this last! */
}

static void JSON_Parser_NullTerminateToken(JSON_Parser parser)
{
   /* Because we always ensure that there are LONGEST_ENCODING_SEQUENCE bytes
      available at the end of the token buffer when we record codepoints, we
      can write the null terminator to the buffer with impunity. */
   static const byte nullTerminatorBytes[LONGEST_ENCODING_SEQUENCE] = { 0 };
   Encoding encoding = (Encoding)((parser->token == T_NUMBER) ? parser->numberEncoding : parser->stringEncoding);
   memcpy(parser->pTokenBytes + parser->tokenBytesUsed, nullTerminatorBytes, (size_t)SHORTEST_ENCODING_SEQUENCE(encoding));
}

static JSON_Status JSON_Parser_FlushParser(JSON_Parser parser)
{
   /* The symbol stack should be empty when parsing finishes. */
   if (!Grammarian_FinishedDocument(&parser->grammarianData))
   {
      JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_ExpectedMoreTokens);
      return JSON_Failure;
   }
   return JSON_Success;
}

typedef JSON_Parser_HandlerResult (JSON_CALL * JSON_Parser_SimpleTokenHandler)(JSON_Parser parser);
static JSON_Status JSON_Parser_CallSimpleTokenHandler(JSON_Parser parser, JSON_Parser_SimpleTokenHandler handler)
{
   if (handler)
   {
      JSON_Parser_HandlerResult result;
      SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      result = handler(parser);
      SET_FLAGS_OFF(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      if (result != JSON_Parser_Continue)
      {
         JSON_Parser_SetErrorAtToken(parser, JSON_Error_AbortedByHandler);
         return JSON_Failure;
      }
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_CallBooleanHandler(JSON_Parser parser)
{
   if (parser->booleanHandler)
   {
      JSON_Parser_HandlerResult result;
      SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      result = parser->booleanHandler(parser, parser->token == T_TRUE ? JSON_True : JSON_False);
      SET_FLAGS_OFF(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      if (result != JSON_Parser_Continue)
      {
         JSON_Parser_SetErrorAtToken(parser, JSON_Error_AbortedByHandler);
         return JSON_Failure;
      }
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_CallStringHandler(JSON_Parser parser, int isObjectMember)
{
   JSON_Parser_StringHandler handler = isObjectMember ? parser->objectMemberHandler : parser->stringHandler;
   if (handler)
   {
      JSON_Parser_HandlerResult result;
      JSON_Parser_NullTerminateToken(parser);
      SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      result = handler(parser, (char*)parser->pTokenBytes, parser->tokenBytesUsed, parser->tokenAttributes);
      SET_FLAGS_OFF(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);

      if (result != JSON_Parser_Continue)
      {
         JSON_Parser_SetErrorAtToken(parser,
               (isObjectMember && result == JSON_Parser_TreatAsDuplicateObjectMember)
               ? JSON_Error_DuplicateObjectMember
               : JSON_Error_AbortedByHandler);
         return JSON_Failure;
      }
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_CallNumberHandler(JSON_Parser parser)
{
   if (parser->numberHandler)
   {
      JSON_Parser_HandlerResult result;
      JSON_Parser_NullTerminateToken(parser);
      SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      result = parser->numberHandler(parser, (char*)parser->pTokenBytes,
            parser->tokenBytesUsed, parser->tokenAttributes);

      SET_FLAGS_OFF(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);

      if (result != JSON_Parser_Continue)
      {
         JSON_Parser_SetErrorAtToken(parser, JSON_Error_AbortedByHandler);
         return JSON_Failure;
      }
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_CallSpecialNumberHandler(JSON_Parser parser)
{
   if (parser->specialNumberHandler)
   {
      JSON_Parser_HandlerResult result;
      SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);
      result = parser->specialNumberHandler(parser, parser->token == T_NAN ? JSON_NaN :
            (parser->token == T_INFINITY ? JSON_Infinity : JSON_NegativeInfinity));
      SET_FLAGS_OFF(ParserState, parser->state, PARSER_IN_TOKEN_HANDLER);

      if (result != JSON_Parser_Continue)
      {
         JSON_Parser_SetErrorAtToken(parser, JSON_Error_AbortedByHandler);
         return JSON_Failure;
      }
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_HandleGrammarEvents(JSON_Parser parser, byte emit)
{
   if (GET_FLAGS(emit, EMIT_ARRAY_ITEM))
   {
      if (!JSON_Parser_CallSimpleTokenHandler(parser, parser->arrayItemHandler))
      {
         return JSON_Failure;
      }
      SET_FLAGS_OFF(byte, emit, EMIT_ARRAY_ITEM);
   }
   switch (emit)
   {
      case EMIT_NULL:
         if (!JSON_Parser_CallSimpleTokenHandler(parser, parser->nullHandler))
            return JSON_Failure;
         break;

      case EMIT_BOOLEAN:
         if (!JSON_Parser_CallBooleanHandler(parser))
            return JSON_Failure;
         break;

      case EMIT_STRING:
         if (!JSON_Parser_CallStringHandler(parser, 0/* isObjectMember */))
            return JSON_Failure;
         break;

      case EMIT_NUMBER:
         if (!JSON_Parser_CallNumberHandler(parser))
            return JSON_Failure;
         break;

      case EMIT_SPECIAL_NUMBER:
         if (!JSON_Parser_CallSpecialNumberHandler(parser))
            return JSON_Failure;
         break;

      case EMIT_START_OBJECT:
         if (!JSON_Parser_CallSimpleTokenHandler(parser, parser->startObjectHandler) ||
               !JSON_Parser_StartContainer(parser, 1/*isObject*/))
            return JSON_Failure;
         break;

      case EMIT_END_OBJECT:
         JSON_Parser_EndContainer(parser, 1/*isObject*/);
         if (!JSON_Parser_CallSimpleTokenHandler(parser, parser->endObjectHandler))
            return JSON_Failure;
         break;
      case EMIT_OBJECT_MEMBER:
         if (!JSON_Parser_AddMemberNameToList(parser) || /* will fail if member is duplicate */
               !JSON_Parser_CallStringHandler(parser, 1 /* isObjectMember */))
            return JSON_Failure;
         break;

      case EMIT_START_ARRAY:
         if (!JSON_Parser_CallSimpleTokenHandler(parser, parser->startArrayHandler) ||
               !JSON_Parser_StartContainer(parser, 0/*isObject*/))
            return JSON_Failure;
         break;

      case EMIT_END_ARRAY:
         JSON_Parser_EndContainer(parser, 0/*isObject*/);
         if (!JSON_Parser_CallSimpleTokenHandler(parser, parser->endArrayHandler))
            return JSON_Failure;
         break;
   }

   if (!parser->depth && GET_FLAGS(parser->flags, PARSER_EMBEDDED_DOCUMENT))
   {
      JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_StoppedAfterEmbeddedDocument);
      return JSON_Failure;
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_ProcessToken(JSON_Parser parser)
{
   GrammarianOutput output;
   output = Grammarian_ProcessToken(&parser->grammarianData, parser->token, &parser->memorySuite);
   switch (GRAMMARIAN_RESULT_CODE(output))
   {
      case ACCEPTED_TOKEN:
         if (!JSON_Parser_HandleGrammarEvents(parser, GRAMMARIAN_EVENT(output)))
            return JSON_Failure;
         break;

      case REJECTED_TOKEN:
         JSON_Parser_SetErrorAtToken(parser, JSON_Error_UnexpectedToken);
         return JSON_Failure;

      case SYMBOL_STACK_FULL:
         JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_OutOfMemory);
         return JSON_Failure;
   }

   /* Reset the lexer to prepare for the next token. */
   parser->lexerState = LEXING_WHITESPACE;
   parser->lexerBits = 0;
   parser->token = T_NONE;
   parser->tokenAttributes = 0;
   parser->tokenBytesUsed = 0;
   return JSON_Success;
}

/* Lexer functions. */

static const byte expectedLiteralChars[] = { 'u', 'l', 'l', 0, 'r', 'u', 'e', 0, 'a', 'l', 's', 'e', 0, 'a', 'N', 0, 'n', 'f', 'i', 'n', 'i', 't', 'y', 0  };

#define NULL_LITERAL_EXPECTED_CHARS_START_INDEX     0
#define TRUE_LITERAL_EXPECTED_CHARS_START_INDEX     4
#define FALSE_LITERAL_EXPECTED_CHARS_START_INDEX    8
#define NAN_LITERAL_EXPECTED_CHARS_START_INDEX      13
#define INFINITY_LITERAL_EXPECTED_CHARS_START_INDEX 16

/* Forward declaration. */
static JSON_Status JSON_Parser_FlushLexer(JSON_Parser parser);
static JSON_Status JSON_Parser_ProcessCodepoint(
      JSON_Parser parser, Codepoint c, size_t encodedLength);

static JSON_Status JSON_Parser_HandleInvalidEncodingSequence(
      JSON_Parser parser, size_t encodedLength)
{
   if (parser->token == T_STRING && GET_FLAGS(parser->flags, PARSER_REPLACE_INVALID))
   {
      /* Since we're inside a string token, replacing the invalid sequence
         with the Unicode replacement character as requested by the client
         is a viable way to avoid a parse failure. Outside a string token,
         such a replacement would simply trigger JSON_Error_UnknownToken
         when we tried to process the replacement character, so it's less
         confusing to stick with JSON_Error_InvalidEncodingSequence in that
         case. */
      SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsReplacedCharacter);
      return JSON_Parser_ProcessCodepoint(parser, REPLACEMENT_CHARACTER_CODEPOINT, encodedLength);
   }
   else if (!parser->depth && GET_FLAGS(parser->flags, PARSER_EMBEDDED_DOCUMENT))
   {
      /* Since we're parsing the top-level value of an embedded
         document, assume that the invalid encoding sequence we've
         encountered does not actually belong to the document, and
         finish parsing by pretending that we've encountered EOF
         instead of an invalid sequence. If the content is valid,
         this will fail with JSON_Error_StoppedAfterEmbeddedDocument;
         otherwise, it will fail with an appropriate error. */
      return (JSON_Status)(JSON_Parser_FlushLexer(parser) && JSON_Parser_FlushParser(parser));
   }
   JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_InvalidEncodingSequence);
   return JSON_Failure;
}

static JSON_Status JSON_Parser_HandleInvalidNumber(JSON_Parser parser,
      Codepoint c, int codepointsSinceValidNumber, TokenAttributes attributesToRemove)
{
   SET_FLAGS_OFF(TokenAttributes, parser->tokenAttributes, attributesToRemove);
   if (!parser->depth && GET_FLAGS(parser->flags, PARSER_EMBEDDED_DOCUMENT))
   {
      /* The invalid number is the top-level value of an embedded document,
         and it has a prefix that can be interpreted as a valid number.
         We want to backtrack so that we are at the end of that prefix,
         and then process the valid token.

         Note that backtracking requires us to make three assumptions, which
         are always valid in the context of a number token:

         1. The input encoding is not JSON_UnknownEncoding.

         2 The codepoints we are backing up across are all in the range
         U+0000 - U+007F, aka ASCII, so we can assume the number of
         bytes comprising them based on the input encoding.

         3. The codepoints we are backing up across do not include any
         line breaks, so we can assume that the line number stays the
         same and the column number can simply be decremented.

         For example:

         "01"     => "0"
         "123.!"  => "123"
         "123e!"  => "123"
         "123e+!" => "123"
         "123e-!" => "123"
         "1.2e!"  => "1.2"
         "1.2e+!" => "1.2"
         "1.2e-!" => "1.2"
         */
      parser->codepointLocationByte -= (size_t)codepointsSinceValidNumber
         * (size_t)SHORTEST_ENCODING_SEQUENCE(parser->inputEncoding);
      parser->codepointLocationColumn -= (size_t)codepointsSinceValidNumber;
      parser->tokenBytesUsed -= (size_t)codepointsSinceValidNumber
         * (size_t)SHORTEST_ENCODING_SEQUENCE(parser->numberEncoding);
      return JSON_Parser_ProcessToken(parser); /* always fails */
   }
   /* Allow JSON_Parser_FlushLexer() to fail. */
   else if (c == EOF_CODEPOINT)
      return JSON_Success;

   JSON_Parser_SetErrorAtToken(parser, JSON_Error_InvalidNumber);
   return JSON_Failure;
}

static void JSON_Parser_StartToken(JSON_Parser parser, Symbol token)
{
   parser->token               = token;
   parser->tokenLocationByte   = parser->codepointLocationByte;
   parser->tokenLocationLine   = parser->codepointLocationLine;
   parser->tokenLocationColumn = parser->codepointLocationColumn;
}

static JSON_Status JSON_Parser_ProcessCodepoint(JSON_Parser parser, Codepoint c, size_t encodedLength)
{
   Encoding tokenEncoding;
   size_t maxTokenLength;
   int tokenFinished           = 0;
   Codepoint codepointToRecord = EOF_CODEPOINT;

   /* If the previous codepoint was U+000D (CARRIAGE RETURN), and the current
      codepoint is U+000A (LINE FEED), then treat the 2 codepoints as a single
      line break. */
   if (GET_FLAGS(parser->state, PARSER_AFTER_CARRIAGE_RETURN))
   {
      if (c == LINE_FEED_CODEPOINT)
         parser->codepointLocationLine--;
      SET_FLAGS_OFF(ParserState, parser->state, PARSER_AFTER_CARRIAGE_RETURN);
   }

reprocess:

   switch (parser->lexerState)
   {
      case LEXING_WHITESPACE:
         if (c == '{')
         {
            JSON_Parser_StartToken(parser, T_LEFT_CURLY);
            tokenFinished = 1;
         }
         else if (c == '}')
         {
            JSON_Parser_StartToken(parser, T_RIGHT_CURLY);
            tokenFinished = 1;
         }
         else if (c == '[')
         {
            JSON_Parser_StartToken(parser, T_LEFT_SQUARE);
            tokenFinished = 1;
         }
         else if (c == ']')
         {
            JSON_Parser_StartToken(parser, T_RIGHT_SQUARE);
            tokenFinished = 1;
         }
         else if (c == ':')
         {
            JSON_Parser_StartToken(parser, T_COLON);
            tokenFinished = 1;
         }
         else if (c == ',')
         {
            JSON_Parser_StartToken(parser, T_COMMA);
            tokenFinished = 1;
         }
         else if (c == 'n')
         {
            JSON_Parser_StartToken(parser, T_NULL);
            parser->lexerBits = NULL_LITERAL_EXPECTED_CHARS_START_INDEX;
            parser->lexerState = LEXING_LITERAL;
         }
         else if (c == 't')
         {
            JSON_Parser_StartToken(parser, T_TRUE);
            parser->lexerBits = TRUE_LITERAL_EXPECTED_CHARS_START_INDEX;
            parser->lexerState = LEXING_LITERAL;
         }
         else if (c == 'f')
         {
            JSON_Parser_StartToken(parser, T_FALSE);
            parser->lexerBits = FALSE_LITERAL_EXPECTED_CHARS_START_INDEX;
            parser->lexerState = LEXING_LITERAL;
         }
         else if (c == '"')
         {
            JSON_Parser_StartToken(parser, T_STRING);
            parser->lexerState = LEXING_STRING;
         }
         else if (c == '-')
         {
            JSON_Parser_StartToken(parser, T_NUMBER);
            parser->tokenAttributes = JSON_IsNegative;
            codepointToRecord = '-';
            parser->lexerState = LEXING_NUMBER_AFTER_MINUS;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == '0')
         {
            JSON_Parser_StartToken(parser, T_NUMBER);
            codepointToRecord = '0';
            parser->lexerState = LEXING_NUMBER_AFTER_LEADING_ZERO;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c >= '1' && c <= '9')
         {
            JSON_Parser_StartToken(parser, T_NUMBER);
            codepointToRecord = c;
            parser->lexerState = LEXING_NUMBER_DECIMAL_DIGITS;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == ' ' || c == TAB_CODEPOINT || c == LINE_FEED_CODEPOINT ||
               c == CARRIAGE_RETURN_CODEPOINT || c == EOF_CODEPOINT)
         {
            /* Ignore whitespace between tokens. */
         }
         else if (c == BOM_CODEPOINT && parser->codepointLocationByte == 0)
         {
            /* OK, we'll allow the BOM. */
            if (GET_FLAGS(parser->flags, PARSER_ALLOW_BOM)) { }
            else
            {
               JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_BOMNotAllowed);
               return JSON_Failure;
            }
         }
         else if (c == '/' && GET_FLAGS(parser->flags, PARSER_ALLOW_COMMENTS))
         {
            /* Comments are not real tokens, but we save the location
               of the comment as the token location in case of an error. */
            parser->tokenLocationByte = parser->codepointLocationByte;
            parser->tokenLocationLine = parser->codepointLocationLine;
            parser->tokenLocationColumn = parser->codepointLocationColumn;
            parser->lexerState = LEXING_COMMENT_AFTER_SLASH;
         }
         else if (c == 'N' && GET_FLAGS(parser->flags, PARSER_ALLOW_SPECIAL_NUMBERS))
         {
            JSON_Parser_StartToken(parser, T_NAN);
            parser->lexerBits = NAN_LITERAL_EXPECTED_CHARS_START_INDEX;
            parser->lexerState = LEXING_LITERAL;
         }
         else if (c == 'I' && GET_FLAGS(parser->flags, PARSER_ALLOW_SPECIAL_NUMBERS))
         {
            JSON_Parser_StartToken(parser, T_INFINITY);
            parser->lexerBits = INFINITY_LITERAL_EXPECTED_CHARS_START_INDEX;
            parser->lexerState = LEXING_LITERAL;
         }
         else
         {
            JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_UnknownToken);
            return JSON_Failure;
         }
         goto advance;

      case LEXING_LITERAL:
         /* While lexing a literal we store an index into expectedLiteralChars
            in lexerBits. */
         if (expectedLiteralChars[parser->lexerBits])
         {
            /* The codepoint should match the next character in the literal. */
            if (c != expectedLiteralChars[parser->lexerBits])
            {
               JSON_Parser_SetErrorAtToken(parser, JSON_Error_UnknownToken);
               return JSON_Failure;
            }
            parser->lexerBits++;

            /* If the literal is the top-level value of an embedded document,
               process it as soon as we consume its last expected codepoint.
               Normally we defer processing until the following codepoint
               has been examined, so that we can treat sequences like "nullx"
               as a single, unknown token rather than a null literal followed
               by an unknown token. */
            if (!parser->depth && GET_FLAGS(parser->flags, PARSER_EMBEDDED_DOCUMENT) &&
                  !expectedLiteralChars[parser->lexerBits])
               tokenFinished = 1;
         }
         else
         {
            /* The literal should be finished, so the codepoint should not be
               a plausible JSON literal character, but rather EOF, whitespace,
               or the first character of the next token. */
            if ((c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') ||
                  (c == '_'))
            {
               JSON_Parser_SetErrorAtToken(parser, JSON_Error_UnknownToken);
               return JSON_Failure;
            }
            if (!JSON_Parser_ProcessToken(parser))
               return JSON_Failure;
            goto reprocess;
         }
         goto advance;

      case LEXING_STRING:
         /* Allow JSON_Parser_FlushLexer() to fail. */
         if (c == EOF_CODEPOINT) { }
         else if (c == '"')
            tokenFinished = 1;
         else if (c == '\\')
            parser->lexerState = LEXING_STRING_ESCAPE;
         else if (c < 0x20 && !GET_FLAGS(parser->flags, PARSER_ALLOW_CONTROL_CHARS))
         {
            /* ASCII control characters (U+0000 - U+001F) are not allowed to
               appear unescaped in string values unless specifically allowed. */
            JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_UnescapedControlCharacter);
            return JSON_Failure;
         }
         else
         {
            codepointToRecord = c;
            goto recordStringCodepointAndAdvance;
         }
         goto advance;

      case LEXING_STRING_ESCAPE:
         if (c == EOF_CODEPOINT)
         {
            /* Allow JSON_Parser_FlushLexer() to fail. */
         }
         else
         {
            if (c == 'u')
               parser->lexerState = LEXING_STRING_HEX_ESCAPE_BYTE_1;
            else
            {
               if (c == '"' || c == '\\' || c == '/')
                  codepointToRecord = c;
               else if (c == 'b')
                  codepointToRecord = BACKSPACE_CODEPOINT;
               else if (c == 't')
                  codepointToRecord = TAB_CODEPOINT;
               else if (c == 'n')
                  codepointToRecord = LINE_FEED_CODEPOINT;
               else if (c == 'f')
                  codepointToRecord = FORM_FEED_CODEPOINT;
               else if (c == 'r')
                  codepointToRecord = CARRIAGE_RETURN_CODEPOINT;
               else
               {
                  /* The current codepoint location is the first character after
                     the backslash that started the escape sequence. The error
                     location should be the beginning of the escape sequence, 1
                     character earlier. */
                  JSON_Parser_SetErrorAtStringEscapeSequenceStart(parser, JSON_Error_InvalidEscapeSequence, 1);
                  return JSON_Failure;
               }
               parser->lexerState = LEXING_STRING;
               goto recordStringCodepointAndAdvance;
            }
         }
         goto advance;

      case LEXING_STRING_HEX_ESCAPE_BYTE_1:
      case LEXING_STRING_HEX_ESCAPE_BYTE_2:
      case LEXING_STRING_HEX_ESCAPE_BYTE_3:
      case LEXING_STRING_HEX_ESCAPE_BYTE_4:
      case LEXING_STRING_HEX_ESCAPE_BYTE_5:
      case LEXING_STRING_HEX_ESCAPE_BYTE_6:
      case LEXING_STRING_HEX_ESCAPE_BYTE_7:
      case LEXING_STRING_HEX_ESCAPE_BYTE_8:
         /* Allow JSON_Parser_FlushLexer() to fail. */
         if (c != EOF_CODEPOINT)
         {
            /* While lexing a string hex escape sequence we store the bytes
               of the escaped codepoint in the low 2 bytes of lexerBits. If
               the escape sequence represents a leading surrogate, we shift
               the leading surrogate into the high 2 bytes and lex a second
               hex escape sequence (which should be a trailing surrogate). */
            int byteNumber = (parser->lexerState - LEXING_STRING_HEX_ESCAPE_BYTE_1) & 0x3;
            uint32_t nibble;
            if (c >= '0' && c <= '9')
               nibble = c - '0';
            else if (c >= 'A' && c <= 'F')
               nibble = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f')
               nibble = c - 'a' + 10;
            else
            {
               /* The current codepoint location is one of the 4 hex digit
                  character slots in the hex escape sequence. The error
                  location should be the beginning of the hex escape
                  sequence, between 2 and 5 bytes earlier. */
               int codepointsAgo = 2 /* for "\u" */ + byteNumber;
               JSON_Parser_SetErrorAtStringEscapeSequenceStart(
                     parser, JSON_Error_InvalidEscapeSequence, codepointsAgo);
               return JSON_Failure;
            }
            /* Store the hex digit's bits in the appropriate byte of lexerBits. */
            nibble <<= (3 - byteNumber) * 4 /* shift left by 12, 8, 4, 0 */ ;
            parser->lexerBits |= nibble;
            if (parser->lexerState == LEXING_STRING_HEX_ESCAPE_BYTE_4)
            {
               /* The escape sequence is complete. We need to check whether
                  it represents a leading surrogate (which implies that it
                  will be immediately followed by a hex-escaped trailing
                  surrogate), a trailing surrogate (which is invalid), or a
                  valid codepoint (which should simply be appended to the
                  string token value). */
               if (IS_LEADING_SURROGATE(parser->lexerBits))
               {
                  /* Shift the leading surrogate into the high 2 bytes of
                     lexerBits so that the trailing surrogate can be stored
                     in the low 2 bytes. */
                  parser->lexerBits <<= 16;
                  parser->lexerState = LEXING_STRING_TRAILING_SURROGATE_HEX_ESCAPE_BACKSLASH;
               }
               else if (IS_TRAILING_SURROGATE(parser->lexerBits))
               {
                  /* The current codepoint location is the last hex digit
                     of the hex escape sequence. The error location should
                     be the beginning of the hex escape sequence, 5
                     characters earlier. */
                  JSON_Parser_SetErrorAtStringEscapeSequenceStart(
                        parser, JSON_Error_UnpairedSurrogateEscapeSequence, 5);
                  return JSON_Failure;
               }
               else
               {
                  /* The escape sequence represents a BMP codepoint. */
                  codepointToRecord = parser->lexerBits;
                  parser->lexerBits = 0;
                  parser->lexerState = LEXING_STRING;
                  goto recordStringCodepointAndAdvance;
               }
            }
            else if (parser->lexerState == LEXING_STRING_HEX_ESCAPE_BYTE_8)
            {
               /* The second hex escape sequence is complete. We need to
                  check whether it represents a trailing surrogate as
                  expected. If so, the surrogate pair represents a single
                  non-BMP codepoint. */
               if (!IS_TRAILING_SURROGATE(parser->lexerBits & 0xFFFF))
               {
                  /* The current codepoint location is the last hex digit of
                     the second hex escape sequence. The error location
                     should be the beginning of the leading surrogate
                     hex escape sequence, 11 characters earlier. */
                  JSON_Parser_SetErrorAtStringEscapeSequenceStart(
                        parser, JSON_Error_UnpairedSurrogateEscapeSequence, 11);
                  return JSON_Failure;
               }
               /* The escape sequence represents a non-BMP codepoint. */
               codepointToRecord = CODEPOINT_FROM_SURROGATES(parser->lexerBits);
               parser->lexerBits = 0;
               parser->lexerState = LEXING_STRING;
               goto recordStringCodepointAndAdvance;
            }
            else
               parser->lexerState++;
         }
         goto advance;

      case LEXING_STRING_TRAILING_SURROGATE_HEX_ESCAPE_BACKSLASH:
         if (c != EOF_CODEPOINT)
         {
            if (c != '\\')
            {
               /* The current codepoint location is the first character after
                  the leading surrogate hex escape sequence. The error
                  location should be the beginning of the leading surrogate
                  hex escape sequence, 6 characters earlier. */
               JSON_Parser_SetErrorAtStringEscapeSequenceStart(
                     parser, JSON_Error_UnpairedSurrogateEscapeSequence, 6);
               return JSON_Failure;
            }
            parser->lexerState = LEXING_STRING_TRAILING_SURROGATE_HEX_ESCAPE_U;
         }
         goto advance;

      case LEXING_STRING_TRAILING_SURROGATE_HEX_ESCAPE_U:
         if (c != EOF_CODEPOINT)
         {
            if (c != 'u')
            {
               /* Distinguish between a totally bogus escape sequence
                  and a valid one that just isn't the hex escape kind
                  that we require for a trailing surrogate. The current
                  codepoint location is the first character after the
                  backslash that should have introduced the trailing
                  surrogate hex escape sequence. */
               if (c == '"' || c == '\\' || c == '/' || c == 'b' ||
                     c == 't' || c == 'n' || c == 'f' || c == 'r')
               {
                  /* The error location should be at that beginning of the
                     leading surrogate's hex escape sequence, 7 characters
                     earlier. */
                  JSON_Parser_SetErrorAtStringEscapeSequenceStart(
                        parser, JSON_Error_UnpairedSurrogateEscapeSequence, 7);
               }
               else
               {
                  /* The error location should be at that backslash, 1
                     character earlier. */
                  JSON_Parser_SetErrorAtStringEscapeSequenceStart(
                        parser, JSON_Error_InvalidEscapeSequence, 1);
               }
               return JSON_Failure;
            }
            parser->lexerState = LEXING_STRING_HEX_ESCAPE_BYTE_5;
         }
         goto advance;

      case LEXING_NUMBER_AFTER_MINUS:
         if (c == EOF_CODEPOINT)
         {
            /* Allow JSON_Parser_FlushLexer() to fail. */
         }
         else if (c == 'I' && GET_FLAGS(parser->flags, PARSER_ALLOW_SPECIAL_NUMBERS))
         {
            parser->token      = T_NEGATIVE_INFINITY; /* changing horses mid-stream, so to speak */
            parser->lexerBits  = INFINITY_LITERAL_EXPECTED_CHARS_START_INDEX;
            parser->lexerState = LEXING_LITERAL;
         }
         else
         {
            if (c == '0')
            {
               codepointToRecord  = '0';
               parser->lexerState = LEXING_NUMBER_AFTER_LEADING_NEGATIVE_ZERO;
               goto recordNumberCodepointAndAdvance;
            }
            else if (c >= '1' && c <= '9')
            {
               codepointToRecord  = c;
               parser->lexerState = LEXING_NUMBER_DECIMAL_DIGITS;
               goto recordNumberCodepointAndAdvance;
            }
            else
            {
               /* We trigger an unknown token error rather than an invalid number
                  error so that "Foo" and "-Foo" trigger the same error. */
               JSON_Parser_SetErrorAtToken(parser, JSON_Error_UnknownToken);
               return JSON_Failure;
            }
         }
         goto advance;

      case LEXING_NUMBER_AFTER_LEADING_ZERO:
      case LEXING_NUMBER_AFTER_LEADING_NEGATIVE_ZERO:
         if (c == '.')
         {
            codepointToRecord = '.';
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsDecimalPoint);
            parser->lexerState = LEXING_NUMBER_AFTER_DOT;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == 'e' || c == 'E')
         {
            codepointToRecord = c;
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsExponent);
            parser->lexerState = LEXING_NUMBER_AFTER_E;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c >= '0' && c <= '9')
         {
            /* JSON does not allow the integer part of a number to have any
               digits after a leading zero. */
            if (!JSON_Parser_HandleInvalidNumber(parser, c, 0, 0))
               return JSON_Failure;
         }
         else if ((c == 'x' || c == 'X') &&
               parser->lexerState == LEXING_NUMBER_AFTER_LEADING_ZERO &&
               GET_FLAGS(parser->flags, PARSER_ALLOW_HEX_NUMBERS))
         {
            codepointToRecord = c;
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_IsHex);
            parser->lexerState = LEXING_NUMBER_AFTER_X;
            goto recordNumberCodepointAndAdvance;
         }
         else
         {
            /* The number is finished. */
            if (!JSON_Parser_ProcessToken(parser))
               return JSON_Failure;
            goto reprocess;
         }
         goto advance;

      case LEXING_NUMBER_AFTER_X:
         if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
         {
            codepointToRecord = c;
            parser->lexerState = LEXING_NUMBER_HEX_DIGITS;
            goto recordNumberCodepointAndAdvance;
         }
         else if (!JSON_Parser_HandleInvalidNumber(parser, c, 1, JSON_IsHex))
            return JSON_Failure;
         goto advance;

      case LEXING_NUMBER_HEX_DIGITS:
         if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
         {
            codepointToRecord = c;
            goto recordNumberCodepointAndAdvance;
         }
         /* The number is finished. */
         if (!JSON_Parser_ProcessToken(parser))
            return JSON_Failure;
         goto reprocess;

      case LEXING_NUMBER_DECIMAL_DIGITS:
         if (c >= '0' && c <= '9')
         {
            codepointToRecord = c;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == '.')
         {
            codepointToRecord = '.';
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsDecimalPoint);
            parser->lexerState = LEXING_NUMBER_AFTER_DOT;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == 'e' || c == 'E')
         {
            codepointToRecord = c;
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsExponent);
            parser->lexerState = LEXING_NUMBER_AFTER_E;
            goto recordNumberCodepointAndAdvance;
         }
         /* The number is finished. */
         if (!JSON_Parser_ProcessToken(parser))
            return JSON_Failure;
         goto reprocess;

      case LEXING_NUMBER_AFTER_DOT:
         if (c >= '0' && c <= '9')
         {
            codepointToRecord = c;
            parser->lexerState = LEXING_NUMBER_FRACTIONAL_DIGITS;
            goto recordNumberCodepointAndAdvance;
         }
         else if (!JSON_Parser_HandleInvalidNumber(parser, c, 1, JSON_ContainsDecimalPoint))
            return JSON_Failure;
         goto advance;

      case LEXING_NUMBER_FRACTIONAL_DIGITS:
         if (c >= '0' && c <= '9')
         {
            codepointToRecord = c;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == 'e' || c == 'E')
         {
            codepointToRecord = c;
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsExponent);
            parser->lexerState = LEXING_NUMBER_AFTER_E;
            goto recordNumberCodepointAndAdvance;
         }
         /* The number is finished. */
         if (!JSON_Parser_ProcessToken(parser))
            return JSON_Failure;
         goto reprocess;

      case LEXING_NUMBER_AFTER_E:
         if (c == '+')
         {
            codepointToRecord = c;
            parser->lexerState = LEXING_NUMBER_AFTER_EXPONENT_SIGN;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c == '-')
         {
            codepointToRecord = c;
            SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsNegativeExponent);
            parser->lexerState = LEXING_NUMBER_AFTER_EXPONENT_SIGN;
            goto recordNumberCodepointAndAdvance;
         }
         else if (c >= '0' && c <= '9')
         {
            codepointToRecord = c;
            parser->lexerState = LEXING_NUMBER_EXPONENT_DIGITS;
            goto recordNumberCodepointAndAdvance;
         }
         else if (!JSON_Parser_HandleInvalidNumber(parser, c, 1, JSON_ContainsExponent))
            return JSON_Failure;
         goto advance;

      case LEXING_NUMBER_AFTER_EXPONENT_SIGN:
         if (c >= '0' && c <= '9')
         {
            codepointToRecord = c;
            parser->lexerState = LEXING_NUMBER_EXPONENT_DIGITS;
            goto recordNumberCodepointAndAdvance;
         }
         else if (!JSON_Parser_HandleInvalidNumber(parser, c, 2, JSON_ContainsExponent | JSON_ContainsNegativeExponent))
            return JSON_Failure;
         goto advance;

      case LEXING_NUMBER_EXPONENT_DIGITS:
         if (c >= '0' && c <= '9')
         {
            codepointToRecord = c;
            goto recordNumberCodepointAndAdvance;
         }
         /* The number is finished. */
         if (!JSON_Parser_ProcessToken(parser))
            return JSON_Failure;
         goto reprocess;

      case LEXING_COMMENT_AFTER_SLASH:
         if (c == '/')
            parser->lexerState = LEXING_SINGLE_LINE_COMMENT;
         else if (c == '*')
            parser->lexerState = LEXING_MULTI_LINE_COMMENT;
         else
         {
            JSON_Parser_SetErrorAtToken(parser, JSON_Error_UnknownToken);
            return JSON_Failure;
         }
         goto advance;

      case LEXING_SINGLE_LINE_COMMENT:
         if (c == CARRIAGE_RETURN_CODEPOINT || c == LINE_FEED_CODEPOINT || c == EOF_CODEPOINT)
            parser->lexerState = LEXING_WHITESPACE;
         goto advance;

      case LEXING_MULTI_LINE_COMMENT:
         if (c == '*')
            parser->lexerState = LEXING_MULTI_LINE_COMMENT_AFTER_STAR;
         goto advance;

      case LEXING_MULTI_LINE_COMMENT_AFTER_STAR:
         if (c == '/')
            parser->lexerState = LEXING_WHITESPACE;
         else if (c != '*')
            parser->lexerState = LEXING_MULTI_LINE_COMMENT;
         goto advance;
   }

recordStringCodepointAndAdvance:

   tokenEncoding  = parser->stringEncoding;
   maxTokenLength = parser->maxStringLength;
   if (!codepointToRecord)
   {
      SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsNullCharacter | JSON_ContainsControlCharacter);
   }
   else if (codepointToRecord < FIRST_NON_CONTROL_CODEPOINT)
   {
      SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsControlCharacter);
   }
   else if (codepointToRecord >= FIRST_NON_BMP_CODEPOINT)
   {
      SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsNonASCIICharacter | JSON_ContainsNonBMPCharacter);
   }
   else if (codepointToRecord >= FIRST_NON_ASCII_CODEPOINT)
   {
      SET_FLAGS_ON(TokenAttributes, parser->tokenAttributes, JSON_ContainsNonASCIICharacter);
   }
   goto recordCodepointAndAdvance;

recordNumberCodepointAndAdvance:

   tokenEncoding = parser->numberEncoding;
   maxTokenLength = parser->maxNumberLength;
   goto recordCodepointAndAdvance;

recordCodepointAndAdvance:

   /* We always ensure that there are LONGEST_ENCODING_SEQUENCE bytes
      available in the buffer for the next codepoint, so we don't have to
      check whether there is room when we decode a new codepoint, and if
      there isn't another codepoint, we have space already allocated for
      the encoded null terminator.*/
   parser->tokenBytesUsed += EncodeCodepoint(codepointToRecord, tokenEncoding, parser->pTokenBytes + parser->tokenBytesUsed);
   if (parser->tokenBytesUsed > maxTokenLength)
   {
      JSON_Parser_SetErrorAtToken(parser, parser->token == T_NUMBER ? JSON_Error_TooLongNumber : JSON_Error_TooLongString);
      return JSON_Failure;
   }
   if (parser->tokenBytesUsed > parser->tokenBytesLength - LONGEST_ENCODING_SEQUENCE)
   {
      byte* pBiggerBuffer = DoubleBuffer(&parser->memorySuite, parser->defaultTokenBytes, parser->pTokenBytes, parser->tokenBytesLength);
      if (!pBiggerBuffer)
      {
         JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_OutOfMemory);
         return JSON_Failure;
      }
      parser->pTokenBytes = pBiggerBuffer;
      parser->tokenBytesLength *= 2;
   }
   goto advance;

advance:

   /* The current codepoint has been accepted, so advance the codepoint
      location counters accordingly. Note that the one time we don't
      do this is when the codepoint is EOF, which doesn't actually
      appear in the input stream. */
   if (c == CARRIAGE_RETURN_CODEPOINT)
   {
      SET_FLAGS_ON(ParserState, parser->state, PARSER_AFTER_CARRIAGE_RETURN);
   }
   if (c != EOF_CODEPOINT)
   {
      parser->codepointLocationByte += encodedLength;
      if (c == CARRIAGE_RETURN_CODEPOINT || c == LINE_FEED_CODEPOINT)
      {
         /* The next character will begin a new line. */
         parser->codepointLocationLine++;
         parser->codepointLocationColumn = 0;
      }
      else
      {
         /* The next character will be on the same line. */
         parser->codepointLocationColumn++;
      }
   }

   if (tokenFinished && !JSON_Parser_ProcessToken(parser))
      return JSON_Failure;

   return JSON_Success;
}

static JSON_Status JSON_Parser_FlushLexer(JSON_Parser parser)
{
   /* Push the EOF codepoint to the lexer so that it can finish the pending
      token, if any. The EOF codepoint is never emitted by the decoder
      itself, since it is outside the Unicode range and therefore cannot
      be encoded in any of the possible input encodings. */
   if (!JSON_Parser_ProcessCodepoint(parser, EOF_CODEPOINT, 0))
      return JSON_Failure;

   /* The lexer should be idle when parsing finishes. */
   if (parser->lexerState != LEXING_WHITESPACE)
   {
      JSON_Parser_SetErrorAtToken(parser, JSON_Error_IncompleteToken);
      return JSON_Failure;
   }
   return JSON_Success;
}

/* Parser's decoder functions. */

static JSON_Status JSON_Parser_CallEncodingDetectedHandler(JSON_Parser parser)
{
   if (parser->encodingDetectedHandler && parser->encodingDetectedHandler(parser) != JSON_Parser_Continue)
   {
      JSON_Parser_SetErrorAtCodepoint(parser, JSON_Error_AbortedByHandler);
      return JSON_Failure;
   }
   return JSON_Success;
}

/* Forward declaration. */
static JSON_Status JSON_Parser_ProcessInputBytes(JSON_Parser parser, const byte* pBytes, size_t length);

static JSON_Status JSON_Parser_ProcessUnknownByte(JSON_Parser parser, byte b)
{
   /* When the input encoding is unknown, the first 4 bytes of input are
      recorded in decoder.bits. */
   byte bytes[LONGEST_ENCODING_SEQUENCE];

   switch (parser->decoderData.state)
   {
      case DECODER_RESET:
         parser->decoderData.state = DECODED_1_OF_4;
         parser->decoderData.bits = (uint32_t)b << 24;
         break;

      case DECODED_1_OF_4:
         parser->decoderData.state = DECODED_2_OF_4;
         parser->decoderData.bits |= (uint32_t)b << 16;
         break;

      case DECODED_2_OF_4:
         parser->decoderData.state = DECODED_3_OF_4;
         parser->decoderData.bits |= (uint32_t)b << 8;
         break;

      case DECODED_3_OF_4:
         bytes[0] = (byte)(parser->decoderData.bits >> 24);
         bytes[1] = (byte)(parser->decoderData.bits >> 16);
         bytes[2] = (byte)(parser->decoderData.bits >> 8);
         bytes[3] = (byte)(b);

         /* We try to match the following patterns in order, where .. is any
            byte value and nz is any non-zero byte value:
            EF BB BF .. => UTF-8 with BOM
            FF FE 00 00 => UTF-32LE with BOM
            FF FE nz 00 => UTF-16LE with BOM
            00 00 FE FF -> UTF-32BE with BOM
            FE FF .. .. => UTF-16BE with BOM
            nz nz .. .. => UTF-8
            nz 00 nz .. => UTF-16LE
            nz 00 00 00 => UTF-32LE
            00 nz .. .. => UTF-16BE
            00 00 00 nz => UTF-32BE
            .. .. .. .. => unknown encoding */
         if (bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
         {
            /* EF BB BF .. */
            parser->inputEncoding = JSON_UTF8;
         }
         else if (bytes[0] == 0xFF && bytes[1] == 0xFE && bytes[3] == 0x00)
         {
            /* FF FE 00 00 or
               FF FE nz 00 */
            parser->inputEncoding = (bytes[2] == 0x00) ? JSON_UTF32LE : JSON_UTF16LE;
         }
         else if (bytes[0] == 0x00 && bytes[1] == 0x00 && bytes[2] == 0xFE && bytes[3] == 0xFF)
         {
            /* 00 00 FE FF */
            parser->inputEncoding = JSON_UTF32BE;
         }
         else if (bytes[0] == 0xFE && bytes[1] == 0xFF)
         {
            /* FE FF .. .. */
            parser->inputEncoding = JSON_UTF16BE;
         }
         else if (bytes[0] != 0x00)
         {
            /* nz .. .. .. */
            if (bytes[1] != 0x00)
            {
               /* nz nz .. .. */
               parser->inputEncoding = JSON_UTF8;
            }
            else if (bytes[2] != 0x00)
            {
               /* nz 00 nz .. */
               parser->inputEncoding = JSON_UTF16LE;
            }
            else if (bytes[3] == 0x00)
            {
               /* nz 00 00 00 */
               parser->inputEncoding = JSON_UTF32LE;
            }
            else
            {
               /* nz 00 00 nz => error */
            }
         }
         else if (bytes[1] != 0x00)
         {
            /* 00 nz .. .. */
            parser->inputEncoding = JSON_UTF16BE;
         }
         else if (bytes[2] == 0x00 && bytes[3] != 0x00)
         {
            /* 00 00 00 nz */
            parser->inputEncoding = JSON_UTF32BE;
         }
         else
         {
            /* 00 00 nz .. or
               00 00 00 00 => error */
         }

         if (parser->inputEncoding == JSON_UnknownEncoding)
            return JSON_Parser_HandleInvalidEncodingSequence(parser, 4);

         if (!JSON_Parser_CallEncodingDetectedHandler(parser))
            return JSON_Failure;

         /* Reset the decoder before reprocessing the bytes. */
         Decoder_Reset(&parser->decoderData);
         return JSON_Parser_ProcessInputBytes(parser, bytes, 4);
   }

   /* We don't have 4 bytes yet. */
   return JSON_Success;
}

JSON_Status JSON_Parser_ProcessInputBytes(JSON_Parser parser, const byte* pBytes, size_t length)
{
   /* Note that if length is 0, pBytes is allowed to be NULL. */
   size_t i = 0;
   while (parser->inputEncoding == JSON_UnknownEncoding && i < length)
   {
      if (!JSON_Parser_ProcessUnknownByte(parser, pBytes[i]))
         return JSON_Failure;
      i++;
   }
   while (i < length)
   {
      DecoderOutput output     = Decoder_ProcessByte(
            &parser->decoderData, parser->inputEncoding, pBytes[i]);
      DecoderResultCode result = DECODER_RESULT_CODE(output);
      switch (result)
      {
         case SEQUENCE_PENDING:
            i++;
            break;

         case SEQUENCE_COMPLETE:
            if (!JSON_Parser_ProcessCodepoint(
                     parser, DECODER_CODEPOINT(output),
                     DECODER_SEQUENCE_LENGTH(output)))
               return JSON_Failure;
            i++;
            break;

         case SEQUENCE_INVALID_INCLUSIVE:
            i++;
            /* fallthrough */
         case SEQUENCE_INVALID_EXCLUSIVE:
            if (!JSON_Parser_HandleInvalidEncodingSequence(
                     parser, DECODER_SEQUENCE_LENGTH(output)))
               return JSON_Failure;
            break;
      }
   }
   return JSON_Success;
}

static JSON_Status JSON_Parser_FlushDecoder(JSON_Parser parser)
{
   /* If the input was 1, 2, or 3 bytes long, and the input encoding was not
      explicitly specified by the client, we can sometimes make a reasonable
      guess. If the input was 1 or 3 bytes long, the only encoding that could
      possibly be valid JSON is UF-8. If the input was 2 bytes long, we try
      to match the following patterns in order, where .. is any byte value
      and nz is any non-zero byte value:
      FF FE => UTF-16LE with BOM
      FE FF => UTF-16BE with BOM
      nz nz => UTF-8
      nz 00 => UTF-16LE
      00 nz => UTF-16BE
      .. .. => unknown encoding
      */
   if (parser->inputEncoding == JSON_UnknownEncoding &&
         parser->decoderData.state != DECODER_RESET)
   {
      byte bytes[3];
      size_t length = 0;
      bytes[0] = (byte)(parser->decoderData.bits >> 24);
      bytes[1] = (byte)(parser->decoderData.bits >> 16);
      bytes[2] = (byte)(parser->decoderData.bits >> 8);

      switch (parser->decoderData.state)
      {
         case DECODED_1_OF_4:
            parser->inputEncoding = JSON_UTF8;
            length = 1;
            break;

         case DECODED_2_OF_4:
            /* FF FE */
            if (bytes[0] == 0xFF && bytes[1] == 0xFE)
               parser->inputEncoding = JSON_UTF16LE;
            /* FE FF */
            else if (bytes[0] == 0xFE && bytes[1] == 0xFF)
               parser->inputEncoding = JSON_UTF16BE;
            else if (bytes[0] != 0x00)
            {
               /* nz nz or
                  nz 00 */
               parser->inputEncoding = bytes[1] ? JSON_UTF8 : JSON_UTF16LE;
            }
            /* 00 nz */
            else if (bytes[1] != 0x00)
               parser->inputEncoding = JSON_UTF16BE;
            /* 00 00 */
            else
               return JSON_Parser_HandleInvalidEncodingSequence(parser, 2);
            length = 2;
            break;

         case DECODED_3_OF_4:
            parser->inputEncoding = JSON_UTF8;
            length = 3;
            break;
      }

      if (!JSON_Parser_CallEncodingDetectedHandler(parser))
         return JSON_Failure;

      /* Reset the decoder before reprocessing the bytes. */
      parser->decoderData.state = DECODER_RESET;
      parser->decoderData.bits = 0;
      if (!JSON_Parser_ProcessInputBytes(parser, bytes, length))
         return JSON_Failure;
   }

   /* The decoder should be idle when parsing finishes. */
   if (Decoder_SequencePending(&parser->decoderData))
      return JSON_Parser_HandleInvalidEncodingSequence(
            parser, DECODER_STATE_BYTES(parser->decoderData.state));
   return JSON_Success;
}

/* Parser API functions. */

JSON_Parser JSON_CALL JSON_Parser_Create(const JSON_MemorySuite* pMemorySuite)
{
   JSON_Parser parser;
   JSON_MemorySuite memorySuite;

   if (pMemorySuite)
   {
      memorySuite = *pMemorySuite;

      /* The full memory suite must be specified. */
      if (!memorySuite.realloc || !memorySuite.free)
         return NULL;
   }
   else
      memorySuite = defaultMemorySuite;

   parser = (JSON_Parser)memorySuite.realloc(memorySuite.userData, NULL, sizeof(struct JSON_Parser_Data));

   if (!parser)
      return NULL;

   parser->memorySuite = memorySuite;
   JSON_Parser_ResetData(parser, 0/* isInitialized */);
   return parser;
}

JSON_Status JSON_CALL JSON_Parser_Free(JSON_Parser parser)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_IN_PROTECTED_API))
      return JSON_Failure;

   SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_PROTECTED_API);

   if (parser->pTokenBytes != parser->defaultTokenBytes)
      parser->memorySuite.free(parser->memorySuite.userData, parser->pTokenBytes);

   while (parser->pMemberNames)
      JSON_Parser_PopMemberNameList(parser);

   Grammarian_FreeAllocations(&parser->grammarianData, &parser->memorySuite);
   parser->memorySuite.free(parser->memorySuite.userData, parser);
   return JSON_Success;
}

JSON_Status JSON_CALL JSON_Parser_Reset(JSON_Parser parser)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_IN_PROTECTED_API))
      return JSON_Failure;
   SET_FLAGS_ON(ParserState, parser->state, PARSER_IN_PROTECTED_API);
   JSON_Parser_ResetData(parser, 1/* isInitialized */);
   /* Note that JSON_Parser_ResetData() unset PARSER_IN_PROTECTED_API for us. */
   return JSON_Success;
}

void* JSON_CALL JSON_Parser_GetUserData(JSON_Parser parser)
{
   return parser ? parser->userData : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetUserData(JSON_Parser parser, void* userData)
{
   if (!parser)
      return JSON_Failure;
   parser->userData = userData;
   return JSON_Success;
}

JSON_Encoding JSON_CALL JSON_Parser_GetInputEncoding(JSON_Parser parser)
{
   return parser ? (JSON_Encoding)parser->inputEncoding : JSON_UnknownEncoding;
}

JSON_Status JSON_CALL JSON_Parser_SetInputEncoding(JSON_Parser parser, JSON_Encoding encoding)
{
   if (     !parser
         || encoding < JSON_UnknownEncoding
         || encoding > JSON_UTF32BE
         || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   parser->inputEncoding = (Encoding)encoding;
   return JSON_Success;
}

JSON_Encoding JSON_CALL JSON_Parser_GetStringEncoding(JSON_Parser parser)
{
   return parser ? (JSON_Encoding)parser->stringEncoding : JSON_UTF8;
}

JSON_Status JSON_CALL JSON_Parser_SetStringEncoding(JSON_Parser parser, JSON_Encoding encoding)
{
   if (
            !parser
         || encoding <= JSON_UnknownEncoding
         || encoding > JSON_UTF32BE
         || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   parser->stringEncoding = (Encoding)encoding;
   return JSON_Success;
}

size_t JSON_CALL JSON_Parser_GetMaxStringLength(JSON_Parser parser)
{
   return parser ? parser->maxStringLength : SIZE_MAX;
}

JSON_Status JSON_CALL JSON_Parser_SetMaxStringLength(JSON_Parser parser, size_t maxLength)
{
   if (     !parser
         || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   parser->maxStringLength = maxLength;
   return JSON_Success;
}

JSON_Encoding JSON_CALL JSON_Parser_GetNumberEncoding(JSON_Parser parser)
{
   return parser ? (JSON_Encoding)parser->numberEncoding : JSON_UTF8;
}

JSON_Status JSON_CALL JSON_Parser_SetNumberEncoding(JSON_Parser parser, JSON_Encoding encoding)
{
   if (!parser || encoding <= JSON_UnknownEncoding || encoding > JSON_UTF32BE || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   parser->numberEncoding = (Encoding)encoding;
   return JSON_Success;
}

size_t JSON_CALL JSON_Parser_GetMaxNumberLength(JSON_Parser parser)
{
   return parser ? parser->maxNumberLength : SIZE_MAX;
}

JSON_Status JSON_CALL JSON_Parser_SetMaxNumberLength(JSON_Parser parser, size_t maxLength)
{
   if (     !parser
         || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   parser->maxNumberLength = maxLength;
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetAllowBOM(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_ALLOW_BOM)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetAllowBOM(JSON_Parser parser, JSON_Boolean allowBOM)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   SET_FLAGS(ParserFlags, parser->flags, PARSER_ALLOW_BOM, allowBOM);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetAllowComments(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_ALLOW_COMMENTS)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetAllowComments(JSON_Parser parser, JSON_Boolean allowComments)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   SET_FLAGS(ParserFlags, parser->flags, PARSER_ALLOW_COMMENTS, allowComments);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetAllowSpecialNumbers(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_ALLOW_SPECIAL_NUMBERS)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetAllowSpecialNumbers(JSON_Parser parser, JSON_Boolean allowSpecialNumbers)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   SET_FLAGS(ParserFlags, parser->flags, PARSER_ALLOW_SPECIAL_NUMBERS, allowSpecialNumbers);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetAllowHexNumbers(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_ALLOW_HEX_NUMBERS)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetAllowHexNumbers(JSON_Parser parser, JSON_Boolean allowHexNumbers)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   SET_FLAGS(ParserFlags, parser->flags, PARSER_ALLOW_HEX_NUMBERS, allowHexNumbers);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetAllowUnescapedControlCharacters(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_ALLOW_CONTROL_CHARS)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetAllowUnescapedControlCharacters(JSON_Parser parser, JSON_Boolean allowUnescapedControlCharacters)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   SET_FLAGS(ParserFlags, parser->flags, PARSER_ALLOW_CONTROL_CHARS, allowUnescapedControlCharacters);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetReplaceInvalidEncodingSequences(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_REPLACE_INVALID)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetReplaceInvalidEncodingSequences(
      JSON_Parser parser, JSON_Boolean replaceInvalidEncodingSequences)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
      return JSON_Failure;
   SET_FLAGS(ParserFlags, parser->flags, PARSER_REPLACE_INVALID, replaceInvalidEncodingSequences);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetTrackObjectMembers(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_TRACK_OBJECT_MEMBERS)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetTrackObjectMembers(JSON_Parser parser, JSON_Boolean trackObjectMembers)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
   {
      return JSON_Failure;
   }
   SET_FLAGS(ParserFlags, parser->flags, PARSER_TRACK_OBJECT_MEMBERS, trackObjectMembers);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Parser_GetStopAfterEmbeddedDocument(JSON_Parser parser)
{
   return (parser && GET_FLAGS(parser->flags, PARSER_EMBEDDED_DOCUMENT)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Parser_SetStopAfterEmbeddedDocument(
      JSON_Parser parser, JSON_Boolean stopAfterEmbeddedDocument)
{
   if (!parser || GET_FLAGS(parser->state, PARSER_STARTED))
   {
      return JSON_Failure;
   }
   SET_FLAGS(ParserFlags, parser->flags, PARSER_EMBEDDED_DOCUMENT, stopAfterEmbeddedDocument);
   return JSON_Success;
}

JSON_Error JSON_CALL JSON_Parser_GetError(JSON_Parser parser)
{
   return parser ? (JSON_Error)parser->error : JSON_Error_None;
}

JSON_Status JSON_CALL JSON_Parser_GetErrorLocation(
      JSON_Parser parser, JSON_Location* pLocation)
{
   if (!pLocation || !parser || parser->error == JSON_Error_None)
      return JSON_Failure;

   if (parser->errorOffset == ERROR_LOCATION_IS_TOKEN_START)
   {
      pLocation->byte = parser->tokenLocationByte;
      pLocation->line = parser->tokenLocationLine;
      pLocation->column = parser->tokenLocationColumn;
   }
   else
   {
      pLocation->byte = parser->codepointLocationByte - (SHORTEST_ENCODING_SEQUENCE(parser->inputEncoding) * parser->errorOffset);
      pLocation->line = parser->codepointLocationLine;
      pLocation->column = parser->codepointLocationColumn - parser->errorOffset;
   }
   pLocation->depth = parser->depth;
   return JSON_Success;
}

JSON_Status JSON_CALL JSON_Parser_GetTokenLocation(
      JSON_Parser parser, JSON_Location* pLocation)
{
   if (!parser || !pLocation || !GET_FLAGS(parser->state, PARSER_IN_TOKEN_HANDLER))
      return JSON_Failure;

   pLocation->byte = parser->tokenLocationByte;
   pLocation->line = parser->tokenLocationLine;
   pLocation->column = parser->tokenLocationColumn;
   pLocation->depth = parser->depth;
   return JSON_Success;
}

JSON_Status JSON_CALL JSON_Parser_GetAfterTokenLocation(
      JSON_Parser parser, JSON_Location* pLocation)
{
   if (!parser || !pLocation || !GET_FLAGS(parser->state, PARSER_IN_TOKEN_HANDLER))
      return JSON_Failure;

   pLocation->byte = parser->codepointLocationByte;
   pLocation->line = parser->codepointLocationLine;
   pLocation->column = parser->codepointLocationColumn;
   pLocation->depth = parser->depth;
   return JSON_Success;
}

JSON_Parser_NullHandler JSON_CALL JSON_Parser_GetEncodingDetectedHandler(JSON_Parser parser)
{
   return parser ? parser->encodingDetectedHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetEncodingDetectedHandler(
      JSON_Parser parser, JSON_Parser_EncodingDetectedHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->encodingDetectedHandler = handler;
   return JSON_Success;
}

JSON_Parser_NullHandler JSON_CALL JSON_Parser_GetNullHandler(JSON_Parser parser)
{
   return parser ? parser->nullHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetNullHandler(
      JSON_Parser parser, JSON_Parser_NullHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->nullHandler = handler;
   return JSON_Success;
}

JSON_Parser_BooleanHandler JSON_CALL JSON_Parser_GetBooleanHandler(JSON_Parser parser)
{
   return parser ? parser->booleanHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetBooleanHandler(
      JSON_Parser parser, JSON_Parser_BooleanHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->booleanHandler = handler;
   return JSON_Success;
}

JSON_Parser_StringHandler JSON_CALL JSON_Parser_GetStringHandler(JSON_Parser parser)
{
   return parser ? parser->stringHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetStringHandler(
      JSON_Parser parser, JSON_Parser_StringHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->stringHandler = handler;
   return JSON_Success;
}

JSON_Parser_NumberHandler JSON_CALL JSON_Parser_GetNumberHandler(JSON_Parser parser)
{
   return parser ? parser->numberHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetNumberHandler(
      JSON_Parser parser, JSON_Parser_NumberHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->numberHandler = handler;
   return JSON_Success;
}

JSON_Parser_SpecialNumberHandler JSON_CALL JSON_Parser_GetSpecialNumberHandler(JSON_Parser parser)
{
   return parser ? parser->specialNumberHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetSpecialNumberHandler(
      JSON_Parser parser, JSON_Parser_SpecialNumberHandler handler)
{
   if (!parser)
      return JSON_Failure;
   parser->specialNumberHandler = handler;
   return JSON_Success;
}

JSON_Parser_StartObjectHandler JSON_CALL JSON_Parser_GetStartObjectHandler(JSON_Parser parser)
{
   return parser ? parser->startObjectHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetStartObjectHandler(
      JSON_Parser parser, JSON_Parser_StartObjectHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->startObjectHandler = handler;
   return JSON_Success;
}

JSON_Parser_EndObjectHandler JSON_CALL JSON_Parser_GetEndObjectHandler(JSON_Parser parser)
{
   return parser ? parser->endObjectHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetEndObjectHandler(
      JSON_Parser parser, JSON_Parser_EndObjectHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->endObjectHandler = handler;
   return JSON_Success;
}

JSON_Parser_ObjectMemberHandler JSON_CALL JSON_Parser_GetObjectMemberHandler(JSON_Parser parser)
{
   return parser ? parser->objectMemberHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetObjectMemberHandler(
      JSON_Parser parser, JSON_Parser_ObjectMemberHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->objectMemberHandler = handler;
   return JSON_Success;
}

JSON_Parser_StartArrayHandler JSON_CALL JSON_Parser_GetStartArrayHandler(JSON_Parser parser)
{
   return parser ? parser->startArrayHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetStartArrayHandler(
      JSON_Parser parser, JSON_Parser_StartArrayHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->startArrayHandler = handler;
   return JSON_Success;
}

JSON_Parser_EndArrayHandler JSON_CALL JSON_Parser_GetEndArrayHandler(JSON_Parser parser)
{
   return parser ? parser->endArrayHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetEndArrayHandler(
      JSON_Parser parser, JSON_Parser_EndArrayHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->endArrayHandler = handler;
   return JSON_Success;
}

JSON_Parser_ArrayItemHandler JSON_CALL JSON_Parser_GetArrayItemHandler(JSON_Parser parser)
{
   return parser ? parser->arrayItemHandler : NULL;
}

JSON_Status JSON_CALL JSON_Parser_SetArrayItemHandler(
      JSON_Parser parser, JSON_Parser_ArrayItemHandler handler)
{
   if (!parser)
      return JSON_Failure;

   parser->arrayItemHandler = handler;
   return JSON_Success;
}

JSON_Status JSON_CALL JSON_Parser_Parse(JSON_Parser parser, const char* pBytes, size_t length, JSON_Boolean isFinal)
{
   JSON_Status status = JSON_Failure;
   if (parser && (pBytes || !length) && !GET_FLAGS(parser->state, PARSER_FINISHED | PARSER_IN_PROTECTED_API))
   {
      int finishedParsing = 0;
      SET_FLAGS_ON(ParserState, parser->state, PARSER_STARTED | PARSER_IN_PROTECTED_API);
      if (JSON_Parser_ProcessInputBytes(parser, (const byte*)pBytes, length))
      {
         /* New input was parsed successfully. */
         if (isFinal)
         {
            /* Make sure there is nothing pending in the decoder, lexer,
               or parser. */
            if (JSON_Parser_FlushDecoder(parser) &&
                  JSON_Parser_FlushLexer(parser) &&
                  JSON_Parser_FlushParser(parser))
               status = JSON_Success;

            finishedParsing = 1;
         }
         else
            status = JSON_Success;
      }
      else
      {
         /* New input failed to parse. */
         finishedParsing = 1;
      }
      if (finishedParsing)
      {
         SET_FLAGS_ON(ParserState, parser->state, PARSER_FINISHED);
      }
      SET_FLAGS_OFF(ParserState, parser->state, PARSER_IN_PROTECTED_API);
   }
   return status;
}

#endif /* JSON_NO_PARSER */

/******************** JSON Writer ********************/

#ifndef JSON_NO_WRITER

/* Combinable writer state flags. */
#define WRITER_RESET            0x0
#define WRITER_STARTED          0x1
#define WRITER_IN_PROTECTED_API 0x2
typedef byte WriterState;

/* Combinable writer settings flags. */
#define WRITER_DEFAULT_FLAGS    0x0
#define WRITER_USE_CRLF         0x1
#define WRITER_REPLACE_INVALID  0x2
#define WRITER_ESCAPE_NON_ASCII 0x4
typedef byte WriterFlags;

/* A writer instance. */
struct JSON_Writer_Data
{
   JSON_MemorySuite          memorySuite;
   void*                     userData;
   WriterState               state;
   WriterFlags               flags;
   Encoding                  outputEncoding;
   Error                     error;
   GrammarianData            grammarianData;
   JSON_Writer_OutputHandler outputHandler;
};

/* Writer internal functions. */

static void JSON_Writer_ResetData(JSON_Writer writer, int isInitialized)
{
   writer->userData = NULL;
   writer->flags = WRITER_DEFAULT_FLAGS;
   writer->outputEncoding = JSON_UTF8;
   writer->error = JSON_Error_None;
   Grammarian_Reset(&writer->grammarianData, isInitialized);
   writer->outputHandler = NULL;
   writer->state = WRITER_RESET; /* do this last! */
}

static void JSON_Writer_SetError(JSON_Writer writer, Error error)
{
   writer->error = error;
}

static JSON_Status JSON_Writer_ProcessToken(JSON_Writer writer, Symbol token)
{
   GrammarianOutput output = Grammarian_ProcessToken(&writer->grammarianData, token, &writer->memorySuite);
   switch (GRAMMARIAN_RESULT_CODE(output))
   {
      case REJECTED_TOKEN:
         JSON_Writer_SetError(writer, JSON_Error_UnexpectedToken);
         return JSON_Failure;

      case SYMBOL_STACK_FULL:
         JSON_Writer_SetError(writer, JSON_Error_OutOfMemory);
         return JSON_Failure;
   }
   return JSON_Success;
}

static JSON_Status JSON_Writer_OutputBytes(JSON_Writer writer, const byte* pBytes, size_t length)
{
   if (writer->outputHandler && length)
   {
      if (writer->outputHandler(writer, (const char*)pBytes, length) != JSON_Writer_Continue)
      {
         JSON_Writer_SetError(writer, JSON_Error_AbortedByHandler);
         return JSON_Failure;
      }
   }
   return JSON_Success;
}

static Codepoint JSON_Writer_GetCodepointEscapeCharacter(JSON_Writer writer, Codepoint c)
{
   switch (c)
   {
      case BACKSPACE_CODEPOINT:
         return 'b';

      case TAB_CODEPOINT:
         return 't';

      case LINE_FEED_CODEPOINT:
         return 'n';

      case FORM_FEED_CODEPOINT:
         return 'f';

      case CARRIAGE_RETURN_CODEPOINT:
         return 'r';

      case '"':
         return '"';
      /* Don't escape forward slashes */
      /*case '/':
         return '/';*/

      case '\\':
         return '\\';

      case DELETE_CODEPOINT:
      case LINE_SEPARATOR_CODEPOINT:
      case PARAGRAPH_SEPARATOR_CODEPOINT:
         return 'u';

      default:
         if (c < FIRST_NON_CONTROL_CODEPOINT || IS_NONCHARACTER(c) ||
               (GET_FLAGS(writer->flags, WRITER_ESCAPE_NON_ASCII) && c > FIRST_NON_ASCII_CODEPOINT))
            return 'u';
         break;
   }
   return 0;
}

typedef struct tag_WriteBufferData
{
   size_t used;
   byte   bytes[256];
} WriteBufferData;
typedef WriteBufferData* WriteBuffer;

static void WriteBuffer_Reset(WriteBuffer buffer)
{
   buffer->used = 0;
}

static JSON_Status WriteBuffer_Flush(WriteBuffer buffer, JSON_Writer writer)
{
   JSON_Status status = JSON_Writer_OutputBytes(writer, buffer->bytes, buffer->used);
   buffer->used = 0;
   return status;
}

static JSON_Status WriteBuffer_WriteBytes(WriteBuffer buffer, JSON_Writer writer, const byte* pBytes, size_t length)
{
   if (buffer->used + length > sizeof(buffer->bytes) &&
         !WriteBuffer_Flush(buffer, writer))
      return JSON_Failure;

   memcpy(&buffer->bytes[buffer->used], pBytes, length);
   buffer->used += length;
   return JSON_Success;
}

static JSON_Status WriteBuffer_WriteCodepoint(WriteBuffer buffer, JSON_Writer writer, Codepoint c)
{
   if (buffer->used + LONGEST_ENCODING_SEQUENCE > sizeof(buffer->bytes) &&
         !WriteBuffer_Flush(buffer, writer))
      return JSON_Failure;

   buffer->used += EncodeCodepoint(c, writer->outputEncoding, &buffer->bytes[buffer->used]);
   return JSON_Success;
}

static JSON_Status WriteBuffer_WriteHexEscapeSequence(WriteBuffer buffer, JSON_Writer writer, Codepoint c)
{
   if (c >= FIRST_NON_BMP_CODEPOINT)
   {
      /* Non-BMP codepoints must be hex-escaped by escaping the UTF-16
         surrogate pair for the codepoint. We put the leading surrogate
         in the low 16 bits of c so that it gets written first, then
         the second pass through the loop will write out the trailing
         surrogate. x*/
      c = SURROGATES_FROM_CODEPOINT(c);
      c = (c << 16) | (c >> 16);
   }
   do
   {
      static const byte hexDigits[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
      byte escapeSequence[6];
      int i;
      escapeSequence[0] = '\\';
      escapeSequence[1] = 'u';
      escapeSequence[2] = hexDigits[(c >> 12) & 0xF];
      escapeSequence[3] = hexDigits[(c >> 8) & 0xF];
      escapeSequence[4] = hexDigits[(c >> 4) & 0xF];
      escapeSequence[5] = hexDigits[c & 0xF];
      for (i = 0; i < sizeof(escapeSequence); i++)
      {
         if (!WriteBuffer_WriteCodepoint(buffer, writer, escapeSequence[i]))
            return JSON_Failure;
      }
      c >>= 16;
   } while (c);
   return JSON_Success;
}

static JSON_Status JSON_Writer_OutputString(JSON_Writer writer, const byte* pBytes, size_t length, Encoding encoding)
{
   static const byte quoteUTF[] = { 0, 0, 0, '"', 0, 0, 0 };
   static const byte* const quoteEncodings[5] = { quoteUTF + 3, quoteUTF + 3, quoteUTF + 2, quoteUTF + 3, quoteUTF };

   const byte* pQuoteEncoded = quoteEncodings[writer->outputEncoding - 1];
   size_t minSequenceLength = (size_t)SHORTEST_ENCODING_SEQUENCE(writer->outputEncoding);
   DecoderData decoderData;
   WriteBufferData bufferData;
   size_t i = 0;

   WriteBuffer_Reset(&bufferData);

   /* Start quote. */
   if (!WriteBuffer_WriteBytes(&bufferData, writer, pQuoteEncoded, minSequenceLength))
      return JSON_Failure;

   /* String contents. */
   Decoder_Reset(&decoderData);
   while (i < length)
   {
      DecoderOutput output = Decoder_ProcessByte(&decoderData, encoding, pBytes[i]);
      DecoderResultCode result = DECODER_RESULT_CODE(output);
      Codepoint c;
      Codepoint escapeCharacter;
      switch (result)
      {
         case SEQUENCE_PENDING:
            i++;
            break;

         case SEQUENCE_COMPLETE:
            c = DECODER_CODEPOINT(output);
            escapeCharacter = JSON_Writer_GetCodepointEscapeCharacter(writer, c);
            switch (escapeCharacter)
            {
               case 0:
                  /* Output the codepoint as a normal encoding sequence. */
                  if (!WriteBuffer_WriteCodepoint(&bufferData, writer, c))
                     return JSON_Failure;
                  break;

               case 'u':
                  /* Output the codepoint as 1 or 2 hex escape sequences. */
                  if (!WriteBuffer_WriteHexEscapeSequence(&bufferData, writer, c))
                     return JSON_Failure;
                  break;

               default:
                  /* Output the codepoint as a simple escape sequence. */
                  if (!WriteBuffer_WriteCodepoint(&bufferData, writer, '\\') ||
                        !WriteBuffer_WriteCodepoint(&bufferData, writer, escapeCharacter))
                     return JSON_Failure;
                  break;
            }
            i++;
            break;

         case SEQUENCE_INVALID_INCLUSIVE:
            i++;
            /* fallthrough */
         case SEQUENCE_INVALID_EXCLUSIVE:
            if (GET_FLAGS(writer->flags, WRITER_REPLACE_INVALID))
            {
               if (!WriteBuffer_WriteHexEscapeSequence(&bufferData, writer, REPLACEMENT_CHARACTER_CODEPOINT))
                  return JSON_Failure;
            }
            else
            {
               /* Output whatever valid bytes we've accumulated before failing. */
               if (WriteBuffer_Flush(&bufferData, writer))
                  JSON_Writer_SetError(writer, JSON_Error_InvalidEncodingSequence);
               return JSON_Failure;
            }
            break;
      }
   }
   if (Decoder_SequencePending(&decoderData))
   {
      if (GET_FLAGS(writer->flags, WRITER_REPLACE_INVALID))
      {
         if (!WriteBuffer_WriteHexEscapeSequence(&bufferData, writer, REPLACEMENT_CHARACTER_CODEPOINT))
            return JSON_Failure;
      }
      else
      {
         /* Output whatever valid bytes we've accumulated before failing. */
         if (WriteBuffer_Flush(&bufferData, writer))
            JSON_Writer_SetError(writer, JSON_Error_InvalidEncodingSequence);
         return JSON_Failure;
      }
   }

   /* End quote. */
   if (!WriteBuffer_WriteBytes(&bufferData, writer, pQuoteEncoded, minSequenceLength) ||
         !WriteBuffer_Flush(&bufferData, writer))
      return JSON_Failure;
   return JSON_Success;
}

static LexerState LexNumberCharacter(LexerState state, Codepoint c)
{
   switch (state)
   {
      case LEXING_WHITESPACE:
         if (c == '-')
            state = LEXING_NUMBER_AFTER_MINUS;
         else if (c == '0')
            state = LEXING_NUMBER_AFTER_LEADING_ZERO;
         else if (c >= '1' && c <= '9')
            state = LEXING_NUMBER_DECIMAL_DIGITS;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_AFTER_MINUS:
         if (c == '0')
            state = LEXING_NUMBER_AFTER_LEADING_NEGATIVE_ZERO;
         else if (c >= '1' && c <= '9')
            state = LEXING_NUMBER_DECIMAL_DIGITS;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_AFTER_LEADING_ZERO:
      case LEXING_NUMBER_AFTER_LEADING_NEGATIVE_ZERO:
         if (c == '.')
            state = LEXING_NUMBER_AFTER_DOT;
         else if (c == 'e' || c == 'E')
            state = LEXING_NUMBER_AFTER_E;
         else if ((c == 'x' || c == 'X') && state == LEXING_NUMBER_AFTER_LEADING_ZERO)
            state = LEXING_NUMBER_AFTER_X;
         else if (c == EOF_CODEPOINT)
            state = LEXING_WHITESPACE;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_AFTER_X:
         if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
            state = LEXING_NUMBER_HEX_DIGITS;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_HEX_DIGITS:
         if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
         {
            /* Still LEXING_NUMBER_HEX_DIGITS. */
         }
         else if (c == EOF_CODEPOINT)
            state = LEXING_WHITESPACE;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_DECIMAL_DIGITS:
         if (c >= '0' && c <= '9')
         {
            /* Still LEXING_NUMBER_DECIMAL_DIGITS. */
         }
         else if (c == '.')
            state = LEXING_NUMBER_AFTER_DOT;
         else if (c == 'e' || c == 'E')
            state = LEXING_NUMBER_AFTER_E;
         else if (c == EOF_CODEPOINT)
            state = LEXING_WHITESPACE;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_AFTER_DOT:
         if (c >= '0' && c <= '9')
            state = LEXING_NUMBER_FRACTIONAL_DIGITS;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_FRACTIONAL_DIGITS:
         if (c >= '0' && c <= '9')
         {
            /* Still LEXING_NUMBER_FRACTIONAL_DIGITS. */
         }
         else if (c == 'e' || c == 'E')
            state = LEXING_NUMBER_AFTER_E;
         else if (c == EOF_CODEPOINT)
            state = LEXING_WHITESPACE;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_AFTER_E:
         if (c == '+' || c == '-')
            state = LEXING_NUMBER_AFTER_EXPONENT_SIGN;
         else if (c >= '0' && c <= '9')
            state = LEXING_NUMBER_EXPONENT_DIGITS;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_AFTER_EXPONENT_SIGN:
         if (c >= '0' && c <= '9')
            state = LEXING_NUMBER_EXPONENT_DIGITS;
         else
            state = LEXER_ERROR;
         break;

      case LEXING_NUMBER_EXPONENT_DIGITS:
         if (c >= '0' && c <= '9')
         {
            /* Still LEXING_NUMBER_EXPONENT_DIGITS. */
         }
         else if (c == EOF_CODEPOINT)
            state = LEXING_WHITESPACE;
         else
            state = LEXER_ERROR;
         break;
   }
   return state;
}

static JSON_Status JSON_Writer_OutputNumber(JSON_Writer writer, const byte* pBytes, size_t length, Encoding encoding)
{
   DecoderData decoderData;
   WriteBufferData bufferData;
   LexerState lexerState = LEXING_WHITESPACE;
   size_t i;
   Decoder_Reset(&decoderData);
   WriteBuffer_Reset(&bufferData);
   for (i = 0; i < length; i++)
   {
      DecoderOutput output = Decoder_ProcessByte(&decoderData, encoding, pBytes[i]);
      DecoderResultCode result = DECODER_RESULT_CODE(output);
      Codepoint c;
      switch (result)
      {
         case SEQUENCE_PENDING:
            break;

         case SEQUENCE_COMPLETE:
            c = DECODER_CODEPOINT(output);
            lexerState = LexNumberCharacter(lexerState, c);
            if (lexerState == LEXER_ERROR)
            {
               /* Output whatever valid bytes we've accumulated before failing. */
               if (WriteBuffer_Flush(&bufferData, writer))
                  JSON_Writer_SetError(writer, JSON_Error_InvalidNumber);
               return JSON_Failure;
            }
            if (!WriteBuffer_WriteCodepoint(&bufferData, writer, c))
               return JSON_Failure;
            break;

         case SEQUENCE_INVALID_INCLUSIVE:
         case SEQUENCE_INVALID_EXCLUSIVE:
            /* Output whatever valid bytes we've accumulated before failing. */
            if (WriteBuffer_Flush(&bufferData, writer))
               JSON_Writer_SetError(writer, JSON_Error_InvalidEncodingSequence);
            return JSON_Failure;
      }
   }
   if (!WriteBuffer_Flush(&bufferData, writer))
      return JSON_Failure;
   if (Decoder_SequencePending(&decoderData))
   {
      JSON_Writer_SetError(writer, JSON_Error_InvalidEncodingSequence);
      return JSON_Failure;
   }
   if (LexNumberCharacter(lexerState, EOF_CODEPOINT) == LEXER_ERROR)
   {
      JSON_Writer_SetError(writer, JSON_Error_InvalidNumber);
      return JSON_Failure;
   }
   return JSON_Success;
}

#define SPACES_PER_CHUNK 8
static JSON_Status JSON_Writer_OutputSpaces(JSON_Writer writer, size_t numberOfSpaces)
{
   static const byte spacesUTF8[SPACES_PER_CHUNK] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
   static const byte spacesUTF16[SPACES_PER_CHUNK * 2 + 1] = { 0, ' ', 0, ' ', 0, ' ', 0, ' ', 0, ' ', 0, ' ', 0, ' ', 0, ' ', 0 };
   static const byte spacesUTF32[SPACES_PER_CHUNK * 4 + 3] = { 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0, ' ', 0, 0, 0 };
   static const byte* const spacesEncodings[5] = { spacesUTF8, spacesUTF16 + 1, spacesUTF16, spacesUTF32 + 3, spacesUTF32 };

   size_t encodedLength = (size_t)SHORTEST_ENCODING_SEQUENCE(writer->outputEncoding);
   const byte* encoded = spacesEncodings[writer->outputEncoding - 1];
   while (numberOfSpaces > SPACES_PER_CHUNK)
   {
      if (!JSON_Writer_OutputBytes(writer, encoded, SPACES_PER_CHUNK * encodedLength))
         return JSON_Failure;
      numberOfSpaces -= SPACES_PER_CHUNK;
   }

   if (!JSON_Writer_OutputBytes(writer, encoded, numberOfSpaces * encodedLength))
      return JSON_Failure;
   return JSON_Success;
}

static JSON_Status JSON_Writer_WriteSimpleToken(JSON_Writer writer, Symbol token, const byte* const* encodings, size_t length)
{
   JSON_Status status = JSON_Failure;
   if (writer && !GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API) && writer->error == JSON_Error_None)
   {
      size_t encodedLength = length * (size_t)SHORTEST_ENCODING_SEQUENCE(writer->outputEncoding);
      SET_FLAGS_ON(WriterState, writer->state, WRITER_STARTED | WRITER_IN_PROTECTED_API);
      if (JSON_Writer_ProcessToken(writer, token) &&
            JSON_Writer_OutputBytes(writer, encodings[writer->outputEncoding - 1], encodedLength))
         status = JSON_Success;
      SET_FLAGS_OFF(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   }
   return status;
}

/* Writer API functions. */

JSON_Writer JSON_CALL JSON_Writer_Create(const JSON_MemorySuite* pMemorySuite)
{
   JSON_Writer writer;
   JSON_MemorySuite memorySuite;
   if (pMemorySuite)
   {
      memorySuite = *pMemorySuite;
      /* The full memory suite must be specified. */
      if (!memorySuite.realloc || !memorySuite.free)
         return NULL;
   }
   else
      memorySuite = defaultMemorySuite;

   writer = (JSON_Writer)memorySuite.realloc(memorySuite.userData, NULL, sizeof(struct JSON_Writer_Data));

   if (!writer)
      return NULL;

   writer->memorySuite = memorySuite;
   JSON_Writer_ResetData(writer, 0/* isInitialized */);
   return writer;
}

JSON_Status JSON_CALL JSON_Writer_Free(JSON_Writer writer)
{
   if (!writer || GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API))
      return JSON_Failure;

   SET_FLAGS_ON(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   Grammarian_FreeAllocations(&writer->grammarianData, &writer->memorySuite);
   writer->memorySuite.free(writer->memorySuite.userData, writer);
   return JSON_Success;
}

JSON_Status JSON_CALL JSON_Writer_Reset(JSON_Writer writer)
{
   if (!writer || GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API))
      return JSON_Failure;

   SET_FLAGS_ON(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   JSON_Writer_ResetData(writer, 1/* isInitialized */);
   /* Note that JSON_Writer_ResetData() unset WRITER_IN_PROTECTED_API for us. */
   return JSON_Success;
}

void* JSON_CALL JSON_Writer_GetUserData(JSON_Writer writer)
{
   return writer ? writer->userData : NULL;
}

JSON_Status JSON_CALL JSON_Writer_SetUserData(JSON_Writer writer, void* userData)
{
   if (!writer)
      return JSON_Failure;

   writer->userData = userData;
   return JSON_Success;
}

JSON_Encoding JSON_CALL JSON_Writer_GetOutputEncoding(JSON_Writer writer)
{
   return writer ? (JSON_Encoding)writer->outputEncoding : JSON_UTF8;
}

JSON_Status JSON_CALL JSON_Writer_SetOutputEncoding(JSON_Writer writer, JSON_Encoding encoding)
{
   if (!writer || GET_FLAGS(writer->state, WRITER_STARTED) || encoding <= JSON_UnknownEncoding || encoding > JSON_UTF32BE)
      return JSON_Failure;

   writer->outputEncoding = (Encoding)encoding;
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Writer_GetUseCRLF(JSON_Writer writer)
{
   return (writer && GET_FLAGS(writer->flags, WRITER_USE_CRLF)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Writer_SetUseCRLF(JSON_Writer writer, JSON_Boolean useCRLF)
{
   if (!writer || GET_FLAGS(writer->state, WRITER_STARTED))
      return JSON_Failure;

   SET_FLAGS(WriterFlags, writer->flags, WRITER_USE_CRLF, useCRLF);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Writer_GetReplaceInvalidEncodingSequences(JSON_Writer writer)
{
   return (writer && GET_FLAGS(writer->flags, WRITER_REPLACE_INVALID)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Writer_SetReplaceInvalidEncodingSequences(JSON_Writer writer, JSON_Boolean replaceInvalidEncodingSequences)
{
   if (!writer || GET_FLAGS(writer->state, WRITER_STARTED))
      return JSON_Failure;

   SET_FLAGS(WriterFlags, writer->flags, WRITER_REPLACE_INVALID, replaceInvalidEncodingSequences);
   return JSON_Success;
}

JSON_Boolean JSON_CALL JSON_Writer_GetEscapeAllNonASCIICharacters(JSON_Writer writer)
{
   return (writer && GET_FLAGS(writer->flags, WRITER_ESCAPE_NON_ASCII)) ? JSON_True : JSON_False;
}

JSON_Status JSON_CALL JSON_Writer_SetEscapeAllNonASCIICharacters(JSON_Writer writer, JSON_Boolean escapeAllNonASCIICharacters)
{
   if (!writer || GET_FLAGS(writer->state, WRITER_STARTED))
      return JSON_Failure;

   SET_FLAGS(WriterFlags, writer->flags, WRITER_ESCAPE_NON_ASCII, escapeAllNonASCIICharacters);
   return JSON_Success;
}

JSON_Error JSON_CALL JSON_Writer_GetError(JSON_Writer writer)
{
   return writer ? (JSON_Error)writer->error : JSON_Error_None;
}

JSON_Writer_OutputHandler JSON_CALL JSON_Writer_GetOutputHandler(JSON_Writer writer)
{
   return writer ? writer->outputHandler : NULL;
}

JSON_Status JSON_CALL JSON_Writer_SetOutputHandler(JSON_Writer writer, JSON_Writer_OutputHandler handler)
{
   if (!writer)
      return JSON_Failure;

   writer->outputHandler = handler;
   return JSON_Success;
}

JSON_Status JSON_CALL JSON_Writer_WriteNull(JSON_Writer writer)
{
   static const byte nullUTF8[] = { 'n', 'u', 'l', 'l' };
   static const byte nullUTF16[] = { 0, 'n', 0, 'u', 0, 'l', 0, 'l', 0 };
   static const byte nullUTF32[] = { 0, 0, 0, 'n', 0, 0, 0, 'u', 0, 0, 0, 'l', 0, 0, 0, 'l', 0, 0, 0 };
   static const byte* const nullEncodings[5] = { nullUTF8, nullUTF16 + 1, nullUTF16, nullUTF32 + 3, nullUTF32 };

   return JSON_Writer_WriteSimpleToken(writer, T_NULL, nullEncodings, sizeof(nullUTF8));
}

JSON_Status JSON_CALL JSON_Writer_WriteBoolean(JSON_Writer writer, JSON_Boolean value)
{
   static const byte trueUTF8[] = { 't', 'r', 'u', 'e' };
   static const byte trueUTF16[] = { 0, 't', 0, 'r', 0, 'u', 0, 'e', 0 };
   static const byte trueUTF32[] = { 0, 0, 0, 't', 0, 0, 0, 'r', 0, 0, 0, 'u', 0, 0, 0, 'e', 0, 0, 0 };
   static const byte* const trueEncodings[5] = { trueUTF8, trueUTF16 + 1, trueUTF16, trueUTF32 + 3, trueUTF32 };

   static const byte falseUTF8[] = { 'f', 'a', 'l', 's', 'e' };
   static const byte falseUTF16[] = { 0, 'f', 0, 'a', 0, 'l', 0, 's', 0, 'e', 0 };
   static const byte falseUTF32[] = { 0, 0, 0, 'f', 0, 0, 0, 'a', 0, 0, 0, 'l', 0, 0, 0, 's', 0, 0, 0, 'e', 0, 0, 0 };
   static const byte* const falseEncodings[5] = { falseUTF8, falseUTF16 + 1, falseUTF16, falseUTF32 + 3, falseUTF32 };

   Symbol token;
   const byte* const* encodings;
   size_t length;
   if (value)
   {
      token = T_TRUE;
      encodings = trueEncodings;
      length = sizeof(trueUTF8);
   }
   else
   {
      token = T_FALSE;
      encodings = falseEncodings;
      length = sizeof(falseUTF8);
   }
   return JSON_Writer_WriteSimpleToken(writer, token, encodings, length);
}

JSON_Status JSON_CALL JSON_Writer_WriteString(JSON_Writer writer, const char* pValue, size_t length, JSON_Encoding encoding)
{
   JSON_Status status = JSON_Failure;
   if (writer && (pValue || !length) && encoding > JSON_UnknownEncoding && encoding <= JSON_UTF32BE &&
         !GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API) && writer->error == JSON_Error_None)
   {
      SET_FLAGS_ON(WriterState, writer->state, WRITER_STARTED | WRITER_IN_PROTECTED_API);
      if (JSON_Writer_ProcessToken(writer, T_STRING))
         status = JSON_Writer_OutputString(writer, (const byte*)pValue, length, (Encoding)encoding);

      SET_FLAGS_OFF(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   }
   return status;
}

JSON_Status JSON_CALL JSON_Writer_WriteNumber(JSON_Writer writer, const char* pValue, size_t length, JSON_Encoding encoding)
{
   JSON_Status status = JSON_Failure;
   if (writer && pValue && length && encoding > JSON_UnknownEncoding && encoding <= JSON_UTF32BE &&
         !GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API) && writer->error == JSON_Error_None)
   {
      SET_FLAGS_ON(WriterState, writer->state, WRITER_STARTED | WRITER_IN_PROTECTED_API);
      if (JSON_Writer_ProcessToken(writer, T_NUMBER))
         status = JSON_Writer_OutputNumber(writer, (const byte*)pValue, length, (Encoding)encoding);

      SET_FLAGS_OFF(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   }
   return status;
}

JSON_Status JSON_CALL JSON_Writer_WriteSpecialNumber(JSON_Writer writer, JSON_SpecialNumber value)
{
   static const byte nanUTF8[] = { 'N', 'a', 'N' };
   static const byte nanUTF16[] = { 0, 'N', 0, 'a', 0, 'N', 0 };
   static const byte nanUTF32[] = { 0, 0, 0, 'N', 0, 0, 0, 'a', 0, 0, 0, 'N', 0, 0, 0 };
   static const byte* const nanEncodings[5] = { nanUTF8, nanUTF16 + 1, nanUTF16, nanUTF32 + 3, nanUTF32 };

   static const byte ninfUTF8[] = { '-', 'I', 'n', 'f', 'i', 'n', 'i', 't', 'y' };
   static const byte ninfUTF16[] = { 0, '-', 0, 'I', 0, 'n', 0, 'f', 0, 'i', 0, 'n', 0, 'i', 0, 't', 0, 'y', 0 };
   static const byte ninfUTF32[] = { 0, 0, 0, '-', 0, 0, 0, 'I', 0, 0, 0, 'n', 0, 0, 0, 'f', 0, 0, 0, 'i', 0, 0, 0, 'n', 0, 0, 0, 'i', 0, 0, 0, 't', 0, 0, 0, 'y', 0, 0, 0 };
   static const byte* const infinityEncodings[5] = { ninfUTF8 + 1, ninfUTF16 + 3, ninfUTF16 + 2, ninfUTF32 + 7, ninfUTF32 + 4 };
   static const byte* const negativeInfinityEncodings[5] = { ninfUTF8, ninfUTF16 + 1, ninfUTF16, ninfUTF32 + 3, ninfUTF32 };

   Symbol token;
   const byte* const* encodings;
   size_t length;
   if (value == JSON_Infinity)
   {
      token = T_INFINITY;
      encodings = infinityEncodings;
      length = sizeof(ninfUTF8) - 1/* - */;
   }
   else if (value == JSON_NegativeInfinity)
   {
      token = T_NEGATIVE_INFINITY;
      encodings = negativeInfinityEncodings;
      length = sizeof(ninfUTF8);
   }
   else
   {
      token = T_NAN;
      encodings = nanEncodings;
      length = sizeof(nanUTF8);
   }
   return JSON_Writer_WriteSimpleToken(writer, token, encodings, length);
}

JSON_Status JSON_CALL JSON_Writer_WriteStartObject(JSON_Writer writer)
{
   static const byte utf[] = { 0, 0, 0, '{', 0, 0, 0 };
   static const byte* const encodings[5] = { utf + 3, utf + 3, utf + 2, utf + 3, utf };

   return JSON_Writer_WriteSimpleToken(writer, T_LEFT_CURLY, encodings, 1);
}

JSON_Status JSON_CALL JSON_Writer_WriteEndObject(JSON_Writer writer)
{
   static const byte utf[] = { 0, 0, 0, '}', 0, 0, 0 };
   static const byte* const encodings[5] = { utf + 3, utf + 3, utf + 2, utf + 3, utf };

   return JSON_Writer_WriteSimpleToken(writer, T_RIGHT_CURLY, encodings, 1);
}

JSON_Status JSON_CALL JSON_Writer_WriteStartArray(JSON_Writer writer)
{
   static const byte utf[] = { 0, 0, 0, '[', 0, 0, 0 };
   static const byte* const encodings[5] = { utf + 3, utf + 3, utf + 2, utf + 3, utf };

   return JSON_Writer_WriteSimpleToken(writer, T_LEFT_SQUARE, encodings, 1);
}

JSON_Status JSON_CALL JSON_Writer_WriteEndArray(JSON_Writer writer)
{
   static const byte utf[] = { 0, 0, 0, ']', 0, 0, 0 };
   static const byte* const encodings[5] = { utf + 3, utf + 3, utf + 2, utf + 3, utf };

   return JSON_Writer_WriteSimpleToken(writer, T_RIGHT_SQUARE, encodings, 1);
}

JSON_Status JSON_CALL JSON_Writer_WriteColon(JSON_Writer writer)
{
   static const byte utf[] = { 0, 0, 0, ':', 0, 0, 0 };
   static const byte* const encodings[5] = { utf + 3, utf + 3, utf + 2, utf + 3, utf };

   return JSON_Writer_WriteSimpleToken(writer, T_COLON, encodings, 1);
}

JSON_Status JSON_CALL JSON_Writer_WriteComma(JSON_Writer writer)
{
   static const byte utf[] = { 0, 0, 0, ',', 0, 0, 0 };
   static const byte* const encodings[5] = { utf + 3, utf + 3, utf + 2, utf + 3, utf };

   return JSON_Writer_WriteSimpleToken(writer, T_COMMA, encodings, 1);
}

JSON_Status JSON_CALL JSON_Writer_WriteSpace(JSON_Writer writer, size_t numberOfSpaces)
{
   JSON_Status status = JSON_Failure;
   if (writer && !GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API) && writer->error == JSON_Error_None)
   {
      SET_FLAGS_ON(WriterState, writer->state, WRITER_STARTED | WRITER_IN_PROTECTED_API);
      status = JSON_Writer_OutputSpaces(writer, numberOfSpaces);
      SET_FLAGS_OFF(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   }
   return status;
}

JSON_Status JSON_CALL JSON_Writer_WriteNewLine(JSON_Writer writer)
{
   static const byte lfUTF[] = { 0, 0, 0, LINE_FEED_CODEPOINT, 0, 0, 0 };
   static const byte* const lfEncodings[5] = { lfUTF + 3, lfUTF + 3, lfUTF + 2, lfUTF + 3, lfUTF };

   static const byte crlfUTF8[] = { CARRIAGE_RETURN_CODEPOINT, LINE_FEED_CODEPOINT };
   static const byte crlfUTF16[] = { 0, CARRIAGE_RETURN_CODEPOINT, 0, LINE_FEED_CODEPOINT, 0 };
   static const byte crlfUTF32[] = { 0, 0, 0, CARRIAGE_RETURN_CODEPOINT, 0, 0, 0, LINE_FEED_CODEPOINT, 0, 0, 0 };
   static const byte* const crlfEncodings[5] = { crlfUTF8, crlfUTF16 + 1, crlfUTF16, crlfUTF32 + 3, crlfUTF32 };

   JSON_Status status = JSON_Failure;
   if (writer && !GET_FLAGS(writer->state, WRITER_IN_PROTECTED_API) && writer->error == JSON_Error_None)
   {
      const byte* const* encodings;
      size_t length;
      size_t encodedLength;
      SET_FLAGS_ON(WriterState, writer->state, WRITER_STARTED | WRITER_IN_PROTECTED_API);
      if (GET_FLAGS(writer->flags, WRITER_USE_CRLF))
      {
         encodings = crlfEncodings;
         length = 2;
      }
      else
      {
         encodings = lfEncodings;
         length = 1;
      }
      encodedLength = length * (size_t)SHORTEST_ENCODING_SEQUENCE(writer->outputEncoding);
      if (JSON_Writer_OutputBytes(writer, encodings[writer->outputEncoding - 1], encodedLength))
         status = JSON_Success;
      SET_FLAGS_OFF(WriterState, writer->state, WRITER_IN_PROTECTED_API);
   }
   return status;
}

#endif /* JSON_NO_WRITER */

/******************** Miscellaneous API ********************/

const JSON_Version* JSON_CALL JSON_LibraryVersion(void)
{
   static JSON_Version version = { JSON_MAJOR_VERSION, JSON_MINOR_VERSION, JSON_MICRO_VERSION };
   return &version;
}

const char* JSON_CALL JSON_ErrorString(JSON_Error error)
{
   /* This array must match the order and number of the JSON_Error enum. */
   static const char* errorStrings[] =
   {
      /* JSON_Error_None */                            "no error",
      /* JSON_Error_OutOfMemory */                     "could not allocate enough memory",
      /* JSON_Error_AbortedByHandler */                "the operation was aborted by a handler",
      /* JSON_Error_BOMNotAllowed */                   "the input begins with a byte-order mark (BOM), which is not allowed by RFC 4627",
      /* JSON_Error_InvalidEncodingSequence */         "the input contains a byte or sequence of bytes that is not valid for the input encoding",
      /* JSON_Error_UnknownToken */                    "the input contains an unknown token",
      /* JSON_Error_UnexpectedToken */                 "the input contains an unexpected token",
      /* JSON_Error_IncompleteToken */                 "the input ends in the middle of a token",
      /* JSON_Error_MoreTokensExpected */              "the input ends when more tokens are expected",
      /* JSON_Error_UnescapedControlCharacter */       "the input contains a string containing an unescaped control character (U+0000 - U+001F)",
      /* JSON_Error_InvalidEscapeSequence */           "the input contains a string containing an invalid escape sequence",
      /* JSON_Error_UnpairedSurrogateEscapeSequence */ "the input contains a string containing an unmatched UTF-16 surrogate codepoint",
      /* JSON_Error_TooLongString */                   "the input contains a string that is too long",
      /* JSON_Error_InvalidNumber */                   "the input contains an invalid number",
      /* JSON_Error_TooLongNumber */                   "the input contains a number that is too long",
      /* JSON_Error_DuplicateObjectMember */           "the input contains an object with duplicate members",
      /* JSON_Error_StoppedAfterEmbeddedDocument */    "the end of the embedded document was reached"
   };
   return ((unsigned int)error < (sizeof(errorStrings) / sizeof(errorStrings[0])))
      ? errorStrings[error]
      : "";
}

static const uint32_t endianEncodings = (((uint32_t)JSON_UTF32BE) << 24) | (((uint32_t)JSON_UTF16BE) << 16) | (((uint32_t)JSON_UTF16LE) << 8) | ((uint32_t)JSON_UTF32LE);

JSON_Encoding JSON_CALL JSON_NativeUTF16Encoding(void)
{
   return (JSON_Encoding)(((byte*)&endianEncodings)[1]);
}

JSON_Encoding JSON_CALL JSON_NativeUTF32Encoding(void)
{
   return (JSON_Encoding)(((byte*)&endianEncodings)[0]);
}
