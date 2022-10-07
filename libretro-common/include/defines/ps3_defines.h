/* Copyright (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ps3_defines.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _PS3_DEFINES_H
#define _PS3_DEFINES_H

/*============================================================
	AUDIO PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <audio/audio.h>
#include <sys/thread.h>

#include <sys/event_queue.h>
#include <lv2/mutex.h>
#include <lv2/cond.h>

/*forward decl. for audioAddData */
extern int audioAddData(uint32_t portNum, float *data,
      uint32_t frames, float volume);

#define PS3_SYS_NO_TIMEOUT 0
#define param_attrib attrib

#else
#include <sdk_version.h>
#include <cell/audio.h>
#include <sys/event.h>
#include <sys/synchronization.h>

#define numChannels nChannel
#define numBlocks nBlock
#define param_attrib attr

#define audioQuit cellAudioQuit 
#define audioInit cellAudioInit
#define audioPortStart cellAudioPortStart
#define audioPortOpen cellAudioPortOpen
#define audioPortClose cellAudioPortClose
#define audioPortStop cellAudioPortStop
#define audioPortParam CellAudioPortParam
#define audioPortOpen cellAudioPortOpen
#define audioAddData cellAudioAddData

/* event queue functions */
#define sysEventQueueReceive sys_event_queue_receive
#define audioSetNotifyEventQueue cellAudioSetNotifyEventQueue
#define audioRemoveNotifyEventQueue cellAudioRemoveNotifyEventQueue
#define audioCreateNotifyEventQueue cellAudioCreateNotifyEventQueue

#define sysLwCondCreate sys_lwcond_create
#define sysLwCondDestroy sys_lwcond_destroy
#define sysLwCondWait sys_lwcond_wait
#define sysLwCondSignal sys_lwcond_signal

#define sysLwMutexDestroy sys_lwmutex_destroy
#define sysLwMutexLock sys_lwmutex_lock
#define sysLwMutexUnlock sys_lwmutex_unlock
#define sysLwMutexCreate sys_lwmutex_create

#define AUDIO_BLOCK_SAMPLES CELL_AUDIO_BLOCK_SAMPLES
#define SYSMODULE_NET CELL_SYSMODULE_NET
#define PS3_SYS_NO_TIMEOUT SYS_NO_TIMEOUT

#define sys_lwmutex_attr_t sys_lwmutex_attribute_t 
#define sys_lwcond_attr_t sys_lwcond_attribute_t 
#define sys_sem_t sys_semaphore_t

#define sysGetSystemTime sys_time_get_system_time
#define sysModuleLoad cellSysmoduleLoadModule
#define sysModuleUnload cellSysmoduleUnloadModule

#define netInitialize sys_net_initialize_network

#endif

