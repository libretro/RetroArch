#include "NHL94GameAI.h"
#include <cstdlib> 
#include <iostream>
#include <assert.h>
#include <random>

//=======================================================
// NHL94GameAI::Init
//=======================================================
void NHL94GameAI::Init(void * ram_ptr, int ram_size)
{
    LoadConfig();

    InitRAM(ram_ptr, ram_size);

    static_assert(NHL94NeuralNetInput::NN_INPUT_MAX == 16);

    isShooting = false;
}

//=======================================================
// NHL94GameAI::SetModelInputs
//=======================================================
void NHL94GameAI::SetModelInputs(std::vector<float> & input, const NHL94Data & data)
{
    // players
    input[NHL94NeuralNetInput::P1_X] = (float)data.p1_x / (float) NHL94NeuralNetInput::MAX_PLAYER_X;
    input[NHL94NeuralNetInput::P1_Y] = (float)data.p1_y / (float) NHL94NeuralNetInput::MAX_PLAYER_Y;
    input[NHL94NeuralNetInput::P2_X] = (float)data.p2_x / (float) NHL94NeuralNetInput::MAX_PLAYER_X;
    input[NHL94NeuralNetInput::P2_Y] = (float) data.p2_y / (float) NHL94NeuralNetInput::MAX_PLAYER_Y;
    input[NHL94NeuralNetInput::G2_X] = (float) data.g2_x / (float) NHL94NeuralNetInput::MAX_PLAYER_X;
    input[NHL94NeuralNetInput::G2_Y] = (float) data.g2_y / (float) NHL94NeuralNetInput::MAX_PLAYER_Y;
    input[NHL94NeuralNetInput::P1_VEL_X] = (float) data.p1_vel_x / (float) NHL94NeuralNetInput::MAX_VEL_XY;
    input[NHL94NeuralNetInput::P1_VEL_Y] = (float) data.p1_vel_y / (float) NHL94NeuralNetInput::MAX_VEL_XY;
    input[NHL94NeuralNetInput::P2_VEL_X] = (float) data.p2_vel_x / (float) NHL94NeuralNetInput::MAX_VEL_XY;
    input[NHL94NeuralNetInput::P2_VEL_Y] = (float) data.p2_vel_y / (float) NHL94NeuralNetInput::MAX_VEL_XY;

    // puck
    input[NHL94NeuralNetInput::PUCK_X] = (float) data.puck_x / (float) NHL94NeuralNetInput::MAX_PLAYER_X;
    input[NHL94NeuralNetInput::PUCK_Y] = (float) data.puck_y / (float) NHL94NeuralNetInput::MAX_PLAYER_Y;
    input[NHL94NeuralNetInput::PUCK_VEL_X] = (float) data.puck_vel_x / (float) NHL94NeuralNetInput::MAX_VEL_XY;
    input[NHL94NeuralNetInput::PUCK_VEL_Y] = (float) data.puck_vel_y / (float) NHL94NeuralNetInput::MAX_VEL_XY;

    input[NHL94NeuralNetInput::P1_HASPUCK] = data.p1_haspuck ? 0.0 : 1.0;
    input[NHL94NeuralNetInput::G1_HASPUCK] = data.g1_haspuck ? 0.0 : 1.0; 
}

//=======================================================
// NHL94GameAI::GotoTarget
//=======================================================
void NHL94GameAI::GotoTarget(std::vector<float> & input, int vec_x, int vec_y)
{
    if (vec_x > 0)
        input[NHL94Buttons::INPUT_LEFT] = 1;
    else
        input[NHL94Buttons::INPUT_RIGHT] = 1;

    if (vec_y > 0)
        input[NHL94Buttons::INPUT_DOWN] = 1;
    else
        input[NHL94Buttons::INPUT_UP] = 1;
}

//=======================================================
// isInsideAttackZone
//=======================================================
static bool isInsideAttackZone(NHL94Data & data)
{
    if (data.attack_zone_y > 0 && data.p1_y >= data.attack_zone_y)
    {
        return true;
    }
    else if (data.attack_zone_y < 0 && data.p1_y <= data.attack_zone_y)
    {
        return true;
    }

    return false;
}

