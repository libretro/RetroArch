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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <errno.h>

#include "utils.h"

#define OVERLAY_HELP_INDENT 8

int opt_verbose;
int opt_dry_run;
static STRING_T *allocated_strings;

struct overlay_help_state_struct
{
    FILE *fp;
    long rec_pos;
    int line_len;
    int line_pos;
    int blank_count;
    int end_of_field;
    char line_buf[82];
};

static int overlay_help_get_line(OVERLAY_HELP_STATE_T *state);

OVERLAY_HELP_STATE_T *overlay_help_open(const char *helpfile)
{
    OVERLAY_HELP_STATE_T *state = NULL;
    FILE *fp = fopen(helpfile, "r");
    if (fp)
    {
        state = calloc(1, sizeof(OVERLAY_HELP_STATE_T));
        if (!state)
                fatal_error("Out of memory");
        state->fp = fp;
        state->line_pos = -1;
        state->rec_pos = -1;
    }

    return state;
}

void overlay_help_close(OVERLAY_HELP_STATE_T *state)
{
    fclose(state->fp);
    free(state);
}

int overlay_help_find(OVERLAY_HELP_STATE_T *state, const char *name)
{
    state->line_pos = -1;
    state->rec_pos = -1;
    state->blank_count = 0;

    fseek(state->fp, 0, SEEK_SET);

    while (overlay_help_find_field(state, "Name"))
    {
        const char *overlay = overlay_help_field_data(state);
        if (overlay && (strcmp(overlay, name) == 0))
        {
            state->rec_pos = (long)ftell(state->fp);
            return 1;
        }
    }

    return 0;
}

int overlay_help_find_field(OVERLAY_HELP_STATE_T *state, const char *field)
{
    int field_len = strlen(field);
    int found = 0;

    if (state->rec_pos >= 0)
        fseek(state->fp, state->rec_pos, SEEK_SET);

    while (!found)
    {
        int line_len = overlay_help_get_line(state);
        if (line_len < 0)
            break;

        /* Check for the "<field>:" prefix */
        if ((line_len >= (field_len + 1)) &&
            (state->line_buf[field_len] == ':') &&
            (memcmp(state->line_buf, field, field_len) == 0))
        {
            /* Found it
               If this initial line has no content then skip it */
            if (line_len > OVERLAY_HELP_INDENT)
                state->line_pos = OVERLAY_HELP_INDENT;
            else
                state->line_pos = -1;
            state->end_of_field = 0;
            found = 1;
        }
        else
        {
            state->line_pos = -1;
        }
    }

    return found;
}

const char *overlay_help_field_data(OVERLAY_HELP_STATE_T *state)
{
    int line_len, pos;

    if (state->end_of_field)
        return NULL;

    line_len = state->line_len;

    if ((state->line_pos < 0) ||
        (state->line_pos >= line_len))
    {
        line_len = overlay_help_get_line(state);

        /* Fields end at the start of the next field or the end of the record */
        if ((line_len < 0) || (state->line_buf[0] != ' '))
        {
            state->end_of_field = 1;
            return NULL;
        }

        if (line_len == 0)
            return "";
    }

    /* Return field data starting at OVERLAY_HELP_INDENT, if there is any */
    pos = line_len;
    if (pos > OVERLAY_HELP_INDENT)
        pos = OVERLAY_HELP_INDENT;

    state->line_pos = -1;
    return &state->line_buf[pos];
}

void overlay_help_print_field(OVERLAY_HELP_STATE_T *state,
                              const char *field, const char *label,
                              int indent, int strip_blanks)
{
    if (!overlay_help_find_field(state, field))
        return;

    while (1)
    {
	const char *line = overlay_help_field_data(state);
	if (!line)
	    break;

	if (label)
	{
	    int spaces = indent - strlen(label);
	    if (spaces < 0)
		spaces = 0;

	    printf("%s%*s%s\n", label, spaces, "", line);
	    label = NULL;
	}
	else if (line[0])
	{
	    printf("%*s%s\n", indent, "", line);
	}
	else if (!strip_blanks)
	{
	    printf("\n");
	}
    }

    if (!strip_blanks)
	printf("\n");
}

/* Returns the length of the line, or -1 on end of file or record */
static int overlay_help_get_line(OVERLAY_HELP_STATE_T *state)
{
    int line_len;

    if (state->line_pos >= 0)
	return state->line_len;

get_next_line:
    state->line_buf[sizeof(state->line_buf) - 1] = ' ';
    line_len = -1;
    if (fgets(state->line_buf, sizeof(state->line_buf), state->fp))
    {
	// Check for overflow

	// Strip the newline
	line_len = strlen(state->line_buf);
	if (line_len && (state->line_buf[line_len - 1] == '\n'))
	{
	    line_len--;
	    state->line_buf[line_len] = '\0';
	}
    }

    if (state->rec_pos >= 0)
    {
	if (line_len == 0)
	{
	    state->blank_count++;
	    if (state->blank_count >= 2)
		return -1;
	    state->line_pos = 0;
	    goto get_next_line;
	}
	else if (state->blank_count)
	{
	    /* Return a single blank line now - the non-empty line will be
	       returned next time */
	    state->blank_count = 0;
	    return 0;
	}
    }

    state->line_len = line_len;
    state->line_pos = (line_len >= 0) ? 0 : -1;
    return line_len;
}