/*============================================================
	INPUT PAD PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <io/pad.h>
#define CELL_PAD_CAPABILITY_SENSOR_MODE      4
#define CELL_PAD_SETTING_SENSOR_ON           4
#define CELL_PAD_STATUS_ASSIGN_CHANGES       2
#define CELL_PAD_BTN_OFFSET_DIGITAL1         2
#define CELL_PAD_BTN_OFFSET_DIGITAL2         3
#define CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X   4
#define CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y   5
#define CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X    6
#define CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y    7
#define CELL_PAD_BTN_OFFSET_PRESS_RIGHT      8
#define CELL_PAD_BTN_OFFSET_PRESS_LEFT       9
#define CELL_PAD_BTN_OFFSET_PRESS_UP         10
#define CELL_PAD_BTN_OFFSET_PRESS_DOWN       11
#define CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE   12
#define CELL_PAD_BTN_OFFSET_PRESS_CIRCLE     13
#define CELL_PAD_BTN_OFFSET_PRESS_CROSS      14
#define CELL_PAD_BTN_OFFSET_PRESS_SQUARE     15
#define CELL_PAD_BTN_OFFSET_PRESS_L1         16
#define CELL_PAD_BTN_OFFSET_PRESS_R1         17
#define CELL_PAD_BTN_OFFSET_PRESS_L2         18
#define CELL_PAD_BTN_OFFSET_PRESS_R2         19
#define CELL_PAD_BTN_OFFSET_SENSOR_X         20
#define CELL_PAD_BTN_OFFSET_SENSOR_Y         21
#define CELL_PAD_BTN_OFFSET_SENSOR_Z         22
#define CELL_PAD_BTN_OFFSET_SENSOR_G         23
#define CELL_PAD_CTRL_LEFT          (128)
#define CELL_PAD_CTRL_DOWN          (64)
#define CELL_PAD_CTRL_RIGHT         (32)
#define CELL_PAD_CTRL_UP            (16)
#define CELL_PAD_CTRL_START         (8)
#define CELL_PAD_CTRL_R3            (4)
#define CELL_PAD_CTRL_L3            (2)
#define CELL_PAD_CTRL_SELECT        (1)
#define CELL_PAD_CTRL_SQUARE        (128)
#define CELL_PAD_CTRL_CROSS         (64)
#define CELL_PAD_CTRL_CIRCLE        (32)
#define CELL_PAD_CTRL_TRIANGLE      (16)
#define CELL_PAD_CTRL_R1            (8)
#define CELL_PAD_CTRL_L1            (4)
#define CELL_PAD_CTRL_R2            (2)
#define CELL_PAD_CTRL_L2            (1)
#define CELL_PAD_CTRL_LDD_PS        (1)
#define CELL_PAD_STATUS_CONNECTED   (1)
#define CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN
#define CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS  (1)
#define CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE (0)
#define now_connect connected
#define CellPadActParam padActParam
#define cellPadSetPortSetting ioPadSetPortSetting
#define cellSysutilGetSystemParamInt sysUtilGetSystemParamInt
#define cellPadSetActDirect ioPadSetActDirect
#define CellPadInfo2 padInfo2
#define cellPadGetInfo2 ioPadGetInfo2
#define CellPadData padData
#define cellPadGetData ioPadGetData
#define cellPadInit ioPadInit 
#define cellPadEnd ioPadEnd
#else
#include <cell/pad.h>
#define padInfo2 CellPadInfo2
#define padData CellPadData
#define ioPadGetInfo2 cellPadGetInfo2 
#define ioPadGetData cellPadGetData
#define ioPadInit cellPadInit
#define ioPadEnd cellPadEnd
#endif

/*============================================================
	INPUT MOUSE PROTOTYPES
============================================================ */

#ifdef HAVE_MOUSE

#ifdef __PSL1GHT__
#include <io/mouse.h>

/* define ps3 mouse structs */
#define CellMouseInfo mouseInfo
#define CellMouseData mouseData

/* define all the ps3 mouse functions */
#define cellMouseInit ioMouseInit
#define cellMouseGetData ioMouseGetData
#define cellMouseEnd ioMouseEnd
#define cellMouseGetInfo ioMouseGetInfo

/* PSL1GHT does not define these in its header */
#define CELL_MOUSE_BUTTON_1 (UINT64_C(1) << 0) /* Button 1 */
#define CELL_MOUSE_BUTTON_2 (UINT64_C(1) << 1) /* Button 2 */
#define CELL_MOUSE_BUTTON_3 (UINT64_C(1) << 2) /* Button 3 */
#define CELL_MOUSE_BUTTON_4 (UINT64_C(1) << 3) /* Button 4 */
#define CELL_MOUSE_BUTTON_5 (UINT64_C(1) << 4) /* Button 5 */
#define CELL_MOUSE_BUTTON_6 (UINT64_C(1) << 5) /* Button 6 */
#define CELL_MOUSE_BUTTON_7 (UINT64_C(1) << 6) /* Button 7 */
#define CELL_MOUSE_BUTTON_8 (UINT64_C(1) << 7) /* Button 8 */

