/* Compile-only Vita stub for the matrix's gxm video lane, carrying
 * the common-dialog update the driver drives each frame. Each
 * declaration is the shape the real header gives it. */
#ifndef STUB_PSP2_COMMON_DIALOG
#define STUB_PSP2_COMMON_DIALOG
#include <psp2/types.h>
#include <psp2/gxm.h>

typedef struct SceCommonDialogRenderTargetInfo {
	ScePVoid depthSurfaceData;
	ScePVoid colorSurfaceData;
	SceGxmColorSurfaceType surfaceType;
	SceGxmColorFormat colorFormat;
	SceUInt32 width;
	SceUInt32 height;
	SceUInt32 strideInPixels;
	SceUInt8 reserved[32];
} SceCommonDialogRenderTargetInfo;

typedef struct SceCommonDialogUpdateParam {
	SceCommonDialogRenderTargetInfo renderTarget;
	SceGxmSyncObject *displaySyncObject;
	SceUInt8 reserved[32];
} SceCommonDialogUpdateParam;

int sceCommonDialogUpdate(const SceCommonDialogUpdateParam *updateParam);

#endif
