/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (jsonsax.c).
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

#include <setjmp.h>
#include <string.h>
#include <ctype.h>

#include <retro_inline.h>
#include <formats/jsonsax.h>

#ifdef JSONSAX_ERRORS
const char* jsonsax_errors[] =
{
  "Ok",
  "Interrupted",
  "Missing key",
  "Unterminated key",
  "Missing value",
  "Unterminated object",
  "Unterminated array",
  "Unterminated string",
  "Invalid value"
};
#endif

typedef struct
{
  const jsonsax_handlers_t* handlers;

  const char* json;
  void*       ud;
  jmp_buf     env;
}
state_t;

static INLINE void skip_spaces( state_t* state )
{
  while ( isspace( (unsigned char)*state->json ) )
    state->json++;
}

static INLINE void skip_digits( state_t* state )
{
  while ( isdigit( (unsigned char)*state->json ) )
    state->json++;
}

#define HANDLE_0( event ) \
  do { \
    if ( state->handlers->event && state->handlers->event( state->ud ) ) \
      longjmp( state->env, JSONSAX_INTERRUPTED ); \
  } while ( 0 )

#define HANDLE_1( event, arg1 ) \
  do { \
    if ( state->handlers->event && state->handlers->event( state->ud, arg1 ) ) \
      longjmp( state->env, JSONSAX_INTERRUPTED ); \
  } while ( 0 )

#define HANDLE_2( event, arg1, arg2 ) \
  do { \
    if ( state->handlers->event && state->handlers->event( state->ud, arg1, arg2 ) ) \
      longjmp( state->env, JSONSAX_INTERRUPTED ); \
  } while ( 0 )

static void jsonx_parse_value(state_t* state);

static void jsonx_parse_object( state_t* state )
{
   state->json++; /* we're sure the current character is a '{' */
   skip_spaces( state );
   HANDLE_0( start_object );

   while ( *state->json != '}' )
   {
      const char *name = NULL;
      if ( *state->json != '"' )
         longjmp( state->env, JSONSAX_MISSING_KEY );

      name = ++state->json;

      for ( ;; )
      {
         const char* quote = strchr( state->json, '"' );

         if ( !quote )
            longjmp( state->env, JSONSAX_UNTERMINATED_KEY );

         state->json = quote + 1;

         if ( quote[ -1 ] != '\\' )
            break;
      }

      HANDLE_2( key, name, state->json - name - 1 );
      skip_spaces( state );

      if ( *state->json != ':' )
         longjmp( state->env, JSONSAX_MISSING_VALUE );

      state->json++;
      skip_spaces( state );
      jsonx_parse_value( state );
      skip_spaces( state );

      if ( *state->json != ',' )
         break;

      state->json++;
      skip_spaces( state );
   }

   if ( *state->json != '}' )
      longjmp( state->env, JSONSAX_UNTERMINATED_OBJECT );

   state->json++;
   HANDLE_0( end_object );
}

static void jsonx_parse_array(state_t* state)
{
   unsigned int ndx = 0;

   state->json++; /* we're sure the current character is a '[' */
   skip_spaces( state );
   HANDLE_0( start_array );

   while ( *state->json != ']' )
   {
      HANDLE_1( array_index, ndx++ );
      jsonx_parse_value( state );
      skip_spaces( state );

      if ( *state->json != ',' )
         break;

      state->json++;
      skip_spaces( state );
   }

   if ( *state->json != ']' )
      longjmp( state->env, JSONSAX_UNTERMINATED_ARRAY );

   state->json++;
   HANDLE_0( end_array );
}

static void jsonx_parse_string(state_t* state)
{
  const char* string = ++state->json;

  for ( ;; )
  {
    const char* quote = strchr( state->json, '"' );

    if ( !quote )
      longjmp( state->env, JSONSAX_UNTERMINATED_STRING );

    state->json = quote + 1;

    if ( quote[ -1 ] != '\\' )
      break;
  }

  HANDLE_2( string, string, state->json - string - 1 );
}

static void jsonx_parse_boolean(state_t* state)
{
   if ( !strncmp( state->json, "true", 4 ) )
   {
      state->json += 4;
      HANDLE_1( boolean, 1 );
   }
   else if ( !strncmp( state->json, "false", 5 ) )
   {
      state->json += 5;
      HANDLE_1( boolean, 0 );
   }
   else
      longjmp( state->env, JSONSAX_INVALID_VALUE );
}

static void jsonx_parse_null(state_t* state)
{
   if ( !strncmp( state->json + 1, "ull", 3 ) ) /* we're sure the current character is a 'n' */
   {
      state->json += 4;
      HANDLE_0( null );
   }
   else
      longjmp( state->env, JSONSAX_INVALID_VALUE );
}

static void jsonx_parse_number(state_t* state)
{
   const char* number = state->json;

   if ( *state->json == '-' )
      state->json++;

   if ( !isdigit( (unsigned char)*state->json ) )
      longjmp( state->env, JSONSAX_INVALID_VALUE );

   skip_digits( state );

   if ( *state->json == '.' )
   {
      state->json++;

      if ( !isdigit( (unsigned char)*state->json ) )
         longjmp( state->env, JSONSAX_INVALID_VALUE );

      skip_digits( state );
   }

   if ( *state->json == 'e' || *state->json == 'E' )
   {
      state->json++;

      if ( *state->json == '-' || *state->json == '+' )
         state->json++;

      if ( !isdigit( (unsigned char)*state->json ) )
         longjmp( state->env, JSONSAX_INVALID_VALUE );

      skip_digits( state );
   }

   HANDLE_2( number, number, state->json - number );
}

static void jsonx_parse_value(state_t* state)
{
   skip_spaces( state );

   switch ( *state->json )
   {
      case '{':
         jsonx_parse_object(state);
         break;
      case '[':
         jsonx_parse_array( state );
         break;
      case '"':
         jsonx_parse_string( state );
         break;
      case 't':
      case 'f':
         jsonx_parse_boolean( state );
         break;
      case 'n':
         jsonx_parse_null( state );
         break;
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '-':
         jsonx_parse_number( state );
         break;

      default:
         longjmp( state->env, JSONSAX_INVALID_VALUE );
   }
}

int jsonsax_parse( const char* json, const jsonsax_handlers_t* handlers, void* userdata )
{
  state_t state;
  int res;

  state.json = json;
  state.handlers = handlers;
  state.ud = userdata;

  if ( ( res = setjmp( state.env ) ) == 0 )
  {
    if ( handlers->start_document )
      handlers->start_document( userdata );

    jsonx_parse_value(&state);

    if ( handlers->end_document )
      handlers->end_document( userdata );

    res = JSONSAX_OK;
  }

  return res;
}
