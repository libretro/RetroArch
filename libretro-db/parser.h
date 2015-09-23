#ifndef PARSER_H
#define PARSER_H

#include <setjmp.h>

#include "lexer.h"

enum
{
  PR_UNTERMINATED_STRING = -1,
  PR_INVALID_CHARACTER = -2,
  PR_UNEXPECTED_TOKEN = -3,
  PR_OUT_OF_MEMORY = -4,
};

typedef struct
{
  const char* key;
  unsigned    key_len;
  const char* value;
  unsigned    value_len;
}
pr_pair_t;

typedef struct pr_node_t pr_node_t;

struct pr_node_t
{
  pr_pair_t  pairs[ 64 ];
  unsigned   count;
  pr_node_t* next;
};

typedef struct
{
  lx_state_t  lexer;
  pr_node_t*  node;
  pr_node_t*  first;
  pr_node_t** prev;
  jmp_buf     env;
}
pr_state_t;

void pr_new( pr_state_t* parser, const char* source, unsigned srclen );
int  pr_parse( pr_state_t* parser );

#endif /* PARSER_H */
