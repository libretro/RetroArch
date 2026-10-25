/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rsubtitle_vtt_css.c).
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


#include <float.h>
#include <stdlib.h>
#include <string.h>
#include "rsubtitle_vtt_internal.h"

enum vtt_css_state
{
   VTT_CSS_START, VTT_CSS_SPACE, VTT_CSS_COMMENT, VTT_CSS_NAME,
   VTT_CSS_STRING, VTT_CSS_URL_START, VTT_CSS_URL, VTT_CSS_URL_END,
   VTT_CSS_BAD_URL, VTT_CSS_INTEGER, VTT_CSS_FRACTION,
   VTT_CSS_EXP_SIGN, VTT_CSS_EXPONENT, VTT_CSS_ESCAPE, VTT_CSS_DONE
};

struct rsubtitle_vtt_css
{
   const char *input;
   char *buffer;
   size_t size, position, capacity, used;
   double number;
   rsubtitle_vtt_css_token_t token;
   enum vtt_css_state state, return_state;
   uint32_t escape;
   unsigned escape_digits, digits, exponent;
   int power, negative, exponent_negative, quote;
   int begun, reset, star, url_space, pending_space;
};

static int vtt_css_peek(const rsubtitle_vtt_css_t *css, size_t offset)
{
   return offset < css->size - css->position
      ? (unsigned char)css->input[css->position + offset] : -1;
}