#else
#include <cell/mouse.h>
#endif

#endif

/*============================================================
	OSK PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <sysutil/osk.h>
/* define all the OSK functions */
#define pOskLoadAsync oskLoadAsync
#define pOskSetLayoutMode oskSetLayoutMode
#define pOskSetKeyLayoutOption oskSetKeyLayoutOption
#define pOskGetSize oskGetSize
#define pOskDisableDimmer oskDisableDimmer
#define pOskAbort oskAbort
#define pOskUnloadAsync oskUnloadAsync

/* define OSK structs / types */
#define sys_memory_container_t sys_mem_container_t
#define CellOskDialogPoint oskPoint
#define CellOskDialogInputFieldInfo oskInputFieldInfo
#define CellOskDialogCallbackReturnParam oskCallbackReturnParam
#define CellOskDialogParam oskParam

#define osk_allowed_panels allowedPanels
#define osk_prohibit_flags prohibitFlags

#define osk_inputfield_message message
#define osk_inputfield_starttext startText
#define osk_inputfield_max_length maxLength
#define osk_callback_return_param res
#define osk_callback_num_chars len
#define osk_callback_return_string str

/* define the OSK defines */
#define CELL_OSKDIALOG_10KEY_PANEL OSK_10KEY_PANEL
#define CELL_OSKDIALOG_FULLKEY_PANEL OSK_FULLKEY_PANEL
#define CELL_OSKDIALOG_LAYOUTMODE_X_ALIGN_CENTER OSK_LAYOUTMODE_HORIZONTAL_ALIGN_CENTER
#define CELL_OSKDIALOG_LAYOUTMODE_Y_ALIGN_TOP OSK_LAYOUTMODE_VERTICAL_ALIGN_TOP
#define CELL_OSKDIALOG_PANELMODE_NUMERAL OSK_PANEL_TYPE_NUMERAL
#define CELL_OSKDIALOG_PANELMODE_NUMERAL_FULL_WIDTH OSK_PANEL_TYPE_NUMERAL_FULL_WIDTH
#define CELL_OSKDIALOG_PANELMODE_ALPHABET OSK_PANEL_TYPE_ALPHABET
#define CELL_OSKDIALOG_PANELMODE_ENGLISH OSK_PANEL_TYPE_ENGLISH
#define CELL_OSKDIALOG_INPUT_FIELD_RESULT_OK (0)
#define CELL_OSKDIALOG_INPUT_FIELD_RESULT_CANCELED (1)
#define CELL_OSKDIALOG_INPUT_FIELD_RESULT_ABORT (2)
#define CELL_OSKDIALOG_INPUT_FIELD_RESULT_NO_INPUT_TEXT (3)
#define CELL_OSKDIALOG_STRING_SIZE (512)
#else
#include <sysutil/sysutil_oskdialog.h>
/* define all the OSK functions */
#define pOskLoadAsync cellOskDialogLoadAsync
#define pOskSetLayoutMode cellOskDialogSetLayoutMode
#define pOskSetKeyLayoutOption cellOskDialogSetKeyLayoutOption
#define pOskGetSize cellOskDialogGetSize
#define pOskDisableDimmer cellOskDialogDisableDimmer
#define pOskAbort cellOskDialogAbort
#define pOskUnloadAsync cellOskDialogUnloadAsync

/* define OSK structs / types */
#define osk_allowed_panels allowOskPanelFlg
#define osk_prohibit_flags prohibitFlgs
#define osk_inputfield_message message
#define osk_inputfield_starttext init_text
#define osk_inputfield_max_length limit_length
#define osk_callback_return_param result
#define osk_callback_num_chars numCharsResultString
#define osk_callback_return_string pResultString
#endif

