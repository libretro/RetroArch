#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <compat/fnmatch.h>
#include <compat/strl.h>

#include "libretrodb.h"
#include "query.h"
#include "rmsgpack_dom.h"

#define MAX_ERROR_LEN   256
#define QUERY_MAX_ARGS  50

struct buffer
{
   const char *data;
   size_t len;
   ssize_t offset;
};

enum argument_type
{
   AT_FUNCTION,
   AT_VALUE
};

struct argument;

typedef struct rmsgpack_dom_value (*rarch_query_func)(
      struct rmsgpack_dom_value input,
      unsigned argc,
      const struct argument *argv
      );

struct invocation
{
   rarch_query_func func;
   unsigned argc;
   struct argument *argv;
};

struct argument
{
   enum argument_type type;
   union
   {
      struct rmsgpack_dom_value value;
      struct invocation invocation;
   } a;
};

struct query
{
   unsigned ref_count;
   struct invocation root;
};

struct registered_func
{
   const char *name;
   rarch_query_func func;
};

static char tmp_error_buff [MAX_ERROR_LEN] = {0};

/* Errors */
static void raise_too_many_arguments(const char **error)
{
   strlcpy(tmp_error_buff, 
         "Too many arguments in function call.", sizeof(tmp_error_buff));
   *error = tmp_error_buff;
}

static void raise_expected_number(ssize_t where, const char **error)
{
#ifdef _WIN32
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%I64u::Expected number",
         (unsigned long long)where);
#else
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%llu::Expected number",
         (unsigned long long)where);
#endif
   *error = tmp_error_buff;
}

static void raise_expected_string(ssize_t where, const char ** error)
{
#ifdef _WIN32
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%I64u::Expected string",
         (unsigned long long)where);
#else
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%llu::Expected string",
         (unsigned long long)where);
#endif
   *error = tmp_error_buff;
}

static void raise_unexpected_eof(ssize_t where, const char ** error)
{
#ifdef _WIN32
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%I64u::Unexpected EOF",
         (unsigned long long)where
         );
#else
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%llu::Unexpected EOF",
         (unsigned long long)where
         );
#endif
   *error = tmp_error_buff;
}

static void raise_enomem(const char **error)
{
   strlcpy(tmp_error_buff, "Out of memory", sizeof(tmp_error_buff));
   *error = tmp_error_buff;
}

static void raise_unknown_function(ssize_t where, const char *name,
      ssize_t len, const char **error)
{
#ifdef _WIN32
   int n = snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%I64u::Unknown function '",
         (unsigned long long)where
         );
#else
   int n = snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%llu::Unknown function '",
         (unsigned long long)where
         );
#endif

   if (len < (MAX_ERROR_LEN - n - 3))
      strncpy(tmp_error_buff + n, name, len);

   strlcpy(tmp_error_buff + n + len, "'", sizeof(tmp_error_buff));
   *error = tmp_error_buff;
}
static void raise_expected_eof(ssize_t where, char found, const char **error)
{
#ifdef _WIN32
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%I64u::Expected EOF found '%c'",
         (unsigned long long)where,
         found
         );
#else
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%llu::Expected EOF found '%c'",
         (unsigned long long)where,
         found
         );
#endif
   *error = tmp_error_buff;
}

static void raise_unexpected_char(ssize_t where, char expected, char found,
      const char **error)
{
#ifdef _WIN32
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%I64u::Expected '%c' found '%c'",
         (unsigned long long)where, expected, found);
#else
   snprintf(tmp_error_buff, MAX_ERROR_LEN,
         "%llu::Expected '%c' found '%c'",
         (unsigned long long)where, expected, found);
#endif
   *error = tmp_error_buff;
}


static void argument_free(struct argument *arg)
{
   unsigned i;

   if (arg->type != AT_FUNCTION)
   {
      rmsgpack_dom_value_free(&arg->a.value);
      return;
   }

   for (i = 0; i < arg->a.invocation.argc; i++)
      argument_free(&arg->a.invocation.argv[i]);
}


static struct buffer parse_argument(struct buffer buff, struct argument *arg,
      const char **error);

static struct rmsgpack_dom_value is_true(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument *argv)
{
   struct rmsgpack_dom_value res;
   memset(&res, 0, sizeof(res));

   res.type  = RDT_BOOL;
   res.val.bool_ = 0;

   if (argc > 0 || input.type != RDT_BOOL)
      res.val.bool_ = 0;
   else
      res.val.bool_ = input.val.bool_;

   return res;
}

