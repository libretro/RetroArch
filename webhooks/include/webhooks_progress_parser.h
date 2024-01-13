#ifndef __WEBHOOKS_PROGRESS_PARSER_H
#define __WEBHOOKS_PROGRESS_PARSER_H

RETRO_BEGIN_DECLS

int wpp_parse_game_progress
(
  const char* game_progress,
  rc_runtime_t* runtime
);

RETRO_END_DECLS

#endif /*__WEBHOOKS_PROGRESS_PARSER_H*/
