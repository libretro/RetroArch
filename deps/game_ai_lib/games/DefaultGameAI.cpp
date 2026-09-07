#include "DefaultGameAI.h"
#include <cstdlib> 
#include <iostream>
#include <assert.h>
#include <random>

enum DefaultButtons {
    INPUT_B = 0,
    INPUT_A = 1,
    INPUT_MODE = 2,
    INPUT_START = 3,
    INPUT_UP = 4,
    INPUT_DOWN = 5,
    INPUT_LEFT = 6,
    INPUT_RIGHT = 7,
    INPUT_C = 8,
    INPUT_Y = 9,
    INPUT_X = 10,
    INPUT_Z = 11,
    INPUT_MAX = 12
};

//=======================================================
// DefaultGameAI::Init
//=======================================================
void DefaultGameAI::Init(void * ram_ptr, int ram_size)
{
    LoadConfig();

    InitRAM(ram_ptr, ram_size);
}

//=======================================================
// DefaultGameAI::Think
//=======================================================
void DefaultGameAI::Think(bool buttons[GAMEAI_MAX_BUTTONS], int player, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format)
{
    std::vector<float> output(DefaultButtons::INPUT_MAX);

    input.data = (void *) frame_data;
    input.width = frame_width;
    input.height = frame_height;
    input.pitch = frame_pitch;
    input.format = pixel_format;

    models["Model"]->Forward(output, input);

    for (int i=0; i < output.size(); i++)
    {
        buttons[i] = output[i] >= 1.0 ? 1 : 0;
    }

    buttons[DefaultButtons::INPUT_START] = 0;
    buttons[DefaultButtons::INPUT_MODE] = 0;
}