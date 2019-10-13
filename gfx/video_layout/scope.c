#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "scope.h"

union number
{
   int   val_int;
   float val_dec;
};

typedef struct generator
{
   bool         is_decimal;
   union number value;
   union number increment;
   int          shift;
}
generator_t;

struct param
{
   char        *name;
   char        *value;
   generator_t *generator;
   param_t     *prev;
   int          level;
};

static void param_deinit(param_t *param)
{
   free(param->generator);
   free(param->value);
   free(param->name);
}

static param_t *param_find(scope_t *scope, const char *name, int level)
{
   param_t *param = scope->param;

   while (param && param->level >= level)
   {
      if (strcmp(param->name, name) == 0)
         return param;
      param = param->prev;
   }

   return NULL;
}

void scope_init(scope_t *scope)
{
   scope->level          = 0;

   scope->param          = NULL;
   scope->elements       = NULL;
   scope->elements_count = 0;
   scope->groups         = NULL;
   scope->groups_count   = 0;
}

void scope_deinit(scope_t *scope)
{
   int i;
   param_t *param;
   param_t *prev;

   for (i = 0; i < scope->elements_count; ++i)
      element_deinit(&scope->elements[i]);
   free(scope->elements);

   for (i = 0; i < scope->groups_count; ++i)
      view_deinit(&scope->groups[i]);
   free(scope->groups);

   for (param = scope->param; param; param = prev)
   {
      prev = param->prev;
      param_deinit(param);
      free(param);
   }
}

void scope_push(scope_t *scope)
{
   ++scope->level;
}

void scope_pop(scope_t *scope)
{
   param_t *param;

   --scope->level;

   while ((param = scope->param))
   {
      if (param->level <= scope->level)
         break;

      scope->param = param->prev;
      param_deinit(param);
      free(param);
   }
}

void scope_repeat(scope_t *scope)
{
   param_t *param;

   for (
         param = scope->param;
         param && param->level >= scope->level;
         param = param->prev)
   {
      generator_t *gen;
      if ((gen = param->generator))
      {
         char tmp[SCOPE_BUFFER_SIZE];
         tmp[0] = '\0';

         if (gen->is_decimal)
         {
            gen->value.val_dec += gen->increment.val_dec;
            if (gen->shift > 0)
               gen->value.val_dec = (float)((int)gen->value.val_dec << gen->shift);
            else if (gen->shift < 0)
               gen->value.val_dec = (float)((int)gen->value.val_dec >> -gen->shift);
            sprintf(tmp, "%f", gen->value.val_dec);
         }
         else
         {
            gen->value.val_int += gen->increment.val_int;
            if(gen->shift > 0)
               gen->value.val_int <<= gen->shift;
            else if (gen->shift < 0)
               gen->value.val_int >>= -gen->shift;
            sprintf(tmp, "%d", gen->value.val_int);
         }

         set_string(&param->value, tmp);
      }
   }
}


void scope_param(scope_t *scope, const char *name, const char *value)
{
   param_t *param;
   char *eval_name = init_string(scope_eval(scope, name));
   char *eval_value = init_string(scope_eval(scope, value));

   if ((param = param_find(scope, eval_name, scope->level)))
   {
      free(param->value);
      param->value = eval_value;
   }
   else
   {
      param = (param_t*)malloc(sizeof(param_t));
      param->name = init_string(name);
      param->value = eval_value;
      param->generator = NULL;
      param->level = scope->level;
      param->prev = scope->param;
      scope->param = param;
   }

   free(eval_name);
}

void scope_generator(scope_t *scope, const char *name, const char *start, const char *increment, const char *lshift, const char *rshift)
{
   char *e_val;
   char *e_inc;
   generator_t *gen;
   param_t *param;
   char *e_name = init_string(scope_eval(scope, name));

   if (param_find(scope, e_name, scope->level))
   {
      free(e_name);
      return;
   }

   e_val = init_string(scope_eval(scope, start));
   e_inc = init_string(scope_eval(scope, increment));

   gen = (generator_t*)malloc(sizeof(generator_t));

   param = (param_t*)malloc(sizeof(param_t));
   param->name = init_string(e_name);
   param->value = init_string(e_val);
   param->generator = gen;
   param->level = scope->level;
   param->prev = scope->param;
   scope->param = param;

   gen->is_decimal = is_decimal(e_val) | is_decimal(e_inc);

   if (gen->is_decimal)
   {
      gen->value.val_dec = get_dec(e_val);
      gen->increment.val_dec = get_dec(e_inc);
   }
   else
   {
      gen->value.val_int = get_int(e_val);
      gen->increment.val_int = get_int(e_inc);
   }

   gen->shift = 0;

   if (lshift)
      gen->shift += get_int(scope_eval(scope, lshift));

   if (rshift)
      gen->shift -= get_int(scope_eval(scope, rshift));

   free(e_inc);
   free(e_val);
   free(e_name);
}

const char *scope_eval(scope_t *scope, const char *src)
{
   const char* next;
   bool in_var;
   char tmp[SCOPE_BUFFER_SIZE];

   if (!src)
      return NULL;

   scope->eval[0] = '\0';
   next = src;

   while (next[0] != '\0')
   {
      const char* cur;
      cur = next;

      if ((in_var = (next[0] == '~')))
         ++cur;

      next = strchr(cur, '~');

      if (next && next != cur)
      {
         size_t len;
         len = next - cur;

         if (in_var)
         {
            param_t *param;

            strncpy(tmp, cur, len);
            tmp[len] = '\0';

            if ((param = param_find(scope, tmp, 0)))
               strcat(scope->eval, param->value);
            else
               strcat(scope->eval, tmp);

            ++next;
         }
         else
         {
            strncat(scope->eval, cur, len);
         }
      }
      else
      {
         if (in_var)
            --cur;
         strcat(scope->eval, cur);
         break;
      }
   }

   return scope->eval;
}

element_t *scope_add_element(scope_t *scope)
{
   element_t *elem;

   vec_size((void**)&scope->elements, sizeof(element_t), ++scope->elements_count);

   elem = &scope->elements[scope->elements_count - 1];
   element_init(elem, NULL, 0);

   return elem;
}

element_t *scope_find_element(scope_t *scope, const char *name)
{
   int i;

   for (i = 0; i < scope->elements_count; ++i)
   {
      if (strcmp(name, scope->elements[i].name) == 0)
         return &scope->elements[i];
   }

   return NULL;
}

view_t *scope_add_group(scope_t *scope)
{
   view_t *group;

   vec_size((void**)&scope->groups, sizeof(view_t), ++scope->groups_count);

   group = &scope->groups[scope->groups_count - 1];
   view_init(group, NULL);

   return group;
}

view_t *scope_find_group(scope_t *scope, const char *name)
{
   int i;

   for (i = 0; i < scope->groups_count; ++i)
   {
      if (strcmp(name, scope->groups[i].name) == 0)
         return &scope->groups[i];
   }

   return NULL;
}
