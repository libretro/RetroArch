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

#ifndef MMAL_UTIL_RATIONAL_H
#define MMAL_UTIL_RATIONAL_H

#include "interface/mmal/mmal_types.h"

/** \defgroup MmalRationalUtilities Rational Utility Functions
 * \ingroup MmalUtilities
 * The rational utility functions allow easy manipulation of rational numbers.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Add 2 rational numbers.
 * It is assumed that both input rational numbers are in
 * their simplest form.
 *
 * @param a First operand
 * @param b Second operand
 *
 * @return a + b
 */
MMAL_RATIONAL_T mmal_rational_add(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b);

/** Subtract 2 rational numbers.
 * It is assumed that both input rational numbers are in
 * their simplest form.
 *
 * @param a        First operand
 * @param b        Second operand
 *
 * @return a - b
 */
MMAL_RATIONAL_T mmal_rational_subtract(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b);

/** Multiply 2 rational numbers.
 * It is assumed that both input rational numbers are in
 * their simplest form.
 *
 * @param a        First operand
 * @param b        Second operand
 *
 * @return a * b
 */
MMAL_RATIONAL_T mmal_rational_multiply(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b);

/** Divide 2 rational numbers.
 * It is assumed that both input rational numbers are in
 * their simplest form.
 *
 * @param a        First operand
 * @param b        Second operand
 *
 * @return a / b
 */
MMAL_RATIONAL_T mmal_rational_divide(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b);

/** Convert a rational number to a 32-bit signed Q16 number.
 * Saturation will occur for rational numbers with an absolute
 * value greater than 32768.
 *
 * @param rational Rational number to convert
 *
 * @return 32-bit signed Q16 number
 */
int32_t mmal_rational_to_fixed_16_16(MMAL_RATIONAL_T rational);

/** Convert a signed 32-bit Q16 number to a rational number.
 *
 * @param fixed    Signed 32-bit Q16 number to convert
 *
 * @return Rational number
 */
MMAL_RATIONAL_T mmal_rational_from_fixed_16_16(int32_t fixed);

/** Reduce a rational number to it's simplest form.
 *
 * @param rational Rational number to simplify
 */
void mmal_rational_simplify(MMAL_RATIONAL_T *rational);

/** Test 2 rational numbers for equality.
 *
 * @param a        First operand
 * @param b        Second operand
 *
 * @return true if equal
 */
MMAL_BOOL_T mmal_rational_equal(MMAL_RATIONAL_T a, MMAL_RATIONAL_T b);

#ifdef __cplusplus
}
#endif

/** @} */

#endif
