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
 * @file ControllerPatcher.hpp
 * @author Maschell
 * @date 30 Mar 2017
 * \brief This files contain all public accessible functions of the controller patcher engine
 *
 * @see https://github.com/Maschell/controller_patcher
 */

#ifndef _CONTROLLER_PATCHER_H_
#define _CONTROLLER_PATCHER_H_

#include <string>

#include "./patcher/ControllerPatcherDefs.h"
#include "./utils/ControllerPatcherThread.hpp"
#include "./utils/CPRetainVars.hpp"
#include "./utils/PadConst.hpp"
#include "./utils/CPStringTools.hpp"

#include "./patcher/ControllerPatcherHID.hpp"
#include "./patcher/ControllerPatcherUtils.hpp"

#include "./config/ConfigValues.hpp"
#include "./config/ConfigParser.hpp"

#include "./network/ControllerPatcherNet.hpp"
#include "./network/TCPServer.hpp"
#include "./network/UDPServer.hpp"
#include "./network/UDPClient.hpp"
#include "./ConfigReader.hpp"

#include "wiiu/vpad.h"

#define BUS_SPEED                       248625000
#define SECS_TO_TICKS(sec)              (((unsigned long long)(sec)) * (BUS_SPEED/4))
#define MILLISECS_TO_TICKS(msec)        (SECS_TO_TICKS(msec) / 1000)
#define MICROSECS_TO_TICKS(usec)        (SECS_TO_TICKS(usec) / 1000000)

#define wiiu_os_usleep(usecs)           OSSleepTicks(MICROSECS_TO_TICKS(usecs))

#define HID_DEBUG 0

class ControllerPatcher{
    public:
        /*-----------------------------------------------------------------------------------------------------------------------------------
         * Initialization
         *----------------------------------------------------------------------------------------------------------------------------------*/
        /**
            \brief Resets the data thats used by the controller configuration
        **/
        static void ResetConfig();
        /**
            \brief Initializes the libraries, functions, values and arrays. Need to be called on each start of an Application. Returns false on errors.
        **/
        static bool Init();

        /**
            \brief De-Initialises the controller_patcher
        **/
        static void DeInit();
        /**
            Initialises the button remapping
        **/
        static void InitButtonMapping();

        /**
            Starts the network server
        **/
        static void startNetworkServer();

        /**
            Stops the network server
        **/
        static void stopNetworkServer();

        /*-----------------------------------------------------------------------------------------------------------------------------------
         * Initialization
         *----------------------------------------------------------------------------------------------------------------------------------*/

        /**
            Sets the data in a given data from HID Devices. The information about which HID Device will be used is stored in the gControllerMapping array int slot 1-4 (counting starts at 0, which is the gamepad). The \p
            chan provides the information of the channel from which the data will be used. The mode sets the type of the buffer.

            @param buffer: A pointer to the struct where the result will be stored.
            @param chan:   Indicates the channel from which slot the information about the mapped HID Device will be used.
            @param mode:   Sets the type of the buffer. PRO_CONTROLLER_MODE_KPADDATA or PRO_CONTROLLER_MODE_WPADReadData

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/

        static CONTROLLER_PATCHER_RESULT_OR_ERROR setProControllerDataFromHID(void * data,s32 chan,s32 mode = PRO_CONTROLLER_MODE_KPADDATA);


        /**
            Sets the data in a given VPADStatus from HID Devices. The information about which HID Device will be used is stored in the gControllerMapping array in slot 0.

            @param buffer: A pointer to an KPADData struct where the result will be stored.
            @param chan:   Indicates the channel from which slot the information about the mapped HID Device will be used.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/

        static CONTROLLER_PATCHER_RESULT_OR_ERROR setControllerDataFromHID(VPADStatus * buffer);

         /*-----------------------------------------------------------------------------------------------------------------------------------
         * Useful functions
         *----------------------------------------------------------------------------------------------------------------------------------*/

        /**
            Enable the Controller mapping.
            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR enableControllerMapping();

        /**
            Disbale the Controller mapping. Afterwards all connected controllers will be used for the gamepad.
            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR disableControllerMapping();

        /**
            Disables the energy settings for the WiiU. Settings can be restored via restoreWiiUEnergySetting.
            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR disableWiiUEnergySetting();

        /**
            Restores the WiiU Energy Settings.
            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR restoreWiiUEnergySetting();

        /**
            Resets the controller mapping for a given controller type.

            @param type: The type of the controller.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR resetControllerMapping(UController_Type type);


        /**
            Adds a controller mapping

            @param type: The type of the controller.
            @param config: information about the added controller.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR addControllerMapping(UController_Type type,ControllerMappingPADInfo config);


        /**

            @return The first active mapping slot for the given controller type will be returned. If the controller type is not set active, -1 will be returned.
        **/
        static s32 getActiveMappingSlot(UController_Type type);

        /**
            @param type: The type of the controller.
            @param mapping_slot: information about the added controller.
            @return When the functions failed result < 0 is returned. Otherwise a pointer to a ControllerMappingPADInfo is returned.
        **/
        static ControllerMappingPADInfo * getControllerMappingInfo(UController_Type type,s32 mapping_slot);

        /**
            Checks if a emulated controller is connected for the given controller type / mapping slot.

            @param type: The type of the controller.
            @param mapping_slot: Slot of the controller mapped to this controller type (usually 0)

            @return
        **/
        static bool isControllerConnectedAndActive(UController_Type type,s32 mapping_slot = 0);

        /**
            Search for a connected mouse and returns a pointer to it's data.
            @return A pointer to the first connected mouse that is found. NULL if no mouse is connected.
        **/
        static HID_Mouse_Data * getMouseData();

        /**
            Sets a rumble status for a controller.

            @param type: The type of the controller.
            @param status: status of the rumble. 0 for off, 1 for on.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR setRumble(UController_Type type,u32 status);

        /**
            Reads the input of all connected HID devices. Each attached controller will write his date into given array until it's full.

            @param output: A pointer to an InputData array where the result will be stored. (Make sure to reset the array before using this function).
            @param array_size:   Size of the given InputData array.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful. If the result is > 0 the number of stored sets in the array is returned.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR gettingInputAllDevices(InputData * output,s32 array_size);

        /**
            Remaps the buttons in the given \p VPADStatus pointer. InitButtonMapping() needs to be called before calling this. The information about the remapping is stored in the config_controller array.
            One easy way to set it is using the a config file on the SD Card.

            @param buffer: A pointer to the buffer where the input will be read from and the result will be stored.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR buttonRemapping(VPADStatus * buffer, s32 buffer_count);

        /**
            Prints the current pressed down buttons of the given \p VPADStatus pointer. Uses the utils/logger.c UDP logger..

            @param buffer: A pointer to the buffer where the input will be read from.

            @return When the functions failed result < 0 is returned. If the result is == 0 the function was successful.
        **/
        static CONTROLLER_PATCHER_RESULT_OR_ERROR printVPADButtons(VPADStatus * buffer);

        static std::string getIdentifierByVIDPID(u16 vid,u16 pid);

        static void destroyConfigHelper();

        static CONTROLLER_PATCHER_RESULT_OR_ERROR doSamplingForDeviceSlot(u16 device_slot);

        static CONTROLLER_PATCHER_RESULT_OR_ERROR setRumbleActivated(bool value);

        static bool isRumbleActivated();
};

#endif /* _CONTROLLER_PATCHER_H_ */