/*============================================================
	JPEG/PNG DECODING/ENCODING PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__

#define spu_enable enable
#define stream_select stream
#define color_alpha alpha
#define color_space space
#define output_mode mode
#define output_bytes_per_line bytes_per_line
#define output_width width
#define output_height height

#define CELL_OK 0
#define PTR_NULL 0

#else
/* define the JPEG/PNG struct member names */
#define spu_enable spuThreadEnable
#define ppu_prio ppuThreadPriority
#define spu_prio spuThreadPriority
#define malloc_func cbCtrlMallocFunc
#define malloc_arg cbCtrlMallocArg
#define free_func cbCtrlFreeFunc
#define free_arg cbCtrlFreeArg
#define stream_select srcSelect
#define file_name fileName
#define file_offset fileOffset
#define file_size fileSize
#define stream_ptr streamPtr
#define stream_size streamSize
#define down_scale downScale
#define color_alpha outputColorAlpha
#define color_space outputColorSpace
#define cmd_ptr commandPtr
#define quality method
#define output_mode outputMode
#define output_bytes_per_line outputBytesPerLine
#define output_width outputWidth
#define output_height outputHeight
#define bit_depth outputBitDepth
#define pack_flag outputPackFlag
#define alpha_select outputAlphaSelect

#define PTR_NULL NULL

#endif

/*============================================================
	TIMER PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#define sys_timer_usleep usleep
#endif

/*============================================================
	THREADING PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <sys/thread.h>

/* FIXME - not sure if this is correct -> FIXED! 1 and not 0 */
#define SYS_THREAD_CREATE_JOINABLE THREAD_JOINABLE

#else
#include <sys/ppu_thread.h>

#define SYS_PROCESS_SPAWN_STACK_SIZE_1M SYS_PROCESS_PRIMARY_STACK_SIZE_1M 
#define SYS_THREAD_CREATE_JOINABLE SYS_PPU_THREAD_CREATE_JOINABLE

#define sysThreadCreate sys_ppu_thread_create 
#define sysThreadJoin sys_ppu_thread_join 
#define sysThreadExit sys_ppu_thread_exit 

#define sysProcessExit sys_process_exit 
#define sysProcessExitSpawn2 sys_game_process_exitspawn 

#endif

/*============================================================
	MEMORY PROTOTYPES
============================================================ */

#ifndef __PSL1GHT__
#define sysMemContainerCreate sys_memory_container_create 
#define sysMemContainerDestroy sys_memory_container_destroy 
#endif

