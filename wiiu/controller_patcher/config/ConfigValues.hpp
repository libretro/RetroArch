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
#ifndef _ConfigValues_H_
#define _ConfigValues_H_

#include <string>
#include <vector>
#include <map>

#include "../ControllerPatcher.hpp"

class ConfigValues
{
friend class ConfigParser;
friend class ControllerPatcher;
private:
    static ConfigValues *getInstance() {
        if(instance == NULL){
            printf("ConfigValues: We need a new instance!!!\n");
            instance = new ConfigValues();
        }
        return instance;
    }

    static void destroyInstance() {
        if(instance){
            delete instance;
            instance = NULL;
        }
    }

    /**
        Returns NULL if not a preset!
    **/
    static const u8 * getValuesStickPreset(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return NULL;
        return cur_instance->getValuesForPreset(cur_instance->presetSticks,possibleValue);
    }

    /**
        Returns -1 if not found
    **/
    static s32 getKeySlotGamePad(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->getValueFromMap(cur_instance->CONTPRStringToValue,possibleValue);
    }
    /**
        Returns -1 if not found
    **/
    static s32 getKeySlotMouse(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->getValueFromMap(cur_instance->mouseLeftValues,possibleValue);
    }

    /**
        Returns -1 if not found
    **/
    static s32 getKeySlotDefaultSingleValue(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->getValueFromMap(cur_instance->CONTPRStringToValueSingle,possibleValue);
    }

    /**
    Returns -1 if not found
    **/
    static s32 getKeySlotDefaultPairedValue(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->getValueFromMap(cur_instance->CONTPRStringToValue,possibleValue);
    }

    /**
        Returns -1 if not found
    **/
    static s32 getPresetValuesKeyboard(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->getValueFromMap(cur_instance->presetKeyboardValues,possibleValue);
    }

