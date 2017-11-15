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
#include "./ConfigValues.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ConfigValues *ConfigValues::instance = NULL;

ConfigValues::ConfigValues(){
    InitValues();
}

ConfigValues::~ConfigValues(){
    if(HID_DEBUG){ printf("ConfigValues::~ConfigValues(line %d){\n",__LINE__);}
}

const u8 * ConfigValues::getValuesForPreset(std::map<std::string,const u8*> values,std::string possibleValue){
    std::map<std::string,const u8*>::iterator it;
    it = values.find(possibleValue);
    if (it != values.end()){
        return it->second;
    }
    return NULL;
}

bool ConfigValues::setIfValueIsAControllerPresetEx(std::string value,s32 slot,s32 keyslot){
    if(setIfValueIsPreset(presetGCValues,value,slot,keyslot)) return true;
    if(setIfValueIsPreset(presetDS3Values,value,slot,keyslot)) return true;
    if(setIfValueIsPreset(presetDS4Values,value,slot,keyslot)) return true;
    if(setIfValueIsPreset(presetXInputValues,value,slot,keyslot)) return true;
    if(setIfValueIsPreset(presetSwitchProValues,value,slot,keyslot)) return true;
    return false;
}

//We need this function here so we can use preset sticks.
bool ConfigValues::setIfValueIsPreset(std::map<std::string,const u8*> values,std::string possibleValue,s32 slot,s32 keyslot){
    if(slot > gHIDMaxDevices || slot < 0 || keyslot < 0 || keyslot >= CONTRPS_MAX_VALUE){
        return false;
    }
    const u8 * values_ = NULL;
    if( keyslot == CONTRPS_VPAD_BUTTON_L_STICK_X ||
        keyslot == CONTRPS_VPAD_BUTTON_L_STICK_Y ||
        keyslot == CONTRPS_VPAD_BUTTON_R_STICK_X ||
        keyslot == CONTRPS_VPAD_BUTTON_R_STICK_Y){
        if(HID_DEBUG){ printf("ConfigValues::setIfValueIsPreset(line %d): This may be a predefined stick %s\n",__LINE__,possibleValue.c_str());}
        if((values_ = ConfigValues::getValuesStickPreset(possibleValue)) != NULL){
            if(HID_DEBUG){ printf("ConfigValues::setIfValueIsPreset(line %d): Found predefined stick!\n",__LINE__);}
            config_controller[slot][keyslot][0] =                           values_[STICK_CONF_BYTE];       //CONTRPS_VPAD_BUTTON_L_STICK_X
            config_controller[slot][keyslot][1] =                           values_[STICK_CONF_DEFAULT];
            config_controller[slot][keyslot+DEF_STICK_OFFSET_INVERT][0] =   CONTROLLER_PATCHER_VALUE_SET;   //CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT
            config_controller[slot][keyslot+DEF_STICK_OFFSET_INVERT][1] =   values_[STICK_CONF_INVERT];
            config_controller[slot][keyslot+DEF_STICK_OFFSET_DEADZONE][0] = CONTROLLER_PATCHER_VALUE_SET;   //CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE
            config_controller[slot][keyslot+DEF_STICK_OFFSET_DEADZONE][1] = values_[STICK_CONF_DEADZONE];
            config_controller[slot][keyslot+DEF_STICK_OFFSET_MINMAX][0] =   values_[STICK_CONF_MIN];        //CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX
            config_controller[slot][keyslot+DEF_STICK_OFFSET_MINMAX][1] =   values_[STICK_CONF_MAX];

            return true;
        }
    }

    if((values_ = getValuesForPreset(values,possibleValue)) != NULL){
        config_controller[slot][keyslot][0] = values_[0];
        config_controller[slot][keyslot][1] = values_[1];
        return true;
    }
    return false;
}


s32 ConfigValues::getValueFromMap(std::map<std::string,int> values,std::string nameOfString){
    std::map<std::string,int>::iterator it;
    it = values.find(nameOfString);
    if (it != values.end()){
       return it->second;
    }

    //Value not found
    return -1;
}

s32 ConfigValues::getPresetValueEx(std::string possibleString){
    s32 rightValue = -1;
    if((rightValue = getValueFromMap(gGamePadValuesToCONTRPSString,possibleString))!= -1){
        if(HID_DEBUG){ printf("ConfigValues::getPresetValueEx(line %d): Used pre-defined VPAD_VALUE! \"%s\" is %d\n",__LINE__,possibleString.c_str(),rightValue); }
    }else if((rightValue = getValueFromMap(presetValues,possibleString))!= -1){
        if(HID_DEBUG){ printf("ConfigValues::getPresetValueEx(line %d): Used pre-defined value! \"%s\" is %d\n",__LINE__,possibleString.c_str(),rightValue); }
    }
    return rightValue;
}

 void ConfigValues::addDeviceNameEx(u16 vid,u16 pid,std::string value){
    deviceNames[CPStringTools::strfmt("%04X%04X",vid,pid).c_str()] = value;
 }

std::string ConfigValues::getStringByVIDPIDEx(u16 vid,u16 pid){
    std::string result = "";
    std::map<std::string,std::string>::iterator it;

    it = deviceNames.find(CPStringTools::strfmt("%04X%04X",vid,pid));
    if (it != deviceNames.end()){
        result = it->second;
    }else{
        result = CPStringTools::strfmt("VID: 0x%04X\nPID: 0x%04X",vid,pid);
    }
    return result;
}
