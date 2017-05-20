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
#include "./ConfigReader.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>

#include "wiiu/fs.h"
#include "wiiu/controller_patcher/utils/FSHelper.h"

#define FS_MOUNT_SOURCE_SIZE            0x300
#define FS_MAX_MOUNTPATH_SIZE           12

#define FS_CLIENT_SIZE                  0x1700
#define FS_CMD_BLOCK_SIZE               0xA80

#define FS_SOURCETYPE_EXTERNAL          0
#define FS_SOURCETYPE_HFIO              1

s32 ConfigReader::numberValidFiles = 0;
ConfigReader *ConfigReader::instance = NULL;

ConfigReader::ConfigReader(){
}

void ConfigReader::ReadAllConfigs(){
    std::vector<std::string> fileList = ScanFolder();
    if(fileList.size() > 0){
        if(HID_DEBUG){ printf("ConfigReader::ConfigReader(line %d): Found %d config files\n",__LINE__,fileList.size()); }
        processFileList(fileList);
    }
}


ConfigReader::~ConfigReader(){
    if(HID_DEBUG){ printf("ConfigReader::~ConfigReader(line %d): ~ConfigReader\n",__LINE__); }
    freeFSHandles();
}

void ConfigReader::freeFSHandles(){
    if(this->pClient != NULL){
        FSDelClient((FSClient *)this->pClient,-1);
        free(this->pClient);
        this->pClient = NULL;
    }
     if(this->pCmd != NULL){
        free(this->pCmd);
        this->pCmd = NULL;
    }
}


// Mounting the sdcard without any external lib to be portable (Currently broken)
s32 ConfigReader::InitSDCard(){
    if(HID_DEBUG){ printf("ConfigReader::InitSDCard(line %d): InitSDCard\n",__LINE__); }

    int result = -1;

    // get command and client
    this->pClient = malloc(sizeof(FSClient));
    this->pCmd = malloc(sizeof(FSCmdBlock));

    if(!pClient || !pCmd) {
        // just in case free if not 0
        if(pClient)
            free(pClient);
        if(pCmd)
            free(pCmd);
        return -2;
    }

    FSInit();
    FSInitCmdBlock((FSCmdBlock*)pCmd);
    FSAddClient((FSClient*)pClient, -1);

    char *mountPath = NULL;

    if((result = FS_Helper_MountFS(pClient, pCmd, &mountPath)) == 0) {
        //free(mountPath);
    }

    return result;
}

std::vector<std::string> ConfigReader::ScanFolder(){
    std::string path = CONTROLLER_PATCHER_PATH;
    s32 dirhandle = 0;
    if(HID_DEBUG){ printf("ConfigReader::ScanFolder(line %d): Opening %s\n",__LINE__,path.c_str()); }
    std::vector<std::string> config_files;
    if (this->pClient && this->pCmd){
        s32 status = 0;
        if((status = FSOpenDir((FSClient*)this->pClient,(FSCmdBlock*)this->pCmd,path.c_str(),(FSDirectoryHandle *)&dirhandle,-1)) == FS_STATUS_OK){
            FSDirectoryEntry dir_entry;
            while (FSReadDir((FSClient*)this->pClient,(FSCmdBlock*)this->pCmd, dirhandle, &dir_entry, -1) == FS_STATUS_OK){
                std::string full_path = path + "/" +  dir_entry.name;
                if((dir_entry.info.flags&FS_STAT_DIRECTORY) != FS_STAT_DIRECTORY){
                    if(CPStringTools::EndsWith(std::string(dir_entry.name),".ini")){
                        config_files.push_back(full_path);
                        if(HID_DEBUG){ printf("ConfigReader::ScanFolder(line %d): %s \n",__LINE__,full_path.c_str()); }
                    }
                }
            }
            FSCloseDir((FSClient*)this->pClient,(FSCmdBlock*)this->pCmd,dirhandle,-1);
        }else{
            printf("ConfigReader::ScanFolder(line %d): Failed to open %s!\n",__LINE__,path.c_str());
        }
    }
    return config_files;
}

void ConfigReader::processFileList(std::vector<std::string> path){

    for(std::vector<std::string>::iterator it = path.begin(); it != path.end(); ++it) {
        printf("ConfigReader::processFileList(line %d): Reading %s\n",__LINE__,it->c_str());
        std::string result = loadFileToString(*it);

        ConfigParser parser(result);
        parser.parseIni();
    }
}

std::string ConfigReader::loadFileToString(std::string path){
    FSFileHandle handle = 0;
    s32 status = 0;
    std::string strBuffer;
    char *  result = NULL;
    if(FS_Helper_GetFile(this->pClient,this->pCmd,path.c_str(), &result) == 0){
        if(result != NULL){
            strBuffer = std::string((char *)result);
            free(result);
            result = NULL;

            //! remove all windows crap signs
            strBuffer = CPStringTools::removeCharFromString(strBuffer,'\r');
            strBuffer = CPStringTools::removeCharFromString(strBuffer,' ');
            strBuffer = CPStringTools::removeCharFromString(strBuffer,'\t');
        }
    }else{
        printf("ConfigReader::loadFileToString(line %d): Failed to load %s\n",__LINE__,path.c_str());
    }
    return strBuffer;
}
