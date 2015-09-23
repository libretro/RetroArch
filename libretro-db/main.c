#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>

#include <retro_file.h>

#include "parser.h"
#include "djb2.h"
#include "libretrodb.h"

static const char *printchar( pr_state_t* parser )
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

static const char *printtoken( pr_state_t* parser )
{
   static char k[ 256 ];
   char       *aux = k;
   const char *end = aux + sizeof( k ) - 1;

   while ( parser->lexer.len-- && aux < end )
      *aux++ = *parser->lexer.start++;

   *aux = 0;
   return k;
}

static char *dup_string( const char* str, unsigned len )
{
   char *dup = (char*)malloc( len + 1 );

   if (dup)
   {
      memcpy( (void*)dup, (const void*)str, len );
      dup[ len ] = 0;
   }

   return dup;
}

static unsigned char *dup_binary( const char* str, unsigned len )
{
   char byte[3];
   unsigned char* dup = (unsigned char*)malloc( len / 2 );
   unsigned char* aux = dup;

   byte[ 2 ] = 0;

   if ( dup )
   {
      len /= 2;

      while ( len-- )
      {
         byte[ 0 ] = *str++;
         byte[ 1 ] = *str++;
         printf( "%s", byte );
         *aux++ = strtol( byte, NULL, 16 );
      }
      printf( "\n" );
   }

   return dup;
}

static int provider( void* ctx, struct rmsgpack_dom_value* out )
{
   unsigned i, hash;
   pr_node_t** game_ptr = (pr_node_t**)ctx;
   pr_node_t* game = *game_ptr;

   if ( game == NULL )
      return 1;

   *game_ptr        = game->next;

   out->type            = RDT_MAP;
   out->val.map.len     = game->count;
   out->val.map.items   = calloc( game->count, sizeof(struct rmsgpack_dom_pair));

   for ( i = 0; i < game->count; i++ )
   {
      out->val.map.items[ i ].key.type = RDT_STRING;
      out->val.map.items[ i ].key.val.string.len = game->pairs[ i ].key_len;
      out->val.map.items[ i ].key.val.string.buff = dup_string( game->pairs[ i ].key, game->pairs[ i ].key_len );

      hash = djb2( game->pairs[ i ].key, game->pairs[ i ].key_len );

      switch ( hash )
      {
         case 0x0b88671dU: /* crc */
         case 0x0f3ea922U: /* crc32 */
         case 0x0b888fabU: /* md5 */
         case 0x7c9de632U: /* sha1 */
            out->val.map.items[ i ].value.type = RDT_BINARY;
            out->val.map.items[ i ].value.val.binary.len = game->pairs[ i ].value_len / 2;
            out->val.map.items[ i ].value.val.binary.buff = dup_binary( game->pairs[ i ].value, game->pairs[ i ].value_len );
            break;

         case 0x7c9dede0U: /* size */
            out->val.map.items[ i ].value.type = RDT_UINT;
            out->val.map.items[ i ].value.val.uint_ = strtol( game->pairs[ i ].value, NULL, 10 );
            break;

         default:
            out->val.map.items[ i ].value.type = RDT_STRING;
            out->val.map.items[ i ].value.val.string.len = game->pairs[ i ].value_len;
            out->val.map.items[ i ].value.val.string.buff = dup_string( game->pairs[ i ].value, game->pairs[ i ].value_len );
            break;
      }
   }

   return 0;
}

int main( int argc, const char* argv[] )
{
   char*      source;
   unsigned   size;
   pr_state_t parser;
   pr_node_t* game;
   pr_node_t* next;
   RFILE     *out, *file;
   int        res;

   if ( argc != 3 )
   {
      fprintf( stderr, "usage:\ndatconv <db file> <dat file>\n\n" );
      return 1;
   }

   file = retro_fopen(argv[ 2 ], RFILE_MODE_READ, -1);

   if (!file)
   {
      fprintf( stderr, "Error opening DAT file: %s\n", argv[2] );
      return 1;
   }

   retro_fseek(file, 0, SEEK_END );
   size = retro_ftell( file );
   retro_fseek( file, 0, SEEK_SET );

   source = (char*)malloc( size + 1 );

   if ( source == NULL )
   {
      retro_fclose( file );
      fprintf( stderr, "Out of memory\n" );
      return 1;
   }

   retro_fread(file, (void*)source, size);
   retro_fclose( file );
   source[ size ] = 0;

   pr_new( &parser, source, size );
   res = pr_parse( &parser );

   switch ( res )
   {
      case PR_UNTERMINATED_STRING:
         fprintf( stderr, "%s:%u: Unterminated string\n", "source", parser.lexer.line );
         break;
      case PR_INVALID_CHARACTER:
         fprintf( stderr, "%s:%u: Invalid character %s\n", "source", parser.lexer.line, printchar( &parser ) );
         break;
      case PR_UNEXPECTED_TOKEN:
         fprintf( stderr, "%s:%u: Unexpected token \"%s\"\n", "source", parser.lexer.line, printtoken( &parser ) );
         break;
      case PR_OUT_OF_MEMORY:
         fprintf( stderr, "%s:%u: Out of memory\n", "source", parser.lexer.line );
         break;
      default:
         game = parser.first;
         out  = retro_fopen( argv[ 1 ], RFILE_MODE_WRITE, -1);

         if (out)
         {
            res = libretrodb_create(out, &provider, (void*)&game );
            retro_fclose(out);

            while (game)
            {
               next = game->next;
               free( game );
               game = next;
            }
         }
         else
            res = 1;
   }

   free( source );
   return res;
}