int run_cmd(const char *fmt, ...)
{
    va_list ap;
    char *cmd;
    int ret;
    va_start(ap, fmt);
    cmd = vsprintf_dup(fmt, ap);
    va_end(ap);

    if (opt_dry_run || opt_verbose)
	fprintf(stderr, "run_cmd: %s\n", cmd);

    ret = opt_dry_run ? 0 : system(cmd);

    free_string(cmd);

    return ret;
}


/* Not thread safe */
void free_string(const char *string)
{
    STRING_T *str;

    if (!string)
	return;

    str = (STRING_T *)(string - sizeof(STRING_T));
    if (str == allocated_strings)
    {
	allocated_strings = str->next;
	if (allocated_strings == str)
	    allocated_strings = NULL;
    }
    str->prev->next = str->next;
    str->next->prev = str->prev;
    free(str);
}

/* Not thread safe */
void free_strings(void)
{
    if (allocated_strings)
    {
	STRING_T *str = allocated_strings;
	do
	{
	    STRING_T *t = str;
	    str = t->next;
	    free(t);
	} while (str != allocated_strings);
	allocated_strings = NULL;
    }
}

/* Not thread safe */
char *sprintf_dup(const char *fmt, ...)
{
    va_list ap;
    char *str;
    va_start(ap, fmt);
    str = vsprintf_dup(fmt, ap);
    va_end(ap);
    return str;
}

/* Not thread safe */
char *vsprintf_dup(const char *fmt, va_list ap)
{
    char scratch[512];
    int len;
    STRING_T *str;
    len = vsnprintf(scratch, sizeof(scratch), fmt, ap) + 1;

    if (len > sizeof(scratch))
	fatal_error("Maximum string length exceeded");

    str = malloc(sizeof(STRING_T) + len);
    if (!str)
	fatal_error("Out of memory");

    memcpy(str->data, scratch, len);
    if (allocated_strings)
    {
	str->next = allocated_strings;
	str->prev = allocated_strings->prev;
	str->next->prev = str;
	str->prev->next = str;
    }
    else
    {
	str->next = str;
	str->prev = str;
	allocated_strings = str;
    }

    return str->data;
}

int dir_exists(const char *dirname)
{
    struct stat finfo;
    return (stat(dirname, &finfo) == 0) && S_ISDIR(finfo.st_mode);
}

int file_exists(const char *dirname)
{
    struct stat finfo;
    return (stat(dirname, &finfo) == 0) && S_ISREG(finfo.st_mode);
}

void string_vec_init(STRING_VEC_T *vec)
{
    vec->num_strings = 0;
    vec->max_strings = 0;
    vec->strings = NULL;
}

char *string_vec_add(STRING_VEC_T *vec, const char *str, int len)
{
    char *copy;
    if (vec->num_strings == vec->max_strings)
    {
	if (vec->max_strings)
	    vec->max_strings *= 2;
	else
	    vec->max_strings = 16;
	vec->strings = realloc(vec->strings, vec->max_strings * sizeof(const char *));
	if (!vec->strings)
	    fatal_error("Out of memory");
    }

    if (len)
    {
	copy = malloc(len + 1);
	strncpy(copy, str, len);
	copy[len] = '\0';
    }
    else
       copy = strdup(str);

    if (!copy)
	fatal_error("Out of memory");

    vec->strings[vec->num_strings++] = copy;

    return copy;
}

int string_vec_find(STRING_VEC_T *vec, const char *str, int len)
{
    int i;

    for (i = 0; i < vec->num_strings; i++)
    {
	if (len)
	{
	    if ((strncmp(vec->strings[i], str, len) == 0) &&
		(vec->strings[i][len] == '\0'))
		return i;
	}
	else if (strcmp(vec->strings[i], str) == 0)
	    return i;
    }

    return -1;
}

int string_vec_compare(const void *a, const void *b)
{
    return strcmp(*(const char **)a, *(const char **)b);
}

void string_vec_sort(STRING_VEC_T *vec)
{
    qsort(vec->strings, vec->num_strings, sizeof(char *), &string_vec_compare);
}

void string_vec_uninit(STRING_VEC_T *vec)
{
    int i;
    for (i = 0; i < vec->num_strings; i++)
	free(vec->strings[i]);
    free(vec->strings);
}

int error(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "* ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    return 1;
}

void fatal_error(const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "* ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}
