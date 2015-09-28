#include <malloc.h>
#include <string.h>

#include "djb2.h"
#include "parser.h"

static void match_any( pr_state_t* parser )
{
   switch ( lx_next( &parser->lexer ) )
   {
      case LX_UNTERMINATED_STRING:
         longjmp( parser->env, PR_UNTERMINATED_STRING );
      case LX_INVALID_CHARACTER:
         longjmp( parser->env, PR_INVALID_CHARACTER );
   }
}

static void match( pr_state_t* parser, int token )
{
   if ( parser->lexer.token != token )
      longjmp( parser->env, PR_UNEXPECTED_TOKEN );

   match_any( parser );
}

static void match_tag( pr_state_t* parser, const char* tag )
{
   if ( parser->lexer.token != LX_TAG || strncmp( parser->lexer.start, tag, strlen( tag ) ) )
      longjmp( parser->env, PR_UNEXPECTED_TOKEN );

   match_any( parser );
}

static void parse_value( pr_state_t* parser, const char* key, unsigned keylen, pr_node_t* node, int isrom )
{
   unsigned i;

   if ( isrom && keylen == 4 && !strncmp( key, "name", 4 ) )
   {
      key = "rom_name";
      keylen = 8;
   }

   for ( i = 0; i < node->count; i++ )
   {
      if ( keylen == node->pairs[ i ].key_len && !strncmp( key, node->pairs[ i ].key, keylen ) )
         break;
   }

   if ( i == node->count )
      node->count++;

   node->pairs[ i ].key = key;
   node->pairs[ i ].key_len = keylen;

   node->pairs[ i ].value = parser->lexer.start;
   node->pairs[ i ].value_len = parser->lexer.len;

   if ( parser->lexer.token == LX_STRING || parser->lexer.token == LX_NUMBER || parser->lexer.token == LX_TAG )
      match_any( parser );
   else
      longjmp( parser->env, PR_UNEXPECTED_TOKEN );
}

static void parse_map( pr_state_t* parser, int skip, int isrom )
{
   pr_node_t   dummy;
   pr_node_t*  node;

   if ( skip )
   {
      node = &dummy;
      dummy.count = 0;
   }
   else
      node = parser->node;

   match( parser, LX_LPAREN );

   while ( parser->lexer.token != LX_RPAREN )
   {
      unsigned    hash;
      const char* key;
      unsigned    keylen;

      if ( parser->lexer.token != LX_TAG )
         longjmp( parser->env, PR_UNEXPECTED_TOKEN );

      key = parser->lexer.start;
      keylen = parser->lexer.len;

      hash = djb2( key, keylen );

      match_any( parser );

      switch ( hash )
      {
         case 0x0b88a693U: /* rom */
            parse_map( parser, skip, 1 );
            break;

         default:
            parse_value( parser, key, keylen, node, isrom );
            break;
      }
   }

   match_any( parser );
}

static void parse_clrmamepro( pr_state_t* parser )
{
   match_tag( parser, "clrmamepro" );
   parse_map( parser, 1, 0 );
}

static void parse_game( pr_state_t* parser )
{
   match_tag( parser, "game" );

   pr_node_t* node = (pr_node_t*)malloc( sizeof( pr_node_t ) );

   if ( node == NULL )
      longjmp( parser->env, PR_OUT_OF_MEMORY );

   node->count   = 0;
   parser->node  = node;
   *parser->prev = node;
   parser->prev  = &node->next;
   parse_map( parser, 0, 0 );
}

void pr_new( pr_state_t* parser, const char* source, unsigned srclen )
{
   lx_new( &parser->lexer, source, srclen );
   parser->prev  = &parser->first;
}

int pr_parse( pr_state_t* parser )
{
   int res;

   if ( ( res = setjmp( parser->env ) ) == 0 )
   {
      match_any( parser );
      parse_clrmamepro( parser );

      while ( parser->lexer.token != LX_EOF )
         parse_game( parser );
   }

   *parser->prev = NULL;
   return res;
}
