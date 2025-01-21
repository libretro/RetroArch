#include "NHL94GameData.h"

//=======================================================
// NHL94Data::Init
//=======================================================
void NHL94Data::Init(const Retro::GameData & data)
{
    // players
    p1_x = data.lookupValue("p1_x").cast<int>();
    p1_y = data.lookupValue("p1_y").cast<int>();
    p2_x = data.lookupValue("p2_x").cast<int>();
    p2_y = data.lookupValue("p2_y").cast<int>();
    p1_vel_x = data.lookupValue("p1_vel_x").cast<int>();
    p1_vel_y = data.lookupValue("p1_vel_y").cast<int>();
    p2_vel_x = data.lookupValue("p2_vel_x").cast<int>();
    p2_vel_y = data.lookupValue("p2_vel_y").cast<int>();

    // goalies
    g1_x = data.lookupValue("g1_x").cast<int>();
    g1_y = data.lookupValue("g1_y").cast<int>();
    g2_x = data.lookupValue("g2_x").cast<int>();
    g2_y = data.lookupValue("g2_y").cast<int>();

    // puck
    puck_x = data.lookupValue("puck_x").cast<int>();
    puck_y = data.lookupValue("puck_y").cast<int>();
    puck_vel_x = data.lookupValue("puck_vel_x").cast<int>();
    puck_vel_y = data.lookupValue("puck_vel_y").cast<int>();

    p1_fullstar_x = data.lookupValue("fullstar_x").cast<int>();
    p1_fullstar_y = data.lookupValue("fullstar_y").cast<int>();
    p2_fullstar_x = data.lookupValue("p2_fullstar_x").cast<int>();
    p2_fullstar_y = data.lookupValue("p2_fullstar_y").cast<int>();

    period = data.lookupValue("period").cast<int>();


    // Knowing if the player has the puck is tricky since the fullstar in the game is not aligned with the player every frame
    // There is an offset of up to 2 sometimes

    if (std::abs(p1_x - p1_fullstar_x) < 3 && std::abs(p1_y - p1_fullstar_y) < 3)
        p1_haspuck = true;
    else
        p1_haspuck = false;

    if(std::abs(p2_x - p1_fullstar_x) < 3 && std::abs(p2_y - p1_fullstar_y) < 3)
        p2_haspuck = true;
    else
        p2_haspuck = false;
            
    if(std::abs(g1_x - p1_fullstar_x) < 3 && std::abs(g1_y - p1_fullstar_y) < 3)
        g1_haspuck = true;
    else
        g1_haspuck = false;

    if(std::abs(g2_x - p1_fullstar_x) < 3 && std::abs(g2_y - p1_fullstar_y) < 3)
        g2_haspuck = true;
    else
        g2_haspuck = false;


    attack_zone_y = NHL94Const::ATACKZONE_POS_Y;
    defense_zone_y = NHL94Const::DEFENSEZONE_POS_Y;
    score_zone_top = NHL94Const::SCORE_ZONE_TOP;
    score_zone_bottom = NHL94Const::SCORE_ZONE_BOTTOM;
}

//=======================================================
// NHL94Data::Flip
//=======================================================
void NHL94Data::Flip()
{
    std::swap(p1_x, p2_x);
    std::swap(p1_y, p2_y);
    std::swap(g1_x, g2_x);
    std::swap(g1_y, g2_y);
    std::swap(p1_haspuck, p2_haspuck);
    std::swap(g1_haspuck, g2_haspuck);

    std::swap(p1_vel_x, p2_vel_x);
    std::swap(p1_vel_y, p2_vel_y);
}

//=======================================================
// NHL94Data::FlipZones
//=======================================================
void NHL94Data::FlipZones()
{
    p1_x = -p1_x;
    p1_y = -p1_y;
    p2_x = -p2_x;
    p2_y = -p2_y;
    g1_x = -g1_x;
    g1_y = -g1_y;
    g2_x = -g2_x;
    g2_y = -g2_y;

    p1_vel_x = -p1_vel_x;
    p1_vel_y = -p1_vel_y;
    p2_vel_x = -p2_vel_x;
    p2_vel_y = -p2_vel_y;

    puck_x = -puck_x;
    puck_y = -puck_y;

    puck_vel_x = -puck_vel_x;
    puck_vel_y = -puck_vel_y;
}