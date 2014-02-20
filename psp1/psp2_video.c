/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <stdlib.h>
#include <assert.h>
#include <sceconst.h>
#include <display.h>
#include <ctrl.h>
#include <gxm.h>
#include <gxt.h>
#include <math.h>
#include <kernel.h>
#include <libdbgfont.h>
#include <stdio.h>
#include <vectormath.h>

#include <gxm.h>
#include <kernel.h>

// Display dimensions
#define DISPLAY_WIDTH				   960
#define DISPLAY_HEIGHT				   544
#define DISPLAY_STRIDE_IN_PIXELS    1024

#define DISPLAY_PIXEL_FORMAT		   SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
#define DISPLAY_BUFFER_COUNT		   3
#define DISPLAY_MAX_PENDING_SWAPS	2

// Supported flip modes
enum FlipMode {
	FLIP_MODE_HSYNC,	///< Flip on next HSYNC
	FLIP_MODE_VSYNC,	///< Flip on next VSYNC
	FLIP_MODE_VSYNC_2	///< Flip on next VSYNC, display for 2 VSYNCs minimum
};

#define ALIGN(x, a)					(((x) + ((a) - 1)) & ~((a) - 1))
#define MAX(a, b)					   (((a) > (b)) ? (a) : (b))

// Data structure to pass through the display queue
typedef struct DisplayData
{
	void *address;				///< Framebuffer address
	uint32_t width;				///< Framebuffer width
	uint32_t height;			///< Framebuffer height
	uint32_t strideInPixels;	///< Framebuffer stride in pixels
	uint32_t flipMode;			///< From #FlipMode
} DisplayData;

typedef struct psp2_video
{
   SceGxmRenderTarget *g_mainRenderTarget;
   SceGxmContext *g_context;
   const SceGxmProgramParameter *g_clearColorParam;
   SceGxmShaderPatcher *g_shaderPatcher;
   SceGxmColorSurface g_displaySurface[DISPLAY_BUFFER_COUNT];
   SceGxmSyncObject *g_displayBufferSync[DISPLAY_BUFFER_COUNT];
   SceUID g_displayBufferUid[DISPLAY_BUFFER_COUNT];
   void *g_displayBufferData[DISPLAY_BUFFER_COUNT];
   void	*g_initializeHostMem;
   bool smooth;
   int rotation; 
   bool vsync;
   bool rgb32;
   unsigned width, height;
} psp2_video_t;

uint32_t	g_displayFrontBufferIndex = DISPLAY_BUFFER_COUNT - 1;

// initialization parameters
SceUID							g_initializeDriverUid = 0;
SceUID							g_initializeParameterBufferUid = 0;

// libgxm context
void							*g_contextHostMem = NULL;
SceUID							g_vdmRingBufferUid = 0;
SceUID							g_vertexRingBufferUid = 0;
SceUID							g_fragmentRingBufferUid = 0;
SceUID							g_fragmentUsseRingBufferUid = 0;

// libgxm shader patcher
SceUID							g_patcherBufferUid = 0;
SceUID							g_patcherCombinedUsseUid = 0;

// libgxm display queue
uint32_t						g_displayBackBufferIndex = 0;

// Depth buffer for display surface
SceUID							g_mainDepthBufferUid = 0;
SceGxmDepthStencilSurface		g_mainDepthSurface;

static void *patcherHostAlloc(void *userData, uint32_t size)
{
   (void)userData;
	return malloc(size);
}

// Callback function to allocate memory for the shader patcher
static void patcherHostFree(void *userData, void *mem)
{
   (void)userData;
	free(mem);
}

static void displayCallback(const void *callbackData)
{
	int err = SCE_OK;
   (void)err;

	// Cast the parameters back
	const DisplayData *displayData = (const DisplayData *)callbackData;

	// Check this buffer has been displayed for the necessary number of VSYNCs
	// (Avoids queuing a flip before the second VSYNC has happened)
	if (displayData->flipMode == FLIP_MODE_VSYNC_2)
		err = sceDisplayWaitSetFrameBufMulti(2);

	// Swap to the new buffer
	SceDisplayFrameBuf framebuf;
	memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size        = sizeof(SceDisplayFrameBuf);
	framebuf.base        = displayData->address;
	framebuf.pitch       = displayData->strideInPixels;
	framebuf.pixelformat = DISPLAY_PIXEL_FORMAT;
	framebuf.width       = displayData->width;
	framebuf.height      = displayData->height;
	err = sceDisplaySetFrameBuf(&framebuf,
		(displayData->flipMode == FLIP_MODE_HSYNC)
			? SCE_DISPLAY_UPDATETIMING_NEXTHSYNC
			: SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
	assert(err == SCE_OK);

	// Block this callback until the swap has occurred and the old buffer
	// is no longer displayed
	if (displayData->flipMode != FLIP_MODE_HSYNC)
   {
		err = sceDisplayWaitSetFrameBuf();
		assert(err == SCE_OK);
	}
}

static void createGxmShaderPatcher(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
   int err = SCE_OK;
   (void)err;

   // set buffer sizes for this sample
   const uint32_t patcherBufferSize		= 64*1024;
   const uint32_t patcherCombinedUsseSize 	= 64*1024;

   // allocate memory for buffers and USSE code
   void *patcherBuffer = gmmAlloc(
         SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
         patcherBufferSize,
         4, 
         SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
         &g_patcherBufferUid);
   uint32_t patcherVertexUsseOffset;
   uint32_t patcherFragmentUsseOffset;
   void *patcherCombinedUsse = combinedUsseAlloc(
         patcherCombinedUsseSize,
         &g_patcherCombinedUsseUid,
         &patcherVertexUsseOffset,
         &patcherFragmentUsseOffset);

   // create a shader patcher
   SceGxmShaderPatcherParams patcherParams;
   memset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
   patcherParams.userData					      = NULL;
   patcherParams.hostAllocCallback			   = &patcherHostAlloc;
   patcherParams.hostFreeCallback			   = &patcherHostFree;
   patcherParams.bufferAllocCallback		   = NULL;
   patcherParams.bufferFreeCallback		      = NULL;
   patcherParams.bufferMem					      = patcherBuffer;
   patcherParams.bufferMemSize				   = patcherBufferSize;
   patcherParams.vertexUsseAllocCallback	   = NULL;
   patcherParams.vertexUsseFreeCallback	   = NULL;
   patcherParams.vertexUsseMem				   = patcherCombinedUsse;
   patcherParams.vertexUsseMemSize			   = patcherCombinedUsseSize;
   patcherParams.vertexUsseOffset			   = patcherVertexUsseOffset;
   patcherParams.fragmentUsseAllocCallback	= NULL;
   patcherParams.fragmentUsseFreeCallback 	= NULL;
   patcherParams.fragmentUsseMem			      = patcherCombinedUsse;
   patcherParams.fragmentUsseMemSize		   = patcherCombinedUsseSize;
   patcherParams.fragmentUsseOffset		      = patcherFragmentUsseOffset;

   err = sceGxmShaderPatcherCreate(&patcherParams, &psp->g_shaderPatcher);
   assert(err == SCE_OK);
}

// Queue a display swap and cycle our buffers
static int cycleDisplayBuffers(void *data, FlipMode flipMode, uint32_t width, uint32_t height, uint32_t strideInPixels)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// queue the display swap for this frame
	DisplayData displayData;
	displayData.address = psp->g_displayBufferData[g_displayBackBufferIndex];
	displayData.width = width;
	displayData.height = height;
	displayData.strideInPixels = strideInPixels;
	displayData.flipMode = flipMode;
	err = sceGxmDisplayQueueAddEntry(
		psp->g_displayBufferSync[psp->g_displayFrontBufferIndex],		// front buffer is OLD buffer
		psp->g_displayBufferSync[g_displayBackBufferIndex],		// back buffer is NEW buffer
		&displayData);
	assert(err == SCE_OK);

	// update buffer indices
	psp->g_displayFrontBufferIndex = g_displayBackBufferIndex;
	g_displayBackBufferIndex = (g_displayBackBufferIndex + 1) % DISPLAY_BUFFER_COUNT;

	// done
	return err;
}

