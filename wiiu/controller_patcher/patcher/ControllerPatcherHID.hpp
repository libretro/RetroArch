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
 * @file ControllerPatcherHID.hpp
 * @author Maschell
 * @date 25 Aug 2016
 * \brief This files contain useful all function for the direct HID Access
 *
 * @see https://github.com/Maschell/controller_patcher
 */

#ifndef _CONTROLLER_PATCHER_HID_H_
#define _CONTROLLER_PATCHER_HID_H_

#include <vector>

#include "wiiu/syshid.h"
#include "wiiu/vpad.h"

#include "../ControllerPatcher.hpp"

#define SWAP16(x) ((x>>8) | ((x&0xFF)<<8))
#define SWAP8(x) ((x>>4) | ((x&0xF)<<4))

class ControllerPatcherHID{
        friend class ControllerPatcher;
        friend class ControllerPatcherUtils;
    public:
        static s32  externAttachDetachCallback(HIDDevice *p_device, u32 attach);
        static void externHIDReadCallback(u32 handle, unsigned char *buf, u32 bytes_transfered, my_cb_user * usr);

    private:
        static CONTROLLER_PATCHER_RESULT_OR_ERROR setVPADControllerData(VPADStatus * buffer,std::vector<HID_Data *>& data);
        static std::vector<HID_Data *> getHIDDataAll();
        static CONTROLLER_PATCHER_RESULT_OR_ERROR getHIDData(u32 hidmask, s32 pad,  HID_Data ** data);

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * Rumble
 *---------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/

        static void HIDRumble(u32 handle,my_cb_user *usr,u32 pad);

        static void HIDGCRumble(u32 handle,my_cb_user *usr);

        static void HIDDS3Rumble(u32 handle,my_cb_user *usr,s32 rumble);

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * HID Callbacks
 *--------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
        static s32 myAttachDetachCallback(HIDClient *p_client, HIDDevice *p_device, u32 attach);

        static void myHIDMouseReadCallback(u32 handle, s32 error, unsigned char *buf, u32 bytes_transfered, void *p_user);
        static void myHIDReadCallback(u32 handle, s32 error, unsigned char *buf, u32 bytes_transfered, void *p_user);

        static s32 AttachDetachCallback(HIDClient *p_client, HIDDevice *p_device, u32 attach);
        static void HIDReadCallback(u32 handle, unsigned char *buf, u32 bytes_transfered, my_cb_user * usr);
};

#endif /* _CONTROLLER_PATCHER_HID_H_ */
