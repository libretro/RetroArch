#pragma once

#include <bitset>
#include <string>
#include <filesystem>
#include <vector>
#include <queue>

typedef void (*debug_log_t)(int level, const char *fmt, ...);

#define GAMEAI_MAX_BUTTONS 16

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


typedef GameAI * (*creategameai_t)(const char *);