#ifndef LEXER_H
#define LEXER_H

enum
{
  LX_UNTERMINATED_STRING = -1,
  LX_INVALID_CHARACTER = -2,
};

enum
{
  LX_EOF = 256,
  LX_TAG,
  LX_NUMBER,
  LX_STRING,
  LX_VERSION,
  LX_LPAREN,
  LX_RPAREN,
};

typedef struct
{
  const char* str;
  unsigned    len;
}
lx_string_t;

typedef struct
{
  /* source code */
  int         line;
  
  /* lexer state */
  const char* current;
  const char* end;
  int         last_was_version;
  
  /* lookahead */
  int         token;
  lx_string_t lexeme;
}
lx_state_t;

int  lx_next( lx_state_t* lexer );
void lx_new( lx_state_t* lexer, const char* source, unsigned srclen );

#endif /* LEXER_H */

