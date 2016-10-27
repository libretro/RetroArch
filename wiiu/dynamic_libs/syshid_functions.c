/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "os_functions.h"
#include "syshid_functions.h"

unsigned int syshid_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(int, HIDSetup,void);
EXPORT_DECL(int, HIDTeardown,void);

EXPORT_DECL(int, HIDAddClient,HIDClient *p_client, HIDAttachCallback attach_callback);
EXPORT_DECL(int, HIDDelClient,HIDClient *p_client);

EXPORT_DECL(int, HIDGetDescriptor,unsigned int handle,u8 descriptor_type,u8 descriptor_index, u16 language_id, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);
EXPORT_DECL(int, HIDSetDescriptor,unsigned int handle,u8 descriptor_type,u8 descriptor_index, u16 language_id, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);

EXPORT_DECL(int, HIDSetProtocol,unsigned int handle,u8 interface_index,u8 protocol, HIDCallback hc, void *p_user);
EXPORT_DECL(int, HIDGetProtocol,unsigned int handle,u8 interface_index,u8 * protocol, HIDCallback hc, void *p_user);

EXPORT_DECL(int, HIDGetReport,u32 handle, u8 report_type, u8 report_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);
EXPORT_DECL(int, HIDSetReport,u32 handle, u8 report_type, u8 report_id, u8 *p_buffer, u32 buffer_length, HIDCallback hc, void *p_user);

EXPORT_DECL(int, HIDSetIdle,unsigned int handle, u8 interface_index,u8 duration, HIDCallback hc, void *p_user);

EXPORT_DECL(int, HIDRead,unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);
EXPORT_DECL(int, HIDWrite,unsigned int handle, unsigned char *p_buffer, unsigned int buffer_length, HIDCallback hc, void *p_user);

void InitAcquireSysHID(void)
{
     OSDynLoad_Acquire("nsyshid.rpl", &syshid_handle);
}

void InitSysHIDFunctionPointers(void)
{
    InitAcquireSysHID();

    if(syshid_handle == 0){
        return;
    }

    unsigned int funcPointer = 0;

    //! assigning those is not mandatory and it does not always work to load them
    OS_FIND_EXPORT(syshid_handle, HIDSetup);
    OS_FIND_EXPORT(syshid_handle, HIDTeardown);
    OS_FIND_EXPORT(syshid_handle, HIDAddClient);
    OS_FIND_EXPORT(syshid_handle, HIDDelClient);
    OS_FIND_EXPORT(syshid_handle, HIDGetDescriptor);
    OS_FIND_EXPORT(syshid_handle, HIDSetDescriptor);
    OS_FIND_EXPORT(syshid_handle, HIDRead);
    OS_FIND_EXPORT(syshid_handle, HIDWrite);
    OS_FIND_EXPORT(syshid_handle, HIDSetProtocol);
    OS_FIND_EXPORT(syshid_handle, HIDGetProtocol);
    OS_FIND_EXPORT(syshid_handle, HIDSetIdle);
    OS_FIND_EXPORT(syshid_handle, HIDGetReport);
    OS_FIND_EXPORT(syshid_handle, HIDSetReport);
}