static struct rmsgpack_dom_value equals(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument * argv)
{
   struct argument arg;
   struct rmsgpack_dom_value res;
   memset(&res, 0, sizeof(res));

   res.type = RDT_BOOL;

   if (argc != 1)
      res.val.bool_ = 0;
   else
   {
      arg = argv[0];

      if (arg.type != AT_VALUE)
         res.val.bool_ = 0;
      else
      {
         if (input.type == RDT_UINT && arg.a.value.type == RDT_INT)
         {
            arg.a.value.type = RDT_UINT;
            arg.a.value.val.uint_ = arg.a.value.val.int_;
         }
         res.val.bool_ = (rmsgpack_dom_value_cmp(&input, &arg.a.value) == 0);
      }
   }
   return res;
}

static struct rmsgpack_dom_value operator_or(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument * argv)
{
   unsigned i;
   struct rmsgpack_dom_value res;
   memset(&res, 0, sizeof(res));

   res.type = RDT_BOOL;
   res.val.bool_ = 0;

   for (i = 0; i < argc; i++)
   {
      if (argv[i].type == AT_VALUE)
         res = equals(input, 1, &argv[i]);
      else
      {
         res = is_true(argv[i].a.invocation.func(input,
                  argv[i].a.invocation.argc,
                  argv[i].a.invocation.argv
                  ), 0, NULL);
      }

      if (res.val.bool_)
         return res;
   }

   return res;
}

static struct rmsgpack_dom_value between(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument * argv)
{
   struct rmsgpack_dom_value res;
   unsigned i                     = 0;

   memset(&res, 0, sizeof(res));

   res.type = RDT_BOOL;
   res.val.bool_ = 0;

   (void)i;

   if (argc != 2)
      return res;
   if (argv[0].type != AT_VALUE || argv[1].type != AT_VALUE)
      return res;
   if (argv[0].a.value.type != RDT_INT || argv[1].a.value.type != RDT_INT)
      return res;

   switch (input.type)
   {
      case RDT_INT:
         res.val.bool_ = ((input.val.int_ >= argv[0].a.value.val.int_) && (input.val.int_ <= argv[1].a.value.val.int_));
         break;
      case RDT_UINT:
         res.val.bool_ = (((unsigned)input.val.int_ >= argv[0].a.value.val.uint_) && (input.val.int_ <= argv[1].a.value.val.int_));
         break;
      default:
         return res;
   }

   return res;
}

static struct rmsgpack_dom_value operator_and(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument * argv)
{
   unsigned i;
   struct rmsgpack_dom_value res;
   memset(&res, 0, sizeof(res));

   res.type = RDT_BOOL;
   res.val.bool_ = 0;

   for (i = 0; i < argc; i++)
   {
      if (argv[i].type == AT_VALUE)
         res = equals(input, 1, &argv[i]);
      else
      {
         res = is_true(
               argv[i].a.invocation.func(input,
                  argv[i].a.invocation.argc,
                  argv[i].a.invocation.argv
                  ),
               0, NULL);
      }

      if (!res.val.bool_)
         return res;
   }
   return res;
}

static struct rmsgpack_dom_value q_glob(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument * argv)
{
   struct rmsgpack_dom_value res;
   unsigned i = 0;
   memset(&res, 0, sizeof(res));

   res.type = RDT_BOOL;
   res.val.bool_ = 0;

   (void)i;

   if (argc != 1)
      return res;
   if (argv[0].type != AT_VALUE || argv[0].a.value.type != RDT_STRING)
      return res;
   if (input.type != RDT_STRING)
      return res;
   res.val.bool_ = rl_fnmatch(
         argv[0].a.value.val.string.buff,
         input.val.string.buff,
         0
         ) == 0;
   return res;
}

static struct rmsgpack_dom_value all_map(struct rmsgpack_dom_value input,
      unsigned argc, const struct argument *argv)
{
   unsigned i;
   struct argument arg;
   struct rmsgpack_dom_value res;
   struct rmsgpack_dom_value nil_value;
   struct rmsgpack_dom_value *value = NULL;
   memset(&res, 0, sizeof(res));

   nil_value.type = RDT_NULL;
   res.type       = RDT_BOOL;
   res.val.bool_      = 1;

   if (argc % 2 != 0)
   {
      res.val.bool_ = 0;
      return res;
   }

   if (input.type != RDT_MAP)
      return res;

   for (i = 0; i < argc; i += 2)
   {
      arg = argv[i];
      if (arg.type != AT_VALUE)
      {
         res.val.bool_ = 0;
         goto clean;
      }
      value = rmsgpack_dom_value_map_value(&input, &arg.a.value);
      if (!value) /* All missing fields are nil */
         value = &nil_value;
      arg = argv[i + 1];
      if (arg.type == AT_VALUE)
         res = equals(*value, 1, &arg);
      else
      {
         res = is_true(arg.a.invocation.func(
                  *value,
                  arg.a.invocation.argc,
                  arg.a.invocation.argv
                  ), 0, NULL);
         value = NULL;
      }
      if (!res.val.bool_)
         break;
   }
clean:
   return res;
}

