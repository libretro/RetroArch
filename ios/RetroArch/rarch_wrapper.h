#ifndef __IOS_RARCH_WRAPPER_H__
#define __IOS_RARCH_WRAPPER_H__

bool ios_load_game(const char* path);
void ios_close_game();
void ios_pause_emulator();
void ios_resume_emulator();
void ios_suspend_emulator();
void ios_activate_emulator();
bool ios_init_game_view();
void ios_destroy_game_view();
void ios_flip_game_view();
void ios_set_game_view_sync(bool on);
void ios_get_game_view_size(unsigned *width, unsigned *height);
void ios_bind_game_view_fbo();

#endif