/*============================================================
	RSX PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <sysutil/video.h>
#define CELL_GCM_FALSE GCM_FALSE
#define CELL_GCM_TRUE GCM_TRUE

#define CELL_GCM_ONE GCM_ONE
#define CELL_GCM_ZERO GCM_ZERO
#define CELL_GCM_ALWAYS GCM_ALWAYS

#define CELL_GCM_LOCATION_LOCAL GCM_LOCATION_RSX
#define CELL_GCM_LOCATION_MAIN GCM_LOCATION_CELL

#define CELL_GCM_MAX_RT_DIMENSION (4096)

#define CELL_GCM_TEXTURE_LINEAR_NEAREST GCM_TEXTURE_LINEAR_MIPMAP_NEAREST
#define CELL_GCM_TEXTURE_LINEAR_LINEAR GCM_TEXTURE_LINEAR_MIPMAP_LINEAR
#define CELL_GCM_TEXTURE_NEAREST_LINEAR GCM_TEXTURE_NEAREST_MIPMAP_LINEAR
#define CELL_GCM_TEXTURE_NEAREST_NEAREST GCM_TEXTURE_NEAREST_MIPMAP_NEAREST
#define CELL_GCM_TEXTURE_NEAREST GCM_TEXTURE_NEAREST
#define CELL_GCM_TEXTURE_LINEAR GCM_TEXTURE_LINEAR

#define CELL_GCM_TEXTURE_A8R8G8B8 GCM_TEXTURE_FORMAT_A8R8G8B8
#define CELL_GCM_TEXTURE_R5G6B5 GCM_TEXTURE_FORMAT_R5G6B5
#define CELL_GCM_TEXTURE_A1R5G5B5 GCM_TEXTURE_FORMAT_A1R5G5B5

#define CELL_GCM_TEXTURE_CLAMP_TO_EDGE GCM_TEXTURE_CLAMP_TO_EDGE

#define CELL_GCM_TEXTURE_MAX_ANISO_1 GCM_TEXTURE_MAX_ANISO_1
#define CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX GCM_TEXTURE_CONVOLUTION_QUINCUNX
#define CELL_GCM_TEXTURE_ZFUNC_NEVER GCM_TEXTURE_ZFUNC_NEVER

#define CELL_GCM_DISPLAY_HSYNC GCM_FLIP_HSYNC
#define CELL_GCM_DISPLAY_VSYNC GCM_FLIP_VSYNC

#define CELL_GCM_CLEAR_R GCM_CLEAR_R
#define CELL_GCM_CLEAR_G GCM_CLEAR_G
#define CELL_GCM_CLEAR_B GCM_CLEAR_B
#define CELL_GCM_CLEAR_A GCM_CLEAR_A

#define CELL_GCM_FUNC_ADD GCM_FUNC_ADD

#define CELL_GCM_SMOOTH	(0x1D01)
#define CELL_GCM_DEBUG_LEVEL2 2

#define CELL_GCM_COMPMODE_DISABLED 0

#define CELL_GCM_TRANSFER_LOCAL_TO_LOCAL 0

#define CELL_GCM_TEXTURE_REMAP_ORDER_XYXY (0)
#define CELL_GCM_TEXTURE_REMAP_ORDER_XXXY (1)

#define CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL (0)

#define CELL_GCM_TEXTURE_REMAP_FROM_A (0)
#define CELL_GCM_TEXTURE_REMAP_FROM_R (1)
#define CELL_GCM_TEXTURE_REMAP_FROM_G (2)
#define CELL_GCM_TEXTURE_REMAP_FROM_B (3)

#define CELL_GCM_TEXTURE_REMAP_ZERO (0)
#define CELL_GCM_TEXTURE_REMAP_ONE (1)
#define CELL_GCM_TEXTURE_REMAP_REMAP (2)

#define CELL_GCM_MAX_TEXIMAGE_COUNT (16)

#define CELL_GCM_TEXTURE_WRAP (1)

#define CELL_GCM_TEXTURE_NR (0x00)
#define CELL_GCM_TEXTURE_LN (0x20)

#define CELL_GCM_TEXTURE_B8 (0x81)

#define CELL_RESC_720x480 RESC_720x480
#define CELL_RESC_720x576 RESC_720x576
#define CELL_RESC_1280x720 RESC_1280x720
#define CELL_RESC_1920x1080 RESC_1920x1080

#define CELL_RESC_FULLSCREEN RESC_FULLSCREEN
#define CELL_RESC_PANSCAN RESC_PANSCAN
#define CELL_RESC_LETTERBOX RESC_LETTERBOX
#define CELL_RESC_CONSTANT_VRAM RESC_CONSTANT_VRAM
#define CELL_RESC_MINIMUM_GPU_LOAD RESC_MINIMUM_GPU_LOAD

#define CELL_RESC_PAL_50 RESC_PAL_50
#define CELL_RESC_PAL_60_DROP RESC_PAL_60_DROP
#define CELL_RESC_PAL_60_INTERPOLATE RESC_PAL_60_INTERPOLATE
#define CELL_RESC_PAL_60_INTERPOLATE_30_DROP RESC_PAL_60_INTERPOLATE_30_DROP
#define CELL_RESC_PAL_60_INTERPOLATE_DROP_FLEXIBLE RESC_PAL_60_INTERPOLATE_DROP_FLEXIBLE

#define CELL_RESC_INTERLACE_FILTER RESC_INTERLACE_FILTER
#define CELL_RESC_NORMAL_BILINEAR RESC_NORMAL_BILINEAR

#define CELL_RESC_ELEMENT_HALF RESC_ELEMENT_HALF

#define CELL_VIDEO_OUT_ASPECT_AUTO VIDEO_ASPECT_AUTO
#define CELL_VIDEO_OUT_ASPECT_4_3 VIDEO_ASPECT_4_3
#define CELL_VIDEO_OUT_ASPECT_16_9 VIDEO_ASPECT_16_9

#define CELL_VIDEO_OUT_RESOLUTION_480 VIDEO_RESOLUTION_480
#define CELL_VIDEO_OUT_RESOLUTION_576 VIDEO_RESOLUTION_576
#define CELL_VIDEO_OUT_RESOLUTION_720 VIDEO_RESOLUTION_720
#define CELL_VIDEO_OUT_RESOLUTION_1080 VIDEO_RESOLUTION_1080
#define CELL_VIDEO_OUT_RESOLUTION_960x1080 VIDEO_RESOLUTION_960x1080
#define CELL_VIDEO_OUT_RESOLUTION_1280x1080 VIDEO_RESOLUTION_1280x1080
#define CELL_VIDEO_OUT_RESOLUTION_1440x1080 VIDEO_RESOLUTION_1440x1080
#define CELL_VIDEO_OUT_RESOLUTION_1600x1080 VIDEO_RESOLUTION_1600x1080

#define CELL_VIDEO_OUT_SCAN_MODE_PROGRESSIVE VIDEO_SCANMODE_PROGRESSIVE

#define CELL_VIDEO_OUT_PRIMARY VIDEO_PRIMARY

#define CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8 VIDEO_BUFFER_FORMAT_XRGB
#define CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_R16G16B16X16_FLOAT VIDEO_BUFFER_FORMAT_FLOAT

#define CellGcmSurface gcmSurface
#define CellGcmTexture gcmTexture
#define CellGcmContextData _gcmCtxData
#define CellGcmConfig gcmConfiguration
#define CellVideoOutConfiguration videoConfiguration
#define CellVideoOutResolution videoResolution
#define CellVideoOutState videoState

#define CellRescInitConfig rescInitConfig
#define CellRescSrc rescSrc
#define CellRescBufferMode rescBufferMode

#define resolutionId resolution
#define memoryFrequency memoryFreq
#define coreFrequency coreFreq

#define cellGcmFinish rsxFinish

#define cellGcmGetFlipStatus gcmGetFlipStatus
#define cellGcmResetFlipStatus gcmResetFlipStatus
#define cellGcmSetWaitFlip gcmSetWaitFlip
#define cellGcmSetDebugOutputLevel gcmSetDebugOutputLevel
#define cellGcmSetDisplayBuffer gcmSetDisplayBuffer
#define cellGcmSetGraphicsHandler gcmSetGraphicsHandler
#define cellGcmSetFlipHandler gcmSetFlipHandler
#define cellGcmSetVBlankHandler gcmSetVBlankHandler
#define cellGcmGetConfiguration gcmGetConfiguration
#define cellGcmSetJumpCommand rsxSetJumpCommand
#define cellGcmFlush rsxFlushBuffer
#define cellGcmSetFlipMode gcmSetFlipMode
#define cellGcmSetFlip gcmSetFlip
#define cellGcmGetLabelAddress gcmGetLabelAddress
#define cellGcmUnbindTile gcmUnbindTile
#define cellGcmBindTile gcmBindTile
#define cellGcmSetTileInfo gcmSetTileInfo
#define cellGcmAddressToOffset gcmAddressToOffset

#define cellRescCreateInterlaceTable rescCreateInterlaceTable
#define cellRescSetDisplayMode rescSetDisplayMode
#define cellRescGetNumColorBuffers rescGetNumColorBuffers
#define cellRescGetBufferSize rescGetBufferSize
#define cellRescSetBufferAddress rescSetBufferAddress
#define cellRescGetFlipStatus rescGetFlipStatus
#define cellRescResetFlipStatus rescResetFlipStatus
#define cellRescSetConvertAndFlip rescSetConvertAndFlip
#define cellRescSetVBlankHandler rescSetVBlankHandler
#define cellRescSetFlipHandler rescSetFlipHandler
#define cellRescAdjustAspectRatio rescAdjustAspectRatio
#define cellRescSetWaitFlip rescSetWaitFlip
#define cellRescSetSrc rescSetSrc
#define cellRescInit rescInit
#define cellRescExit rescExit

#define cellVideoOutConfigure videoConfigure
#define cellVideoOutGetState videoGetState
#define cellVideoOutGetResolution videoGetResolution
#define cellVideoOutGetResolutionAvailability videoGetResolutionAvailability

#define cellGcmSetViewportInline rsxSetViewport
#define cellGcmSetReferenceCommandInline rsxSetReferenceCommand
#define cellGcmSetBlendEquationInline rsxSetBlendEquation
#define cellGcmSetWriteBackEndLabelInline rsxSetWriteBackendLabel
#define cellGcmSetWaitLabelInline rsxSetWaitLabel
#define cellGcmSetDepthTestEnableInline rsxSetDepthTestEnable
#define cellGcmSetScissorInline rsxSetScissor
#define cellGcmSetBlendEnableInline rsxSetBlendEnable
#define cellGcmSetClearColorInline rsxSetClearColor
#define cellGcmSetBlendFuncInline rsxSetBlendFunc
#define cellGcmSetBlendColorInline rsxSetBlendColor
#define cellGcmSetTextureFilterInline rsxTextureFilter
#define cellGcmSetTextureControlInline rsxTextureControl
#define cellGcmSetCullFaceEnableInline rsxSetCullFaceEnable
#define cellGcmSetShadeModeInline rsxSetShadeModel
#define cellGcmSetTransferImage rsxSetTransferImage
#define cellGcmSetBlendColor rsxSetBlendColor
#define cellGcmSetBlendEquation rsxSetBlendEquation
#define cellGcmSetBlendFunc rsxSetBlendFunc
#define cellGcmSetClearColor rsxSetClearColor
#define cellGcmSetScissor rsxSetScissor
#define celGcmSetInvalidateVertexCache(fifo) rsxInvalidateTextureCache(fifo, GCM_INVALIDATE_VERTEX_TEXTURE)
#else
#define cellGcmSetTransferImage cellGcmSetTransferImageInline
#define celGcmSetInvalidateVertexCache cellGcmSetInvalidateVertexCacheInline
#endif

/*============================================================
	NETWORK PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <net/netctl.h>

#define cellNetCtlInit netCtlInit
#define cellNetCtlGetState netCtlGetState
#define cellNetCtlTerm netCtlTerm

#define CELL_NET_CTL_STATE_IPObtained NET_CTL_STATE_IPObtained
#else
#define netCtlInit cellNetCtlInit
#define netCtlGetState cellNetCtlGetState
#define netCtlTerm cellNetCtlTerm
#define NET_CTL_STATE_IPObtained CELL_NET_CTL_STATE_IPObtained
#endif

/*============================================================
	NET PROTOTYPES
============================================================ */

