/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PS3_SDK_DEFINES_H
#define _PS3_SDK_DEFINES_H

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <sdk_version.h>
#endif

/*============================================================
	AUDIO PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <audio/audio.h>
#include <sys/thread.h>

#include <sys/event_queue.h>
#include <lv2/mutex.h>
#include <lv2/cond.h>

/* define all the audio/audio port functions */
#define cellAudioQuit audioQuit
#define cellAudioInit audioInit
#define cellAudioPortStart audioPortStart
#define cellAudioPortOpen audioPortOpen
#define cellAudioPortClose audioPortClose
#define cellAudioPortStop audioPortStop
#define CellAudioPortParam audioPortParam
#define cellAudioPortOpen audioPortOpen
#define cellAudioAddData audioAddData

/* define all the event queue functions */
#define sys_event_queue_receive sysEventQueueReceive
#define cellAudioSetNotifyEventQueue audioSetNotifyEventQueue
#define cellAudioRemoveNotifyEventQueue audioRemoveNotifyEventQueue
#define cellAudioCreateNotifyEventQueue audioCreateNotifyEventQueue

/* define all the lightweight mutex functions */
#define sys_lwmutex_destroy sysLwMutexDestroy
#define sys_lwmutex_lock sysLwMutexLock
#define sys_lwmutex_unlock sysLwMutexUnlock
#define sys_lwmutex_create sysLwMutexCreate

//forward decl. for audioAddData
extern int audioAddData(uint32_t portNum, float *data, uint32_t frames, float volume);

/* define all the lightweight condition functions */
#define sys_lwcond_create sysLwCondCreate
#define sys_lwcond_destroy sysLwCondDestroy
#define sys_lwcond_wait sysLwCondWait
#define sys_lwcond_signal sysLwCondSignal

#define CELL_AUDIO_BLOCK_SAMPLES AUDIO_BLOCK_SAMPLES
#define SYS_NO_TIMEOUT 0
#define param_attrib attrib
#define sys_lwmutex_attribute_t sys_lwmutex_attr_t
#define sys_lwcond_attribute_t sys_lwcond_attr_t
#define sys_semaphore_t sys_sem_t

#else
#include <cell/audio.h>
#include <sys/event.h>
#include <sys/synchronization.h>

#define numChannels nChannel
#define numBlocks nBlock
#define param_attrib attr

#endif

/*============================================================
	INPUT PAD PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#include <io/pad.h>
/* define all the ps3 pad structs */
#define CellPadInfo2 padInfo2
#define CellPadData padData

/* define all the ps3 pad functions */
#define cellPadGetInfo2 ioPadGetInfo2
#define cellPadGetData ioPadGetData
#define cellPadInit ioPadInit
#define cellPadEnd ioPadEnd

#define now_connect connected
#else
#include <cell/pad.h>
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
#define CELL_MOUSE_BUTTON_1 (1ULL << 0) /* Button 1 */
#define CELL_MOUSE_BUTTON_2 (1ULL << 1) /* Button 2 */
#define CELL_MOUSE_BUTTON_3 (1ULL << 2) /* Button 3 */
#define CELL_MOUSE_BUTTON_4 (1ULL << 3) /* Button 4 */
#define CELL_MOUSE_BUTTON_5 (1ULL << 4) /* Button 5 */
#define CELL_MOUSE_BUTTON_6 (1ULL << 5) /* Button 6 */
#define CELL_MOUSE_BUTTON_7 (1ULL << 6) /* Button 7 */
#define CELL_MOUSE_BUTTON_8 (1ULL << 7) /* Button 8 */

#else
#include <cell/mouse.h>
#endif

#endif

/*============================================================
	OSK PROTOTYPES
============================================================ */

#ifdef HAVE_OSK

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

#endif

/*============================================================
	JPEG/PNG DECODING/ENCODING PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
/* define all of the JPEG/PNG structs */
#define CellJpgDecMainHandle int
#define CellPngDecMainHandle int
#define CellJpgDecSubHandle int
#define CellPngDecSubHandle int
#define CellJpgDecThreadInParam jpgDecThreadInParam
#define CellPngDecThreadInParam pngDecThreadInParam
#define CellJpgDecThreadOutParam jpgDecThreadOutParam
#define CellPngDecThreadOutParam pngDecThreadOutParam
#define CellJpgDecSrc jpgDecSource
#define CellPngDecSrc pngDecSource
#define CellJpgDecOpnInfo uint32_t
#define CellPngDecOpnInfo uint32_t
#define CellJpgDecInfo jpgDecInfo
#define CellPngDecInfo pngDecInfo
#define CellJpgDecInParam jpgDecInParam
#define CellPngDecInParam pngDecInParam
#define CellJpgDecOutParam jpgDecOutParam
#define CellPngDecOutParam pngDecOutParam
#define CellJpgDecDataOutInfo jpgDecDataInfo
#define CellPngDecDataOutInfo pngDecDataInfo
#define CellJpgDecDataCtrlParam uint64_t
#define CellPngDecDataCtrlParam uint64_t

