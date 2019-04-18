/*
Copyright (c) 2016 Raspberry Pi (Trading) Ltd.
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

#ifndef UTILS_H
#define UTILS_H

typedef struct string_struct
{
    struct string_struct *prev;
    struct string_struct *next;
    char data[0];
} STRING_T;

typedef struct string_vec_struct
{
    int num_strings;
    int max_strings;
    char **strings;
} STRING_VEC_T;

typedef struct overlay_help_state_struct OVERLAY_HELP_STATE_T;

extern int opt_verbose;
extern int opt_dry_run;

OVERLAY_HELP_STATE_T *overlay_help_open(const char *helpfile);
void overlay_help_close(OVERLAY_HELP_STATE_T *state);
int overlay_help_find(OVERLAY_HELP_STATE_T *state, const char *name);
int overlay_help_find_field(OVERLAY_HELP_STATE_T *state, const char *field);
const char *overlay_help_field_data(OVERLAY_HELP_STATE_T *state);
void overlay_help_print_field(OVERLAY_HELP_STATE_T *state,
			      const char *field, const char *label,
			      int indent, int strip_blanks);

int run_cmd(const char *fmt, ...);
void free_string(const char *string); /* Not thread safe */
void free_strings(void); /* Not thread safe */
char *sprintf_dup(const char *fmt, ...); /* Not thread safe */
char *vsprintf_dup(const char *fmt, va_list ap); /* Not thread safe */
int dir_exists(const char *dirname);
int file_exists(const char *dirname);
void string_vec_init(STRING_VEC_T *vec);
char *string_vec_add(STRING_VEC_T *vec, const char *str, int len);
int string_vec_find(STRING_VEC_T *vec, const char *str, int len);
void string_vec_sort(STRING_VEC_T *vec);
void string_vec_uninit(STRING_VEC_T *vec);
int error(const char *fmt, ...);
void fatal_error(const char *fmt, ...);

#endif
