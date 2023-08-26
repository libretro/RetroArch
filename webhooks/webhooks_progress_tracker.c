#include "webhooks_progress_tracker.h"

#include "../deps/rcheevos/src/rcheevos/rc_internal.h"
#include "../deps/rcheevos/src/rcheevos/rc_compat.h"

#include "rc_runtime_types.h"

#include <ctype.h>
#include <stdbool.h>

typedef struct progress_t progress_t;

struct progress_t {
  unsigned short display_type;
  const char* text;
  rc_typed_value_t value;
  struct progress_t* next;
};

int frames_count = 0;
long frames_counter = 0;

const int REFRESH_RATE = 30;

struct progress_t* root_display;
struct progress_t* last_display;

static unsigned unused_function(unsigned address, unsigned num_bytes, void* ud)
{
  return 0;
}

int wpt_process_frame(rc_runtime_t* runtime)
{
  rc_runtime_richpresence_t* runtime_richpresence = runtime->richpresence;
  
  if(runtime_richpresence == NULL) {
    //  Not initialized yet.
    frames_counter = 0;
    return 0;
  }
  
  frames_counter++;
  
  rc_richpresence_t* richpresence = runtime_richpresence->richpresence;
  
  //  Gets the latest values from the memory.
  rc_update_memref_values(richpresence->memrefs, &unused_function, NULL);
  rc_update_variables(richpresence->variables, &unused_function, NULL, NULL);

  //  
  rc_richpresence_display_t* new_display;
  progress_t* copied_display_part;
  
  for (rc_richpresence_display_t* display = richpresence->first_display; display; display = display->next) {
    if (!display->next) {
      new_display = display;
      break;
    }
  }
  
  //  --------------------------------------------------------------------
  //  Creates a copy of the displays to be used for frame by frame comparison
  //  --------------------------------------------------------------------
  if (frames_count == 0) {

    root_display = malloc(sizeof(struct progress_t));
    copied_display_part = root_display;
    
    rc_richpresence_display_part_t* new_display_part = new_display->display;
  
    for (; new_display_part; new_display_part = new_display_part->next) {
      
      if (copied_display_part->next != NULL) {
        copied_display_part = copied_display_part->next;
      }
      
      unsigned long text_length;
      rc_typed_value_t new_value;
      
      copied_display_part->display_type = new_display_part->display_type;
      
      switch (copied_display_part->display_type) {
        case 101: //RC_FORMAT_STRING:
          text_length = strlen(new_display_part->text);
          copied_display_part->text = malloc(text_length + 1);
          memcpy(copied_display_part->text, new_display_part->text, text_length + 1);
        break;
        case 102: //RC_FORMAT_LOOKUP:
          //  LOOKUPs are kept for Retro Achievement Rich Presence support.
        default:
          rc_typed_value_from_memref_value(&new_value, new_display_part->value);
          copied_display_part->value = new_value;
          break;
      }
      
      if (new_display_part->next != NULL) {
        copied_display_part->next = malloc(sizeof(progress_t));
      }
    }
    
    //  ----------------------------------------------------------------------
    
               
    if (last_display != NULL) {
      
      bool state_changed = false;
      
      progress_t* new_display_part = root_display;
      progress_t* last_display_part = last_display;
      unsigned long last_text_length, new_text_length;
      rc_typed_value_t new_value;
      
      for (; new_display_part && last_display_part; new_display_part = new_display_part->next, last_display_part = last_display_part->next) {
        
        if (last_display_part->display_type != new_display_part->display_type) {
          state_changed = true;
          break;
        }
        
        switch (last_display_part->display_type) {
          case 101: //RC_FORMAT_STRING:
            last_text_length = strlen(new_display_part->text);
            new_text_length = strlen(last_display_part->text);
            
            state_changed = last_text_length != new_text_length || strcmp(new_display_part->text, last_display_part->text) != 0;
            break;
          case 102: //RC_FORMAT_LOOKUP:
          default:
            rc_typed_value_from_memref_value(&new_value, (const rc_memref_value_t*)&new_display_part->value);

            state_changed = new_value.value.u32 != last_display_part->value.value.u32
                    || new_value.value.i32 != last_display_part->value.value.i32
                    || new_value.value.f32 != last_display_part->value.value.f32
                    //|| new_value.type != last_display_part->value.type
                    ;
            break;
        }
        
        if (state_changed)
          break;
      }
      
      if (state_changed) {
        for (; last_display; last_display = last_display->next) {
          if (last_display->text != NULL) {
            free(last_display->text);
            last_display->text = NULL;
          }
        }
        
        last_display = root_display;
        return 1;
      }
      else {
        for (; root_display; root_display = root_display->next) {
          if (root_display->text != NULL) {
            free(root_display->text);
            root_display->text = NULL;
          }
        }
      }
    }
  }
  
  if (last_display == NULL) {
    last_display = root_display;
    return 1;
  }
  
  frames_count = (++frames_count) % 30;

  return 0;
}