#define spu_enable enable
#define stream_select stream
#define color_alpha alpha
#define color_space space
#define output_mode mode
#define output_bytes_per_line bytes_per_line
#define output_width width
#define output_height height

/* define all of the JPEG/PNG functions */
#define cellJpgDecCreate jpgDecCreate
#define cellJpgDecOpen jpgDecOpen
#define cellJpgDecReadHeader jpgDecReadHeader
#define cellJpgDecSetParameter jpgDecSetParameter
#define cellJpgDecDecodeData jpgDecDecodeData
#define cellJpgDecClose jpgDecClose
#define cellJpgDecDestroy jpgDecDestroy

#define cellPngDecCreate pngDecCreate
#define cellPngDecOpen pngDecOpen
#define cellPngDecReadHeader pngDecReadHeader
#define cellPngDecSetParameter pngDecSetParameter
#define cellPngDecDecodeData pngDecDecodeData
#define cellPngDecClose pngDecClose
#define cellPngDecDestroy pngDecDestroy

/* define all of the JPEG/PNG defines */
#define CELL_PNGDEC_SPU_THREAD_ENABLE 1
#define CELL_JPGDEC_SPU_THREAD_ENABLE 1
#define CELL_JPGDEC_FILE JPGDEC_FILE
#define CELL_PNGDEC_FILE PNGDEC_FILE
#define CELL_JPGDEC_SPU_THREAD_ENABLE 1
#define CELL_JPGDEC_FAST JPGDEC_LOW_QUALITY
#define CELL_JPGDEC_TOP_TO_BOTTOM JPGDEC_TOP_TO_BOTTOM
#define CELL_PNGDEC_TOP_TO_BOTTOM PNGDEC_TOP_TO_BOTTOM
#define CELL_JPG_ARGB JPGDEC_ARGB
#define CELL_PNGDEC_ARGB PNGDEC_ARGB
#define CELL_JPGDEC_DEC_STATUS_FINISH 0
#define CELL_PNGDEC_DEC_STATUS_FINISH 0
#define CELL_PNGDEC_1BYTE_PER_1PIXEL 1
#define CELL_PNGDEC_STREAM_ALPHA 1
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
/* define all the thread functions  */
#define sys_ppu_thread_create sysThreadCreate
#define sys_ppu_thread_join sysThreadJoin
#define sys_ppu_thread_exit sysThreadExit

#define SYS_PPU_THREAD_CREATE_JOINABLE 0 /* FIXME - not sure if this is correct */
#endif

/*============================================================
	MEMORY PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#define sys_memory_container_create sysMemContainerCreate
#define sys_memory_container_destroy sysMemContainerDestroy
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
	GCM PROTOTYPES
============================================================ */

#ifdef __PSL1GHT__
#define CELL_GCM_METHOD_HEADER_TEXTURE_OFFSET(unit, val) (((val) << (18)) | ((0x00001a00) + (unit) * 0x20))
#define CELL_GCM_METHOD_DATA_TEXTURE_OFFSET(val) (val)
#define CELL_GCM_METHOD_DATA_TEXTURE_CONTROL3(pitch, depth) ((pitch) | ((depth) << 20))
#define CELL_GCM_METHOD_DATA_TEXTURE_IMAGE_RECT(height, width) ((height) | ((width) << 16))
#define CELL_GCM_METHOD_DATA_TEXTURE_FILTER(bias, min, mag, filter) (((bias)) | ((filter) << 13) | ((min) << 16) | ((mag) << 24))
#define CELL_GCM_METHOD_DATA_TEXTURE_CONTROL0(val0, minlod, maxlod, filter) (((minlod << 2) | (filter) << 4) | (maxlod << 7) | (minlod << 19) | (val0 << 31))
#define CELL_GCM_METHOD_DATA_TEXTURE_ADDRESS(wraps, wrapt, wrapr,  unsignedremap, zfunc, gamma) ((wraps) | ((0) << 4) | ((wrapt) << 8) | (unsignedremap << 12) | ((wrapr) << 16) | (gamma << 20) | (zfunc << 28))
#endif

#define CELL_GCM_REMAP_MODE(order, inputA, inputR, inputG, inputB, outputA, outputR, outputG, outputB) \
	(((order)<<16)|((inputA))|((inputR)<<2)|((inputG)<<4)|((inputB)<<6)|((outputA)<<8)|((outputR)<<10)|((outputG)<<12)|((outputB)<<14))

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
#endif

/*============================================================
	NET PROTOTYPES
============================================================ */

#if defined(HAVE_NETPLAY)
#ifdef __PSL1GHT__
#include <net/net.h>

#define socketselect select
#define socketclose close

#define sys_net_initialize_network netInitialize
#else
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
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

#define CELL_GAME_ATTRIBUTE_APP_HOME   (1ULL <<1) /* boot from / app_home/PS3_GAME */
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

#define cellSysmoduleLoadModule sysModuleLoad
#define cellSysmoduleUnloadModule sysModuleUnload

#else
#include <cell/sysmodule.h>
#endif
#endif
