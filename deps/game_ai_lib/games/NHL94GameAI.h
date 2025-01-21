#pragma once

#include "../GameAILocal.h"
#include "NHL94GameData.h"

class NHL94GameAI : public GameAILocal {
public:
    virtual void    Init(void * ram_ptr, int ram_size);
    virtual void    Think(bool buttons[GAMEAI_MAX_BUTTONS], int player, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format);

    void            SetModelInputs(std::vector<float> & input, const NHL94Data & data);
    void            GotoTarget(std::vector<float> & input, int vec_x, int vec_y);

private:
    bool isShooting;
};