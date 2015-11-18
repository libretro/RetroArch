#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include "parser.h"
#include "libretrodb.h"

static int cmpkeystr_( const pr_key_t* key, const char** str )
{
  int res = 1;
  
  if ( key->prev )
    res = cmpkeystr_( key->prev, str );
  
  res = res && !strncmp( key->key.str, *str, key->key.len );
  ( *str ) += key->key.len + 1;
  return res;
}

static int cmpkeystr( const pr_key_t* key, const char* str )
{
  return cmpkeystr_( key, &str );
}

static int cmpkeys( const pr_key_t* key1, const pr_key_t* key2 )
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
  
  return cmpkeys( key1->prev, key2->prev );
}

static const char* printchar( pr_state_t* parser )
{
  static char k[ 16 ];
  
  if ( *parser->lexer.current < 32 )
  {
    snprintf( k, sizeof( k ), "\\x%02x", (unsigned char)*parser->lexer.current );
    k[ sizeof( k ) - 1 ] = 0;
  }
  else
  {
    k[ 0 ] = *parser->lexer.current;
    k[ 1 ] = 0;
  }
  
  return k;
}

static void printkey( const pr_key_t* key )
{
  int i;
  
  if ( key->prev )
  {
    printkey( key->prev );
    printf( "." );
  }
  
  for ( i = 0; i < key->key.len; i++ )
  {
    printf( "%c", key->key.str[ i ] );
  }
}

static void printstr( const lx_string_t* str )
{
  int i;
  
  for ( i = 0; i < str->len; i++ )
  {
    printf( "%c", str->str[ i ] );
  }
}

static const char* printtoken( pr_state_t* parser )
{
  static char k[ 256 ];
  char* aux = k;
  const char* end = aux + sizeof( k ) - 1;
  
  while ( parser->lexer.lexeme.len-- && aux < end )
  {
    *aux++ = *parser->lexer.lexeme.str++;
  }
  
  *aux = 0;
  return k;
}

static char* dup_string( const lx_string_t* str )
{
  char* dup = (char*)malloc( str->len + 1 );
  
  if ( dup )
  {
    memcpy( (void*)dup, (const void*)str->str, str->len );
    dup[ str->len ] = 0;
  }
  
  return dup;
}

static unsigned char* dup_hexa( const lx_string_t* str )
{
  unsigned len = str->len / 2;
  unsigned char* dup = (unsigned char*)malloc( len );
  unsigned char* aux = dup;
  char byte[ 3 ];
  byte[ 2 ] = 0;
  
  if ( dup )
  {
    const char* s = str->str;
    
    while ( len-- )
    {
      byte[ 0 ] = *s++;
      byte[ 1 ] = *s++;
      *aux++ = strtol( byte, NULL, 16 );
    }
  }
  
  return dup;
}

static void* dup_memory( const lx_string_t* str )
{
  void* dup = (unsigned char*)malloc( str->len );
  
  if ( dup )
  {
    memcpy( dup, (void*)str->str, str->len );
  }
  
  return dup;
}

static void merge( pr_node_t* game, pr_node_t* game2, const char* key )
{
  unsigned     i, j, k, found, merged;
  pr_node_t*   game1;
  pr_node_t*   newgame;
  pr_key_t*    key1;
  lx_string_t* value1;
  pr_key_t*    key2;
  lx_string_t* value2;
  
  for ( ; game2; game2 = game2->next )
  {
    value2 = NULL;
    
    for ( i = 0; i < game2->count; i++ )
    {
      key2 = &game2->pairs[ i ].key;
      
      if ( cmpkeystr( key2, key ) )
      {
        value2 = &game2->pairs[ i ].value;
        break;
      }
    }
    
    if ( !value2 )
    {
      continue;
    }
    
    merged = 0;
    
    for ( game1 = game, value1 = NULL; game1; game1 = game1->next )
    {
      for ( i = 0; i < game1->count; i++ )
      {
        key1 = &game1->pairs[ i ].key;
        
        if ( cmpkeystr( key1, key ) )
        {
          value1 = &game1->pairs[ i ].value;
          
          if ( value1->len == value2->len && !strncmp( value1->str, value2->str, value1->len ) )
          {
            merged = 1;
            
            for ( k = 0; k < game2->count; k++ )
            {
              key2 = &game2->pairs[ k ].key;
              found = game1->count;
              
              for ( j = 0; j < game1->count; j++ )
              {
                key1 = &game1->pairs[ j ].key;
                
                if ( cmpkeys( key1, key2 ) )
                {
                  found = j;
                  break;
                }
              }
              
              if ( found == game1->count )
              {
                game1->pairs[ found ] = game2->pairs[ k ];
                game1->count++;
              }
              else if ( game2->pairs[ k ].value.len )
              {
                /* only overwrite if value is not empty */
                game1->pairs[ found ] = game2->pairs[ k ];
              }
            }
          }
        }
      }
    }
    
    if ( !merged )
    {
      /* add */
      newgame = (pr_node_t*)malloc( sizeof( pr_node_t ) );
      *newgame = *game2;
      newgame->next = game->next;
      game->next = newgame;
    }
  }
}