static SceGxmRenderTarget *createRenderTarget(uint32_t width, uint32_t height, SceGxmMultisampleMode msaaMode)
{
	int err = SCE_OK;
   (void)err;

	// set up parameters
	SceGxmRenderTargetParams params;
	memset(&params, 0, sizeof(SceGxmRenderTargetParams));
	params.flags				= 0;
	params.width				= width;
	params.height				= height;
	params.scenesPerFrame		= 1;
	params.multisampleMode		= msaaMode;
	params.multisampleLocations	= 0;
	params.hostMem				= NULL;
	params.hostMemSize			= 0;
	params.driverMemBlock		= -1;

	// compute sizes
	uint32_t hostMemSize, driverMemSize;
	err = sceGxmGetRenderTargetMemSizes(&params, &hostMemSize, &driverMemSize);
	assert(err == SCE_OK);

	// allocate host memory
	params.hostMem = malloc(hostMemSize);
	params.hostMemSize = hostMemSize;

	// allocate driver memory
	params.driverMemBlock = sceKernelAllocMemBlock(
		"SampleRT", 
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, 
		driverMemSize, 
		NULL);
	assert(params.driverMemBlock >= SCE_OK);

	// create the render target
	SceGxmRenderTarget *renderTarget;
	err = sceGxmCreateRenderTarget(&params, &renderTarget);
	assert(err == SCE_OK);
	return renderTarget;	
}

static void destroyRenderTarget(void *data, SceGxmRenderTarget *renderTarget)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// grab the host memory and driver memblock
	void *hostMem;
	err = sceGxmRenderTargetGetHostMem(renderTarget, &hostMem);
	assert(err == SCE_OK);
	SceUID driverMemBlock;
	err = sceGxmRenderTargetGetDriverMemBlock(renderTarget, &driverMemBlock);
	assert(err == SCE_OK);

	// destroy the render target
	err = sceGxmDestroyRenderTarget(renderTarget);
	assert(err == SCE_OK);

	// free memory
	sceKernelFreeMemBlock(driverMemBlock);
	free(hostMem);
}

static void *gmmAlloc(SceKernelMemBlockType type, uint32_t size, uint32_t alignment, uint32_t attribs, SceUID *uid)
{
	int err = SCE_OK;
   (void)err;

	// page align the size
	if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA)
   {
		// CDRAM memblocks must be 256KiB aligned
		assert(alignment <= 256*1024);
		size = ALIGN(size, 256*1024);
	}
   else
   {
      // LPDDR memblocks must be 4KiB aligned
      assert(alignment <= 4*1024);
      size = ALIGN(size, 4*1024);
   }
   (void)alignment;

	// allocate some memory
	*uid = sceKernelAllocMemBlock("common", type, size, NULL);
	assert(*uid >= SCE_OK);

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(*uid, &mem);
	assert(err == SCE_OK);

	// map for the GPU
	err = sceGxmMapMemory(mem, size, attribs);
	assert(err == SCE_OK);

	// done
	return mem;
}

static void gmmFree(SceUID uid)
{
	int err = SCE_OK;
   (void)err;

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(uid, &mem);
	assert(err == SCE_OK);

	// unmap memory
	err = sceGxmUnmapMemory(mem);
	assert(err == SCE_OK);

	// free the memory block
	err = sceKernelFreeMemBlock(uid);
	assert(err == SCE_OK);
}

