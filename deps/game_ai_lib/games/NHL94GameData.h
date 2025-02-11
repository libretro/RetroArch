#pragma once

#include "memory.h"
#include "../utils/data.h"

enum NHL94Buttons {
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

enum NHL94NeuralNetInput {
    P1_X = 0,
    P1_Y,
    P1_VEL_X,
    P1_VEL_Y,
    P2_X,
    P2_Y,
    P2_VEL_X,
    P2_VEL_Y,
    PUCK_X,
    PUCK_Y,
    PUCK_VEL_X,
    PUCK_VEL_Y,
    G2_X,
    G2_Y,
    P1_HASPUCK,
    G1_HASPUCK,
    NN_INPUT_MAX,

    // Used for normalization
    MAX_PLAYER_X = 120,
    MAX_PLAYER_Y = 270,
    MAX_PUCK_X = 130,
    MAX_PUCK_Y = 270,
    MAX_VEL_XY = 50
};


enum NHL94Const {
    ATACKZONE_POS_Y = 100,
    DEFENSEZONE_POS_Y = -80,
    SCORE_ZONE_TOP = 230,
    SCORE_ZONE_BOTTOM = 210,
};

class NHL94Data {
public:
    int p1_x;
    int p1_y;
    int p2_x;
    int p2_y;
    int p1_vel_x;
    int p1_vel_y;
    int p2_vel_x;
    int p2_vel_y;
    int g1_x;
    int g1_y;
    int g2_x;
    int g2_y;

    int puck_x;
    int puck_y;
    int puck_vel_x;
    int puck_vel_y;

    int p1_fullstar_x;
    int p1_fullstar_y;
    int p2_fullstar_x;
    int p2_fullstar_y;


    bool p1_haspuck;
    bool g1_haspuck;
    bool p2_haspuck;
    bool g2_haspuck;

    int attack_zone_y;
    int defense_zone_y;
    int score_zone_top;
    int score_zone_bottom;

    int period;

    void Init(const Retro::GameData & data);
    
    void Flip();

    void FlipZones();
};