static int vtt_css_space(int c)
{
   return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static int vtt_css_digit(int c)
{
   return c >= '0' && c <= '9';
}

static int vtt_css_hex(int c)
{
   if (vtt_css_digit(c)) return c - '0';
   if (c >= 'a' && c <= 'f') return c - 'a' + 10;
   return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static int vtt_css_name_start(int c)
{
   return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
      || c == '_' || c >= 128 || c == 0;
}

static int vtt_css_escape_valid(int a, int b)
{
   return a == '\\' && b != '\n' && b != '\r' && b != '\f';
}

static int vtt_css_ident(int a, int b, int c)
{
   if (a == '-') return vtt_css_name_start(b) || b == '-' || vtt_css_escape_valid(b, c);
   return vtt_css_name_start(a) || vtt_css_escape_valid(a, b);
}

static int vtt_css_number_start(int a, int b, int c)
{
   if (a == '+' || a == '-') return vtt_css_digit(b) || (b == '.' && vtt_css_digit(c));
   return vtt_css_digit(a) || (a == '.' && vtt_css_digit(b));
}

static void vtt_css_scalar(rsubtitle_vtt_css_t *css, uint32_t c)
{
   if (css->state == VTT_CSS_ESCAPE && css->return_state == VTT_CSS_BAD_URL) return;
   if (!c || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) c = 0xfffd;
   if (c < 0x80) css->buffer[css->used++] = (char)c;
   else if (c < 0x800)
   {
      css->buffer[css->used++] = (char)(0xc0 | (c >> 6));
      css->buffer[css->used++] = (char)(0x80 | (c & 63));
   }
   else if (c < 0x10000)
   {
      css->buffer[css->used++] = (char)(0xe0 | (c >> 12));
      css->buffer[css->used++] = (char)(0x80 | ((c >> 6) & 63));
      css->buffer[css->used++] = (char)(0x80 | (c & 63));
   }
   else
   {
      css->buffer[css->used++] = (char)(0xf0 | (c >> 18));
      css->buffer[css->used++] = (char)(0x80 | ((c >> 12) & 63));
      css->buffer[css->used++] = (char)(0x80 | ((c >> 6) & 63));
      css->buffer[css->used++] = (char)(0x80 | (c & 63));
   }
}

static void vtt_css_byte(rsubtitle_vtt_css_t *css, int c)
{
   if (c) css->buffer[css->used++] = (char)c;
   else vtt_css_scalar(css, 0);
   css->position++;
}

static int vtt_css_emit(rsubtitle_vtt_css_t *css, rsubtitle_vtt_css_token_t *token)
{
   css->buffer[css->used] = 0;
   css->token.text = css->buffer;
   css->token.length = css->used;
   *token = css->token;
   css->reset = 1;
   return 1;
}

static void vtt_css_escape_begin(rsubtitle_vtt_css_t *css)
{
   css->return_state = css->state;
   css->state = VTT_CSS_ESCAPE;
   css->escape = css->escape_digits = 0;
   css->position++;
}

static double vtt_css_numeric_value(const rsubtitle_vtt_css_t *css)
{
   static const double powers[] = {1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256};
   int power = css->power + (css->exponent_negative ? -(int)css->exponent : (int)css->exponent);
   unsigned i, magnitude;
   double value = css->number;
   if (!value || power < -340) value = 0.0;
   else if (power > 308) value = DBL_MAX;
   else
   {
      magnitude = (unsigned)(power < 0 ? -power : power);
      for (i = 0; i < sizeof(powers) / sizeof(powers[0]); i++)
         if (magnitude & (1u << i))
         {
            if (power < 0) value /= powers[i];
            else if (value > DBL_MAX / powers[i]) value = DBL_MAX;
            else value *= powers[i];
         }
   }
   return css->negative ? -value : value;
}

static int vtt_css_numeric_step(rsubtitle_vtt_css_t *css,
      rsubtitle_vtt_css_token_t *token, int c)
{
   int next = vtt_css_peek(css, 1), third = vtt_css_peek(css, 2);
   if (css->state == VTT_CSS_EXP_SIGN)
   {
      css->exponent_negative = c == '-';
      if (c == '+' || c == '-') css->position++;
      css->state = VTT_CSS_EXPONENT;
      return 0;
   }
   if (vtt_css_digit(c))
   {
      unsigned digit = (unsigned)(c - '0');
      if (css->state == VTT_CSS_EXPONENT)
      {
         if (css->exponent < 100000) css->exponent = css->exponent * 10 + digit;
         if (css->exponent > 100000) css->exponent = 100000;
      }
      else if (!css->number && !digit)
      {
         if (css->state == VTT_CSS_FRACTION) css->power--;
      }
      else if (css->digits < 17)
      {
         css->number = css->number * 10 + digit;
         css->digits++;
         if (css->state == VTT_CSS_FRACTION) css->power--;
      }
      else if (css->state == VTT_CSS_INTEGER) css->power++;
      css->position++;
      return 0;
   }
   if (css->state == VTT_CSS_INTEGER && c == '.' && vtt_css_digit(next))
   {
      css->token.flags &= ~RSUBTITLE_CSS_INTEGER; css->state = VTT_CSS_FRACTION; css->position++;
      return 0;
   }
   if (css->state != VTT_CSS_EXPONENT && (c == 'e' || c == 'E')
         && (vtt_css_digit(next) || ((next == '+' || next == '-') && vtt_css_digit(third))))
   {
      css->token.flags &= ~RSUBTITLE_CSS_INTEGER; css->state = VTT_CSS_EXP_SIGN; css->position++;
      return 0;
   }
   css->token.number = vtt_css_numeric_value(css);
   css->state = VTT_CSS_START;
   if (c == '%')
   {
      css->token.kind = RSUBTITLE_CSS_PERCENT; css->position++;
   }
   else if (vtt_css_ident(c, next, third))
   {
      css->token.kind = RSUBTITLE_CSS_DIMENSION; css->state = VTT_CSS_NAME;
      return 0;
   }
   return vtt_css_emit(css, token);
}

rsubtitle_vtt_css_t *rsubtitle_vtt_css_new(void)
{
   return (rsubtitle_vtt_css_t*)calloc(1, sizeof(rsubtitle_vtt_css_t));
}

void rsubtitle_vtt_css_free(rsubtitle_vtt_css_t *css)
{
   if (css) { free(css->buffer); free(css); }
}

int rsubtitle_vtt_css_begin(rsubtitle_vtt_css_t *css, const char *input, size_t size)
{
   char *buffer;
   size_t capacity;
   if (!css || (!input && size) || size > 65536
         || (css->begun && css->state != VTT_CSS_DONE)) return 0;
   capacity = size * 3 + 4;
   if (css->capacity < capacity)
   {
      buffer = (char*)realloc(css->buffer, capacity);
      if (!buffer) return 0;
      css->buffer = buffer; css->capacity = capacity;
   }
   css->input = input; css->size = size; css->position = 0;
   css->state = VTT_CSS_START; css->return_state = VTT_CSS_START;
   css->used = css->pending_space = css->star = css->url_space = 0;
   css->begun = css->reset = 1;
   return 1;
}

static int vtt_css_start_step(rsubtitle_vtt_css_t *css,
      rsubtitle_vtt_css_token_t *token, int c, int next, int third)
{
   if (c < 0) { css->state = VTT_CSS_DONE; return 2; }
   if (c == '/' && next == '*')
   {
      css->state = VTT_CSS_COMMENT; css->star = 0; css->position += 2;
   }
   else if (vtt_css_space(c)) css->state = VTT_CSS_SPACE;
   else if (c == '"' || c == '\'')
   {
      css->quote = c; css->state = VTT_CSS_STRING;
      css->token.kind = RSUBTITLE_CSS_STRING; css->position++;
   }
   else if (c == '<' && next == '!' && third == '-' && vtt_css_peek(css, 3) == '-')
   {
      css->position += 4; css->token.kind = RSUBTITLE_CSS_CDO;
      return vtt_css_emit(css, token);
   }
   else if (c == '-' && next == '-' && third == '>')
   {
      css->position += 3; css->token.kind = RSUBTITLE_CSS_CDC;
      return vtt_css_emit(css, token);
   }
   else if (vtt_css_number_start(c, next, third))
   {
      css->negative = c == '-'; css->number = 0; css->power = 0;
      css->digits = css->exponent = css->exponent_negative = 0;
      css->state = VTT_CSS_INTEGER; css->token.kind = RSUBTITLE_CSS_NUMBER;
      css->token.flags = RSUBTITLE_CSS_INTEGER;
      if (c == '+') css->token.flags |= RSUBTITLE_CSS_PLUS;
      else if (c == '-') css->token.flags |= RSUBTITLE_CSS_MINUS;
      if (c == '+' || c == '-') css->position++;
   }
   else if (c == '#' && (vtt_css_name_start(next) || vtt_css_digit(next)
            || next == '-' || vtt_css_escape_valid(next, third)))
   {
      css->token.kind = RSUBTITLE_CSS_HASH;
      css->token.flags = vtt_css_ident(next, third, vtt_css_peek(css, 3)) ? RSUBTITLE_CSS_ID : 0;
      css->state = VTT_CSS_NAME; css->position++;
   }
   else if (c == '@' && vtt_css_ident(next, third, vtt_css_peek(css, 3)))
   {
      css->token.kind = RSUBTITLE_CSS_AT; css->state = VTT_CSS_NAME; css->position++;
   }
   else if (vtt_css_ident(c, next, third))
   {
      css->token.kind = RSUBTITLE_CSS_IDENT; css->state = VTT_CSS_NAME;
   }
   else
   {
      css->token.kind = RSUBTITLE_CSS_DELIM; css->token.delimiter = (unsigned)c;
      css->position++;
      return vtt_css_emit(css, token);
   }
   return 0;
}

static int vtt_css_name_step(rsubtitle_vtt_css_t *css,
      rsubtitle_vtt_css_token_t *token, int c, int next)
{
   if (vtt_css_name_start(c) || vtt_css_digit(c) || c == '-') vtt_css_byte(css, c);
   else if (vtt_css_escape_valid(c, next)) vtt_css_escape_begin(css);
   else
   {
      css->state = VTT_CSS_START;
      if (c == '(' && css->token.kind == RSUBTITLE_CSS_IDENT)
      {
         css->position++; css->token.kind = RSUBTITLE_CSS_FUNCTION;
         if (css->used == 3 && (css->buffer[0] == 'u' || css->buffer[0] == 'U')
               && (css->buffer[1] == 'r' || css->buffer[1] == 'R')
               && (css->buffer[2] == 'l' || css->buffer[2] == 'L'))
         {
            css->url_space = 0; css->state = VTT_CSS_URL_START;
            return 0;
         }
      }
      return vtt_css_emit(css, token);
   }
   return 0;
}

static int vtt_css_string_step(rsubtitle_vtt_css_t *css,
      rsubtitle_vtt_css_token_t *token, int c, int next, int third)
{
   if (c < 0 || c == css->quote)
   {
      if (c >= 0) css->position++;
      css->state = VTT_CSS_START;
      return vtt_css_emit(css, token);
   }
   if (c == '\n' || c == '\r' || c == '\f')
   {
      css->used = 0; css->token.kind = RSUBTITLE_CSS_BAD_STRING; css->state = VTT_CSS_START;
      return vtt_css_emit(css, token);
   }
   if (c == '\\')
   {
      if (next < 0) css->position++;
      else if (next == '\n' || next == '\r' || next == '\f')
         css->position += next == '\r' && third == '\n' ? 3 : 2;
      else vtt_css_escape_begin(css);
   }
   else vtt_css_byte(css, c);
   return 0;
}

static int vtt_css_url_step(rsubtitle_vtt_css_t *css,
      rsubtitle_vtt_css_token_t *token, int c, int next)
{
   switch (css->state)
   {
      case VTT_CSS_URL_START:
         if (vtt_css_space(c)) { css->position++; css->url_space = 1; return 0; }
         if (c == '"' || c == '\'')
         {
            css->pending_space = css->url_space; css->state = VTT_CSS_START;
            return vtt_css_emit(css, token);
         }
         css->used = 0; css->token.kind = RSUBTITLE_CSS_URL; css->state = VTT_CSS_URL;
         return 0;
      case VTT_CSS_URL:
      case VTT_CSS_URL_END:
         if (c < 0 || c == ')')
         {
            if (c >= 0) css->position++;
            css->state = VTT_CSS_START;
            return vtt_css_emit(css, token);
         }
         if (vtt_css_space(c)) { css->position++; css->state = VTT_CSS_URL_END; return 0; }
         if (css->state == VTT_CSS_URL_END || c == '"' || c == '\'' || c == '('
               || (c > 0 && c <= 8) || c == 11 || (c >= 14 && c <= 31) || c == 127
               || (c == '\\' && !vtt_css_escape_valid(c, next)))
         {
            css->state = VTT_CSS_BAD_URL; css->token.kind = RSUBTITLE_CSS_BAD_URL;
         }
         else if (c == '\\') vtt_css_escape_begin(css);
         else vtt_css_byte(css, c);
         return 0;
      case VTT_CSS_BAD_URL:
         if (c < 0 || c == ')')
         {
            if (c >= 0) css->position++;
            css->used = 0; css->state = VTT_CSS_START;
            return vtt_css_emit(css, token);
         }
         if (vtt_css_escape_valid(c, next)) vtt_css_escape_begin(css);
         else css->position++;
         return 0;
      default: return -1;
   }
}

static int vtt_css_escape_step(rsubtitle_vtt_css_t *css, int c, int next)
{
   int hex = vtt_css_hex(c);
   if (hex >= 0 && css->escape_digits < 6)
   {
      css->escape = css->escape * 16 + (unsigned)hex;
      css->escape_digits++; css->position++;
      return 0;
   }
   if (css->escape_digits)
   {
      if (vtt_css_space(c)) css->position += c == '\r' && next == '\n' ? 2 : 1;
      vtt_css_scalar(css, css->escape);
   }
   else if (c < 0) vtt_css_scalar(css, 0xfffd);
   else
   {
      if (css->return_state != VTT_CSS_BAD_URL) vtt_css_byte(css, c);
      else css->position++;
   }
   css->state = css->return_state;
   return 0;
}

int rsubtitle_vtt_css_step(rsubtitle_vtt_css_t *css, rsubtitle_vtt_css_token_t *token)
{
   int c, next, third;
   if (!css || !token || !css->begun) return -1;
   if (css->reset)
   {
      memset(&css->token, 0, sizeof(css->token));
      css->used = css->reset = 0;
   }
   if (css->pending_space)
   {
      css->pending_space = 0; css->token.kind = RSUBTITLE_CSS_SPACE;
      css->buffer[css->used++] = ' ';
      return vtt_css_emit(css, token);
   }
   c = vtt_css_peek(css, 0); next = vtt_css_peek(css, 1); third = vtt_css_peek(css, 2);
   if (css->state >= VTT_CSS_INTEGER && css->state <= VTT_CSS_EXPONENT)
      return vtt_css_numeric_step(css, token, c);
   switch (css->state)
   {
      case VTT_CSS_START: return vtt_css_start_step(css, token, c, next, third);
      case VTT_CSS_SPACE:
         if (vtt_css_space(c)) { css->position++; return 0; }
         css->state = VTT_CSS_START; css->token.kind = RSUBTITLE_CSS_SPACE;
         css->buffer[css->used++] = ' ';
         return vtt_css_emit(css, token);
      case VTT_CSS_COMMENT:
         if (c < 0) { css->state = VTT_CSS_DONE; return 2; }
         css->position++;
         if (css->star && c == '/') css->state = VTT_CSS_START;
         css->star = c == '*';
         return 0;
      case VTT_CSS_NAME: return vtt_css_name_step(css, token, c, next);
      case VTT_CSS_STRING: return vtt_css_string_step(css, token, c, next, third);
      case VTT_CSS_URL_START:
      case VTT_CSS_URL:
      case VTT_CSS_URL_END:
      case VTT_CSS_BAD_URL: return vtt_css_url_step(css, token, c, next);
      case VTT_CSS_ESCAPE: return vtt_css_escape_step(css, c, next);
      case VTT_CSS_DONE: return 2;
      default: return -1;
   }
}
