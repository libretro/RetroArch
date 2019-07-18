/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_path.h).
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
#include <stddef.h>
#include <boolean.h>

void label_sanitize(char *label, size_t size, bool (*left)(char*), bool (*right)(char*));

bool left_parens(char *left);
bool right_parens(char *right);

bool left_brackets(char *left);
bool right_brackets(char *right);

bool left_parens_or_brackets(char *left);
bool right_parens_or_brackets(char *right);

bool left_parens_or_brackets_excluding_region(char *left);

bool left_parens_or_brackets_excluding_disc(char *left);

bool left_parens_or_brackets_excluding_region_or_disc(char *left);

void label_default_display(char *label, size_t size);

void label_remove_parens(char *label, size_t size);

void label_remove_brackets(char *label, size_t size);

void label_remove_parens_and_brackets(char *label, size_t size);

void label_keep_region(char *label, size_t size);

void label_keep_disc(char *label, size_t size);

void label_keep_region_and_disc(char *label, size_t size);
