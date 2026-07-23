/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_vcdiff.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_ENCODING_VCDIFF_H
#define __LIBRETRO_SDK_ENCODING_VCDIFF_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* VCDIFF (RFC 3284) decoder, written from the specification.
 *
 * This exists to apply .xdelta content patches, which is the only thing
 * the frontend ever asks of the format: it applies patches, it never
 * creates them, so there is no encoder here.
 *
 * The subset is the one xdelta3 actually emits for content patches: the
 * default code table, both window kinds (VCD_SOURCE and no-source), the
 * full address-mode set including the near and same caches, and all
 * instruction forms.  A patch that asks for anything outside it - a
 * secondary compressor, a custom code table - is refused rather than
 * guessed at, so a patch this cannot apply fails cleanly instead of
 * producing plausible wrong content.
 *
 * Errors are deliberately not enumerated.  The caller's only recourse
 * is the same either way: leave the content unpatched.  Everything a
 * user could act on is logged where it is detected.
 */

/* Apply @patch to @src, allocating the result.
 *
 * On success *out receives a malloc'd buffer the caller frees and
 * *out_len its length.  On failure both are left untouched and the
 * content should be used unpatched.
 *
 * @src may be NULL only when @src_len is 0 (a patch that carries its
 * whole target as literal data). */
bool vcdiff_decode(const uint8_t *patch, size_t patch_len,
      const uint8_t *src, size_t src_len,
      uint8_t **out, size_t *out_len);

RETRO_END_DECLS

#endif
