/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0555 */
/* at Fri Feb 19 11:21:40 2010
 */
/* Compiler settings for interfaces\nvdaController\nvdaController.idl, interfaces\nvdaController\nvdaController.acf:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 7.00.0555 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __nvdaController_h__
#define __nvdaController_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __NvdaController_INTERFACE_DEFINED__
#define __NvdaController_INTERFACE_DEFINED__

/* interface NvdaController */
/* [implicit_handle][version][uuid] */ 

/* [comm_status][fault_status] */ error_status_t __stdcall nvdaController_testIfRunning( void);

/* [comm_status][fault_status] */ error_status_t __stdcall nvdaController_speakText( 
    /* [string][in] */ const wchar_t *text);

/* [comm_status][fault_status] */ error_status_t __stdcall nvdaController_cancelSpeech( void);

/* [comm_status][fault_status] */ error_status_t __stdcall nvdaController_brailleMessage( 
    /* [string][in] */ const wchar_t *message);


extern handle_t nvdaControllerBindingHandle;


extern RPC_IF_HANDLE nvdaController_NvdaController_v1_0_c_ifspec;
extern RPC_IF_HANDLE NvdaController_v1_0_c_ifspec;
extern RPC_IF_HANDLE nvdaController_NvdaController_v1_0_s_ifspec;
#endif /* __NvdaController_INTERFACE_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif
