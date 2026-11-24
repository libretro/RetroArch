/*
  https://github.com/superwills/NibbleAndAHalf
  base64.h -- Fast base64 encoding and decoding.
  version 1.0.0, April 17, 2013 143a
  Copyright (C) 2013 William Sherif
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
  William Sherif
  will.sherif@gmail.com
  YWxsIHlvdXIgYmFzZSBhcmUgYmVsb25nIHRvIHVz


  Modified for RetroArch formatting, logging, and header files.
*/


#include <stdio.h>
#include <stdlib.h>
#include <encodings/base64.h>

static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* maps A=>0,B=>1.. */
static const unsigned char unb64[]={
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,  62,   0,   0,   0,  63,  52,  53,
 54,  55,  56,  57,  58,  59,  60,  61,   0,   0,
  0,   0,   0,   0,   0,   0,   1,   2,   3,   4,
  5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
 15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
 25,   0,   0,   0,   0,   0,   0,  26,  27,  28,
 29,  30,  31,  32,  33,  34,  35,  36,  37,  38,
 39,  40,  41,  42,  43,  44,  45,  46,  47,  48,
 49,  50,  51,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,
}; /* This array has 256 elements */

/*
   Converts binary data of length=len to base64 characters.
   Length of the resultant string is stored in flen
   (you must pass pointer flen).
*/
char* base64(const void* binaryData, int len, int *flen)
{
   char* res;
   int byteNo; /* I need this after the loop */
   const unsigned char* bin = (const unsigned char*) binaryData;
   int rc                   = 0; /* result counter */
   int modulusLen           = len % 3 ;
   /* 2 gives 1 and 1 gives 2, but 0 gives 0. */
   int pad                  = ((modulusLen&1)<<1) + ((modulusLen&2)>>1);

   *flen                    = 4*(len + pad)/3;
   if (!(res = (char*) malloc(*flen + 1))) /* and one for the NULL */
      return 0;
  
   for (byteNo=0; byteNo <= len-3; byteNo+=3)
   {
      unsigned char BYTE0            = bin[byteNo];
      unsigned char BYTE1            = bin[byteNo+1];
      unsigned char BYTE2            = bin[byteNo+2];

      res[rc++] = b64[BYTE0 >> 2];
      res[rc++] = b64[((0x3&BYTE0)<<4) + (BYTE1 >> 4)];
      res[rc++] = b64[((0x0f&BYTE1)<<2) + (BYTE2>>6)];
      res[rc++] = b64[0x3f&BYTE2];
   }
  
   if (pad==2)
   {
      res[rc++] = b64[bin[byteNo] >> 2];
      res[rc++] = b64[(0x3&bin[byteNo])<<4];
      res[rc++] = '=';
      res[rc++] = '=';
   }
   else if (pad==1)
   {
      res[rc++] = b64[bin[byteNo] >> 2];
      res[rc++] = b64[((0x3&bin[byteNo])<<4) + (bin[byteNo+1] >> 4)];
      res[rc++] = b64[(0x0f&bin[byteNo+1])<<2];
      res[rc++] = '=';
   }
  
   res[rc]=0; /* NULL TERMINATOR! ;) */
   return res;
}

unsigned char* unbase64(const char* ascii, int len, int *flen)
{
   int charNo;
   unsigned char *bin;
   const unsigned char *safeAsciiPtr = (const unsigned char*) ascii;
   int cb                            = 0;
   int pad                           = 0;

   /* Valid base64 has length that is a non-zero multiple of 4.
    * Shorter or misaligned inputs are malformed and have historically
    * caused 1-byte heap overflows when pad > 0 (e.g. "AB="). */
   if (len < 4 || (len & 3) != 0)
   {
      *flen = 0;
      return 0;
   }

   if (safeAsciiPtr[len-1]=='=')
      ++pad;
   if (safeAsciiPtr[len-2]=='=')
      ++pad;
  
   *flen = 3*len/4 - pad;
   if (!(bin = (unsigned char*)malloc(*flen)))
      return 0;
  
   for (charNo=0; charNo <= len-4-pad; charNo+=4)
   {
      int A = unb64[safeAsciiPtr[charNo]];
      int B = unb64[safeAsciiPtr[charNo+1]];
      int C = unb64[safeAsciiPtr[charNo+2]];
      int D = unb64[safeAsciiPtr[charNo+3]];
    
      bin[cb++] = (A<<2) | (B>>4);
      bin[cb++] = (B<<4) | (C>>2);
      bin[cb++] = (C<<6) | (D);
   }
  
   if (pad==1)
   {
      int A = unb64[safeAsciiPtr[charNo]];
      int B = unb64[safeAsciiPtr[charNo+1]];
      int C = unb64[safeAsciiPtr[charNo+2]];
    
      bin[cb++] = (A<<2) | (B>>4);
      bin[cb++] = (B<<4) | (C>>2);
   }
   else if (pad==2)
   {
      int A = unb64[safeAsciiPtr[charNo]];
      int B = unb64[safeAsciiPtr[charNo+1]];
    
      bin[cb++] = (A<<2) | (B>>4);
   }
  
   return bin;
}

/* unbase64_into(): 0-63 a sextet, 64 whitespace (skipped), 128 '=' or
 * a character outside the alphabet (the end of the run). */
static const unsigned char unb64_into[256] = {
  128,128,128,128,128,128,128,128,128, 64, 64,128,128, 64,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
   64,128,128,128,128,128,128,128,128,128,128, 62,128,128,128, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,128,128,128,128,128,128,
  128,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,128,128,128,128,128,
  128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
  128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
};

size_t unbase64_into(const char *ascii, size_t len, unsigned char *out)
{
   const unsigned char *in  = (const unsigned char*)ascii;
   const unsigned char *end = in + len;
   unsigned char       *o   = out;
   unsigned long        acc = 0;
   unsigned             n   = 0;

   /* Each group of three is written after its fourth character is
    * read, so with @out at or before @ascii the writes stay behind
    * the reads. */
   for (; in < end; in++)
   {
      unsigned v = unb64_into[*in];
      if (v & 64)
         continue;
      if (v & 128)
         break;
      acc = (acc << 6) | v;
      if (++n == 4)
      {
         o[0] = (unsigned char)(acc >> 16);
         o[1] = (unsigned char)(acc >> 8);
         o[2] = (unsigned char)acc;
         o   += 3;
         acc  = 0;
         n    = 0;
      }
   }
   if (in < end && *in != '=')
      return (size_t)-1;
   switch (n)
   {
      case 1:
         return (size_t)-1;
      case 2:
         *o++ = (unsigned char)(acc >> 4);
         break;
      case 3:
         o[0] = (unsigned char)(acc >> 10);
         o[1] = (unsigned char)(acc >> 2);
         o   += 2;
         break;
   }
   return (size_t)(o - out);
}