struct registered_func registered_functions[100] = {
   {"is_true", is_true},
   {"or", operator_or},
   {"and", operator_and},
   {"between", between},
   {"glob", q_glob},
   {NULL, NULL}
};

static struct buffer chomp(struct buffer buff)
{
   for (; (unsigned)buff.offset < buff.len && isspace((int)buff.data[buff.offset]); buff.offset++);
   return buff;
}

static struct buffer expect_char(struct buffer buff,
      char c, const char ** error)
{
   if ((unsigned)buff.offset >= buff.len)
      raise_unexpected_eof(buff.offset, error);
   else if (buff.data[buff.offset] != c)
      raise_unexpected_char(
            buff.offset, c, buff.data[buff.offset], error);
   else
      buff.offset++;
   return buff;
}

static struct buffer expect_eof(struct buffer buff, const char ** error)
{
   buff = chomp(buff);
   if ((unsigned)buff.offset < buff.len)
      raise_expected_eof(buff.offset, buff.data[buff.offset], error);
   return buff;
}

static int peek(struct buffer buff, const char * data)
{
   size_t remain = buff.len - buff.offset;

   if (remain < strlen(data))
      return 0;

   return (strncmp(buff.data + buff.offset,
            data, strlen(data)) == 0);
}

static int is_eot(struct buffer buff)
{
   return ((unsigned)buff.offset >= buff.len);
}

static void peek_char(struct buffer buff, char *c, const char **error)
{
   if (is_eot(buff))
   {
      raise_unexpected_eof(buff.offset, error);
      return;
   }

   *c = buff.data[buff.offset];
}

static struct buffer get_char(struct buffer buff, char * c,
      const char ** error)
{
   if (is_eot(buff))
   {
      raise_unexpected_eof(buff.offset, error);
      return buff;
   }
   *c = buff.data[buff.offset];
   buff.offset++;
   return buff;
}

static struct buffer parse_string(struct buffer buff,
      struct rmsgpack_dom_value *value, const char **error)
{
   const char * str_start = NULL;
   char terminator        = '\0';
   char c                 = '\0';
   int  is_binstr         = 0;

   (void)c;

   buff = get_char(buff, &terminator, error);

   if (*error)
      return buff;

   if (terminator == 'b')
   {
      is_binstr = 1;
      buff = get_char(buff, &terminator, error);
   }

   if (terminator != '"' && terminator != '\'')
   {
      buff.offset--;
      raise_expected_string(buff.offset, error);
   }

   str_start = buff.data + buff.offset;
   buff = get_char(buff, &c, error);

   while (!*error)
   {
      if (c == terminator)
         break;
      buff = get_char(buff, &c, error);
   }

   if (!*error)
   {
      size_t count;
      value->type = is_binstr ? RDT_BINARY : RDT_STRING;
      value->val.string.len = (buff.data + buff.offset) - str_start - 1;

      count = is_binstr ? (value->val.string.len + 1) / 2 : (value->val.string.len + 1);
      value->val.string.buff = (char*)calloc(count, sizeof(char));

      if (!value->val.string.buff)
         raise_enomem(error);
      else if (is_binstr)
      {
         unsigned i;
         const char *tok = str_start;
         unsigned      j = 0;

         for (i = 0; i < value->val.string.len; i += 2)
         {
            uint8_t hi, lo;
            char hic = tok[i];
            char loc = tok[i + 1];

            if (hic <= '9')
               hi = hic - '0';
            else
               hi = (hic - 'A') + 10;

            if (loc <= '9')
               lo = loc - '0';
            else
               lo = (loc - 'A') + 10;

            value->val.string.buff[j++] = hi * 16 + lo;
         }

         value->val.string.len = j;
      }
      else
         memcpy(value->val.string.buff, str_start, value->val.string.len);
   }
   return buff;
}

static struct buffer parse_integer(struct buffer buff,
      struct rmsgpack_dom_value *value, const char **error)
{
   bool test   = false;

