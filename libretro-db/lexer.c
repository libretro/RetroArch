#include <string.h>
#include <ctype.h>

#include "lexer.h"

static void skip(lx_state_t* lexer)
{
   if ( lexer->current < lexer->end )
      lexer->current++;
}

int lx_next(lx_state_t* lexer)
{
   /* skip spaces */
   for ( ;; )
   {
      if ( isspace( *lexer->current ) )
      {
         if ( *lexer->current == '\n' )
         {
            lexer->line++;
         }

         skip( lexer );
      }
      else if ( *lexer->current != 0 )
      {
         break;
      }
      else
      {
         /* return LX_EOF if we've reached the end of the input */
         lexer->start = "<eof>";
         lexer->len = 5;
         lexer->token = LX_EOF;
         return 0;
      }
   }

   lexer->start = lexer->current;

   /* if the character is alphabetic or '_', the token is an identifier */
   if ( isalpha( *lexer->current ) || *lexer->current == '_' )
   {
      /* get all alphanumeric and '_' characters */
      do
      {
         skip( lexer );
      }
      while ( isalnum( *lexer->current ) || *lexer->current == '_' );

      lexer->len = lexer->current - lexer->start;
      lexer->token = LX_TAG;
      return 0;
   }

   /* if the character is an hex digit, the token is a number */
   if ( isxdigit( *lexer->current ) )
   {
      do
      {
         skip( lexer );
      }
      while ( isxdigit( *lexer->current ) );

      if ( *lexer->current == '-' )
      {
         do
         {
            skip( lexer );
         }
         while ( isxdigit( *lexer->current ) );

         lexer->token = LX_VERSION;
      }
      else
      {
         lexer->token = LX_NUMBER;
      }

      lexer->len = lexer->current - lexer->start;
      return 0;
   }

   /* if the character is a quote, it's a string */
   if ( *lexer->current == '"' )
   {
      /* get anything until another quote */
      do
      {
         skip( lexer );

         if ( *lexer->current == '"' && lexer->current[ -1 ] != '\\' )
         {
            break;
         }
      }
      while ( lexer->current < lexer->end );

      if ( lexer->current == lexer->end )
      {
         return LX_UNTERMINATED_STRING;
      }

      skip( lexer );
      lexer->start++;
      lexer->len = lexer->current - lexer->start - 1;
      lexer->token = LX_STRING;
      return 0;
   }

   /* otherwise the token is a symbol */
   lexer->len = 1;

   switch ( *lexer->current++ )
   {
      case '(':
         lexer->token = LX_LPAREN;
         return 0;
      case ')':
         lexer->token = LX_RPAREN;
         return 0;
   }

   return LX_INVALID_CHARACTER;
}

void lx_new( lx_state_t* lexer, const char* source, unsigned srclen )
{
   lexer->line = 1;
   lexer->current = source;
   lexer->end = source + srclen;
   lexer->start = NULL;
   lexer->len = 0;
   lexer->token = 0;
}
