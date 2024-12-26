#ifndef RC_API_RUNTIME_H
#define RC_API_RUNTIME_H

#include "rc_api_request.h"

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Fetch Image --- */

/**
 * API parameters for a fetch image request.
 * NOTE: fetch image server response is the raw image data. There is no rc_api_process_fetch_image_response function.
 */
typedef struct rc_api_fetch_image_request_t {
  /* The name of the image to fetch */
  const char* image_name;
  /* The type of image to fetch */
  uint32_t image_type;
}
rc_api_fetch_image_request_t;

#define RC_IMAGE_TYPE_GAME 1
#define RC_IMAGE_TYPE_ACHIEVEMENT 2
#define RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED 3
#define RC_IMAGE_TYPE_USER 4

int rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params);

/* --- Resolve Hash --- */

/**
 * API parameters for a resolve hash request.
 */
typedef struct rc_api_resolve_hash_request_t {
  /* Unused - hash lookup does not require credentials */
  const char* username;
  /* Unused - hash lookup does not require credentials */
  const char* api_token;
  /* The generated hash of the game to be identified */
  const char* game_hash;
}
rc_api_resolve_hash_request_t;

/**
 * Response data for a resolve hash request.
 */
typedef struct rc_api_resolve_hash_response_t {
  /* The unique identifier of the game, 0 if no match was found */
  uint32_t game_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_resolve_hash_response_t;

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params);
int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response);
int rc_api_process_resolve_hash_server_response(rc_api_resolve_hash_response_t* response, const rc_api_server_response_t* server_response);
void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response);

/* --- Fetch Game Data --- */

/**
 * API parameters for a fetch game data request.
 */
typedef struct rc_api_fetch_game_data_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
}
rc_api_fetch_game_data_request_t;

/* A leaderboard definition */
typedef struct rc_api_leaderboard_definition_t {
  /* The unique identifier of the leaderboard */
  uint32_t id;
  /* The format to pass to rc_format_value to format the leaderboard value */
  int format;
  /* The title of the leaderboard */
  const char* title;
  /* The description of the leaderboard */
  const char* description;
  /* The definition of the leaderboard to be passed to rc_runtime_activate_lboard */
  const char* definition;
  /* Non-zero if lower values are better for this leaderboard */
  uint8_t lower_is_better;
  /* Non-zero if the leaderboard should not be displayed in a list of leaderboards */
  uint8_t hidden;
}
rc_api_leaderboard_definition_t;

/* An achievement definition */
typedef struct rc_api_achievement_definition_t {
  /* The unique identifier of the achievement */
  uint32_t id;
  /* The number of points the achievement is worth */
  uint32_t points;
  /* The achievement category (core, unofficial) */
  uint32_t category;
  /* The title of the achievement */
  const char* title;
  /* The dscription of the achievement */
  const char* description;
  /* The definition of the achievement to be passed to rc_runtime_activate_achievement */
  const char* definition;
  /* The author of the achievment */
  const char* author;
  /* The image name for the achievement badge */
  const char* badge_name;
  /* When the achievement was first uploaded to the server */
  time_t created;
  /* When the achievement was last modified on the server */
  time_t updated;
}
rc_api_achievement_definition_t;

#define RC_ACHIEVEMENT_CATEGORY_CORE 3
#define RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL 5

/**
 * Response data for a fetch game data request.
 */
typedef struct rc_api_fetch_game_data_response_t {
  /* The unique identifier of the game */
  uint32_t id;
  /* The console associated to the game */
  uint32_t console_id;
  /* The title of the game */
  const char* title;
  /* The image name for the game badge */
  const char* image_name;
  /* The rich presence script for the game to be passed to rc_runtime_activate_richpresence */
  const char* rich_presence_script;

  /* An array of achievements for the game */
  rc_api_achievement_definition_t* achievements;
  /* The number of items in the achievements array */
  uint32_t num_achievements;

  /* An array of leaderboards for the game */
  rc_api_leaderboard_definition_t* leaderboards;
  /* The number of items in the leaderboards array */
  uint32_t num_leaderboards;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_game_data_response_t;

int rc_api_init_fetch_game_data_request(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params);
int rc_api_process_fetch_game_data_response(rc_api_fetch_game_data_response_t* response, const char* server_response);
int rc_api_process_fetch_game_data_server_response(rc_api_fetch_game_data_response_t* response, const rc_api_server_response_t* server_response);
void rc_api_destroy_fetch_game_data_response(rc_api_fetch_game_data_response_t* response);

/* --- Ping --- */

/**
 * API parameters for a ping request.
 */
typedef struct rc_api_ping_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  uint32_t game_id;
  /* (optional) The current rich presence evaluation for the user */
  const char* rich_presence;
}
rc_api_ping_request_t;

/**
 * Response data for a ping request.
 */
typedef struct rc_api_ping_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_ping_response_t;

int rc_api_init_ping_request(rc_api_request_t* request, const rc_api_ping_request_t* api_params);
int rc_api_process_ping_response(rc_api_ping_response_t* response, const char* server_response);
int rc_api_process_ping_server_response(rc_api_ping_response_t* response, const rc_api_server_response_t* server_response);
void rc_api_destroy_ping_response(rc_api_ping_response_t* response);

/* --- Award Achievement --- */

/**
 * API parameters for an award achievement request.
 */
typedef struct rc_api_award_achievement_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the achievement */
  uint32_t achievement_id;
  /* Non-zero if the achievement was earned in hardcore */
  uint32_t hardcore;
  /* The hash associated to the game being played */
  const char* game_hash;
}
rc_api_award_achievement_request_t;

/**
 * Response data for an award achievement request.
 */
typedef struct rc_api_award_achievement_response_t {
  /* The unique identifier of the achievement that was awarded */
  uint32_t awarded_achievement_id;
  /* The updated player score */
  uint32_t new_player_score;
  /* The updated player softcore score */
  uint32_t new_player_score_softcore;
  /* The number of achievements the user has not yet unlocked for this game
   * (in hardcore/non-hardcore per hardcore flag in request) */
  uint32_t achievements_remaining;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_award_achievement_response_t;

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params);
int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response);
int rc_api_process_award_achievement_server_response(rc_api_award_achievement_response_t* response, const rc_api_server_response_t* server_response);
void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response);

/* --- Submit Leaderboard Entry --- */

/**
 * API parameters for a submit lboard entry request.
 */
typedef struct rc_api_submit_lboard_entry_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the leaderboard */
  uint32_t leaderboard_id;
  /* The value being submitted */
  int32_t score;
  /* The hash associated to the game being played */
  const char* game_hash;
}
rc_api_submit_lboard_entry_request_t;

/* A leaderboard entry */
typedef struct rc_api_lboard_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* The rank of the entry */
  uint32_t rank;
  /* The value of the entry */
  int32_t score;
}
rc_api_lboard_entry_t;

/**
 * Response data for a submit lboard entry request.
 */
typedef struct rc_api_submit_lboard_entry_response_t {
  /* The value that was submitted */
  int32_t submitted_score;
  /* The player's best submitted value */
  int32_t best_score;
  /* The player's new rank within the leaderboard */
  uint32_t new_rank;
  /* The total number of entries in the leaderboard */
  uint32_t num_entries;

  /* An array of the top entries for the leaderboard */
  rc_api_lboard_entry_t* top_entries;
  /* The number of items in the top_entries array */
  uint32_t num_top_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_submit_lboard_entry_response_t;

int rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params);
int rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response);
int rc_api_process_submit_lboard_entry_server_response(rc_api_submit_lboard_entry_response_t* response, const rc_api_server_response_t* server_response);
void rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_RUNTIME_H */