#if defined(HAVE_NETWORKING)
#ifdef __PSL1GHT__
#include <net/net.h>

#define socketselect select
#define socketclose close

#define sys_net_initialize_network netInitialize
#define sys_net_finalize_network netFinalizeNetwork
#else
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>

#define netInitialize sys_net_initialize_network
#define netFinalizeNetwork sys_net_finalize_network
#endif
#endif

/*============================================================
	SYSUTIL PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <sysutil/game.h>
#define CellGameContentSize sysGameContentSize
#define cellGameContentPermit sysGameContentPermit
#define cellGameBootCheck sysGameBootCheck

#define CELL_GAME_ATTRIBUTE_APP_HOME   (UINT64_C(1) <<1) /* boot from / app_home/PS3_GAME */
#define CELL_GAME_DIRNAME_SIZE			32

#define CELL_GAME_GAMETYPE_SYS		0
#define CELL_GAME_GAMETYPE_DISC		1
#define CELL_GAME_GAMETYPE_HDD		2
#define CELL_GAME_GAMETYPE_GAMEDATA	3
#define CELL_GAME_GAMETYPE_HOME		4

#endif

#if defined(HAVE_SYSUTILS)
#ifdef __PSL1GHT__
#include <sysutil/sysutil.h>

#define CELL_SYSUTIL_REQUEST_EXITGAME SYSUTIL_EXIT_GAME