static void *vertexUsseAlloc(uint32_t size, SceUID *uid, uint32_t *usseOffset)
{
	int err = SCE_OK;
   (void)err;

	// align to memblock alignment for LPDDR
	size = ALIGN(size, 4096);
	
	// allocate some memory
	*uid = sceKernelAllocMemBlock("basic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, size, NULL);
	assert(*uid >= SCE_OK);

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(*uid, &mem);
	assert(err == SCE_OK);

	// map as vertex USSE code for the GPU
	err = sceGxmMapVertexUsseMemory(mem, size, usseOffset);
	assert(err == SCE_OK);

	// done
	return mem;
}

static void vertexUsseFree(SceUID uid)
{
	int err = SCE_OK;
   (void)err;

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(uid, &mem);
	assert(err == SCE_OK);

	// unmap memory
	err = sceGxmUnmapVertexUsseMemory(mem);
	assert(err == SCE_OK);

	// free the memory block
	err = sceKernelFreeMemBlock(uid);
	assert(err == SCE_OK);
}

static void *fragmentUsseAlloc(uint32_t size, SceUID *uid, uint32_t *usseOffset)
{
	int err = SCE_OK;
   (void)err;

	// align to memblock alignment for LPDDR
	size = ALIGN(size, 4096);
	
	// allocate some memory
	*uid = sceKernelAllocMemBlock("basic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, size, NULL);
	assert(*uid >= SCE_OK);

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(*uid, &mem);
	assert(err == SCE_OK);

	// map as fragment USSE code for the GPU
	err = sceGxmMapFragmentUsseMemory(mem, size, usseOffset);
	assert(err == SCE_OK);

	// done
	return mem;
}

static void fragmentUsseFree(SceUID uid)
{
	int err = SCE_OK;
   (void)err;

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(uid, &mem);
	assert(err == SCE_OK);

	// unmap memory
	err = sceGxmUnmapFragmentUsseMemory(mem);
	assert(err == SCE_OK);

	// free the memory block
	err = sceKernelFreeMemBlock(uid);
	assert(err == SCE_OK);
}

static void *combinedUsseAlloc(uint32_t size, SceUID *uid, uint32_t *vertexUsseOffset, uint32_t *fragmentUsseOffset)
{
	int err = SCE_OK;
   (void)err;

	// align to memblock alignment for LPDDR
	size = ALIGN(size, 4096);
	
	// allocate some memory
	*uid = sceKernelAllocMemBlock("basic", SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE, size, NULL);
	assert(*uid >= SCE_OK);

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(*uid, &mem);
	assert(err == SCE_OK);

	// map as both vertex and fragment USSE for code the GPU
	err = sceGxmMapVertexUsseMemory(mem, size, vertexUsseOffset);
	assert(err == SCE_OK);
	err = sceGxmMapFragmentUsseMemory(mem, size, fragmentUsseOffset);
	assert(err == SCE_OK);

	// done
	return mem;
}

void combinedUsseFree(SceUID uid)
{
	int err = SCE_OK;
   (void)err;

	// grab the base address
	void *mem = NULL;
	err = sceKernelGetMemBlockBase(uid, &mem);
	assert(err == SCE_OK);

	// unmap memory
	err = sceGxmUnmapFragmentUsseMemory(mem);
	assert(err == SCE_OK);
	err = sceGxmUnmapVertexUsseMemory(mem);
	assert(err == SCE_OK);

	// free the memory block
	err = sceKernelFreeMemBlock(uid);
	assert(err == SCE_OK);
}

using namespace sce::Vectormath::Scalar::Aos;

// Embedded GXM shader programs
extern const SceGxmProgram _binary_clear_v_gxp_start;
extern const SceGxmProgram _binary_clear_f_gxp_start;;
extern const SceGxmProgram _binary_cube_v_gxp_start;
extern const SceGxmProgram _binary_cube_f_gxp_start;
extern const uint8_t _binary_test_gxt_start[];

// Data structure for clear geometry
typedef struct ClearVertex {
	float x;
	float y;
} ClearVertex;

// Data structure for basic geometry
typedef struct BasicVertex {
	float x;
	float y;
	float z;
	uint32_t color;
	uint16_t u;
	uint16_t v;
} BasicVertex;

// clear geometry data
SceGxmShaderPatcherId			g_clearVertexProgramId;
SceGxmShaderPatcherId			g_clearFragmentProgramId;
SceGxmVertexProgram				*g_clearVertexProgram = NULL;
SceGxmFragmentProgram			*g_clearFragmentProgram = NULL;
SceUID							   g_clearVerticesUid;
SceUID							   g_clearIndicesUid;
ClearVertex						   *g_clearVertices = NULL;
uint16_t						      *g_clearIndices = NULL;

// cube geometry data
SceGxmShaderPatcherId			g_cubeVertexProgramId;
SceGxmShaderPatcherId			g_cubeFragmentProgramId;
SceGxmVertexProgram				*g_cubeVertexProgram = NULL;
SceGxmFragmentProgram			*g_cubeFragmentProgram = NULL;
SceUID							   g_cubeVerticesUid;
SceUID							   g_cubeIndicesUid;
BasicVertex						   *g_cubeVertices = NULL;
uint16_t						      *g_cubeIndices = NULL;
const SceGxmProgramParameter	*g_cubeWvpParam = NULL;

// offscreen surface data and render target
SceUID							   g_offscreenColorBufferUid;
void							      *g_offscreenColorBufferData;
SceGxmColorSurface				g_offscreenColorSurface;
SceGxmTexture					   g_offscreenTexture;
SceUID							   g_offscreenDepthBufferUid;
void							      *g_offscreenDepthBufferData;
SceGxmDepthStencilSurface		g_offscreenDepthSurface;
SceGxmRenderTarget				*g_offscreenRenderTarget;

// test texture
SceUID							   g_testTextureDataUid;
uint8_t							   *g_testTextureData;
SceGxmTexture					   g_testTexture;

// update data
float							      g_rotationAngle = 0.0f;
Matrix4							   g_offscreenWvpMatrix;
Matrix4							   g_mainWvpMatrix;

// Create data for clear draw call
static void createClearData(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// register programs with the shader patcher
	err = sceGxmShaderPatcherRegisterProgram(psp->g_shaderPatcher, &_binary_clear_v_gxp_start, &g_clearVertexProgramId);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherRegisterProgram(psp->g_shaderPatcher, &_binary_clear_f_gxp_start, &g_clearFragmentProgramId);
	assert(err == SCE_OK);

	// find attributes by name to create vertex format bindings
	const SceGxmProgram *clearVertexProgram = sceGxmShaderPatcherGetProgramFromId(g_clearVertexProgramId);
	const SceGxmProgramParameter *paramPositionAttribute = sceGxmProgramFindParameterByName(clearVertexProgram, "aPosition");
	assert(paramPositionAttribute && (sceGxmProgramParameterGetCategory(paramPositionAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE));

	// find fragment uniforms by name and cache parameter info
	// note: name lookup is a slow load-time operation
	const SceGxmProgram *clearFragmentProgram = sceGxmShaderPatcherGetProgramFromId(g_clearFragmentProgramId);
	assert(clearFragmentProgram);
	psp->g_clearColorParam = sceGxmProgramFindParameterByName(clearFragmentProgram, "color");
	assert(psp->g_clearColorParam && (sceGxmProgramParameterGetCategory(psp->g_clearColorParam) == SCE_GXM_PARAMETER_CATEGORY_UNIFORM));

	// create clear vertex format
	SceGxmVertexAttribute clearVertexAttributes[1];
	SceGxmVertexStream clearVertexStreams[1];
	clearVertexAttributes[0].streamIndex = 0;
	clearVertexAttributes[0].offset = 0;
	clearVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttributes[0].componentCount = 2;
	clearVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramPositionAttribute);
	clearVertexStreams[0].stride = sizeof(ClearVertex);
	clearVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create clear programs
	err = sceGxmShaderPatcherCreateVertexProgram(
		psp->g_shaderPatcher,
		g_clearVertexProgramId,
		clearVertexAttributes,
		1,
		clearVertexStreams,
		1,
		&g_clearVertexProgram);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherCreateFragmentProgram(
		psp->g_shaderPatcher,
		g_clearFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE,
		NULL,
		sceGxmShaderPatcherGetProgramFromId(g_clearVertexProgramId),
		&g_clearFragmentProgram);
	assert(err == SCE_OK);

	// allocate vertices and indices
	g_clearVertices = (ClearVertex *)gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		3*sizeof(ClearVertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_clearVerticesUid);
	g_clearIndices = (uint16_t *)gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		3*sizeof(uint16_t),
		2,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_clearIndicesUid);

	// write vertex data
	g_clearVertices[0].x = -1.0f;
	g_clearVertices[0].y = -1.0f;
	g_clearVertices[1].x =  3.0f;
	g_clearVertices[1].y = -1.0f;
	g_clearVertices[2].x = -1.0f;
	g_clearVertices[2].y =  3.0f;

	// write index data
	g_clearIndices[0] = 0;
	g_clearIndices[1] = 1;
	g_clearIndices[2] = 2;
}

static void destroyClearData(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// release the shaderss
	err = sceGxmShaderPatcherReleaseFragmentProgram(psp->g_shaderPatcher, g_clearFragmentProgram);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherReleaseVertexProgram(psp->g_shaderPatcher, g_clearVertexProgram);
	assert(err == SCE_OK);

	// free the memory used for vertices and indices
	gmmFree(g_clearIndicesUid);
	gmmFree(g_clearVerticesUid);

	// unregister programs since we don't need them any more
	err = sceGxmShaderPatcherUnregisterProgram(psp->g_shaderPatcher, g_clearFragmentProgramId);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherUnregisterProgram(psp->g_shaderPatcher, g_clearVertexProgramId);
	assert(err == SCE_OK);
}

// Create data for cube draw call
static void createCubeData(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// register programs with the patcher
	err = sceGxmShaderPatcherRegisterProgram(psp->g_shaderPatcher, &_binary_cube_v_gxp_start, &g_cubeVertexProgramId);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherRegisterProgram(psp->g_shaderPatcher, &_binary_cube_f_gxp_start, &g_cubeFragmentProgramId);
	assert(err == SCE_OK);

	// find vertex uniforms by name and cache parameter info
	// note: name lookup is a slow load-time operation
	const SceGxmProgram *cubeVertexProgram = sceGxmShaderPatcherGetProgramFromId(g_cubeVertexProgramId);
	assert(cubeVertexProgram);
	g_cubeWvpParam  = sceGxmProgramFindParameterByName(cubeVertexProgram, "wvp");
	assert(g_cubeWvpParam && (sceGxmProgramParameterGetCategory(g_cubeWvpParam) == SCE_GXM_PARAMETER_CATEGORY_UNIFORM));

	// find attributes by name to create vertex format bindings
	const SceGxmProgramParameter *paramPositionAttribute = sceGxmProgramFindParameterByName(cubeVertexProgram, "aPosition");
	assert(paramPositionAttribute && (sceGxmProgramParameterGetCategory(paramPositionAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE));
	const SceGxmProgramParameter *paramColorAttribute = sceGxmProgramFindParameterByName(cubeVertexProgram, "aColor");
	assert(paramColorAttribute && (sceGxmProgramParameterGetCategory(paramColorAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE));
	const SceGxmProgramParameter *paramTexCoordAttribute = sceGxmProgramFindParameterByName(cubeVertexProgram, "aTexCoord");
	assert(paramTexCoordAttribute && (sceGxmProgramParameterGetCategory(paramTexCoordAttribute) == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE));

	// create shaded triangle vertex format
	SceGxmVertexAttribute basicVertexAttributes[3];
	SceGxmVertexStream basicVertexStreams[1];
	basicVertexAttributes[0].streamIndex = 0;
	basicVertexAttributes[0].offset = 0;
	basicVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	basicVertexAttributes[0].componentCount = 3;
	basicVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramPositionAttribute);
	basicVertexAttributes[1].streamIndex = 0;
	basicVertexAttributes[1].offset = 12;
	basicVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	basicVertexAttributes[1].componentCount = 4;
	basicVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorAttribute);
	basicVertexAttributes[2].streamIndex = 0;
	basicVertexAttributes[2].offset = 16;
	basicVertexAttributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F16;
	basicVertexAttributes[2].componentCount = 2;
	basicVertexAttributes[2].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexCoordAttribute);
	basicVertexStreams[0].stride = sizeof(BasicVertex);
	basicVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create cube vertex program
	err = sceGxmShaderPatcherCreateVertexProgram(
		psp->g_shaderPatcher,
		g_cubeVertexProgramId,
		basicVertexAttributes,
		3,
		basicVertexStreams,
		1,
		&g_cubeVertexProgram);
	assert(err == SCE_OK);

	// create cube fragment program
	err = sceGxmShaderPatcherCreateFragmentProgram(
		psp->g_shaderPatcher,
		g_cubeFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		SCE_GXM_MULTISAMPLE_NONE,
		NULL,
		sceGxmShaderPatcherGetProgramFromId(g_cubeVertexProgramId),
		&g_cubeFragmentProgram);
	assert(err == SCE_OK);

	// allocate memory for vertex and index data
	g_cubeVertices = (BasicVertex *)gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		24*sizeof(BasicVertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_cubeVerticesUid);
	g_cubeIndices = (uint16_t *)gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		36*sizeof(uint16_t),
		2,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_cubeIndicesUid);

	// write vertices
	BasicVertex *vertexData = g_cubeVertices;
	for (uint32_t face = 0; face < 6; ++face)
   {
		float sign = ((face & 0x1) ? 1.0f : -1.0f);
		uint32_t axis = face >> 1;

		float ox = (axis == 0) ? sign : 0.0f;
		float oy = (axis == 1) ? sign : 0.0f;
		float oz = (axis == 2) ? sign : 0.0f;

		float ux = (axis != 0) ? 1.0f : 0.0f;
		float uy = (axis == 0) ? 1.0f : 0.0f;
		float uz = 0.0f;

		float vx = 0.0f;
		float vy = (axis == 2) ? 1.0f : 0.0f;
		float vz = (axis != 2) ? 1.0f : 0.0f;

		uint16_t half0 = 0x0000;
		uint16_t half1 = 0x3c00;

		uint32_t color = 0;
		switch (axis)
      {
         case 0:
            color = 0xffffffff;
            break;
         case 1:
            color = 0xffdddddd;
            break;
         case 2:
            color = 0xffbbbbbb;
            break;
      }

		vertexData->x = ox - ux - vx;
		vertexData->y = oy - uy - vy;
		vertexData->z = oz - uz - vz;
		vertexData->color = color;
		vertexData->u = half0;
		vertexData->v = half1;
		++vertexData;

		vertexData->x = ox + ux - vx;
		vertexData->y = oy + uy - vy;
		vertexData->z = oz + uz - vz;
		vertexData->color = color;
		vertexData->u = half1;
		vertexData->v = half1;
		++vertexData;

		vertexData->x = ox + ux + vx;
		vertexData->y = oy + uy + vy;
		vertexData->z = oz + uz + vz;
		vertexData->color = color;
		vertexData->u = half1;
		vertexData->v = half0;
		++vertexData;

		vertexData->x = ox - ux + vx;
		vertexData->y = oy - uy + vy;
		vertexData->z = oz - uz + vz;
		vertexData->color = color;
		vertexData->u = half0;
		vertexData->v = half0;
		++vertexData;
	}

	// write indices
	uint16_t *indexData = g_cubeIndices;
	for (uint32_t face = 0; face < 6; ++face)
   {
		uint32_t offset = 4*face;

		*indexData++ = offset + 0;
		*indexData++ = offset + 1;
		*indexData++ = offset + 2;

		*indexData++ = offset + 2;
		*indexData++ = offset + 3;
		*indexData++ = offset + 0;
	}
}

