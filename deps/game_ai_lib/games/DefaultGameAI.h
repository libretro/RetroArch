#pragma once

#include "../GameAILocal.h"
#include "memory.h"
#include "../utils/data.h"


class DefaultGameAI : public GameAILocal {
public:
    virtual void Init(void * ram_ptr, int ram_size);
    virtual void Think(bool buttons[GAMEAI_MAX_BUTTONS], int player, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format);

private:
    RetroModel *        model;
    RetroModelFrameData input;
};