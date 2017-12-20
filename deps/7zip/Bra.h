/* Bra.h -- Branch converters for executables
   2009-02-07 : Igor Pavlov : Public domain */

#ifndef __BRA_H
#define __BRA_H

#include "7zTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

   /*
      These functions convert relative addresses to absolute addresses
      in CALL instructions to increase the compression ratio.

In:
data     - data buffer
size     - size of data
ip       - current virtual Instruction Pinter (IP) value
state    - state variable for x86 converter
encoding - 0 (for decoding), 1 (for encoding)

Out:
state    - state variable for x86 converter

Returns:
The number of processed bytes. If you call these functions with multiple calls,
you must start next call with first byte after block of processed bytes.

Type   Endian  Alignment  LookAhead

x86    little      1          4
ARMT   little      2          2
ARM    little      4          0
PPC     big        4          0
SPARC   big        4          0
IA64   little     16          0

size must be >= Alignment + LookAhead, if it's not last block.
If (size < Alignment + LookAhead), converter returns 0.

Example:

uint32_t ip = 0;
for ()
{
; size must be >= Alignment + LookAhead, if it's not last block
size_t processed = Convert(data, size, ip, 1);
data += processed;
size -= processed;
ip += processed;
}
*/

#define x86_Convert_Init(state) { state = 0; }
size_t x86_Convert(uint8_t *data, size_t size, uint32_t ip, uint32_t *state, int encoding);
size_t ARM_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding);
size_t ARMT_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding);
size_t PPC_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding);
size_t SPARC_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding);
size_t IA64_Convert(uint8_t *data, size_t size, uint32_t ip, int encoding);

#ifdef __cplusplus
}
#endif

#endif
