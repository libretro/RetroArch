/****************************************************************************
 * Copyright (C) 2016,2017 Maschell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef _ConfigParser_H_
#define _ConfigParser_H_

#include <string>
#include <vector>
#include <map>

#include <stdio.h>

#include "../ControllerPatcher.hpp"

enum PARSE_TYPE{
    PARSE_CONTROLLER,
    PARSE_GAMEPAD,
    PARSE_MOUSE,
    PARSE_KEYBOARD
};

class ConfigParser{
    friend class ConfigReader;
    friend class ControllerPatcher;
private:
    //!Constructor
    ConfigParser(std::string configData);
    //!Destructor
    ~ConfigParser();

    PARSE_TYPE getType();
    void setType(PARSE_TYPE newType);

    u16 getSlot();
    void setSlot(u16 newSlot);

    bool parseIni();

    bool Init();

    bool parseConfigString(std::string content);

    s32 getSlotController(std::string identify);

    s32 checkExistingController(s32 vid, s32 pid);

    s32 getValueFromKeyValue(std::string value_pair,std::string expectedKey,std::string delimiter);

    bool resetConfig();

    void parseSingleLine(std::string line);
    u16 slot_b;
    PARSE_TYPE type_b;

    u16 vid;
    u16 pid;

    std::string content;
    std::vector<std::string> contentLines;
};
#endif