static void destroyCubeData(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// release the shaderss
	err = sceGxmShaderPatcherReleaseFragmentProgram(psp->g_shaderPatcher, g_cubeFragmentProgram);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherReleaseVertexProgram(psp->g_shaderPatcher, g_cubeVertexProgram);
	assert(err == SCE_OK);

	// free the memory used for vertices and indices
	gmmFree(g_cubeIndicesUid);
	gmmFree(g_cubeVerticesUid);

	// unregister programs since we don't need them any more
	err = sceGxmShaderPatcherUnregisterProgram(psp->g_shaderPatcher, g_cubeFragmentProgramId);
	assert(err == SCE_OK);
	err = sceGxmShaderPatcherUnregisterProgram(psp->g_shaderPatcher, g_cubeVertexProgramId);
	assert(err == SCE_OK);
}

// Create test texture data
static void createTestTextureData(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;

	// validate gxt
	const void *gxt = _binary_test_gxt_start;
	assert(sceGxtCheckData(gxt) == SCE_OK);

	// get the size of the texture data
	const uint32_t dataSize = sceGxtGetDataSize(gxt);

	// allocate memory
	g_testTextureData = (uint8_t *)gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		dataSize,
		SCE_GXM_TEXTURE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_testTextureDataUid);

	// copy texture data
	const void *dataSrc = sceGxtGetDataAddress(gxt);
	memcpy(g_testTextureData, dataSrc, dataSize);

	// set up the texture control words
	err = sceGxtInitTexture(&g_testTexture, gxt, g_testTextureData, 0);
	assert(err == SCE_OK);

	// set linear filtering
	err = sceGxmTextureSetMagFilter(
		&g_testTexture,
		SCE_GXM_TEXTURE_FILTER_LINEAR);
	assert(err == SCE_OK);
	err = sceGxmTextureSetMinFilter(
		&g_testTexture,
		SCE_GXM_TEXTURE_FILTER_LINEAR);
	assert(err == SCE_OK);
}

