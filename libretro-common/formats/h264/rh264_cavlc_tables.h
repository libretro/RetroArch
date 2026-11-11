/* SPDX-License-Identifier: BSD-2-Clause AND MIT
 *
 * The CAVLC tables, adapted from OpenH264 (see README.licenses):
 *
 * Copyright (c) 2013, Cisco Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The 4:2:2 chroma DC tables (H.264 Tables 9-5 and 9-9):
 *
 * Copyright (C) 2026 The RetroArch team, under the MIT terms in rh264.c.
 */

/* H.264 CAVLC code tables, {len, code} each, shared by rh264 and
 * rh264_encode:
 *   rh264_ct_*[table][total_coeff][trailing_ones]  coeff_token, the
 *       table by nC: 0-1, 2-3, 4-7, 8 and up (a 6-bit code)
 *   rh264_ctc_*[total_coeff][trailing_ones]        4:2:0 chroma DC (nC -1)
 *   rh264_tz_*[total_coeff][total_zeros]           4x4 blocks
 *   rh264_rb_*[min(zeros_left, 7)][run_before]
 *   rh264_ctc422_*, rh264_cdctz422_*               4:2:2 chroma DC
 *   rh264_cdctz_*[total_coeff][total_zeros]        4:2:0 chroma DC */

#ifndef __RH264_CAVLC_TABLES_H
#define __RH264_CAVLC_TABLES_H

#include <stdint.h>

/* Base CAVLC tables adapted from OpenH264 encoder tables (BSD-2-Clause);
 * verified prefix-free. All {len,code}. */
static const uint8_t rh264_ct_len[4][17][4]={
 {{1,0,0,0},{6,2,0,0},{8,6,3,0},{9,8,7,5},{10,9,8,6},{11,10,9,7},{13,11,10,8},{13,13,11,9},{13,13,13,10},{14,14,13,11},{14,14,14,13},{15,15,14,14},{15,15,15,14},{16,15,15,15},{16,16,16,15},{16,16,16,16},{16,16,16,16},},
 {{2,0,0,0},{6,2,0,0},{6,5,3,0},{7,6,6,4},{8,6,6,4},{8,7,7,5},{9,8,8,6},{11,9,9,6},{11,11,11,7},{12,11,11,9},{12,12,12,11},{12,12,12,11},{13,13,13,12},{13,13,13,13},{13,14,13,13},{14,14,14,13},{14,14,14,14},},
 {{4,0,0,0},{6,4,0,0},{6,5,4,0},{6,5,5,4},{7,5,5,4},{7,5,5,4},{7,6,6,4},{7,6,6,4},{8,7,7,5},{8,8,7,6},{9,8,8,7},{9,9,8,8},{9,9,9,8},{10,9,9,9},{10,10,10,10},{10,10,10,10},{10,10,10,10},},
 {{6,0,0,0},{6,6,0,0},{6,6,6,0},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},{6,6,6,6},},
};
static const uint8_t rh264_ct_code[4][17][4]={
 {{1,0,0,0},{5,1,0,0},{7,4,1,0},{7,6,5,3},{7,6,5,3},{7,6,5,4},{15,6,5,4},{11,14,5,4},{8,10,13,4},{15,14,9,4},{11,10,13,12},{15,14,9,12},{11,10,13,8},{15,1,9,12},{11,14,13,8},{7,10,9,12},{4,6,5,8},},
 {{3,0,0,0},{11,2,0,0},{7,7,3,0},{7,10,9,5},{7,6,5,4},{4,6,5,6},{7,6,5,8},{15,6,5,4},{11,14,13,4},{15,10,9,4},{11,14,13,12},{8,10,9,8},{15,14,13,12},{11,10,9,12},{7,11,6,8},{9,8,10,1},{7,6,5,4},},
 {{15,0,0,0},{15,14,0,0},{11,15,13,0},{8,12,14,12},{15,10,11,11},{11,8,9,10},{9,14,13,9},{8,10,9,8},{15,14,13,13},{11,14,10,12},{15,10,13,12},{11,14,9,12},{8,10,13,8},{13,7,9,12},{9,12,11,10},{5,8,7,6},{1,4,3,2},},
 {{3,0,0,0},{0,1,0,0},{4,5,6,0},{8,9,10,11},{12,13,14,15},{16,17,18,19},{20,21,22,23},{24,25,26,27},{28,29,30,31},{32,33,34,35},{36,37,38,39},{40,41,42,43},{44,45,46,47},{48,49,50,51},{52,53,54,55},{56,57,58,59},{60,61,62,63},},
};
static const uint8_t rh264_ctc_len[17][4]={
 {2,0,0,0},
 {6,1,0,0},
 {6,6,3,0},
 {6,7,7,6},
 {6,8,8,7},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
};
static const uint8_t rh264_ctc_code[17][4]={
 {1,0,0,0},
 {7,1,0,0},
 {4,6,1,0},
 {3,3,2,5},
 {2,3,2,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
 {0,0,0,0},
};
static const uint8_t rh264_tz_len[16][16]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,3,3,4,4,5,5,6,6,7,7,8,8,9,9,9},
 {3,3,3,3,3,4,4,4,4,5,5,6,6,6,6,0},
 {4,3,3,3,4,4,3,3,4,5,5,6,5,6,0,0},
 {5,3,4,4,3,3,3,4,3,4,5,5,5,0,0,0},
 {4,4,4,3,3,3,3,3,4,5,4,5,0,0,0,0},
 {6,5,3,3,3,3,3,3,4,3,6,0,0,0,0,0},
 {6,5,3,3,3,2,3,4,3,6,0,0,0,0,0,0},
 {6,4,5,3,2,2,3,3,6,0,0,0,0,0,0,0},
 {6,6,4,2,2,3,2,5,0,0,0,0,0,0,0,0},
 {5,5,3,2,2,2,4,0,0,0,0,0,0,0,0,0},
 {4,4,3,3,1,3,0,0,0,0,0,0,0,0,0,0},
 {4,4,2,1,3,0,0,0,0,0,0,0,0,0,0,0},
 {3,3,1,2,0,0,0,0,0,0,0,0,0,0,0,0},
 {2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
static const uint8_t rh264_tz_code[16][16]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,3,2,3,2,3,2,3,2,3,2,3,2,3,2,1},
 {7,6,5,4,3,5,4,3,2,3,2,3,2,1,0,0},
 {5,7,6,5,4,3,4,3,2,3,2,1,1,0,0,0},
 {3,7,5,4,6,5,4,3,3,2,2,1,0,0,0,0},
 {5,4,3,7,6,5,4,3,2,1,1,0,0,0,0,0},
 {1,1,7,6,5,4,3,2,1,1,0,0,0,0,0,0},
 {1,1,5,4,3,3,2,1,1,0,0,0,0,0,0,0},
 {1,1,1,3,3,2,2,1,0,0,0,0,0,0,0,0},
 {1,0,1,3,2,1,1,1,0,0,0,0,0,0,0,0},
 {1,0,1,3,2,1,1,0,0,0,0,0,0,0,0,0},
 {0,1,1,2,1,3,0,0,0,0,0,0,0,0,0,0},
 {0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0},
 {0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};
