#ifndef __WEBHOOKS_MACRO_MANAGER_H
#define __WEBHOOKS_MACRO_MANAGER_H

typedef void (*on_game_progress_downloaded_t)
(
  wb_locals_t* locals,
  const char* game_progress,
  size_t length
);

void wpd_download_game_progress
(
  wb_locals_t* locals,
  on_game_progress_downloaded_t on_game_progress_downloaded
);

#endif //__WEBHOOKS_MACRO_MANAGER_H