static void destroyTestTextureData(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	gmmFree(g_testTextureDataUid);
}

static void createOffscreenBuffer(void *data, const video_info_t *video)
{
   psp2_video_t *psp = (psp2_video_t*)data;
	int err = SCE_OK;
   (void)err;
   /* TODO - ensure width/height is POT - if libgxm cares about that */

	// allocate memory
	g_offscreenColorBufferData = gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA,
		video->width * video->height * (video->rgb32 ? 4 : 2),
		MAX(SCE_GXM_TEXTURE_ALIGNMENT, SCE_GXM_COLOR_SURFACE_ALIGNMENT),
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&g_offscreenColorBufferUid);

	// set up the surface
	err = sceGxmColorSurfaceInit(
		&g_offscreenColorSurface,
		video->rgb32 ? SCE_GXM_COLOR_FORMAT_A8R8G8B8 : SCE_GXM_COLOR_FORMAT_R5G6B5,
		SCE_GXM_COLOR_SURFACE_LINEAR,
		SCE_GXM_COLOR_SURFACE_SCALE_NONE,
		SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
		video->width,
		video->height,
		video->width,
		g_offscreenColorBufferData);
	assert(err == SCE_OK);

	// set up the texture
	err = sceGxmTextureInitLinear(
		&g_offscreenTexture,
		g_offscreenColorBufferData,
		video->rgb32 ? SCE_GXM_TEXTURE_FORMAT_A8B8G8R8 : SCE_GXM_TEXTURE_FORMAT_R5G6B5,
		video->width,
		video->height,
		1);
	assert(err == SCE_OK);

	// set linear filtering
	err = sceGxmTextureSetMagFilter(&g_offscreenTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	assert(err == SCE_OK);
	err = sceGxmTextureSetMinFilter(&g_offscreenTexture, SCE_GXM_TEXTURE_FILTER_LINEAR);
	assert(err == SCE_OK);

	// create the depth/stencil surface
	const uint32_t alignedWidth = ALIGN(DISPLAY_WIDTH, SCE_GXM_TILE_SIZEX);
	const uint32_t alignedHeight = ALIGN(DISPLAY_HEIGHT, SCE_GXM_TILE_SIZEY);
	uint32_t sampleCount = alignedWidth*alignedHeight;
	uint32_t depthStrideInSamples = alignedWidth;

	g_offscreenDepthBufferData = gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		4*sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&g_offscreenDepthBufferUid);

	err = sceGxmDepthStencilSurfaceInit(
		&g_offscreenDepthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		g_offscreenDepthBufferData,
		NULL);

	// create a render target
	g_offscreenRenderTarget = createRenderTarget(video->width, video->height, SCE_GXM_MULTISAMPLE_NONE);
}