#define cellSysutilRegisterCallback sysUtilRegisterCallback
#define cellSysutilCheckCallback sysUtilCheckCallback
#else
#include <sysutil/sysutil_screenshot.h>
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_gamecontent.h>
#endif
#endif

#if(CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

/*============================================================
	SYSMODULE PROTOTYPES
============================================================ */

#if defined(HAVE_SYSMODULES)
#ifdef __PSL1GHT__
#include <sysmodule/sysmodule.h>

#define CELL_SYSMODULE_IO SYSMODULE_IO
#define CELL_SYSMODULE_FS SYSMODULE_FS
#define CELL_SYSMODULE_NET SYSMODULE_NET
#define CELL_SYSMODULE_SYSUTIL_NP SYSMODULE_SYSUTIL_NP
#define CELL_SYSMODULE_JPGDEC SYSMODULE_JPGDEC
#define CELL_SYSMODULE_PNGDEC SYSMODULE_PNGDEC
#define CELL_SYSMODULE_FONT SYSMODULE_FONT
#define CELL_SYSMODULE_FREETYPE SYSMODULE_FREETYPE
#define CELL_SYSMODULE_FONTFT SYSMODULE_FONTFT

#define cellSysmoduleLoadModule sysModuleLoad
#define cellSysmoduleUnloadModule sysModuleUnload

