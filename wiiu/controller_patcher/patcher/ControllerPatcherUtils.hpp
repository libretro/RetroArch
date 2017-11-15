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

/**
 * @file ControllerPatcherUtil.hpp
 * @author Maschell
 * @date 25 Aug 2016
 * \brief This files contain useful functions for the controller patcher engine
 *
 * @see https://github.com/Maschell/controller_patcher
 */

#ifndef _CONTROLLER_PATCHER_UTIL_H_
#define _CONTROLLER_PATCHER_UTIL_H_


#include "wiiu/vpad.h"
#include "wiiu/kpad.h"

#include "../ControllerPatcher.hpp"

class ControllerPatcherUtils{
    //give the other classes access to the private functions.
    friend class ControllerPatcher;
    friend class ControllerPatcherHID;
    friend class ConfigParser;

    public:
        /**
        \brief Returns the device slot for a given HID-Mask.

        \param hidmask Given HID-Mask

        \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful. The returned value is the deviceslot of the given HID-Mask
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getDeviceSlot(u32 hidmask);

        /**
        \brief Returns the device slot for a given HID-Mask.

        \param handle Given HID-handle
        \param data Given my_cb_user ** where the result will be stored. Valid pointer when result is >= 0.

        \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful. The actual result will be store in the given my_cb_user **.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getDataByHandle(s32 handle, my_cb_user ** data);

        /**
        \brief Returns the VID/PID for the given device slot.

        \param deviceslot Given device slot
        \param vidpid Pointer to the DeviceVIDPIDInfo struct where the result will be stored.

        \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful. The actual result will be store in the given DeviceVIDPIDInfo *.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getVIDPIDbyDeviceSlot(s32 deviceslot, DeviceVIDPIDInfo * vidpid);

            /** \brief Set the VPAD data for a given KPAD data.
         *
         * \param vpad_buffer VPADStatus* A pointer to the VPAD Data where the result will be stored.
         * \param pro_buffer KPADData* A pointer to the given KPADData data.
         * \param lastButtonsPressesPRO u32* A pointer to the button presses of the previous call. Will be updated while calling.
         * \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR translateToVPAD(VPADStatus * vpad_buffer,KPADData * pro_buffer,u32 * lastButtonsPressesVPAD);
    private:
    /*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     * Analyse inputs
     *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

        /** \brief Checks if a the given @p VPADButton was pressed in the given HID @data. When it was pressed, the result will be set the in given @p buttons_hold
         *
         * \param data Pointer to the HID_Data from where the input is read.
         * \param buttons_hold Pointer to the u32 where the result will be written to.
         * \param VPADButton The button that will be checked
         * \return When the functions failed result < 0 is returned.If the result is >= 0 the function was successful.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getButtonPressed(HID_Data * data, s32 * buttons_hold, s32 VPADButton);


        /** \brief Checks if a given value is set in the HID_DATA given the data in the slot number provided by cur_config.
         *
         * \param data Pointer to the HID_Data from where the input is read.
         * \param cur_config slot of the configuration array which will be checked.
         * \return When the functions failed result < 0 is returned. If the value is set, 1 will be returned. Otherwise 0.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR isValueSet(HID_Data * data,s32 cur_config);


        /** \brief Checks if a given key in the keyboard data is pressed.
         *
         * \param keyboardData A pointer to the keyboard data.
         * \param key A pointer to the keyboard data.
         * \return When the functions failed result < 0 is returned. If the key is active pressed, 1 is returned.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR isInKeyboardData(unsigned char * keyboardData,s32 key);

    /*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     * Utils for setting the Button data
     *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

        /** \brief Checks if a @p VPADButton (VPAD_BUTTON_XXX) is set in the given @p CONTRPS_SLOT (usually the one for buttons remapping) of the GamePad. When its set it'll be
         *         set for the corresponding Button (aka button remapping). When the @p CONTRPS_SLOT is not valid, the normal buttons layout will be used.
         *
         * \param old_buffer A pointer to a VPADStatus struct from which will be read.
         * \param new_buffer A pointer to a VPADStatus struct where the result will be written.
         * \param VPADButton The buttons that will be may replaced
         * \param CONTRPS_SLOT The CONTRPS_SLOT where the VPAD_Buttons we want to use instead of the parameter "VPADButton" could be saved.
         * \return When the functions failed result < 0 is returned. If the pad is active/connected, 1 is returned.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR setButtonRemappingData(VPADStatus * old_buffer, VPADStatus * new_buffer,u32 VPADButton, s32 CONTRPS_SLOT);

        /**
            \brief Checks if a given button (oldVPADButton) is set in a given VPADStatus struct (old_buffer). If its set, it will set an other
            button (newVPADButton) to the second given VPADStatus struct (new_buffer)

            \param old_buffer       A pointer to a VPADStatus struct from which will be read.
            \param new_buffer       A pointer to a VPADStatus struct where the result will be written.
            \param oldVPADButton    The buttons that need to be set in the first VPADStatus
            \param newVPADButton    The buttons that will be set in the second VPADStatus, when the oldVPADButton is pressed in the first buffer.

            \return When the functions failed result < 0 is returned. If the pad is active/connected, 1 is returned.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR setButtonData(VPADStatus * old_buffer, VPADStatus * new_buffer,u32 oldVPADButton,u32 newVPADButton);

    /*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     * Pad Status functions
     *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
        /**
            \brief Checks if a controller is attached for the given HID-Mask and pad.

            \param hidmask      Bit-Mask of the target hid-device.
            \param pad          Defines for which pad the connection will be checked.

            \return When the functions failed result < 0 is returned. If the pad is active/connected, 1 is returned.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR checkActivePad(u32 hidmask,s32 pad);

        /**
            \brief Returns the first active pad of devices with the given HID-Mask. Currently only implemented for the GC-Adapter. Every other pad will always return 0.

            \param hidmask      Bit-Mask of the target hid-device.

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful. The returned value is fist active pad.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getActivePad(u32 hidmask);

    /*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     * Stick functions
     *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
        /**
            \brief Normalizes the stick to valid values.

            \param stick  Pointer to the stick that will be normalized

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR normalizeStickValues(VPADVec2D * stick);

        /**
            \brief Converts the digital absolute stick data into a float value. It also applies the deadzones, and can invert the result.

            \param value        Given current value of the stick axis
            \param default_val  Value in neutral axis-position
            \param min          Value that represents -1.0f
            \param max          Value that represents 1.0f
            \param invert       Set to 1 if the axis needs to be inverted
            \param deadzone     Deadzone

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static f32 convertAnalogValue(u8 value, u8 default_val, u8 min, u8 max, u8 invert,u8 deadzone);

        /**
            \brief Calculates a the stick data (VPADVec2D) from given digital direction.

            \param stick_values bits need to set for each direction. (STICK_VALUE_UP,STICK_VALUE_DOWN,STICK_VALUE_LEFT,STICK_VALUE_RIGHT)

            \return The VPADVec2D with the set values.
        **/
        static VPADVec2D getAnalogValueByButtons(u8 stick_values);