static void destroyOffscreenBuffer(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;

	// destroy render target
	destroyRenderTarget(psp, g_offscreenRenderTarget);

	// free the memory
	gmmFree(g_offscreenDepthBufferUid);
	gmmFree(g_offscreenColorBufferUid);
}

static void update(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;

	// advance rotation
	g_rotationAngle += 0.25f*SCE_MATH_TWOPI/60.0f;
	if (g_rotationAngle > SCE_MATH_TWOPI)
		g_rotationAngle -= SCE_MATH_TWOPI;

	// copmute our matrices
	Matrix4	offscreenProjectionMatrix = Matrix4::perspective(
		SCE_MATH_PI/4.0f,
		(float)psp->width /(float)psp->height,
		0.1f,
		10.0f);
	Matrix4	mainProjectionMatrix = Matrix4::perspective(
		SCE_MATH_PI/4.0f,
		(float)DISPLAY_WIDTH/(float)DISPLAY_HEIGHT,
		0.1f,
		10.0f);
	Matrix4 viewMatrix		= Matrix4::translation(Vector3(0.0f, 0.0f, -5.0f));
	Matrix4 worldMatrix		= Matrix4::rotation(g_rotationAngle, Vector3(0.707f, 0.707f, 0.0f));
	g_offscreenWvpMatrix	= offscreenProjectionMatrix * viewMatrix * worldMatrix;
	g_mainWvpMatrix			= mainProjectionMatrix * viewMatrix * worldMatrix;
}

static void renderOffscreen(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;

	// set up a scene, offscreen render target, no sync required
	sceGxmBeginScene(
		psp->g_context,
		0, 
		g_offscreenRenderTarget,
		NULL,
		NULL,
		NULL,
		&g_offscreenColorSurface,
		&g_offscreenDepthSurface);

	// set clear shaders
	sceGxmSetVertexProgram(psp->g_context, g_clearVertexProgram);
	sceGxmSetFragmentProgram(psp->g_context, g_clearFragmentProgram);

	// set the fragment program constants
	void *fragmentDefaultBuffer;
	sceGxmReserveFragmentDefaultUniformBuffer(psp->g_context, &fragmentDefaultBuffer);
	float clearColor[4] = { 1.0f, 1.0f, 0.2f, 0.0f };
	sceGxmSetUniformDataF(fragmentDefaultBuffer, psp->g_clearColorParam, 0, 4, clearColor);

	// draw geometry
	sceGxmSetVertexStream(psp->g_context, 0, g_clearVertices);
	sceGxmDraw(psp->g_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, g_clearIndices, 3);

	// render the cube
	sceGxmSetVertexProgram(psp->g_context, g_cubeVertexProgram);
	sceGxmSetFragmentProgram(psp->g_context, g_cubeFragmentProgram);
	sceGxmSetVertexStream(psp->g_context, 0, g_cubeVertices);
	sceGxmSetFragmentTexture(psp->g_context, 0, &g_testTexture);

	// set the vertex program constants
	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(psp->g_context, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, g_cubeWvpParam, 0, 16, (float *)&g_offscreenWvpMatrix);

	// draw the cube
	sceGxmDraw(psp->g_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, g_cubeIndices, 36);

	// stop rendering to the offscreen render target
	sceGxmEndScene(psp->g_context, NULL, NULL);
}