#else
#include <cell/sysmodule.h>

#define sysModuleLoad cellSysmoduleLoadModule
#define sysModuleUnload cellSysmoduleUnloadModule
#define SYSMODULE_NET CELL_SYSMODULE_NET
#endif
#endif

/*============================================================
	FS PROTOTYPES
============================================================ */
#define FS_SUCCEEDED 0
#define FS_TYPE_DIR 1
#ifdef __PSL1GHT__
#include <lv2/sysfs.h>
#ifndef O_RDONLY
#define O_RDONLY SYS_O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY SYS_O_WRONLY
#endif
#ifndef O_CREAT
#define O_CREAT SYS_O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC SYS_O_TRUNC
#endif
#ifndef O_RDWR
#define O_RDWR SYS_O_RDWR
#endif
#else
#include <cell/cell_fs.h>
#ifndef O_RDONLY
#define O_RDONLY CELL_FS_O_RDONLY
#endif
#ifndef O_WRONLY
#define O_WRONLY CELL_FS_O_WRONLY
#endif
#ifndef O_CREAT
#define O_CREAT CELL_FS_O_CREAT
#endif
#ifndef O_TRUNC
#define O_TRUNC CELL_FS_O_TRUNC
#endif
#ifndef O_RDWR
#define O_RDWR CELL_FS_O_RDWR
#endif
#define sysFsStat cellFsStat
#define sysFSStat CellFsStat
#define sysFSDirent CellFsDirent
#define sysFsOpendir cellFsOpendir
#define sysFsReaddir cellFsReaddir
#define sysFSDirent CellFsDirent
#define sysFsClosedir cellFsClosedir
#endif

#endif
