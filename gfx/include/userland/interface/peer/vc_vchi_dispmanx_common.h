/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef VC_VCHI_DISPMANX_COMMON_H
#define VC_VCHI_DISPMANX_COMMON_H

typedef enum  {
   // IMPORTANT - DO NOT ALTER THE ORDER OF COMMANDS IN THIS ENUMERATION
   // NEW FUNCTIONS SHOULD BE ADDED TO THE END, AND MUST ALSO BE ADDED TO
   // THE HOST SIDE FUNCTION TABLE IN display_server.c.
   
   // No function configured - do not use
   EDispmanNoFunction = 0,
   
   // Dispman pre-configure functions
   EDispmanGetDevices,
   EDispmanGetModes,
   
   // Dispman resource-related functions
   EDispmanResourceCreate,
   EDispmanResourceCreateFromImage,
   EDispmanResourceDelete,
   EDispmanResourceGetData,
   EDispmanResourceGetImage,
   
   // Dispman display-related functions
   EDispmanDisplayOpen,
   EDispmanDisplayOpenMode,
   EDispmanDisplayOpenOffscreen,
   EDispmanDisplayReconfigure,
   EDispmanDisplaySetDestination,
   EDispmanDisplaySetBackground,
   EDispmanDisplayGetInfo,
   EDispmanDisplayClose,
   
   // Dispman update-related functions
   EDispmanUpdateStart,
   EDispmanUpdateSubmit,
   EDispmanUpdateSubmitSync,
   
   // Dispman element-related functions
   EDispmanElementAdd,
   EDispmanElementModified,
   EDispmanElementRemove,
   EDispmanElementChangeSource,
   EDispmanElementChangeLayer,
   EDispmanElementChangeAttributes,

   //More commands go here...
   EDispmanResourceFill,    //Comes from uideck
   EDispmanQueryImageFormats,
   EDispmanBulkWrite,
   EDispmanBulkRead,
   EDispmanDisplayOrientation,
   EDispmanSnapshot,
   EDispmanSetPalette,
   EDispmanVsyncCallback,

   EDispmanMaxFunction
} DISPMANX_COMMAND_T;

#endif