static void renderMain(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;

	// set up a scene, main render target, synchronised with the back buffer sync
	sceGxmBeginScene(
		psp->g_context,
		0,
		psp->g_mainRenderTarget,
		NULL,
		NULL,
		psp->g_displayBufferSync[g_displayBackBufferIndex],
		&psp->g_displaySurface[g_displayBackBufferIndex],
		&g_mainDepthSurface);

	// set clear shaders
	sceGxmSetVertexProgram(psp->g_context, g_clearVertexProgram);
	sceGxmSetFragmentProgram(psp->g_context, g_clearFragmentProgram);

	// set the fragment program constants
	void *fragmentDefaultBuffer;
	sceGxmReserveFragmentDefaultUniformBuffer(psp->g_context, &fragmentDefaultBuffer);
	float clearColor[4] = { 0.2f, 0.2f, 0.2f, 0.0f };
	sceGxmSetUniformDataF(fragmentDefaultBuffer, psp->g_clearColorParam, 0, 4, clearColor);

	// draw geometry
	sceGxmSetVertexStream(psp->g_context, 0, g_clearVertices);
	sceGxmDraw(psp->g_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, g_clearIndices, 3);

	// render the cube
	sceGxmSetVertexProgram(psp->g_context, g_cubeVertexProgram);
	sceGxmSetFragmentProgram(psp->g_context, g_cubeFragmentProgram);
	sceGxmSetVertexStream(psp->g_context, 0, g_cubeVertices);
	sceGxmSetFragmentTexture(psp->g_context, 0, &g_offscreenTexture);

	// set the vertex program constants
	void *vertexDefaultBuffer;
	sceGxmReserveVertexDefaultUniformBuffer(psp->g_context, &vertexDefaultBuffer);
	sceGxmSetUniformDataF(vertexDefaultBuffer, g_cubeWvpParam, 0, 16, (float *)&g_mainWvpMatrix);

	// draw the cube
	sceGxmDraw(psp->g_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, g_cubeIndices, 36);

	// stop rendering to the main render target
	sceGxmEndScene(psp->g_context, NULL, NULL);

	// PA heartbeat to notify end of frame
	sceGxmPadHeartbeat(
		&psp->g_displaySurface[g_displayBackBufferIndex],
		psp->g_displayBufferSync[g_displayBackBufferIndex]);
}

static void *psp2_init(const video_info_t *video,
      const input_driver_t **input, void **input_data)
{
   void *pspinput;
   psp2_video_t *psp = (psp2_video_t*)driver.video_data;

   if (!psp)
   {
      // first time init
      psp = (psp2_video_t*)calloc(1, sizeof(psp2_video_t));
   
      if (!psp)
         goto error;
      
   }

	int err = SCE_OK;
   (void)err;

	// initialize libgxm
	// set up parameters
	SceGxmInitializeParams initializeParams;
	memset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
	initializeParams.flags							= 0;
	initializeParams.displayQueueMaxPendingCount	= DISPLAY_MAX_PENDING_SWAPS;
	initializeParams.displayQueueCallback			= displayCallback;
	initializeParams.displayQueueCallbackDataSize	= sizeof(DisplayData);
	initializeParams.parameterBufferSize			= SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

	// start libgxm
	err = sceGxmInitialize(&initializeParams);
	assert(err == SCE_OK);

	// create a rendering context
	// allocate host memory
	g_contextHostMem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);

	// allocate ring buffer memory using default sizes
	void *vdmRingBuffer = gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_vdmRingBufferUid);
	void *vertexRingBuffer = gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_vertexRingBufferUid);
	void *fragmentRingBuffer = gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&g_fragmentRingBufferUid);
	uint32_t fragmentUsseRingBufferOffset;
	void *fragmentUsseRingBuffer = fragmentUsseAlloc(
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE,
		&g_fragmentUsseRingBufferUid,
		&fragmentUsseRingBufferOffset);

	// set up parameters
	SceGxmContextParams contextParams;
	memset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem						= g_contextHostMem;
	contextParams.hostMemSize					= SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem				= vdmRingBuffer;
	contextParams.vdmRingBufferMemSize			= SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	contextParams.vertexRingBufferMem			= vertexRingBuffer;
	contextParams.vertexRingBufferMemSize		= SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	contextParams.fragmentRingBufferMem			= fragmentRingBuffer;
	contextParams.fragmentRingBufferMemSize		= SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferMem		= fragmentUsseRingBuffer;
	contextParams.fragmentUsseRingBufferMemSize	= SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	contextParams.fragmentUsseRingBufferOffset	= fragmentUsseRingBufferOffset;
	
	// create the context
	err = sceGxmCreateContext(&contextParams, &psp->g_context);
	assert(err == SCE_OK);

	// create a shader patcher
	createGxmShaderPatcher(psp);

	// allocate memory and sync objects for display buffers
	// FIXME: ensure physical contiguity for SceDisplay properly
	for (uint32_t i = 0; i < DISPLAY_BUFFER_COUNT; ++i)
   {
		// allocate memory with large (1MiB) size to ensure physical contiguity
		psp->g_displayBufferData[i] = gmmAlloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RWDATA,
			ALIGN(4*DISPLAY_STRIDE_IN_PIXELS*DISPLAY_HEIGHT, 1*1024*1024),
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&psp->g_displayBufferUid[i]);

		// memset the buffer to a noticeable debug color
		for (uint32_t y = 0; y < DISPLAY_HEIGHT; ++y)
      {
			uint32_t *row = (uint32_t *)psp->g_displayBufferData[i] + y*DISPLAY_STRIDE_IN_PIXELS;
			for (uint32_t x = 0; x < DISPLAY_WIDTH; ++x)
         {
				row[x] = 0xffff00ff;
			}
		}

		// initialize a color surface for this display buffer
		err = sceGxmColorSurfaceInit(
			&psp->g_displaySurface[i],
         video->rgb32 ? SCE_GXM_COLOR_FORMAT_A8R8G8B8 : SCE_GXM_COLOR_FORMAT_R5G6B5,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			SCE_GXM_COLOR_SURFACE_SCALE_NONE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			DISPLAY_WIDTH,
			DISPLAY_HEIGHT,
			DISPLAY_STRIDE_IN_PIXELS,
			psp->g_displayBufferData[i]);
		assert(err == SCE_OK);

		// create a sync object that we will associate with this buffer
		err = sceGxmSyncObjectCreate(&psp->g_displayBufferSync[i]);
		assert(err == SCE_OK);
	}

	// create a depth buffer
	const uint32_t alignedWidth = ALIGN(DISPLAY_WIDTH, SCE_GXM_TILE_SIZEX);
	const uint32_t alignedHeight = ALIGN(DISPLAY_HEIGHT, SCE_GXM_TILE_SIZEY);
	uint32_t sampleCount = alignedWidth*alignedHeight;
	uint32_t depthStrideInSamples = alignedWidth;
	
	void *mainDepthBufferData = gmmAlloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RWDATA_UNCACHE,
		4*sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&g_mainDepthBufferUid);

	err = sceGxmDepthStencilSurfaceInit(
		&g_mainDepthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		mainDepthBufferData,
		NULL);

	// swap to the current front buffer with VSYNC
	// (also ensures that future calls with HSYNC are successful)
	SceDisplayFrameBuf framebuf;
	memset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size        = sizeof(SceDisplayFrameBuf);
	framebuf.base        = psp->g_displayBufferData[psp->g_displayFrontBufferIndex];
	framebuf.pitch       = DISPLAY_STRIDE_IN_PIXELS;
	framebuf.pixelformat = DISPLAY_PIXEL_FORMAT;
	framebuf.width       = DISPLAY_WIDTH;
	framebuf.height      = DISPLAY_HEIGHT;
	err = sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
	assert(err == SCE_OK);
	err = sceDisplayWaitSetFrameBuf();
	assert(err == SCE_OK);

	// create a render target that describes the tiling setup we want to use
	psp->g_mainRenderTarget = createRenderTarget(DISPLAY_WIDTH, DISPLAY_HEIGHT, SCE_GXM_MULTISAMPLE_NONE);

	// create graphics data
	createClearData(psp);
	createTestTextureData(psp);
	createCubeData(psp);
   psp->rgb32  = video->rgb32;
   psp->width  = video->width;
   psp->width  = video->height;
	createOffscreenBuffer(psp, video);
   
   if (input && input_data)
   {
      pspinput = input_psp.init();
      *input = pspinput ? &input_psp : NULL;
      *input_data = pspinput;
   }
   
   return psp;
