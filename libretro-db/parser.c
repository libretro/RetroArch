#include <malloc.h>
#include <string.h>

#include "parser.h"

static void match_any( pr_state_t* parser )
{
  switch ( lx_next( &parser->lexer ) )
  {
  case LX_UNTERMINATED_STRING: longjmp( parser->env, PR_UNTERMINATED_STRING );
  case LX_INVALID_CHARACTER:   longjmp( parser->env, PR_INVALID_CHARACTER );
  }
}

static void match( pr_state_t* parser, int token )
{
  if ( parser->lexer.token != token )
  {
    longjmp( parser->env, PR_UNEXPECTED_TOKEN );
  }
  
  match_any( parser );
}

static void match_tag( pr_state_t* parser, const char* tag )
{
  if ( parser->lexer.token != LX_TAG || strncmp( parser->lexer.lexeme.str, tag, strlen( tag ) ) )
  {
    longjmp( parser->env, PR_UNEXPECTED_TOKEN );
  }
  
  match_any( parser );
}

static int cmp_keys( const pr_key_t* key1, const pr_key_t* key2 )
{
  if ( !key1 && !key2 )
  {
    return 1;
  }
  
  if ( ( key1 && !key2 ) || ( !key1 && key2 ) )
  {
    return 0;
  }
  
  if ( key1->key.len != key2->key.len || strncmp( key1->key.str, key2->key.str, key1->key.len ) )
  {
    return 0;
  }
  
  return cmp_keys( key1->prev, key2->prev );
}

static pr_pair_t* find_key( pr_node_t* node, const pr_key_t* key )
{
  int i;
  
  for ( i = 0; i < node->count; i++ )
  {
    const pr_key_t* other = &node->pairs[ i ].key;
    
    if ( cmp_keys( key, other ) )
    {
      return node->pairs + i;
    }
  }
  
  return node->pairs + node->count++;
}

static void parse_value( pr_state_t* parser, const pr_key_t* key, pr_node_t* node )
{
  pr_pair_t* pair = find_key( node, key );
  int token = parser->lexer.token;
  
  pair->key = *key;
  pair->value = parser->lexer.lexeme;
  
  if ( token == LX_STRING || token == LX_NUMBER || token == LX_VERSION || token == LX_TAG )
  {
    match_any( parser );
  }
  else
  {
    longjmp( parser->env, PR_UNEXPECTED_TOKEN );
  }
}

static void parse_map( pr_state_t* parser, const pr_key_t* prev, int skip )
{
  pr_node_t  dummy;
  pr_node_t* node;
  pr_key_t   key;
  
  if ( skip )
  {
    dummy.count = 0;
    node = &dummy;
  }
  else
  {
    node = parser->node;
  }
  
  match( parser, LX_LPAREN );
  
  while ( parser->lexer.token != LX_RPAREN )
  {
    key.key = parser->lexer.lexeme;
    key.prev = prev;
    match( parser, LX_TAG );
    
    if ( parser->lexer.token == LX_LPAREN )
    {
      parse_map( parser, &key, skip );
    }
    else
    {
      parse_value( parser, &key, node );
    }
  }
  
  match_any( parser );
}

static void parse_clrmamepro( pr_state_t* parser )
{
  static const pr_key_t clrmamepro = { { "clrmamepro", 10 }, NULL };
  
  match_tag( parser, clrmamepro.key.str );
  parse_map( parser, &clrmamepro, 1 );
}

static void parse_game( pr_state_t* parser )
{
  static const pr_key_t game = { { "game", 4 }, NULL };
  
  match_tag( parser, game.key.str );
  
  pr_node_t* node = (pr_node_t*)malloc( sizeof( pr_node_t ) );
  
  if ( node == NULL )
  {
    longjmp( parser->env, PR_OUT_OF_MEMORY );
  }
  
  node->count = 0;
  parser->node = node;
  *parser->prev = node;
  parser->prev = &node->next;
  parse_map( parser, NULL, 0 );
}

void pr_new( pr_state_t* parser, const char* source, unsigned srclen )
{
  lx_new( &parser->lexer, source, srclen );
  parser->prev = &parser->first;
}

int pr_parse( pr_state_t* parser )
{
  int res;
  
  if ( ( res = setjmp( parser->env ) ) == 0 )
  {
    match_any( parser );
    parse_clrmamepro( parser );
    
    while ( parser->lexer.token != LX_EOF )
    {
      parse_game( parser );
    }
  }
  
  *parser->prev = NULL;
  return res;
}

