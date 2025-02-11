#pragma once

#include "GameAI.h"
#include "RetroModel.h"

#include <bitset>
#include <string>
#include <filesystem>
#include <vector>
#include <queue>
#include "utils/data.h"
#include "./utils/json.hpp"



class GameAILocal : public GameAI {
public:
        GameAILocal():showDebug(false),
                debugLogFunc(nullptr){};


        RetroModel *    LoadModel(const char * path);
        void            SetShowDebug(const bool show){ this->showDebug = show; };
        void            SetDebugLog(debug_log_t func){debugLogFunc = func;};
        void            DebugPrint(const char * msg);

protected:
        void            InitRAM(void * ram_ptr, int ram_size);
        void            LoadConfig();
        void            LoadConfig_Player(const nlohmann::detail::iter_impl<const nlohmann::json> &player);


        bool            showDebug;
        debug_log_t     debugLogFunc;
        Retro::GameData retro_data;

        std::map<std::string, RetroModel*> models;

public:
        std::string full_path;
        std::string dir_path;
        std::string game_name;
};


