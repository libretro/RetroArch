#include <boolean.h>

#include "../core.h"
#include "../dynamic.h"

#include "mylist.h"
#include "mem_util.h"

bool input_is_dirty;
static MyList *inputStateList;

typedef struct InputListElement_t
{
   unsigned port;
   unsigned device;
   unsigned index;
   int16_t state[36];
} InputListElement;

typedef struct retro_core_t _retro_core_t;
extern _retro_core_t current_core;
extern struct retro_callbacks retro_ctx;

typedef bool(*LoadStateFunction)(const void*, size_t);

static function_t retro_reset_callback_original = NULL;
static LoadStateFunction retro_unserialize_callback_original = NULL;
static retro_input_state_t input_state_callback_original;

static void reset_hook(void);
static bool unserialze_hook(const void *buf, size_t size);

static void* InputListElementConstructor(void)
{
   const int size = sizeof(InputListElement);
   void      *ptr = malloc_zero(size);

   return ptr;
}

static void input_state_destory(void)
{
   mylist_destroy(&inputStateList);
}

static void input_state_setlast(unsigned port, unsigned device,
      unsigned index, unsigned id, int16_t value)
{
   int i;
   InputListElement *element;

   if (!inputStateList)
      mylist_create(&inputStateList, 16, InputListElementConstructor, free);

   /* find list item */
   for (i = 0; i < inputStateList->size; i++)
   {
      element = (InputListElement*)inputStateList->data[i];
      if (     element->port == port 
            && element->device == device && element->index == index)
      {
         element->state[id] = value;
         return;
      }
   }
   element = (InputListElement*)mylist_add_element(inputStateList);
   element->port = port;
   element->device = device;
   element->index = index;
   element->state[id] = value;
}

static int16_t input_state_getlast(unsigned port, unsigned device, unsigned index, unsigned id)
{
   int i;
   InputListElement *element = NULL;

   if (!inputStateList)
      return 0;

   /* find list item */
   for (i = 0; i < inputStateList->size; i++)
   {
      element = (InputListElement*)inputStateList->data[i];
      if (element->port == port && element->device == device && element->index == index)
         return element->state[id];
   }
   return 0;
}

static int16_t input_state_with_logging(unsigned port, unsigned device, unsigned index, unsigned id)
{
   if (input_state_callback_original != NULL)
   {
      int16_t result = input_state_callback_original(port, device, index, id);
      int16_t lastInput = input_state_getlast(port, device, index, id);
      if (result != lastInput)
         input_is_dirty = true;
      input_state_setlast(port, device, index, id, result);
      return result;
   }
   return 0;
}

static void reset_hook(void)
{
   input_is_dirty = true;
   if (retro_reset_callback_original)
      retro_reset_callback_original();
}

static bool unserialze_hook(const void *buf, size_t size)
{
   input_is_dirty = true;
   if (retro_unserialize_callback_original)
      return retro_unserialize_callback_original(buf, size);
   return false;
}

void add_input_state_hook(void)
{
   if (input_state_callback_original == NULL)
   {
      input_state_callback_original = retro_ctx.state_cb;
      retro_ctx.state_cb = input_state_with_logging;
      current_core.retro_set_input_state(retro_ctx.state_cb);
   }
   if (retro_reset_callback_original == NULL)
   {
      retro_reset_callback_original = current_core.retro_reset;
      current_core.retro_reset = reset_hook;
   }
   if (retro_unserialize_callback_original == NULL)
   {
      retro_unserialize_callback_original = current_core.retro_unserialize;
      current_core.retro_unserialize = unserialze_hook;
   }
}

void remove_input_state_hook(void)
{
   if (input_state_callback_original != NULL)
   {
      retro_ctx.state_cb = input_state_callback_original;
      current_core.retro_set_input_state(retro_ctx.state_cb);
      input_state_callback_original = NULL;
      input_state_destory();
   }
   if (retro_reset_callback_original != NULL)
   {
      current_core.retro_reset = retro_reset_callback_original;
      retro_reset_callback_original = NULL;
   }
   if (retro_unserialize_callback_original != NULL)
   {
      current_core.retro_unserialize = retro_unserialize_callback_original;
      retro_unserialize_callback_original = NULL;
   }
}

