#include <stdlib.h>

#include <boolean.h>

#include "../core.h"
#include "../dynamic.h"

#include "mylist.h"
#include "mem_util.h"
#include "dirty_input.h"

bool input_is_dirty             = false;
static MyList *input_state_list = NULL;

typedef struct InputListElement_t
{
   unsigned port;
   unsigned device;
   unsigned index;
   int16_t state[36];
} InputListElement;

extern struct retro_core_t current_core;
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
   void      *ptr = calloc(1, size);

   return ptr;
}

static void input_state_destroy(void)
{
   mylist_destroy(&input_state_list);
}

static void input_state_set_last(unsigned port, unsigned device,
      unsigned index, unsigned id, int16_t value)
{
   unsigned i;
   InputListElement *element = NULL;

   if (!input_state_list)
      mylist_create(&input_state_list, 16,
            InputListElementConstructor, free);

   /* find list item */
   for (i = 0; i < (unsigned)input_state_list->size; i++)
   {
      element = (InputListElement*)input_state_list->data[i];
      if (     element->port == port 
            && element->device == device && element->index == index)
      {
         element->state[id] = value;
         return;
      }
   }

   element            = (InputListElement*)
      mylist_add_element(input_state_list);
   element->port      = port;
   element->device    = device;
   element->index     = index;
   element->state[id] = value;
}

static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   unsigned i;

   if (!input_state_list)
      return 0;

   /* find list item */
   for (i = 0; i < (unsigned)input_state_list->size; i++)
   {
      InputListElement *element = 
         (InputListElement*)input_state_list->data[i];

      if (  (element->port   == port)   && 
            (element->device == device) &&
            (element->index  == index)
         )
         return element->state[id];
   }
   return 0;
}

static int16_t input_state_with_logging(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   if (input_state_callback_original)
   {
      int16_t result     = input_state_callback_original(
            port, device, index, id);
      int16_t last_input = input_state_get_last(port, device, index, id);
      if (result != last_input)
         input_is_dirty = true;
      input_state_set_last(port, device, index, id, result);
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
   if (!input_state_callback_original)
   {
      input_state_callback_original = retro_ctx.state_cb;
      retro_ctx.state_cb            = input_state_with_logging;
      current_core.retro_set_input_state(retro_ctx.state_cb);
   }

   if (!retro_reset_callback_original)
   {
      retro_reset_callback_original = current_core.retro_reset;
      current_core.retro_reset      = reset_hook;
   }

   if (!retro_unserialize_callback_original)
   {
      retro_unserialize_callback_original = current_core.retro_unserialize;
      current_core.retro_unserialize      = unserialze_hook;
   }
}

void remove_input_state_hook(void)
{
   if (input_state_callback_original)
   {
      retro_ctx.state_cb            = input_state_callback_original;
      current_core.retro_set_input_state(retro_ctx.state_cb);
      input_state_callback_original = NULL;
      input_state_destroy();
   }

   if (retro_reset_callback_original)
   {
      current_core.retro_reset      = retro_reset_callback_original;
      retro_reset_callback_original = NULL;
   }

   if (retro_unserialize_callback_original)
   {
      current_core.retro_unserialize      = retro_unserialize_callback_original;
      retro_unserialize_callback_original = NULL;
   }
}

