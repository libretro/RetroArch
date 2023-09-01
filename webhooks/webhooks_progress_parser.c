#include <stdlib.h>

#include "webhooks_progress_parser.h"

const int ERROR_MESSAGE_LENGTH = 512;
const int PROGRESS_LENGTH = 2048;

struct wpp_game_event_t {
  int id;
  const char* macro;
};

struct wpp_game_progress_t {
  int succeeded;
  char error_message[ERROR_MESSAGE_LENGTH];
  const char* progress;
  struct wpp_game_event_t* game_events;
  unsigned num_game_events;
};

struct wpp_game_progress_t game_progress_response;

//  ---------------------------------------------------------------------------
//  Extracts the game progress information as well as the game events.
//  ---------------------------------------------------------------------------
int wpp_parse_game_progress
(
  const char* game_progress,
  rc_runtime_t* runtime
)
{
  //  For now, the parser acts as a Facade to the rc_runtime_activate_richpresence
  //  that actually does the parse of the macro.
  //  This gives an opportunity to branch out in the future in case the
  //  need to Rich Presence diverge from the need of the webhooks.

  rc_json_field_t fields[] = {
    RC_JSON_NEW_FIELD("Success"),
    RC_JSON_NEW_FIELD("Error"),
    RC_JSON_NEW_FIELD("Progress"),
    RC_JSON_NEW_FIELD("Events")
  };

  rc_json_field_t events_fields[] = {
    RC_JSON_NEW_FIELD("Id"),
    RC_JSON_NEW_FIELD("Macro")
  };

  rc_json_field_t iterator;
  
  rc_api_buffer_t* api_buffer = malloc(sizeof(rc_api_buffer_t));
  rc_buf_init(api_buffer);

  int result = rc_json_parse_response(&game_progress_response, game_progress, fields, sizeof(fields) / sizeof(fields[0]));

  if (result != 0)
    return result;

  if (!rc_json_get_required_array(&game_progress_response.num_game_events, &iterator, &api_buffer, &fields[3], "Events"))
    return -1;

  if (!rc_json_get_required_string(&game_progress_response.progress, &api_buffer, &fields[2], "Progress"))
    return -1;

  if (game_progress_response.num_game_events) {
   game_progress_response.game_events = (struct wpp_game_event_t*)rc_buf_alloc(api_buffer, game_progress_response.num_game_events * sizeof(struct wpp_game_event_t));
    if (!game_progress_response.game_events)
      return RC_OUT_OF_MEMORY;

    struct wpp_game_event_t* game_event = game_progress_response.game_events;

    while (rc_json_get_array_entry_object(events_fields, sizeof(events_fields) / sizeof(events_fields[0]), &iterator)) {
      if (!rc_json_get_required_unum(&game_event->id, &game_progress_response, &events_fields[0], "Id"))
        return RC_MISSING_VALUE;
      if (!rc_json_get_required_string(&game_event->macro, &game_progress_response, &events_fields[1], "Macro"))
        return RC_MISSING_VALUE;

      //  Activate the trigger.
      //result = rc_runtime_activate_achievement(&locals.runtime, game_event->id, game_event->macro, NULL, 0);

      ++game_event;
    }
  }
  
  result = rc_runtime_activate_richpresence(runtime, game_progress_response.progress, NULL, 0);

  //  -->
  //int result = rc_runtime_activate_achievement(&locals.runtime, 1, "0xH0048=5_0xH0049>=20_0xH0050=0_0xH00b0=1_0xH0d65=0_0xH0d45=221", NULL, 0);

  //int result = rc_runtime_activate_richpresence(runtime, game_progress, NULL, 0);

  return -1;
}