typedef struct
{
  const char* key;
  const char* new_key;
  int         type;
}
mapping_t;

enum
{
  TYPE_STRING,
  TYPE_UINT,
  TYPE_HEXA,
  TYPE_RAW
}
type_t;

static int provider( void* ctx, struct rmsgpack_dom_value* out )
{
  static mapping_t map[] =
  {
    { "name",           "name",           TYPE_STRING },
    { "description",    "description",    TYPE_STRING },
    { "rom.name",       "rom_name",       TYPE_STRING },
    { "rom.size",       "size",           TYPE_UINT   },
    { "users",          "users",          TYPE_UINT   },
    { "releasemonth",   "releasemonth",   TYPE_UINT   },
    { "releaseyear",    "releaseyear",    TYPE_UINT   },
    { "rumble",         "rumble",         TYPE_UINT   },
    { "analog",         "analog",         TYPE_UINT   },
    { "famitsu_rating", "famitsu_rating", TYPE_UINT   },
    { "edge_rating",    "edge_rating",    TYPE_UINT   },
    { "edge_issue",     "edge_issue",     TYPE_UINT   },
    { "edge_review",    "edge_review",    TYPE_STRING },
    { "enhancement_hw", "enhancement_hw", TYPE_STRING },
    { "barcode",        "barcode",        TYPE_STRING },
    { "esrb_rating",    "esrb_rating",    TYPE_STRING },
    { "elspa_rating",   "elspa_rating",   TYPE_STRING },
    { "pegi_rating",    "pegi_rating",    TYPE_STRING },
    { "cero_rating",    "cero_rating",    TYPE_STRING },
    { "franchise",      "franchise",      TYPE_STRING },
    { "developer",      "developer",      TYPE_STRING },
    { "publisher",      "publisher",      TYPE_STRING },
    { "origin",         "origin",         TYPE_STRING },
    { "rom.crc",        "crc",            TYPE_HEXA   },
    { "rom.md5",        "md5",            TYPE_HEXA   },
    { "rom.sha1",       "sha1",           TYPE_HEXA   },
    { "serial",         "serial",         TYPE_RAW    },
    { "rom.serial",     "serial",         TYPE_RAW    },
  };
  
  unsigned i, j, index, fields;
  pr_node_t** game_ptr = (pr_node_t**)ctx;
  pr_node_t* game = *game_ptr;
  
  if ( game == NULL )
  {
    return 1;
  }
  
  *game_ptr = game->next;
  
  out->type = RDT_MAP;
  out->val.map.items = calloc( game->count, sizeof( struct rmsgpack_dom_pair ) );
  
  index = fields = 0;
  
  for ( i = 0; i < game->count; i++ )
  {
    out->val.map.items[ i ].key.type = RDT_STRING;
    
    for ( j = 0; j < sizeof( map ) / sizeof( map[ 0 ] ); j++ )
    {
      if ( ( fields & ( 1 << j ) ) == 0 && cmpkeystr( &game->pairs[ i ].key, map[ j ].key ) )
      {
        fields |= 1 << j; /* avoid overrides */
        
        out->val.map.items[ index ].key.val.string.len = strlen( map[ j ].new_key );
        out->val.map.items[ index ].key.val.string.buff = strdup( map[ j ].new_key );
        
        switch ( map[ j ].type )
        {
        case TYPE_STRING:
          out->val.map.items[ index ].value.type = RDT_STRING;
          out->val.map.items[ index ].value.val.string.len = game->pairs[ i ].value.len;
          out->val.map.items[ index ].value.val.string.buff = dup_string( &game->pairs[ i ].value );
          index++;
          break;
          
        case TYPE_HEXA:
          out->val.map.items[ index ].value.type = RDT_BINARY;
          out->val.map.items[ index ].value.val.binary.len = game->pairs[ i ].value.len / 2;
          out->val.map.items[ index ].value.val.binary.buff = dup_hexa( &game->pairs[ i ].value );
          index++;
          break;
          
        case TYPE_UINT:
          out->val.map.items[ index ].value.type = RDT_UINT;
          out->val.map.items[ index ].value.val.uint_ = strtol( game->pairs[ i ].value.str, NULL, 10 );
          index++;
          break;
          
        case TYPE_RAW:
          out->val.map.items[ index ].value.type = RDT_BINARY;
          out->val.map.items[ index ].value.val.binary.len = game->pairs[ i ].value.len;
          out->val.map.items[ index ].value.val.binary.buff = dup_memory( &game->pairs[ i ].value );
          index++;
          break;
          
        default:
          fprintf( stderr, "Unhandled type in mapping %u (%s => %s)\n", j, map[ j ].key, map[ j ].new_key );
          break;
        }
        
        break;
      }
    }
  }
  
  out->val.map.len = index;
  return 0;
}