    /**
        Returns -1 if not found
    **/
    static s32 getPresetValue(std::string possibleValue)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->getPresetValueEx(possibleValue);
    }

    /**
        Returns -1 if not found
    **/
    static s32 setIfValueIsAControllerPreset(std::string value,s32 slot,s32 keyslot)
    {
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return -1;
        return cur_instance->setIfValueIsAControllerPresetEx(value,slot,keyslot);
    }

    static void addDeviceName(u16 vid,u16 pid,std::string value){
        ConfigValues * cur_instance = getInstance();
        if(cur_instance != NULL){
            cur_instance->addDeviceNameEx(vid,pid,value);
        }
    }

    /**
        Returns empty String if not found
    **/
    static std::string getStringByVIDPID(u16 vid,u16 pid){
        ConfigValues * cur_instance = getInstance();
        if(cur_instance ==  NULL) return "";
        return cur_instance->getStringByVIDPIDEx(vid,pid);
    }

    //!Constructor
    ConfigValues();
    //!Destructor
    ~ConfigValues();

    static ConfigValues *instance;

    std::map<std::string,int> mouseLeftValues;
    std::map<std::string,int> CONTPRStringToValue;
    std::map<std::string,int> CONTPRStringToValueSingle;
    std::map<std::string,int> presetValues;
    std::map<std::string,int> gGamePadValuesToCONTRPSString;
    std::map<std::string,int> presetKeyboardValues;

    std::map<std::string,std::string> deviceNames;

    std::map<std::string,const u8*> presetGCValues;
    std::map<std::string,const u8*> presetDS3Values;
    std::map<std::string,const u8*> presetDS4Values;
    std::map<std::string,const u8*> presetXInputValues;
    std::map<std::string,const u8*> presetSwitchProValues;
    std::map<std::string,const u8*> presetSticks;

    s32 getValueFromMap(std::map<std::string,int> values,std::string nameOfString);

    bool checkIfValueIsAControllerPreset(std::string value,s32 slot,s32 keyslot);

    s32 getPresetValueEx(std::string possibleString);

	void InitValues(){
        printf("ConfigValues::InitValues: Init values for the configuration\n");
        CONTPRStringToValue["VPAD_BUTTON_A"] =                          CONTRPS_VPAD_BUTTON_A;
        CONTPRStringToValue["VPAD_BUTTON_B"] =                          CONTRPS_VPAD_BUTTON_B;
        CONTPRStringToValue["VPAD_BUTTON_X"] =                          CONTRPS_VPAD_BUTTON_X;
        CONTPRStringToValue["VPAD_BUTTON_Y"] =                          CONTRPS_VPAD_BUTTON_Y;
        /* Normal DPAD */
        CONTPRStringToValue["VPAD_BUTTON_LEFT"] =                       CONTRPS_VPAD_BUTTON_LEFT;
        CONTPRStringToValue["VPAD_BUTTON_RIGHT"] =                      CONTRPS_VPAD_BUTTON_RIGHT;
        CONTPRStringToValue["VPAD_BUTTON_UP"] =                         CONTRPS_VPAD_BUTTON_UP;
        CONTPRStringToValue["VPAD_BUTTON_DOWN"] =                       CONTRPS_VPAD_BUTTON_DOWN;
        /* DPAD hat mode */
        CONTPRStringToValue["VPAD_BUTTON_DPAD_N"] =                     CONTRPS_VPAD_BUTTON_DPAD_N;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_NE"] =                    CONTRPS_VPAD_BUTTON_DPAD_NE;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_E"] =                     CONTRPS_VPAD_BUTTON_DPAD_E;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_SE"] =                    CONTRPS_VPAD_BUTTON_DPAD_SE;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_S"] =                     CONTRPS_VPAD_BUTTON_DPAD_S;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_SW"] =                    CONTRPS_VPAD_BUTTON_DPAD_SW;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_W"] =                     CONTRPS_VPAD_BUTTON_DPAD_W;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_NW"] =                    CONTRPS_VPAD_BUTTON_DPAD_NW;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_NEUTRAL"] =               CONTRPS_VPAD_BUTTON_DPAD_NEUTRAL;
        /* DPAD Absolute mode */
        CONTPRStringToValue["VPAD_BUTTON_DPAD_ABS_UP"] =                CONTRPS_VPAD_BUTTON_DPAD_ABS_UP;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_ABS_DOWN"] =              CONTRPS_VPAD_BUTTON_DPAD_ABS_DOWN;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_ABS_LEFT"] =              CONTRPS_VPAD_BUTTON_DPAD_ABS_LEFT;
        CONTPRStringToValue["VPAD_BUTTON_DPAD_ABS_RIGHT"] =             CONTRPS_VPAD_BUTTON_DPAD_ABS_RIGHT;
        /* */
        CONTPRStringToValue["VPAD_BUTTON_ZL"] =                         CONTRPS_VPAD_BUTTON_ZL;
        CONTPRStringToValue["VPAD_BUTTON_ZR"] =                         CONTRPS_VPAD_BUTTON_ZR;
        CONTPRStringToValue["VPAD_BUTTON_L"] =                          CONTRPS_VPAD_BUTTON_L;
        CONTPRStringToValue["VPAD_BUTTON_R"] =                          CONTRPS_VPAD_BUTTON_R;
        CONTPRStringToValue["VPAD_BUTTON_PLUS"] =                       CONTRPS_VPAD_BUTTON_PLUS;
        CONTPRStringToValue["VPAD_BUTTON_MINUS"] =                      CONTRPS_VPAD_BUTTON_MINUS;
        CONTPRStringToValue["VPAD_BUTTON_HOME"] =                       CONTRPS_VPAD_BUTTON_HOME;
        CONTPRStringToValue["VPAD_BUTTON_SYNC"] =                       CONTRPS_VPAD_BUTTON_SYNC;
        CONTPRStringToValue["VPAD_BUTTON_STICK_R"] =                    CONTRPS_VPAD_BUTTON_STICK_R;
        CONTPRStringToValue["VPAD_BUTTON_STICK_L"] =                    CONTRPS_VPAD_BUTTON_STICK_L;

        CONTPRStringToValue["VPAD_STICK_R_EMULATION_LEFT"] =            CONTRPS_VPAD_STICK_R_EMULATION_LEFT;
        CONTPRStringToValue["VPAD_STICK_R_EMULATION_RIGHT"] =           CONTRPS_VPAD_STICK_R_EMULATION_RIGHT;
        CONTPRStringToValue["VPAD_STICK_R_EMULATION_UP"] =              CONTRPS_VPAD_STICK_R_EMULATION_UP;
        CONTPRStringToValue["VPAD_STICK_R_EMULATION_DOWN"] =            CONTRPS_VPAD_STICK_R_EMULATION_DOWN;
        CONTPRStringToValue["VPAD_STICK_L_EMULATION_LEFT"] =            CONTRPS_VPAD_STICK_L_EMULATION_LEFT;
        CONTPRStringToValue["VPAD_STICK_L_EMULATION_RIGHT"] =           CONTRPS_VPAD_STICK_L_EMULATION_RIGHT;
        CONTPRStringToValue["VPAD_STICK_L_EMULATION_UP"] =              CONTRPS_VPAD_STICK_L_EMULATION_UP;
        CONTPRStringToValue["VPAD_STICK_L_EMULATION_DOWN"] =            CONTRPS_VPAD_STICK_L_EMULATION_DOWN;

        CONTPRStringToValue["VPAD_L_STICK_UP"] =                        CONTRPS_VPAD_BUTTON_L_STICK_UP;
        CONTPRStringToValue["VPAD_L_STICK_DOWN"] =                      CONTRPS_VPAD_BUTTON_L_STICK_DOWN;
        CONTPRStringToValue["VPAD_L_STICK_LEFT"] =                      CONTRPS_VPAD_BUTTON_L_STICK_LEFT;
        CONTPRStringToValue["VPAD_L_STICK_RIGHT"] =                     CONTRPS_VPAD_BUTTON_L_STICK_RIGHT;

        CONTPRStringToValue["VPAD_R_STICK_UP"] =                        CONTRPS_VPAD_BUTTON_R_STICK_UP;
        CONTPRStringToValue["VPAD_R_STICK_DOWN"] =                      CONTRPS_VPAD_BUTTON_R_STICK_DOWN;
        CONTPRStringToValue["VPAD_R_STICK_LEFT"] =                      CONTRPS_VPAD_BUTTON_R_STICK_LEFT;
        CONTPRStringToValue["VPAD_R_STICK_RIGHT"] =                     CONTRPS_VPAD_BUTTON_R_STICK_RIGHT;

        CONTPRStringToValue["VPAD_L_STICK_X"] =                         CONTRPS_VPAD_BUTTON_L_STICK_X;
        CONTPRStringToValue["VPAD_L_STICK_X_MINMAX"] =                  CONTRPS_VPAD_BUTTON_L_STICK_X_MINMAX;
        CONTPRStringToValue["VPAD_L_STICK_Y"] =                         CONTRPS_VPAD_BUTTON_L_STICK_Y;
        CONTPRStringToValue["VPAD_L_STICK_Y_MINMAX"] =                  CONTRPS_VPAD_BUTTON_L_STICK_Y_MINMAX;
        CONTPRStringToValue["VPAD_R_STICK_X"] =                         CONTRPS_VPAD_BUTTON_R_STICK_X;
        CONTPRStringToValue["VPAD_R_STICK_X_MINMAX"] =                  CONTRPS_VPAD_BUTTON_R_STICK_X_MINMAX;
        CONTPRStringToValue["VPAD_R_STICK_Y"] =                         CONTRPS_VPAD_BUTTON_R_STICK_Y;
        CONTPRStringToValue["VPAD_R_STICK_Y_MINMAX"] =                  CONTRPS_VPAD_BUTTON_R_STICK_Y_MINMAX;
        CONTPRStringToValue["VPAD_BUTTON_TV"] =                         CONTRPS_VPAD_BUTTON_TV;

        CONTPRStringToValue["DOUBLE_USE_BUTTON_ACTIVATOR"] =            CONTRPS_DOUBLE_USE_BUTTON_ACTIVATOR,
        CONTPRStringToValue["INPUT_FILTER"] =                           CONTRPS_INPUT_FILTER;
        CONTPRStringToValue["PAD1_FILTER"] =                            CONTRPS_PAD1_FILTER;
        CONTPRStringToValue["PAD2_FILTER"] =                            CONTRPS_PAD2_FILTER;
        CONTPRStringToValue["PAD3_FILTER"] =                            CONTRPS_PAD3_FILTER;
        CONTPRStringToValue["PAD4_FILTER"] =                            CONTRPS_PAD4_FILTER;
        CONTPRStringToValue["PAD5_FILTER"] =                            CONTRPS_PAD5_FILTER;

        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_1_PRESSED"] =      CONTRPS_DOUBLE_USE_BUTTON_1_PRESSED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_2_PRESSED"] =      CONTRPS_DOUBLE_USE_BUTTON_2_PRESSED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_3_PRESSED"] =      CONTRPS_DOUBLE_USE_BUTTON_3_PRESSED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_4_PRESSED"] =      CONTRPS_DOUBLE_USE_BUTTON_4_PRESSED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_5_PRESSED"] =      CONTRPS_DOUBLE_USE_BUTTON_5_PRESSED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_1_RELEASED"] =     CONTRPS_DOUBLE_USE_BUTTON_1_RELEASED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_2_RELEASED"] =     CONTRPS_DOUBLE_USE_BUTTON_2_RELEASED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_3_RELEASED"] =     CONTRPS_DOUBLE_USE_BUTTON_3_RELEASED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_4_RELEASED"] =     CONTRPS_DOUBLE_USE_BUTTON_4_RELEASED;
        CONTPRStringToValueSingle["DOUBLE_USE_BUTTON_5_RELEASED"] =     CONTRPS_DOUBLE_USE_BUTTON_5_RELEASED;

        CONTPRStringToValueSingle["BUF_SIZE"] =                         CONTRPS_BUF_SIZE;
        CONTPRStringToValueSingle["DPAD_MODE"] =                        CONTRPS_DPAD_MODE;
        CONTPRStringToValueSingle["DPAD_MASK"] =                        CONTRPS_DPAD_MASK;
        CONTPRStringToValueSingle["VPAD_L_STICK_X_DEADZONE"] =          CONTRPS_VPAD_BUTTON_L_STICK_X_DEADZONE;
        CONTPRStringToValueSingle["VPAD_L_STICK_Y_DEADZONE"] =          CONTRPS_VPAD_BUTTON_L_STICK_Y_DEADZONE;
        CONTPRStringToValueSingle["VPAD_R_STICK_X_DEADZONE"] =          CONTRPS_VPAD_BUTTON_R_STICK_X_DEADZONE;
        CONTPRStringToValueSingle["VPAD_R_STICK_Y_DEADZONE"] =          CONTRPS_VPAD_BUTTON_R_STICK_Y_DEADZONE;
        CONTPRStringToValueSingle["VPAD_L_STICK_X_INVERT"] =            CONTRPS_VPAD_BUTTON_L_STICK_X_INVERT;
        CONTPRStringToValueSingle["VPAD_L_STICK_Y_INVERT"] =            CONTRPS_VPAD_BUTTON_L_STICK_Y_INVERT;
        CONTPRStringToValueSingle["VPAD_R_STICK_X_INVERT"] =            CONTRPS_VPAD_BUTTON_R_STICK_X_INVERT;
        CONTPRStringToValueSingle["VPAD_R_STICK_Y_INVERT"] =            CONTRPS_VPAD_BUTTON_R_STICK_Y_INVERT;

        CONTPRStringToValueSingle["DOUBLE_USE"] =                       CONTRPS_DOUBLE_USE;
        CONTPRStringToValueSingle["PAD_COUNT"] =                        CONTRPS_PAD_COUNT;

        mouseLeftValues["LEFT_CLICK"] =                                 CONTRPS_VPAD_BUTTON_LEFT;
        mouseLeftValues["RIGHT_CLICK"] =                                CONTRPS_VPAD_BUTTON_RIGHT;
        mouseLeftValues["EMULATED_STICK"] =                             CONTRPS_MOUSE_STICK;

        presetGCValues["GC_BUTTON_A"] =                                 HID_GC_BUTTON_A;
        presetGCValues["GC_BUTTON_B"] =                                 HID_GC_BUTTON_B;
        presetGCValues["GC_BUTTON_X"] =                                 HID_GC_BUTTON_X;
        presetGCValues["GC_BUTTON_Y"] =                                 HID_GC_BUTTON_Y;
        presetGCValues["GC_BUTTON_LEFT"] =                              HID_GC_BUTTON_LEFT;
        presetGCValues["GC_BUTTON_RIGHT"] =                             HID_GC_BUTTON_RIGHT;
        presetGCValues["GC_BUTTON_DOWN"] =                              HID_GC_BUTTON_DOWN;
        presetGCValues["GC_BUTTON_UP"] =                                HID_GC_BUTTON_UP;
        presetGCValues["GC_BUTTON_START"] =                             HID_GC_BUTTON_START;
        presetGCValues["GC_BUTTON_Z"] =                                 HID_GC_BUTTON_Z;
        presetGCValues["GC_BUTTON_L"] =                                 HID_GC_BUTTON_L;
        presetGCValues["GC_BUTTON_R"] =                                 HID_GC_BUTTON_R;

        presetDS3Values["DS3_BUTTON_CROSS"] =                           HID_DS3_BUTTON_CROSS;
        presetDS3Values["DS3_BUTTON_CIRCLE"] =                          HID_DS3_BUTTON_CIRCLE;
        presetDS3Values["DS3_BUTTON_SQUARE"] =                          HID_DS3_BUTTON_SQUARE;
        presetDS3Values["DS3_BUTTON_TRIANGLE"] =                        HID_DS3_BUTTON_TRIANGLE;

        presetDS3Values["DS3_BUTTON_L1"] =                              HID_DS3_BUTTON_L1;
        presetDS3Values["DS3_BUTTON_L2"] =                              HID_DS3_BUTTON_L2;
        presetDS3Values["DS3_BUTTON_L3"] =                              HID_DS3_BUTTON_L3;
        presetDS3Values["DS3_BUTTON_R1"] =                              HID_DS3_BUTTON_R1;
        presetDS3Values["DS3_BUTTON_R2"] =                              HID_DS3_BUTTON_R2;
        presetDS3Values["DS3_BUTTON_R3"] =                              HID_DS3_BUTTON_R3;

        presetDS3Values["DS3_BUTTON_SELECT"] =                          HID_DS3_BUTTON_SELECT;
        presetDS3Values["DS3_BUTTON_START"] =                           HID_DS3_BUTTON_START;
        presetDS3Values["DS3_BUTTON_LEFT"] =                            HID_DS3_BUTTON_LEFT;
        presetDS3Values["DS3_BUTTON_RIGHT"] =                           HID_DS3_BUTTON_RIGHT;
        presetDS3Values["DS3_BUTTON_UP"] =                              HID_DS3_BUTTON_UP;
        presetDS3Values["DS3_BUTTON_DOWN"] =                            HID_DS3_BUTTON_DOWN;
        presetDS3Values["DS3_BUTTON_GUIDE"] =                           HID_DS3_BUTTON_GUIDE;

        presetDS4Values["DS4_BUTTON_CROSS"] =                           HID_DS4_BUTTON_CROSS;
        presetDS4Values["DS4_BUTTON_CIRCLE"] =                          HID_DS4_BUTTON_CIRCLE;
        presetDS4Values["DS4_BUTTON_SQUARE"] =                          HID_DS4_BUTTON_SQUARE;
        presetDS4Values["DS4_BUTTON_TRIANGLE"] =                        HID_DS4_BUTTON_TRIANGLE;

        presetDS4Values["DS4_BUTTON_L1"] =                              HID_DS4_BUTTON_L1;
        presetDS4Values["DS4_BUTTON_L2"] =                              HID_DS4_BUTTON_L2;
        presetDS4Values["DS4_BUTTON_L3"] =                              HID_DS4_BUTTON_L3;
        presetDS4Values["DS4_BUTTON_R1"] =                              HID_DS4_BUTTON_R1;
        presetDS4Values["DS4_BUTTON_R2"] =                              HID_DS4_BUTTON_R2;
        presetDS4Values["DS4_BUTTON_R3"] =                              HID_DS4_BUTTON_R3;

        presetDS4Values["DS4_BUTTON_SHARE"] =                           HID_DS4_BUTTON_SHARE;
        presetDS4Values["DS4_BUTTON_OPTIONS"] =                         HID_DS4_BUTTON_OPTIONS;
        presetDS4Values["DS4_BUTTON_DPAD_TYPE"] =                       HID_DS4_BUTTON_DPAD_TYPE;

        presetDS4Values["DS4_BUTTON_DPAD_N"] =                          HID_DS4_BUTTON_DPAD_N;
        presetDS4Values["DS4_BUTTON_DPAD_NE"] =                         HID_DS4_BUTTON_DPAD_NE;
        presetDS4Values["DS4_BUTTON_DPAD_E"] =                          HID_DS4_BUTTON_DPAD_E;
        presetDS4Values["DS4_BUTTON_DPAD_SE"] =                         HID_DS4_BUTTON_DPAD_SE;
        presetDS4Values["DS4_BUTTON_DPAD_S"] =                          HID_DS4_BUTTON_DPAD_S;
        presetDS4Values["DS4_BUTTON_DPAD_SW"] =                         HID_DS4_BUTTON_DPAD_SW;
        presetDS4Values["DS4_BUTTON_DPAD_W"] =                          HID_DS4_BUTTON_DPAD_W;
        presetDS4Values["DS4_BUTTON_DPAD_NW"] =                         HID_DS4_BUTTON_DPAD_NW;
        presetDS4Values["DS4_BUTTON_DPAD_NEUTRAL"] =                    HID_DS4_BUTTON_DPAD_NEUTRAL;

        presetDS4Values["DS4_BUTTON_GUIDE"] =                           HID_DS4_BUTTON_GUIDE;
        presetDS4Values["DS4_BUTTON_T_PAD_CLICK"] =                     HID_DS4_BUTTON_T_PAD_CLICK;

        presetXInputValues["XINPUT_BUTTON_A"] =                         HID_XINPUT_BUTTON_A;
        presetXInputValues["XINPUT_BUTTON_B"] =                         HID_XINPUT_BUTTON_B;
        presetXInputValues["XINPUT_BUTTON_X"] =                         HID_XINPUT_BUTTON_X;
        presetXInputValues["XINPUT_BUTTON_Y"] =                         HID_XINPUT_BUTTON_Y;

        presetXInputValues["XINPUT_BUTTON_LB"] =                        HID_XINPUT_BUTTON_LB;
        presetXInputValues["XINPUT_BUTTON_LT"] =                        HID_XINPUT_BUTTON_LT;
        presetXInputValues["XINPUT_BUTTON_L3"] =                        HID_XINPUT_BUTTON_L3;
        presetXInputValues["XINPUT_BUTTON_RB"] =                        HID_XINPUT_BUTTON_RB;
        presetXInputValues["XINPUT_BUTTON_RT"] =                        HID_XINPUT_BUTTON_RT;
        presetXInputValues["XINPUT_BUTTON_R3"] =                        HID_XINPUT_BUTTON_R3;

        presetXInputValues["XINPUT_BUTTON_START"] =                     HID_XINPUT_BUTTON_START;
        presetXInputValues["XINPUT_BUTTON_BACK"] =                      HID_XINPUT_BUTTON_BACK;
        presetXInputValues["XINPUT_BUTTON_DPAD_TYPE"] =                 HID_XINPUT_BUTTON_DPAD_TYPE;

        presetXInputValues["XINPUT_BUTTON_DPAD_UP"] =                   HID_XINPUT_BUTTON_UP;
        presetXInputValues["XINPUT_BUTTON_DPAD_DOWN"] =                 HID_XINPUT_BUTTON_DOWN;
        presetXInputValues["XINPUT_BUTTON_DPAD_LEFT"] =                 HID_XINPUT_BUTTON_LEFT;
        presetXInputValues["XINPUT_BUTTON_DPAD_RIGHT"] =                HID_XINPUT_BUTTON_RIGHT;

        presetXInputValues["XINPUT_BUTTON_GUIDE"] =                     HID_XINPUT_BUTTON_GUIDE;

        presetSwitchProValues["SWITCH_PRO_BUTTON_A"] =                  HID_SWITCH_PRO_BT_BUTTON_A;
        presetSwitchProValues["SWITCH_PRO_BUTTON_B"] =                  HID_SWITCH_PRO_BT_BUTTON_B;
        presetSwitchProValues["SWITCH_PRO_BUTTON_X"] =                  HID_SWITCH_PRO_BT_BUTTON_X;
        presetSwitchProValues["SWITCH_PRO_BUTTON_Y"] =                  HID_SWITCH_PRO_BT_BUTTON_Y;

        presetSwitchProValues["SWITCH_PRO_BUTTON_PLUS"] =               HID_SWITCH_PRO_BT_BUTTON_PLUS;
        presetSwitchProValues["SWITCH_PRO_BUTTON_MINUS"] =              HID_SWITCH_PRO_BT_BUTTON_MINUS;
        presetSwitchProValues["SWITCH_PRO_BUTTON_HOME"] =               HID_SWITCH_PRO_BT_BUTTON_HOME;

        presetSwitchProValues["SWITCH_PRO_BUTTON_L"] =                  HID_SWITCH_PRO_BT_BUTTON_L;
        presetSwitchProValues["SWITCH_PRO_BUTTON_R"] =                  HID_SWITCH_PRO_BT_BUTTON_R;

        presetSwitchProValues["SWITCH_PRO_BUTTON_ZL"] =                 HID_SWITCH_PRO_BT_BUTTON_ZL;
        presetSwitchProValues["SWITCH_PRO_BUTTON_ZR"] =                 HID_SWITCH_PRO_BT_BUTTON_ZR;

        presetSwitchProValues["SWITCH_PRO_BUTTON_STICK_L"] =            HID_SWITCH_PRO_BT_BUTTON_STICK_L;
        presetSwitchProValues["SWITCH_PRO_BUTTON_STICK_R"] =            HID_SWITCH_PRO_BT_BUTTON_STICK_R;

        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_N"] =             HID_SWITCH_PRO_BT_BUTTON_DPAD_N;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_NE"] =            HID_SWITCH_PRO_BT_BUTTON_DPAD_NE;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_E"] =             HID_SWITCH_PRO_BT_BUTTON_DPAD_E;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_SE"] =            HID_SWITCH_PRO_BT_BUTTON_DPAD_SE;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_S"] =             HID_SWITCH_PRO_BT_BUTTON_DPAD_S;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_SW"] =            HID_SWITCH_PRO_BT_BUTTON_DPAD_SW;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_W"] =             HID_SWITCH_PRO_BT_BUTTON_DPAD_W;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_NW"] =            HID_SWITCH_PRO_BT_BUTTON_DPAD_NW;
        presetSwitchProValues["SWITCH_PRO_BUTTON_DPAD_NEUTRAL"] =       HID_SWITCH_PRO_BT_BUTTON_DPAD_NEUTRAL;

        presetKeyboardValues["KEYBOARD_SHIFT"] =                        HID_KEYBOARD_BUTTON_SHIFT;
        presetKeyboardValues["KEYBOARD_A"] =                            HID_KEYBOARD_BUTTON_A;
        presetKeyboardValues["KEYBOARD_B"] =                            HID_KEYBOARD_BUTTON_B;
        presetKeyboardValues["KEYBOARD_C"] =                            HID_KEYBOARD_BUTTON_C;
        presetKeyboardValues["KEYBOARD_D"] =                            HID_KEYBOARD_BUTTON_D;
        presetKeyboardValues["KEYBOARD_E"] =                            HID_KEYBOARD_BUTTON_E;
        presetKeyboardValues["KEYBOARD_F"] =                            HID_KEYBOARD_BUTTON_F;
        presetKeyboardValues["KEYBOARD_G"] =                            HID_KEYBOARD_BUTTON_G;
        presetKeyboardValues["KEYBOARD_H"] =                            HID_KEYBOARD_BUTTON_H;
        presetKeyboardValues["KEYBOARD_I"] =                            HID_KEYBOARD_BUTTON_I;
        presetKeyboardValues["KEYBOARD_J"] =                            HID_KEYBOARD_BUTTON_J;
        presetKeyboardValues["KEYBOARD_K"] =                            HID_KEYBOARD_BUTTON_K;
        presetKeyboardValues["KEYBOARD_L"] =                            HID_KEYBOARD_BUTTON_L;
        presetKeyboardValues["KEYBOARD_M"] =                            HID_KEYBOARD_BUTTON_M;
        presetKeyboardValues["KEYBOARD_N"] =                            HID_KEYBOARD_BUTTON_N;
        presetKeyboardValues["KEYBOARD_O"] =                            HID_KEYBOARD_BUTTON_O;
        presetKeyboardValues["KEYBOARD_P"] =                            HID_KEYBOARD_BUTTON_P;
        presetKeyboardValues["KEYBOARD_Q"] =                            HID_KEYBOARD_BUTTON_Q;
        presetKeyboardValues["KEYBOARD_R"] =                            HID_KEYBOARD_BUTTON_R;
        presetKeyboardValues["KEYBOARD_S"] =                            HID_KEYBOARD_BUTTON_S;
        presetKeyboardValues["KEYBOARD_T"] =                            HID_KEYBOARD_BUTTON_T;
        presetKeyboardValues["KEYBOARD_U"] =                            HID_KEYBOARD_BUTTON_U;
        presetKeyboardValues["KEYBOARD_V"] =                            HID_KEYBOARD_BUTTON_V;
        presetKeyboardValues["KEYBOARD_W"] =                            HID_KEYBOARD_BUTTON_W;
        presetKeyboardValues["KEYBOARD_X"] =                            HID_KEYBOARD_BUTTON_X;
        presetKeyboardValues["KEYBOARD_Y"] =                            HID_KEYBOARD_BUTTON_Y;
        presetKeyboardValues["KEYBOARD_Z"] =                            HID_KEYBOARD_BUTTON_Z;
        presetKeyboardValues["KEYBOARD_F1"] =                           HID_KEYBOARD_BUTTON_F1;
        presetKeyboardValues["KEYBOARD_F2"] =                           HID_KEYBOARD_BUTTON_F2;
        presetKeyboardValues["KEYBOARD_F3"] =                           HID_KEYBOARD_BUTTON_F3;
        presetKeyboardValues["KEYBOARD_F4"] =                           HID_KEYBOARD_BUTTON_F4;
        presetKeyboardValues["KEYBOARD_F5"] =                           HID_KEYBOARD_BUTTON_F5;
        presetKeyboardValues["KEYBOARD_F6"] =                           HID_KEYBOARD_BUTTON_F6;
        presetKeyboardValues["KEYBOARD_F7"] =                           HID_KEYBOARD_BUTTON_F7;
        presetKeyboardValues["KEYBOARD_F8"] =                           HID_KEYBOARD_BUTTON_F8;
        presetKeyboardValues["KEYBOARD_F9"] =                           HID_KEYBOARD_BUTTON_F9;
        presetKeyboardValues["KEYBOARD_F10"] =                          HID_KEYBOARD_BUTTON_F10;
        presetKeyboardValues["KEYBOARD_F11"] =                          HID_KEYBOARD_BUTTON_F11;
        presetKeyboardValues["KEYBOARD_F12"] =                          HID_KEYBOARD_BUTTON_F12;
        presetKeyboardValues["KEYBOARD_1"] =                            HID_KEYBOARD_BUTTON_1;
        presetKeyboardValues["KEYBOARD_2"] =                            HID_KEYBOARD_BUTTON_2;
        presetKeyboardValues["KEYBOARD_3"] =                            HID_KEYBOARD_BUTTON_3;
        presetKeyboardValues["KEYBOARD_4"] =                            HID_KEYBOARD_BUTTON_4;
        presetKeyboardValues["KEYBOARD_5"] =                            HID_KEYBOARD_BUTTON_5;
        presetKeyboardValues["KEYBOARD_6"] =                            HID_KEYBOARD_BUTTON_6;
        presetKeyboardValues["KEYBOARD_7"] =                            HID_KEYBOARD_BUTTON_7;
        presetKeyboardValues["KEYBOARD_8"] =                            HID_KEYBOARD_BUTTON_8;
        presetKeyboardValues["KEYBOARD_9"] =                            HID_KEYBOARD_BUTTON_9;
        presetKeyboardValues["KEYBOARD_0"] =                            HID_KEYBOARD_BUTTON_0;

        presetKeyboardValues["KEYBOARD_RETURN"] =                       HID_KEYBOARD_BUTTON_RETURN;
        presetKeyboardValues["KEYBOARD_ESCAPE"] =                       HID_KEYBOARD_BUTTON_ESCAPE;
        presetKeyboardValues["KEYBOARD_DELETE"] =                       HID_KEYBOARD_BUTTON_DELETE;
        presetKeyboardValues["KEYBOARD_TAB"] =                          HID_KEYBOARD_BUTTON_TAB;
        presetKeyboardValues["KEYBOARD_SPACEBAR"] =                     HID_KEYBOARD_BUTTON_SPACEBAR;
        presetKeyboardValues["KEYBOARD_CAPSLOCK"] =                     HID_KEYBOARD_BUTTON_CAPSLOCK;
        presetKeyboardValues["KEYBOARD_PRINTSCREEN"] =                  HID_KEYBOARD_BUTTON_PRINTSCREEN;
        presetKeyboardValues["KEYBOARD_SCROLLLOCK"] =                   HID_KEYBOARD_BUTTON_SCROLLLOCK;
        presetKeyboardValues["KEYBOARD_PAUSE"] =                        HID_KEYBOARD_BUTTON_PAUSE;
        presetKeyboardValues["KEYBOARD_INSERT"] =                       HID_KEYBOARD_BUTTON_INSERT;
        presetKeyboardValues["KEYBOARD_HOME"] =                         HID_KEYBOARD_BUTTON_HOME;
        presetKeyboardValues["KEYBOARD_PAGEUP"] =                       HID_KEYBOARD_BUTTON_PAGEUP;
        presetKeyboardValues["KEYBOARD_PAGEDOWN"] =                     HID_KEYBOARD_BUTTON_PAGEDOWN;
        presetKeyboardValues["KEYBOARD_DELETEFORWARD"] =                HID_KEYBOARD_BUTTON_DELETEFORWARD;
        presetKeyboardValues["KEYBOARD_LEFT_CONTROL"] =                 HID_KEYBOARD_BUTTON_LEFT_CONTROL;
        presetKeyboardValues["KEYBOARD_LEFT_ALT"] =                     HID_KEYBOARD_BUTTON_LEFT_ALT;
        presetKeyboardValues["KEYBOARD_RIGHT_CONTROL"] =                HID_KEYBOARD_BUTTON_RIGHT_CONTROL;
        presetKeyboardValues["KEYBOARD_RIGHT_SHIFT"] =                  HID_KEYBOARD_BUTTON_RIGHT_SHIFT;
        presetKeyboardValues["KEYBOARD_RIGHT_ALT"] =                    HID_KEYBOARD_BUTTON_RIGHT_ALT;
        presetKeyboardValues["KEYBOARD_END"] =                          HID_KEYBOARD_BUTTON_END;

        presetKeyboardValues["KEYBOARD_LEFT"] =                         HID_KEYBOARD_BUTTON_LEFT;
        presetKeyboardValues["KEYBOARD_RIGHT"] =                        HID_KEYBOARD_BUTTON_RIGHT;
        presetKeyboardValues["KEYBOARD_DOWN"] =                         HID_KEYBOARD_BUTTON_DOWN;
        presetKeyboardValues["KEYBOARD_UP"] =                           HID_KEYBOARD_BUTTON_UP;

        presetKeyboardValues["KEYBOARD_KEYPAD_1"] =                     HID_KEYBOARD_KEYPAD_BUTTON_1;
        presetKeyboardValues["KEYBOARD_KEYPAD_2"] =                     HID_KEYBOARD_KEYPAD_BUTTON_2;
        presetKeyboardValues["KEYBOARD_KEYPAD_3"] =                     HID_KEYBOARD_KEYPAD_BUTTON_3;
        presetKeyboardValues["KEYBOARD_KEYPAD_4"] =                     HID_KEYBOARD_KEYPAD_BUTTON_4;
        presetKeyboardValues["KEYBOARD_KEYPAD_5"] =                     HID_KEYBOARD_KEYPAD_BUTTON_5;
        presetKeyboardValues["KEYBOARD_KEYPAD_6"] =                     HID_KEYBOARD_KEYPAD_BUTTON_6;
        presetKeyboardValues["KEYBOARD_KEYPAD_7"] =                     HID_KEYBOARD_KEYPAD_BUTTON_7;
        presetKeyboardValues["KEYBOARD_KEYPAD_8"] =                     HID_KEYBOARD_KEYPAD_BUTTON_8;
        presetKeyboardValues["KEYBOARD_KEYPAD_9"] =                     HID_KEYBOARD_KEYPAD_BUTTON_9;
        presetKeyboardValues["KEYBOARD_KEYPAD_0"] =                     HID_KEYBOARD_KEYPAD_BUTTON_0;
        presetKeyboardValues["KEYBOARD_KEYPAD_NUMLOCK"] =               HID_KEYBOARD_KEYPAD_BUTTON_NUMLOCK;
        presetKeyboardValues["KEYBOARD_KEYPAD_MINUS"] =                 HID_KEYBOARD_KEYPAD_BUTTON_MINUS;
        presetKeyboardValues["KEYBOARD_KEYPAD_PLUS"] =                  HID_KEYBOARD_KEYPAD_BUTTON_PLUS;

        presetValues["VPAD_L_STICK"] =                                  DEF_L_STICK;
        presetValues["VPAD_R_STICK"] =                                  DEF_R_STICK;

        presetValues["DPAD_NORMAL"] =                                   CONTRPDM_Normal;
        presetValues["DPAD_HAT"] =                                      CONTRPDM_Hat;
        presetValues["DPAD_ABSOLUTE_2VALUES"] =                         CONTRPDM_Absolute_2Values;
        presetValues["TRUE"] =                                          1;
        presetValues["YES"] =                                           1;
        presetValues["ON"] =                                            1;
        presetValues["FALSE"] =                                         0;
        presetValues["NO"] =                                            0;
        presetValues["OFF"] =                                           0;

        presetSticks["GC_STICK_L_X"] =                                  HID_GC_STICK_L_X;
        presetSticks["GC_STICK_L_Y"] =                                  HID_GC_STICK_L_Y;
        presetSticks["GC_STICK_R_X"] =                                  HID_GC_STICK_R_X;
        presetSticks["GC_STICK_R_Y"] =                                  HID_GC_STICK_R_Y;

        presetSticks["DS3_STICK_L_X"] =                                 HID_DS3_STICK_L_X;
        presetSticks["DS3_STICK_L_Y"] =                                 HID_DS3_STICK_L_Y;
        presetSticks["DS3_STICK_R_X"] =                                 HID_DS3_STICK_R_X;
        presetSticks["DS3_STICK_R_Y"] =                                 HID_DS3_STICK_R_Y;

        presetSticks["DS4_STICK_L_X"] =                                 HID_DS4_STICK_L_X;
        presetSticks["DS4_STICK_L_Y"] =                                 HID_DS4_STICK_L_Y;
        presetSticks["DS4_STICK_R_X"] =                                 HID_DS4_STICK_R_X;
        presetSticks["DS4_STICK_R_Y"] =                                 HID_DS4_STICK_R_Y;

        presetSticks["XINPUT_STICK_L_X"] =                              HID_XINPUT_STICK_L_X;
        presetSticks["XINPUT_STICK_L_Y"] =                              HID_XINPUT_STICK_L_Y;
        presetSticks["XINPUT_STICK_R_X"] =                              HID_XINPUT_STICK_R_X;
        presetSticks["XINPUT_STICK_R_Y"] =                              HID_XINPUT_STICK_R_Y;

        presetSticks["SWITCH_PRO_STICK_L_X"] =                          HID_SWITCH_PRO_BT_STICK_L_X;
        presetSticks["SWITCH_PRO_STICK_L_Y"] =                          HID_SWITCH_PRO_BT_STICK_L_Y;
        presetSticks["SWITCH_PRO_STICK_R_X"] =                          HID_SWITCH_PRO_BT_STICK_R_X;
        presetSticks["SWITCH_PRO_STICK_R_Y"] =                          HID_SWITCH_PRO_BT_STICK_R_Y;

        presetSticks["GC_DPAD_MODE"] =                                  HID_GC_BUTTON_DPAD_TYPE;
        presetSticks["DS3_DPAD_MODE"] =                                 HID_DS3_BUTTON_DPAD_TYPE;
        presetSticks["DS4_DPAD_MODE"] =                                 HID_DS4_BUTTON_DPAD_TYPE;
        presetSticks["XINPUT_DPAD_MODE"] =                              HID_XINPUT_BUTTON_DPAD_TYPE;
        presetSticks["SWITCH_PRO_DPAD_MODE"] =                          HID_SWITCH_PRO_BT_BUTTON_DPAD_TYPE;

        gGamePadValuesToCONTRPSString["VPAD_BUTTON_A"] =  				CONTRPS_VPAD_BUTTON_A;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_B"] =                CONTRPS_VPAD_BUTTON_B;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_X"] =                CONTRPS_VPAD_BUTTON_X;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_Y"] =                CONTRPS_VPAD_BUTTON_Y;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_LEFT"] =             CONTRPS_VPAD_BUTTON_LEFT;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_RIGHT"] =            CONTRPS_VPAD_BUTTON_RIGHT;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_UP"] =               CONTRPS_VPAD_BUTTON_UP;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_DOWN"] =             CONTRPS_VPAD_BUTTON_DOWN;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_ZL"] =               CONTRPS_VPAD_BUTTON_ZL;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_ZR"] =               CONTRPS_VPAD_BUTTON_ZR;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_L"] =                CONTRPS_VPAD_BUTTON_L;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_R"] =                CONTRPS_VPAD_BUTTON_R;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_PLUS"] =             CONTRPS_VPAD_BUTTON_PLUS;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_MINUS"] =            CONTRPS_VPAD_BUTTON_MINUS;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_HOME"] =             CONTRPS_VPAD_BUTTON_HOME;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_SYNC"] =             CONTRPS_VPAD_BUTTON_SYNC;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_STICK_R"] =          CONTRPS_VPAD_BUTTON_STICK_R;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_STICK_L"] =          CONTRPS_VPAD_BUTTON_STICK_L;
        gGamePadValuesToCONTRPSString["VPAD_BUTTON_TV"] =               CONTRPS_VPAD_BUTTON_TV;

        gGamePadValuesToCONTRPSString["VPAD_STICK_R_EMULATION_LEFT"] =  CONTRPS_VPAD_STICK_R_EMULATION_LEFT;
        gGamePadValuesToCONTRPSString["VPAD_STICK_R_EMULATION_RIGHT"] = CONTRPS_VPAD_STICK_R_EMULATION_RIGHT;
        gGamePadValuesToCONTRPSString["VPAD_STICK_R_EMULATION_UP"] =    CONTRPS_VPAD_STICK_R_EMULATION_UP;
        gGamePadValuesToCONTRPSString["VPAD_STICK_R_EMULATION_DOWN"] =  CONTRPS_VPAD_STICK_R_EMULATION_DOWN;
        gGamePadValuesToCONTRPSString["VPAD_STICK_L_EMULATION_LEFT"] =  CONTRPS_VPAD_STICK_L_EMULATION_LEFT;
        gGamePadValuesToCONTRPSString["VPAD_STICK_L_EMULATION_RIGHT"] = CONTRPS_VPAD_STICK_L_EMULATION_RIGHT;
        gGamePadValuesToCONTRPSString["VPAD_STICK_L_EMULATION_UP"] =    CONTRPS_VPAD_STICK_L_EMULATION_UP;
        gGamePadValuesToCONTRPSString["VPAD_STICK_L_EMULATION_DOWN"] =  CONTRPS_VPAD_STICK_L_EMULATION_DOWN;

        deviceNames[CPStringTools::strfmt("%04X%04X",HID_GC_VID,        HID_GC_PID).c_str()]                = HID_GC_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_KEYBOARD_VID,  HID_KEYBOARD_PID).c_str()]          = HID_KEYBOARD_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_MOUSE_VID,     HID_MOUSE_PID).c_str()]             = HID_MOUSE_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_DS3_VID,       HID_DS3_PID).c_str()]               = HID_DS3_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_NEW_DS4_VID,   HID_NEW_DS4_PID).c_str()]           = HID_NEW_DS4_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_DS4_VID,       HID_DS4_PID).c_str()]               = HID_DS4_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_XINPUT_VID,    HID_XINPUT_PID).c_str()]            = HID_XINPUT_STRING;
        deviceNames[CPStringTools::strfmt("%04X%04X",HID_SWITCH_PRO_VID,HID_SWITCH_PRO_PID).c_str()]        = HID_SWITCH_PRO_STRING;
    }

    const u8 * getValuesForPreset(std::map<std::string,const u8*> values,std::string possibleValue);

    bool setIfValueIsPreset(std::map<std::string,const u8*> values,std::string possibleValue,s32 slot,s32 keyslot);
    bool setIfValueIsAControllerPresetEx(std::string value,s32 slot,s32 keyslot);

    void addDeviceNameEx(u16 vid,u16 pid,std::string value);
    std::string getStringByVIDPIDEx(u16 vid,u16 pid);
};
#endif