//=======================================================
// isInsideScoreZone
//=======================================================
static bool isInsideScoreZone(NHL94Data & data)
{
    if (data.p1_y < data.score_zone_top && data.p1_y > data.score_zone_bottom)
    {
        return true;
    }
    
    return false;
}

//=======================================================
// isInsideDefenseZone
//=======================================================
static bool isInsideDefenseZone(NHL94Data & data)
{
    if (data.defense_zone_y > 0 && data.p1_y >= data.defense_zone_y)
    {
        return true;
    }
    else if (data.defense_zone_y < 0 && data.p1_y <= data.defense_zone_y)
    {
        return true;
    }

    return false;
}

//=======================================================
// NHL94GameAI::Think
//=======================================================
void NHL94GameAI::Think(bool buttons[GAMEAI_MAX_BUTTONS], int player, const void *frame_data, unsigned int frame_width, unsigned int frame_height, unsigned int frame_pitch, unsigned int pixel_format)
{
    NHL94Data data;
    data.Init(retro_data);

    if(player == 1)
    {
        data.Flip();

        if(data.period % 2 == 0)
        {
            data.FlipZones();
        }
    }
    else if (player == 0)
    {
        if(data.period % 2 == 1)
        {
            data.FlipZones();
        }
    }
    
    
    std::vector<float> input(16);
    std::vector<float> output(12);

    this->SetModelInputs(input, data);

    if (data.p1_haspuck)
    {
        DebugPrint("have puck");

        if (isInsideAttackZone(data))
        {
            DebugPrint("      in attackzone");
            models["ScoreGoal"]->Forward(output, input);
            output[NHL94Buttons::INPUT_C] = 0;
            output[NHL94Buttons::INPUT_B] = 0;

            if (isInsideScoreZone(data))
            {
                if (data.p1_vel_x >= 30 && data.puck_x > -23 && data.puck_x < 0)
                {
                    DebugPrint("Shoot");
                    output[NHL94Buttons::INPUT_C] = 1;
                    isShooting = true;
                }
                else if(data.p1_vel_x <= -30 && data.puck_x < 23 && data.puck_x > 0)
                {
                    DebugPrint("Shoot");
                    output[NHL94Buttons::INPUT_C] = 1;
                    isShooting = true;
                }
            }
        }
        else
        {
            this->GotoTarget(output, data.p1_x, -data.attack_zone_y);
        }
    }
    else if (data.g1_haspuck)
    {
        if (rand() > (RAND_MAX / 2))
            output[NHL94Buttons::INPUT_B] = 1;
    }
    else
    {
        DebugPrint("Don't have puck");
        isShooting = false;

        if (isInsideDefenseZone(data) && data.p2_haspuck)
        {
            DebugPrint("    DefenseModel->Forward");
            models["DefenseZone"]->Forward(output, input);
        }
        else
        {
            DebugPrint("    GOTO TARGET");            
            GotoTarget(output, data.p1_x - data.puck_x, data.p1_y - data.puck_y);
        }
            
        if (isShooting)
        {
            //output[NHL94Buttons::INPUT_MODE] = 1;
            DebugPrint("Shooting");
            output[NHL94Buttons::INPUT_C] = 1;
        }
    }

    assert(output.size() <= 16);
    for (int i=0; i < output.size(); i++)
    {
        buttons[i] = output[i] >= 1.0 ? 1 : 0;
    }

   
    buttons[NHL94Buttons::INPUT_START] = 0;
    buttons[NHL94Buttons::INPUT_MODE] = 0;
    buttons[NHL94Buttons::INPUT_A] = 0;
    //buttons[NHL94Buttons::INPUT_B] = 0;
    //buttons[NHL94Buttons::INPUT_C] = 0;
    buttons[NHL94Buttons::INPUT_X] = 0;
    buttons[NHL94Buttons::INPUT_Y] = 0;
    buttons[NHL94Buttons::INPUT_Z] = 0;

    //Flip directions
    if(data.period % 2 != player)
    {
        std::swap(buttons[NHL94Buttons::INPUT_UP], buttons[NHL94Buttons::INPUT_DOWN]);
        std::swap(buttons[NHL94Buttons::INPUT_LEFT], buttons[NHL94Buttons::INPUT_RIGHT]);
    }
}