   value->type = RDT_INT;

#ifdef _WIN32
   test        = (sscanf(buff.data + buff.offset,
            "%I64d",
            (signed long long*)&value->val.int_) == 0);
#else
   test        = (sscanf(buff.data + buff.offset,
            "%lld",
            (signed long long*)&value->val.int_) == 0);
#endif

   if (test)
      raise_expected_number(buff.offset, error);
   else
   {
      while (isdigit((int)buff.data[buff.offset]))
         buff.offset++;
   }

   return buff;
}

static struct buffer parse_value(struct buffer buff,
      struct rmsgpack_dom_value *value, const char **error)
{
   buff = chomp(buff);

   if (peek(buff, "nil"))
   {
      buff.offset += strlen("nil");
      value->type = RDT_NULL;
   }
   else if (peek(buff, "true"))
   {
      buff.offset += strlen("true");
      value->type = RDT_BOOL;
      value->val.bool_ = 1;
   }
   else if (peek(buff, "false"))
   {
      buff.offset += strlen("false");
      value->type = RDT_BOOL;
      value->val.bool_ = 0;
   }
   else if (peek(buff, "b") || peek(buff, "\"") || peek(buff, "'"))
      buff = parse_string(buff, value, error);
   else if (isdigit((int)buff.data[buff.offset]))
      buff = parse_integer(buff, value, error);
   return buff;
}

static struct buffer get_ident(struct buffer buff,
      const char **ident,
      size_t *len, const char **error)
{
   char c = '\0';

   if (is_eot(buff))
   {
      raise_unexpected_eof(buff.offset, error);
      return buff;
   }

   *ident = buff.data + buff.offset;
   *len   = 0;
   peek_char(buff, &c, error);

   if (*error)
      goto clean;
   if (!isalpha((int)c))
      return buff;

   buff.offset++;
   *len = *len + 1;
   peek_char(buff, &c, error);

   while (!*error)
   {
      if (!(isalpha((int)c) || isdigit((int)c) || c == '_'))
         break;
      buff.offset++;
      *len = *len + 1;
      peek_char(buff, &c, error);
   }

clean:
   return buff;
}

static struct buffer parse_method_call(struct buffer buff,
      struct invocation *invocation, const char **error)
{
   size_t func_name_len;
   unsigned i;
   struct argument args[QUERY_MAX_ARGS];
   unsigned argi = 0;
   const char *func_name = NULL;
   struct registered_func *rf = registered_functions;

   invocation->func = NULL;

   buff = get_ident(buff, &func_name, &func_name_len, error);
   if (*error)
      goto clean;

   buff = chomp(buff);
   buff = expect_char(buff, '(', error);
   if (*error)
      goto clean;

   while (rf->name)
   {
      if (strncmp(rf->name, func_name, func_name_len) == 0)
      {
         invocation->func = rf->func;
         break;
      }
      rf++;
   }

   if (!invocation->func)
   {
      raise_unknown_function(buff.offset, func_name,
            func_name_len, error);
      goto clean;
   }

   buff = chomp(buff);
   while (!peek(buff, ")"))
   {
      if (argi >= QUERY_MAX_ARGS)
      {
         raise_too_many_arguments(error);
         goto clean;
      }

      buff = parse_argument(buff, &args[argi], error);

      if (*error)
         goto clean;

      argi++;
      buff = chomp(buff);
      buff = expect_char(buff, ',', error);

      if (*error)
      {
         *error = NULL;
         break;
      }
      buff = chomp(buff);
   }
   buff = expect_char(buff, ')', error);

   if (*error)
      goto clean;

   invocation->argc = argi;
   invocation->argv = (struct argument*)
      malloc(sizeof(struct argument) * argi);

   if (!invocation->argv)
   {
      raise_enomem(error);
      goto clean;
   }
   memcpy(invocation->argv, args,
         sizeof(struct argument) * argi);

   goto success;
clean:
   for (i = 0; i < argi; i++)
      argument_free(&args[i]);
success:
   return buff;
}

static struct buffer parse_table(struct buffer buff,
      struct invocation *invocation, const char **error)
{
   unsigned i;
   size_t ident_len;
   struct argument args[QUERY_MAX_ARGS];
   const char *ident_name = NULL;
   unsigned argi = 0;

   buff = chomp(buff);
   buff = expect_char(buff, '{', error);

   if (*error)
      goto clean;

   buff = chomp(buff);

