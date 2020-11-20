/* Copyright  (C) 2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (json.h).
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

#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <formats/jsonsax.h>

#include "json.h"

struct parser_stack_item_t;

struct parser_stack_item_t
{
   struct json_node_t *node;
   struct parser_stack_item_t *previous;
   char *key;
   size_t key_len;
};

struct parser_state_t
{
   struct json_node_t *root;
   struct parser_stack_item_t *stack;
};

static void json_array_add_item(struct json_array_t *array, struct json_node_t *node)
{
   struct json_array_item_t *cur_item;

   if (!array->element)
   {
      array->element = (struct json_array_item_t *)calloc(1, sizeof(struct json_array_item_t));
      array->element->value = node;
   } else
   {
      cur_item = array->element;
      while (cur_item->next)
      {
         cur_item = cur_item->next;
      }

      cur_item->next = (struct json_array_item_t *)calloc(1, sizeof(struct json_array_item_t));
      cur_item->next->value = node;
   }
}

static void json_map_add_pair(struct json_map_t *map, char *key, size_t key_len, struct json_node_t *node)
{
   struct json_map_pair_t *cur_pair;

   if (!map->pair)
   {
      map->pair = (struct json_map_pair_t *)calloc(1, sizeof(struct json_map_pair_t));
      map->pair->key = key;
      map->pair->key_len = key_len;
      map->pair->value = node;
   } else
   {
      cur_pair = map->pair;
      while (cur_pair->next)
      {
         cur_pair = cur_pair->next;
      }

      cur_pair->next = (struct json_map_pair_t *)calloc(1, sizeof(struct json_map_pair_t));
      cur_pair->next->key = key;
      cur_pair->next->key_len = key_len;
      cur_pair->next->value = node;
   }
}

bool json_map_have_key(struct json_map_t map, const char *key)
{
   struct json_map_pair_t *cur_pair;
   size_t key_len;

   if (!map.pair)
   {
      return false;
   }

   key_len = strlen(key);

   cur_pair = map.pair;
   while (cur_pair)
   {
      if (cur_pair->key_len == key_len && !strncmp(key, cur_pair->key, key_len))
      {
         return true;
      } else
      {
         cur_pair = cur_pair->next;
      }
   }

   return false;
}

struct json_node_t *json_map_get_value(struct json_map_t map, const char *key)
{
   struct json_map_pair_t *cur_pair;
   size_t key_len;

   if (!map.pair)
   {
      return NULL;
   }

   key_len = strlen(key);

   cur_pair = map.pair;
   while (cur_pair)
   {
      if (cur_pair->key_len == key_len && !strncmp(key, cur_pair->key, key_len))
      {
         return cur_pair->value;
      } else
      {
         cur_pair = cur_pair->next;
      }
   }

   return NULL;
}

bool json_map_have_value_null(struct json_map_t map, const char *key)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   return node && node->node_type == NULL_VALUE;
}

bool json_map_get_value_boolean(struct json_map_t map, const char *key, bool *value)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   if (node && node->node_type == BOOLEAN_VALUE)
   {
      *value = node->value.boolean_value;
      return true;
   } else
   {
      return false;
   }
}

bool json_map_get_value_string(struct json_map_t map, const char *key, char **value, size_t *length)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   if (node && node->node_type == STRING_VALUE)
   {
      *value = node->value.string_value.string;
      *length = node->value.string_value.length;
      return true;
   } else
   {
      return false;
   }
}

bool json_map_get_value_int(struct json_map_t map, const char *key, int64_t *value)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   if (node && node->node_type == INTEGER_VALUE)
   {
      *value = node->value.int_value;
      return true;
   } else
   {
      return false;
   }
}

bool json_map_get_value_double(struct json_map_t map, const char *key, double *value)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   if (node && node->node_type == DOUBLE_VALUE)
   {
      *value = node->value.double_value;
      return true;
   } else
   {
      return false;
   }
}

bool json_map_get_value_array(struct json_map_t map, const char *key, struct json_array_t **value)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   if (node && node->node_type == ARRAY_VALUE)
   {
      *value = &(node->value.array_value);
      return true;
   } else
   {
      return false;
   }
}

bool json_map_get_value_map(struct json_map_t map, const char *key, struct json_map_t **value)
{
   struct json_node_t *node;

   node = json_map_get_value(map, key);
   if (node && node->node_type == OBJECT_VALUE)
   {
      *value = &(node->value.map_value);
      return true;
   } else
   {
      return false;
   }
}

static int start_document_handler(void* userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;

   if (state->root)
   {
      return 1;
   }

   return 0;
}

static int end_document_handler(void* userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct parser_stack_item_t *item;

   if (state->root)
   {
      while (state->stack)
      {
         item = state->stack->previous;
         state->stack->previous = NULL;
         free(state->stack);

         state->stack = item;
      }

      return 0;
   }

   return 1;
}

bool can_add_node(struct parser_state_t *state)
{
   if (!state->root)
   {
      return true;
   }

   if (state->stack && state->stack->node)
   {
      if (state->stack->node->node_type == ARRAY_VALUE)
      {
         return true;
      } else if (state->stack->node->node_type == OBJECT_VALUE && state->stack->key)
      {
         return true;
      }
   }

   return false;
}

static void add_node(struct parser_state_t *state, struct json_node_t *new_node)
{
   if (!state->root)
   {
      state->root = new_node;
   } else if (state->stack->node->node_type == ARRAY_VALUE)
   {
      json_array_add_item(&(state->stack->node->value.array_value), new_node);
   } else if (state->stack->node->node_type == OBJECT_VALUE)
   {
      json_map_add_pair(&(state->stack->node->value.map_value), state->stack->key, state->stack->key_len, new_node);
      state->stack->key = NULL;
      state->stack->key_len = 0;
   }
}

static int start_object_handler(void *userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct parser_stack_item_t *stack_item;
   struct json_node_t *new_node;

   if (can_add_node(state))
   {
      new_node = (struct json_node_t *)calloc(1, sizeof(struct json_node_t));
      new_node->node_type = OBJECT_VALUE;
      new_node->value.map_value.pair = NULL;
      add_node(state, new_node);

      stack_item = (struct parser_stack_item_t *)calloc(1, sizeof(struct parser_stack_item_t));
      stack_item->node = new_node;
      stack_item->previous = state->stack;
      state->stack = stack_item;

      return 0;
   }

   return 1;
}

static int end_object_handler(void* userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct parser_stack_item_t *stack_item;

   if (state->stack && state->stack->node && state->stack->node->node_type == OBJECT_VALUE)
   {
      stack_item = state->stack;
      state->stack = state->stack->previous;
      free(stack_item);

      return 0;
   }

   return 1;
}

static int start_array_handler(void* userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct parser_stack_item_t *stack_item;
   struct json_node_t *new_node;

   if (can_add_node(state))
   {
      new_node = (struct json_node_t *)calloc(1, sizeof(struct json_node_t));
      new_node->node_type = ARRAY_VALUE;
      new_node->value.array_value.element = NULL;
      add_node(state, new_node);

      stack_item = (struct parser_stack_item_t *)calloc(1, sizeof(struct parser_stack_item_t));
      stack_item->node = new_node;
      stack_item->previous = state->stack;
      state->stack = stack_item;

      return 0;
   }

   return 1;
}

static int end_array_handler(void* userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct parser_stack_item_t *stack_item;

   if (state->stack && state->stack->node && state->stack->node->node_type == ARRAY_VALUE)
   {
      stack_item = state->stack;
      state->stack = state->stack->previous;
      free(stack_item);

      return 0;
   } else
   {
      return 1;
   }
}

static int key_handler(void* userdata, const char* name, size_t length)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;

   if (state->stack && state->stack->node && state->stack->node->node_type == OBJECT_VALUE && !state->stack->key)
   {
      state->stack->key = (char *)name;
      state->stack->key_len = length;
      return 0;
   } else
   {
      return 1;
   }
}

static int array_index_handler(void* userdata, unsigned int index)
{
   return 0;
}

static int string_handler(void* userdata, const char* string, size_t length)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct json_node_t *new_node;

   if (can_add_node(state))
   {
      new_node = (struct json_node_t *)calloc(1, sizeof(struct json_node_t));
      new_node->node_type = STRING_VALUE;
      new_node->value.string_value.string = (char *)string;
      new_node->value.string_value.length = length;
      add_node(state, new_node);

      return 0;
   }

   return 1;
}

static int number_handler(void* userdata, const char* number, size_t length)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct json_node_t *new_node;
   int64_t int_value = 0;
   double float_value = 0.0;
   size_t i;
   bool is_int = true;
   char *temp_string;

   if (can_add_node(state))
   {
      for (i = 0;i < length;i++)
      {
         if (number[i] == '.')
         {
            is_int = false;
            break;
         }
      }

      temp_string = (char *)calloc(length + 1, sizeof(char));
      strncpy(temp_string, number, length);
      if (is_int)
      {
         int_value = atoll(temp_string);
      } else
      {
         float_value = atof(temp_string);
      }
      free(temp_string);

      new_node = (struct json_node_t *)calloc(1, sizeof(struct json_node_t));
      if (is_int)
      {
         new_node->node_type = INTEGER_VALUE;
         new_node->value.int_value = int_value;
      } else
      {
         new_node->node_type = DOUBLE_VALUE;
         new_node->value.double_value = float_value;
      }

      add_node(state, new_node);
      return 0;
   }

   return 1;
}

static int boolean_handler(void* userdata, int istrue)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct json_node_t *new_node;

   if (can_add_node(state))
   {
      new_node = (struct json_node_t *)calloc(1, sizeof(struct json_node_t));
      new_node->node_type = BOOLEAN_VALUE;
      new_node->value.boolean_value = istrue ? true : false;
      add_node(state, new_node);

      return 0;
   }

   return 1;
}

static int null_handler(void* userdata)
{
   struct parser_state_t *state = (struct parser_state_t *)userdata;
   struct json_node_t *new_node;

   if (can_add_node(state))
   {
      new_node = (struct json_node_t *)calloc(1, sizeof(struct json_node_t));
      new_node->node_type = NULL_VALUE;
      new_node->value.null_value = NULL;
      add_node(state, new_node);

      return 0;
   }

   return 1;
}

static void array_free(struct json_array_t array)
{
   struct json_array_item_t *item;

   while (array.element)
   {
      json_node_free(array.element->value);
      item = array.element->next;
      free(array.element);
      array.element = item;
   }
}

static void map_free(struct json_map_t map)
{
   struct json_map_pair_t *pair;

   while (map.pair)
   {
      json_node_free(map.pair->value);
      pair = map.pair->next;
      free(map.pair);
      map.pair = pair;
   }
}

void json_node_free(struct json_node_t *node)
{
   if (node)
   {
      switch (node->node_type)
      {
      case ARRAY_VALUE:
         array_free(node->value.array_value);
         break;
      case OBJECT_VALUE:
         map_free(node->value.map_value);
         break;
      default:
         break;
      }

      free(node);
   }
}

void parser_state_free(struct parser_state_t *state)
{
   struct parser_stack_item_t *stack_item;

   if (state)
   {
      while (state->stack)
      {
         json_node_free(state->stack->node);

         stack_item = state->stack->previous;
         free(state->stack);
         state->stack = stack_item;
      }

      free(state);
   }
}

struct json_node_t *string_to_json(char *s)
{
   struct parser_state_t *state;
   jsonsax_handlers_t *handlers;
   struct json_node_t *ret_val;

   handlers = (jsonsax_handlers_t *)calloc(1, sizeof(jsonsax_handlers_t));
   handlers->start_document = start_document_handler;
   handlers->end_document = end_document_handler;
   handlers->start_object = start_object_handler;
   handlers->end_object = end_object_handler;
   handlers->start_array = start_array_handler;
   handlers->end_array = end_array_handler;
   handlers->key = key_handler;
   handlers->array_index = array_index_handler;
   handlers->string = string_handler;
   handlers->number = number_handler;
   handlers->boolean = boolean_handler;
   handlers->null = null_handler;

   state = (struct parser_state_t *)calloc(1, sizeof(struct parser_state_t));

   if (!jsonsax_parse(s, handlers, state))
   {
      free(handlers);

      ret_val = state->root;
      parser_state_free(state);
      return ret_val;
   } else
   {
      free(handlers);

      json_node_free(state->root);
      parser_state_free(state);
      return NULL;
   }
}