error:
   RARCH_ERR("PSP2 video could not be initialized.\n");
   return (void*)-1;
}

static bool psp2_frame(void *data, const void *frame,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   psp2_video_t *psp = (psp2_video_t*)data;

   update(psp);
   renderOffscreen(psp);
   renderMain(psp);

   cycleDisplayBuffers(psp, FLIP_MODE_VSYNC, width, heigh, pitch);

   return true;
}

static void psp2_set_nonblock_state(void *data, bool toggle)
{
   psp2_video_t *psp = (psp2_video_t*)data;
   psp->vsync = !toggle;
}

static bool psp2_alive(void *data)
{
   (void)data;
   return true;
}

static bool psp2_focus(void *data)
{
   (void)data;
   return true;
}

static void psp2_free(void *data)
{
   psp2_video_t *psp = (psp2_video_t*)data;
   (void)psp;

	// wait until rendering is done
	sceGxmFinish(psp->g_context);

	// destroy graphics data
	destroyOffscreenBuffer(psp);
	destroyCubeData(psp);
	destroyTestTextureData(psp);
	destroyClearData(psp);

	// terminate graphics
	int err = SCE_OK;
   (void)err;

	// destroy render target
	destroyRenderTarget(psp, psp->g_mainRenderTarget);

	// destroy depth buffer
	gmmFree(g_mainDepthBufferUid);

	// wait for display processing to finish before deallocating buffers
	err = sceGxmDisplayQueueFinish();
	assert(err == SCE_OK);

	// free the display buffers and sync objects
	for (uint32_t i = 0; i < DISPLAY_BUFFER_COUNT; ++i)
   {
		// clear the buffer and deallocate it
		memset(psp->g_displayBufferData[i], 0, DISPLAY_HEIGHT*DISPLAY_STRIDE_IN_PIXELS*4);
		gmmFree(psp->g_displayBufferUid[i]);

		// destroy sync object
		err = sceGxmSyncObjectDestroy(psp->g_displayBufferSync[i]);
		assert(err == SCE_OK);
	}

	// destroy the shader patcher
	err = sceGxmShaderPatcherDestroy(psp->g_shaderPatcher);
	assert(err == SCE_OK);
	combinedUsseFree(g_patcherCombinedUsseUid);
	gmmFree(g_patcherBufferUid);

	// destroy the rendering context
	err = sceGxmDestroyContext(psp->g_context);
	assert(err == SCE_OK);
	fragmentUsseFree(g_fragmentUsseRingBufferUid);
	gmmFree(g_fragmentRingBufferUid);
	gmmFree(g_vertexRingBufferUid);
	gmmFree(g_vdmRingBufferUid);
	free(g_contextHostMem);
	
	// terminate libgxm
	err = sceGxmTerminate();
	assert(err == SCE_OK);
	sceKernelFreeMemBlock(g_initializeParameterBufferUid);
	sceKernelFreeMemBlock(g_initializeDriverUid);
	free(psp->g_initializeHostMem);
}

#ifdef HAVE_MENU
static void psp2_restart(void) {}
#endif

static void psp2_set_rotation(void *data, unsigned rotation)
{
   psp2_video_t *psp = (psp2_video_t*)data;
   psp->rotation = rotation;
}

const video_driver_t video_psp2 = {
   psp2_init,
   psp2_frame,
   psp2_set_nonblock_state,
   psp2_alive,
   psp2_focus,
   NULL,
   psp2_free,
   "psp2",

#if defined(HAVE_MENU)
   psp2_restart,
#endif

   psp2_set_rotation,
   NULL,
   NULL,
#ifdef HAVE_OVERLAY
   NULL,
#endif
   NULL,
};
