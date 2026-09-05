#pragma once

#ifdef __cplusplus

#include <bitset>
#include <string>
#include <filesystem>
#include <vector>
#include <queue>

#endif


typedef void (*debug_log_t)(int level, const char *fmt, ...);

#define GAMEAI_MAX_BUTTONS 16
#define GAMEAI_SKIPFRAMES 4


#ifdef __cplusplus

class GameAI {
public:
        virtual void    Init(void * ram_ptr, int ram_size) {};
        virtual void    Think(bool buttons[GAMEAI_MAX_BUTTONS], int player, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format) {};
        void            SetShowDebug(const bool show){ this->showDebug = show; };
        void            SetDebugLog(debug_log_t func){debugLogFunc = func;};

private:
        bool            showDebug;
        debug_log_t     debugLogFunc;
};

#endif

typedef void * (*create_game_ai_t)(const char *);
typedef void (*destroy_game_ai_t)(void * obj_ptr);
typedef void (*game_ai_lib_init_t)(void * obj_ptr, void * ram_ptr, int ram_size);
typedef void (*game_ai_lib_think_t)(void * obj_ptr, bool buttons[GAMEAI_MAX_BUTTONS], int player, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format);
typedef void (*game_ai_lib_set_show_debug_t)(void * obj_ptr, const bool show);
typedef void (*game_ai_lib_set_debug_log_t)(void * obj_ptr, debug_log_t func);
