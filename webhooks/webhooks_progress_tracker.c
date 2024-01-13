#include "include/webhooks_progress_tracker.h"

#include "include/webhooks.h"

#include "../deps/rcheevos/src/rcheevos/rc_internal.h"
#include "../deps/rcheevos/src/rc_compat.h"

#include "rc_runtime_types.h"

#include <stdbool.h>

typedef struct progress_t progress_t;

struct progress_t {
  unsigned short display_type;
  const char* text;
  rc_typed_value_t value;
  struct progress_t* next;
};

char frame_progress[2048];
char last_progress[2048];
bool first_run = false;

//  ---------------------------------------------------------------------------
//  Clears the last progress
//  ---------------------------------------------------------------------------
void wpt_clear_progress() {
  memset(last_progress, 0, sizeof(last_progress));
}

//  ---------------------------------------------------------------------------
//  Returns the last progress computed
//  ---------------------------------------------------------------------------
const char* wpt_get_last_progress() {
  return last_progress;
}

//  ---------------------------------------------------------------------------
//
//  ---------------------------------------------------------------------------
int wpt_process_frame(rc_runtime_t* runtime)
{
  rc_runtime_richpresence_t* runtime_richpresence = runtime->richpresence;
  
  if (runtime_richpresence == NULL) {
    //  Not initialized yet.
    first_run = true;
    return 0;
  }

  //  Gets the latest values from the memory.
  rc_update_memref_values(runtime->memrefs, &wb_peek, NULL);
  rc_update_variables(runtime->variables, &wb_peek, NULL, NULL);

  rc_richpresence_t* richpresence = runtime_richpresence->richpresence;

  //
  rc_richpresence_display_t* new_display = NULL;

  //  This is needed to support all the rich presences defined with Retro Achievement.
  //  For the webhooks, there should not be multiple displays needed but only one.
  for (rc_richpresence_display_t* display = richpresence->first_display; display; display = display->next) {
    if (!display->next) {
      new_display = display;
      break;
    }
  }

  rc_richpresence_display_part_t* new_display_part = NULL;

  if(new_display != NULL)
    new_display_part = new_display->display;

  int charactersWritten = 0;
  int frame_progress_position = 0;

  if(new_display_part != NULL)
  {
    for (; new_display_part; new_display_part = new_display_part->next) {

      rc_typed_value_t new_value;

      switch (new_display_part->display_type) {
      case 101: //RC_FORMAT_STRING:
        charactersWritten = sprintf(&frame_progress[frame_progress_position], "%s", new_display_part->text);
        frame_progress_position += charactersWritten;
        break;
      case 102: //RC_FORMAT_LOOKUP:
        //  LOOKUPs are kept for Retro Achievement's Rich Presence support.
      default:
        if (new_display_part->value != NULL) {
          rc_typed_value_from_memref_value(&new_value, new_display_part->value);
          switch(new_value.type) {
          case RC_VALUE_TYPE_UNSIGNED:
            charactersWritten = sprintf(&frame_progress[frame_progress_position], "0x%X", new_value.value.u32);
            break;
          case RC_VALUE_TYPE_SIGNED:
            charactersWritten = sprintf(&frame_progress[frame_progress_position], "0x%X", new_value.value.i32);
            break;
          case RC_VALUE_TYPE_FLOAT:
            charactersWritten = sprintf(&frame_progress[frame_progress_position], "%.2f", new_value.value.f32);
            break;
          }
          frame_progress_position += charactersWritten;
        }
        break;
      }
    }
  }

  frame_progress[frame_progress_position] = '\0';

  //  ----------------------------------------------------------------------

  bool state_changed = strcmp(frame_progress, last_progress) != 0 || first_run;
  first_run = false;
  
  if (state_changed) {
    strlcpy(last_progress, frame_progress, 2047);
    return PROGRESS_UPDATED;
  }

  return PROGRESS_UNCHANGED;
}