        /**
            \brief Handles the analog-stick data of HID devices. The result will written in the VPADStatus buffer.

            \param data Pointer to the current data of the HID device
            \param buffer Pointer to VPADStatus where the analog-stick data will be set.

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR convertAnalogSticks(HID_Data * data,VPADStatus * buffer);

    /*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     * Mouse functions
     *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
        /**
            \brief Set the touch data in the VPADStatus buffer.
            Currently its only possible to set the touch data from a Mouse

            \param data The current data of the HID device
            \param buffer Pointer to VPADStatus where the touch data will be set.

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR setTouch(HID_Data * data,VPADStatus * buffer);

        /** \brief  Checks if the mouse mode needs to be changed. Sets it to the new mode if necessary.
         *         Currently the incoming data needs to be from a keyboard.
         *
         * \param data HID_Data* Pointer to the current data
         * \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR checkAndSetMouseMode(HID_Data * data);

        /**
            \brief Set the emulated sticks for a given VPAD data.

            \param buffer: A pointer to the given VPAD Data.
            \param last_emulatedSticks: A pointer to the button presses of the previous call. Will be updated while calling.

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR setEmulatedSticks(VPADStatus * buffer, u32 * last_emulatedSticks);

    /*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
     * Other functions
     *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
        /** \brief Set the Pro Controller for a given VPAD data.
         *
         * \param vpad_buffer VPADStatus* A pointer to the given VPAD Data.
         * \param pro_buffer KPADData* A pointer to the KPADData where the result will be stored.
         * \param lastButtonsPressesPRO u32* A pointer to the button presses of the previous call. Will be updated while calling.
         * \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
         *
         */
        static CONTROLLER_PATCHER_RESULT_OR_ERROR translateToPro(VPADStatus * vpad_buffer,KPADData * pro_buffer,u32 * lastButtonsPressesPRO);
        static CONTROLLER_PATCHER_RESULT_OR_ERROR translateToProWPADRead(VPADStatus * vpad_buffer,WPADReadData * pro_buffer);

        /**
            \brief Checks if the value at the given device + CONTRPS slot equals the expected value.

            \param device_slot
            \param CONTRPS_slot
            \param expectedValue

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR checkValueinConfigController(s32 device_slot,s32 CONTRPS_slot,s32 expectedValue);

        /**
            \brief Sets two u8 values to the given pointer.

            \param dest: pointer to the destination array.
            \param first: Value that will be written in @p dest[0]
            \param second: Value that will be written in @p dest[1]
        **/
        static void setConfigValue(u8 * dest , u8 first, u8 second);

        /**
            \brief Saves a new free device slot and the corresponding HID-Mask in the given @p HIDSlotData pointer

            \param slotdata Pointer to the HIDSlotData struct where the result will be saved.

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getNextSlotData(HIDSlotData * slotdata);

        /**
            \brief Fills up a given DeviceInfo, which provides a valid VID/PID, with HIDSlotData.

            \param info Pointer the target DeviceInfo. The VID/PID need to be set, the HIDSlotData will be filled with data.

            \return When the functions failed result < 0 is returned. If the result is >= 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getDeviceInfoFromVidPid(DeviceInfo * info);

         /**
            \brief returns the internal slot number of the device. Some adapters have multiple slot and send the data for each one
                alternating (with an identifier at the beginning). This function searches for the identifier (if it's set) and returns the
                slot number relative to this pad.

            \param device slot
            \param current input data
            \return The relative slot in the device
        **/
        static s32 getPadSlotInAdapter(s32 deviceslot, u8 * input_data);

         /**
        \brief returns a pointer to the ControllerMapping to the given controller type

        \param type controller type

        \return pointer to ControllerMapping data, null is type was invalid
        **/
        static ControllerMappingPAD * getControllerMappingByType(UController_Type type);

        static CONTROLLER_PATCHER_RESULT_OR_ERROR doSampling(u16 deviceslot,u8 padslot,bool ignorePadSlot);
};

#endif /* _CONTROLLER_PATCHER_UTIL_H_ */