   while (!peek(buff, "}"))
   {
      if (argi >= QUERY_MAX_ARGS)
      {
         raise_too_many_arguments(error);
         goto clean;
      }

      if (isalpha((int)buff.data[buff.offset]))
      {
         buff = get_ident(buff, &ident_name, &ident_len, error);

         if (!*error)
         {
            args[argi].a.value.type = RDT_STRING;
            args[argi].a.value.val.string.len = ident_len;
            args[argi].a.value.val.string.buff = (char*)calloc(
                  ident_len + 1,
                  sizeof(char)
                  );

            if (!args[argi].a.value.val.string.buff)
               goto clean;

            strncpy(
                  args[argi].a.value.val.string.buff,
                  ident_name,
                  ident_len
                  );
         }
      }
      else
         buff = parse_string(buff, &args[argi].a.value, error);

      if (*error)
         goto clean;

      args[argi].type = AT_VALUE;
      buff = chomp(buff);
      argi++;
      buff = expect_char(buff, ':', error);

      if (*error)
         goto clean;

      buff = chomp(buff);

      if (argi >= QUERY_MAX_ARGS)
      {
         raise_too_many_arguments(error);
         goto clean;
      }

      buff = parse_argument(buff, &args[argi], error);

      if (*error)
         goto clean;
      argi++;
      buff = chomp(buff);
      buff = expect_char(buff, ',', error);

      if (*error)
      {
         *error = NULL;
         break;
      }
      buff = chomp(buff);
   }

   buff = expect_char(buff, '}', error);

   if (*error)
      goto clean;

   invocation->func = all_map;
   invocation->argc = argi;
   invocation->argv = (struct argument*)
      malloc(sizeof(struct argument) * argi);

   if (!invocation->argv)
   {
      raise_enomem(error);
      goto clean;
   }
   memcpy(invocation->argv, args,
         sizeof(struct argument) * argi);

   goto success;
clean:
   for (i = 0; i < argi; i++)
      argument_free(&args[i]);
success:
   return buff;
}

static struct buffer parse_argument(struct buffer buff,
      struct argument *arg, const char **error)
{
   buff = chomp(buff);

   if (
         isalpha((int)buff.data[buff.offset])
         && !(
            peek(buff, "nil")
            || peek(buff, "true")
            || peek(buff, "false")
            || peek(buff, "b\"") || peek(buff, "b'") /* bin string prefix*/
            )
      )
   {
      arg->type = AT_FUNCTION;
      buff = parse_method_call(buff, &arg->a.invocation, error);
   }
   else if (peek(buff, "{"))
   {
      arg->type = AT_FUNCTION;
      buff = parse_table(buff, &arg->a.invocation, error);
   }
   else
   {
      arg->type = AT_VALUE;
      buff = parse_value(buff, &arg->a.value, error);
   }
   return buff;
}

void libretrodb_query_free(void *q)
{
   unsigned i;
   struct query *real_q = (struct query*)q;

   real_q->ref_count--;
   if (real_q->ref_count > 0)
      return;

   for (i = 0; i < real_q->root.argc; i++)
      argument_free(&real_q->root.argv[i]);

   free(real_q->root.argv);
   real_q->root.argv = NULL;
   real_q->root.argc = 0;
   free(real_q);
}

void *libretrodb_query_compile(libretrodb_t *db,
      const char *query, size_t buff_len, const char **error)
{
   struct buffer buff;
   struct query *q = (struct query*)calloc(1, sizeof(*q));

   if (!q)
      goto clean;

   q->ref_count = 1;
   buff.data    = query;
   buff.len     = buff_len;
   buff.offset  = 0;
   *error       = NULL;

   buff         = chomp(buff);

   if (peek(buff, "{"))
   {
      buff = parse_table(buff, &q->root, error);
      if (*error)
         goto clean;
   }
   else if (isalpha((int)buff.data[buff.offset]))
      buff = parse_method_call(buff, &q->root, error);

   buff = expect_eof(buff, error);
   if (*error)
      goto clean;

   if (!q->root.func)
   {
      raise_unexpected_eof(buff.offset, error);
      return NULL;
   }
   goto success;
clean:
   if (q)
      libretrodb_query_free(q);
success:
   return q;
}

void libretrodb_query_inc_ref(libretrodb_query_t *q)
{
   struct query *rq = (struct query*)q;
   if (rq)
      rq->ref_count += 1;
}

int libretrodb_query_filter(libretrodb_query_t *q,
      struct rmsgpack_dom_value *v)
{
   struct invocation inv = ((struct query *)q)->root;
   struct rmsgpack_dom_value res = inv.func(*v, inv.argc, inv.argv);
   return (res.type == RDT_BOOL && res.val.bool_);
}

