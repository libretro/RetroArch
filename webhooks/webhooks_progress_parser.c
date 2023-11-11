#include <stdlib.h>

#include "webhooks_progress_parser.h"

const int ERROR_MESSAGE_LENGTH = 512;
const int PROGRESS_LENGTH = 2048;

struct wpp_game_event_t {
  unsigned int id;
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
  WEBHOOKS_LOG(WEBHOOKS_TAG "Parsing the downloaded game progress\n");

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
  
  rc_api_response_t* api_response = (rc_api_response_t*)&game_progress_response;
  
  int result = rc_json_parse_response(api_response, game_progress, fields, sizeof(fields) / sizeof(fields[0]));

  if (result != 0) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to parse the downloaded game progress. Result is '%d'\n", result);
    return result;
  }

  rc_json_field_t iterator;
  rc_api_buffer_t* api_buffer = malloc(sizeof(rc_api_buffer_t));
  rc_buf_init(api_buffer);

  if (!rc_json_get_required_array(&game_progress_response.num_game_events, &iterator, api_response, &fields[3], "Events")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to get the 'Events' array from the downloaded game progress\n");
    free(api_buffer);
    return -1;
  }

  if (!rc_json_get_required_string(&game_progress_response.progress, api_response, &fields[2], "Progress")) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to get the 'Progress' string from the downloaded game progress\n");
    free(api_buffer);
    return -1;
  }

  WEBHOOKS_LOG(WEBHOOKS_TAG "Game progress contains '%d' supported events\n", game_progress_response.num_game_events);

  if (game_progress_response.num_game_events) {

    game_progress_response.game_events = (struct wpp_game_event_t*)rc_buf_alloc(api_buffer, game_progress_response.num_game_events * sizeof(struct wpp_game_event_t));

    if (!game_progress_response.game_events) {
      WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to allocate memory for the downloaded game events\n");
      free(api_buffer);
      return -1;
    }

    struct wpp_game_event_t* game_event = game_progress_response.game_events;

    while (rc_json_get_array_entry_object(events_fields, sizeof(events_fields) / sizeof(events_fields[0]), &iterator)) {

      if (!rc_json_get_required_unum(&game_event->id, api_response, &events_fields[0], "Id")) {
        WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to get the 'Id' from the game event\n");
        free(api_buffer);
        return -1;
      }

      if (!rc_json_get_required_string(&game_event->macro, api_response, &events_fields[1], "Macro")) {
        WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to get the 'Macro' from the game event ID '%d'\n", game_event->id);
        free(api_buffer);
        return -1;
      }

      //  Activate the trigger.
      result = rc_runtime_activate_achievement(&locals.runtime, game_event->id, game_event->macro, NULL, 0);

      if (result != RC_OK) {
        WEBHOOKS_LOG(WEBHOOKS_TAG "Unable to activate the game event ID '%d'\n", game_event->id);
        free(api_buffer);
        return -1;
      }

      WEBHOOKS_LOG(WEBHOOKS_TAG "Game event '%d' is activated\n", game_event->id);

      ++game_event;
    }
  }

  result = rc_runtime_activate_richpresence(runtime, game_progress_response.progress, NULL, 0);

  if (result != 0) {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Rich presence could not be activated (Result='%d')\n", result);
  }
  else {
    WEBHOOKS_LOG(WEBHOOKS_TAG "Rich presence has been been activated\n");
  }

  free(api_buffer);

  return result;
}
