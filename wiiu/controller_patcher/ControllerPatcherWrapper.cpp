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
 #include "controller_patcher/ControllerPatcher.hpp"
 #include "controller_patcher/ControllerPatcherWrapper.h"

extern "C" void ControllerPatcherInit(void){
    ControllerPatcher::Init();
    ControllerPatcher::disableControllerMapping();
    ControllerPatcher::startNetworkServer();
    ControllerPatcher::disableWiiUEnergySetting();
}

extern "C" CONTROLLER_PATCHER_RESULT_OR_ERROR setControllerDataFromHID(VPADStatus * data){
    ControllerPatcher::setControllerDataFromHID(data);
}

extern "C" CONTROLLER_PATCHER_RESULT_OR_ERROR gettingInputAllDevices(InputData * output,s32 array_size){
    ControllerPatcher::gettingInputAllDevices(output,array_size);
}

extern "C" void ControllerPatcherDeInit(void){
    ControllerPatcher::restoreWiiUEnergySetting();
    ControllerPatcher::stopNetworkServer();
    ControllerPatcher::DeInit();
}