static const uint8_t rh264_rb_len[8][15]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,2,2,0,0,0,0,0,0,0,0,0,0,0,0},
 {2,2,2,2,0,0,0,0,0,0,0,0,0,0,0},
 {2,2,2,3,3,0,0,0,0,0,0,0,0,0,0},
 {2,2,3,3,3,3,0,0,0,0,0,0,0,0,0},
 {2,3,3,3,3,3,3,0,0,0,0,0,0,0,0},
 {3,3,3,3,3,3,3,4,5,6,7,8,9,10,11},
};
static const uint8_t rh264_rb_code[8][15]={
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {3,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
 {3,2,1,1,0,0,0,0,0,0,0,0,0,0,0},
 {3,2,3,2,1,0,0,0,0,0,0,0,0,0,0},
 {3,0,1,3,2,5,4,0,0,0,0,0,0,0,0},
 {7,6,5,4,3,2,1,1,1,1,1,1,1,1,1},
};
/* coeff_token for the chroma DC block of a 4:2:2 macroblock (nC == -2,
 * eight coefficients).  Indexed [trailing_ones][total_coeff]. */
/* H.264 Tables 9-5 and 9-9: 4:2:2 chroma DC. */
static const uint8_t rh264_ctc422_len[4][9]={
   { 1, 7, 7, 9, 9,10,11,12,13},
   { 0, 2, 7, 7, 9,10,11,12,12},
   { 0, 0, 3, 7, 7, 9,10,11,12},
   { 0, 0, 0, 5, 6, 7, 7,10,11}
};
static const uint8_t rh264_ctc422_code[4][9]={
   { 1,15,14, 7, 6, 7, 7, 7, 7},
   { 0, 1,13,12, 5, 6, 6, 6, 5},
   { 0, 0, 1,11,10, 4, 5, 5, 4},
   { 0, 0, 0, 1, 1, 9, 8, 4, 4}
};
/* total_zeros for the same block, indexed [total_coeff][zeros]. */
static const uint8_t rh264_cdctz422_len[8][8]={
   { 0},
   { 1, 3, 3, 4, 4, 4, 5, 5},
   { 3, 2, 3, 3, 3, 3, 3, 0},
   { 3, 3, 2, 2, 3, 3, 0, 0},
   { 3, 2, 2, 2, 3, 0, 0, 0},
   { 2, 2, 2, 2, 0, 0, 0, 0},
   { 2, 2, 1, 0, 0, 0, 0, 0},
   { 1, 1, 0, 0, 0, 0, 0, 0}
};
static const uint8_t rh264_cdctz422_code[8][8]={
   { 0},
   { 1, 2, 3, 2, 3, 1, 1, 0},
   { 0, 1, 1, 4, 5, 6, 7, 0},
   { 0, 1, 1, 2, 6, 7, 0, 0},
   { 6, 0, 1, 2, 7, 0, 0, 0},
   { 0, 1, 2, 3, 0, 0, 0, 0},
   { 0, 1, 1, 0, 0, 0, 0, 0},
   { 0, 1, 0, 0, 0, 0, 0, 0}
};

static const uint8_t rh264_cdctz_len[4][4]={
 {0,0,0,0},
 {1,2,3,3},
 {1,2,2,0},
 {1,1,0,0},
};
static const uint8_t rh264_cdctz_code[4][4]={
 {0,0,0,0},
 {1,1,1,0},
 {1,1,0,0},
 {1,0,0,0},
};

#endif