static const char* read_file( const char* filename, unsigned* size )
{
  char* source;
  RFILE* file = retro_fopen( filename, RFILE_MODE_READ, -1 );
  
  if ( file == NULL )
  {
    fprintf( stderr, "Error opening file: %s\n", strerror( errno ) );
    return NULL;
  }
  
  retro_fseek( file, 0, SEEK_END );
  *size = retro_ftell( file );
  retro_fseek( file, 0, SEEK_SET );
  
  source = (char*)malloc( *size + 1 );
  
  if ( source == NULL )
  {
    retro_fclose( file );
    fprintf( stderr, "Out of memory\n" );
    return NULL;
  }
  
  retro_fread( file, (void*)source, *size );
  retro_fclose( file );
  source[ *size ] = 0;
  
  return source;
}

static int parse( pr_state_t* parser, const char* filename, const char* source, unsigned size, const char* key )
{
  pr_node_t** prev;
  pr_node_t* game;
  int i, found;
  
  printf( "Parsing dat file '%s'...\n", filename );
  
  switch ( pr_parse( parser ) )
  {
  case PR_UNTERMINATED_STRING: fprintf( stderr, "%s:%u: Unterminated string\n", filename, parser->lexer.line ); return -1;
  case PR_INVALID_CHARACTER:   fprintf( stderr, "%s:%u: Invalid character '%s'\n", filename, parser->lexer.line, printchar( parser ) ); return -1;
  case PR_UNEXPECTED_TOKEN:    fprintf( stderr, "%s:%u: Unexpected token \"%s\"\n", filename, parser->lexer.line, printtoken( parser ) ); return -1;
  case PR_OUT_OF_MEMORY:       fprintf( stderr, "%s:%u: Out of memory\n", filename, parser->lexer.line ); return -1;
  }
  
  prev = &parser->first;
  game = parser->first;
  
  while ( game )
  {
    found = 0;
    
    for ( i = 0; i < game->count; i++ )
    {
      if ( cmpkeystr( &game->pairs[ i ].key, key ) )
      {
        found = 1;
        break;
      }
    }
    
    if ( found )
    {
      prev = &game->next;
    }
    else
    {
      *prev = game->next;
    }
    
    game = game->next;
  }
  
  return 0;
}

int main( int argc, const char* argv[] )
{
  const char* source;
  unsigned    size;
  pr_state_t  parser;
  pr_node_t*  game;
  RFILE*      out;
  int         i, res = 1;
  
  if ( argc < 4 )
  {
    fprintf( stderr, "usage:\nplain_converter <db file> <key name> <dat files...>\n\n" );
    return 1;
  }
  
  source = read_file( argv[ 3 ], &size );
  pr_new( &parser, source, size );
  
  if ( source && parse( &parser, argv[ 3 ], source, size, argv[ 2 ] ) == 0 )
  {
    game = parser.first;
    
    for ( i = 4; i < argc; i++ )
    {
      source = read_file( argv[ i ], &size );
      pr_new( &parser, source, size );
      
      if ( source && parse( &parser, argv[ i ], source, size, argv[ 2 ] ) == 0 )
      {
        merge( game, parser.first, argv[ 2 ] );
      }
    }
    
    out = retro_fopen( argv[ 1 ], RFILE_MODE_WRITE, -1 );
    
    if ( out != NULL )
    {
      res = libretrodb_create( out, &provider, (void*)&game );
      retro_fclose( out );
      res = 0;
    }
  }
  
  /* HACK Don't free anything, let the OS take care of that */
  return res;
}

