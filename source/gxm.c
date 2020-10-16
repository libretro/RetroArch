/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * gxm.c:
 * Implementation for setup and cleanup for sceGxm specific stuffs
 */

#include "shared.h"

static uint32_t gxm_param_buf_size = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE; // Param buffer size for sceGxm

static void *vdm_ring_buffer_addr; // VDM ring buffer memblock starting address
static void *vertex_ring_buffer_addr; // vertex ring buffer memblock starting address
static void *fragment_ring_buffer_addr; // fragment ring buffer memblock starting address
static void *fragment_usse_ring_buffer_addr; // fragment USSE ring buffer memblock starting address

static SceGxmRenderTarget *gxm_render_target; // Display render target
static SceGxmColorSurface gxm_color_surfaces[DISPLAY_BUFFER_COUNT]; // Display color surfaces
static void *gxm_color_surfaces_addr[DISPLAY_BUFFER_COUNT]; // Display color surfaces memblock starting addresses
static SceGxmSyncObject *gxm_sync_objects[DISPLAY_BUFFER_COUNT]; // Display sync objects
static unsigned int gxm_front_buffer_index; // Display front buffer id
static unsigned int gxm_back_buffer_index; // Display back buffer id
static unsigned int gxm_scene_flags = 0; // Current gxm scene flags

static void *gxm_shader_patcher_buffer_addr; // Shader PAtcher buffer memblock starting address
static void *gxm_shader_patcher_vertex_usse_addr; // Shader Patcher vertex USSE memblock starting address
static void *gxm_shader_patcher_fragment_usse_addr; // Shader Patcher fragment USSE memblock starting address

void *gxm_depth_surface_addr; // Depth surface memblock starting address
static void *gxm_stencil_surface_addr; // Stencil surface memblock starting address
static SceGxmDepthStencilSurface gxm_depth_stencil_surface; // Depth/Stencil surfaces setup for sceGxm

static SceUID shared_fb; // In-use hared framebuffer identifier
static SceSharedFbInfo shared_fb_info; // In-use shared framebuffer info struct

SceGxmContext *gxm_context; // sceGxm context instance
GLenum vgl_error = GL_NO_ERROR; // Error returned by glGetError
SceGxmShaderPatcher *gxm_shader_patcher; // sceGxmShaderPatcher shader patcher instance

matrix4x4 mvp_matrix; // ModelViewProjection Matrix
matrix4x4 projection_matrix; // Projection Matrix
matrix4x4 modelview_matrix; // ModelView Matrix

int DISPLAY_WIDTH; // Display width in pixels
int DISPLAY_HEIGHT; // Display height in pixels
int DISPLAY_STRIDE; // Display stride in pixels
float DISPLAY_WIDTH_FLOAT; // Display width in pixels (float)
float DISPLAY_HEIGHT_FLOAT; // Display height in pixels (float)

uint8_t system_app_mode = 0; // Flag for system app mode usage
static uint8_t gxm_initialized = 0; // Current sceGxm state

// sceDisplay callback data
struct display_queue_callback_data {
	void *addr;
};

// sceGxmShaderPatcher custom allocator
static void *shader_patcher_host_alloc_cb(void *user_data, unsigned int size) {
	return malloc(size);
}

// sceGxmShaderPatcher custom deallocator
static void shader_patcher_host_free_cb(void *user_data, void *mem) {
	free(mem);
}

// sceDisplay callback
static void display_queue_callback(const void *callbackData) {
	// Populating sceDisplay framebuffer parameters
	SceDisplayFrameBuf display_fb;
	const struct display_queue_callback_data *cb_data = callbackData;
	memset(&display_fb, 0, sizeof(SceDisplayFrameBuf));
	display_fb.size = sizeof(SceDisplayFrameBuf);
	display_fb.base = cb_data->addr;
	display_fb.pitch = DISPLAY_STRIDE;
	display_fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
	display_fb.width = DISPLAY_WIDTH;
	display_fb.height = DISPLAY_HEIGHT;

	// Setting sceDisplay framebuffer
	sceDisplaySetFrameBuf(&display_fb, SCE_DISPLAY_SETBUF_NEXTFRAME);

	// Performing VSync if enabled
	if (vblank)
		sceDisplayWaitVblankStart();
}

void initGxm(void) {
	if (gxm_initialized)
		return;
	
	// Initializing runtime shader compiler
	if (use_shark) {
#ifdef HAVE_SHARK
		if (shark_init(NULL) >= 0) {
			is_shark_online = 1;
#ifdef HAVE_SHARK_LOG
			shark_install_log_cb(shark_log_cb);
			shark_set_warnings_level(SHARK_WARN_MAX);
#endif
		} else
#endif
			is_shark_online = 0;
	}
	
	// Checking if the running application is a system one
	SceAppMgrBudgetInfo info;
	info.size = sizeof(SceAppMgrBudgetInfo);
	if (!sceAppMgrGetBudgetInfo(&info))
		system_app_mode = 1;

	// Initializing sceGxm init parameters
	SceGxmInitializeParams gxm_init_params;
	memset(&gxm_init_params, 0, sizeof(SceGxmInitializeParams));
	gxm_init_params.flags = system_app_mode ? 0x0A : 0;
	gxm_init_params.displayQueueMaxPendingCount = DISPLAY_BUFFER_COUNT - 1;
	gxm_init_params.displayQueueCallback = display_queue_callback;
	gxm_init_params.displayQueueCallbackDataSize = sizeof(struct display_queue_callback_data);
	gxm_init_params.parameterBufferSize = gxm_param_buf_size;

	// Initializing sceGxm
	if (system_app_mode)
		sceGxmVshInitialize(&gxm_init_params);
	else
		sceGxmInitialize(&gxm_init_params);
	gxm_initialized = 1;
}

void initGxmContext(void) {
	vglMemType type = VGL_MEM_VRAM;

	// Allocating VDM ring buffer
	vdm_ring_buffer_addr = gpu_alloc_mapped(SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, &type);

	// Allocating vertex ring buffer
	vertex_ring_buffer_addr = gpu_alloc_mapped(SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, &type);

	// Allocating fragment ring buffer
	fragment_ring_buffer_addr = gpu_alloc_mapped(SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, &type);

	// Allocating fragment USSE ring buffer
	unsigned int fragment_usse_offset;
	fragment_usse_ring_buffer_addr = gpu_fragment_usse_alloc_mapped(
		SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, &fragment_usse_offset);

	// Setting sceGxm context parameters
	SceGxmContextParams gxm_context_params;
	memset(&gxm_context_params, 0, sizeof(SceGxmContextParams));
	gxm_context_params.hostMem = malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	gxm_context_params.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	gxm_context_params.vdmRingBufferMem = vdm_ring_buffer_addr;
	gxm_context_params.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	gxm_context_params.vertexRingBufferMem = vertex_ring_buffer_addr;
	gxm_context_params.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	gxm_context_params.fragmentRingBufferMem = fragment_ring_buffer_addr;
	gxm_context_params.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	gxm_context_params.fragmentUsseRingBufferMem = fragment_usse_ring_buffer_addr;
	gxm_context_params.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
	gxm_context_params.fragmentUsseRingBufferOffset = fragment_usse_offset;

	// Initializing sceGxm context
	sceGxmCreateContext(&gxm_context_params, &gxm_context);
}

void termGxmContext(void) {
	// Deallocating ring buffers
	mempool_free(vdm_ring_buffer_addr, VGL_MEM_VRAM);
	mempool_free(vertex_ring_buffer_addr, VGL_MEM_VRAM);
	mempool_free(fragment_ring_buffer_addr, VGL_MEM_VRAM);
	gpu_fragment_usse_free_mapped(fragment_usse_ring_buffer_addr);

	// Destroying sceGxm context
	sceGxmDestroyContext(gxm_context);

	if (system_app_mode) {
		sceSharedFbBegin(shared_fb, &shared_fb_info);
		sceGxmUnmapMemory(shared_fb_info.fb_base);
		sceSharedFbEnd(shared_fb);
		sceSharedFbClose(shared_fb);
	}
#ifdef HAVE_SHARK
	// Shutting down runtime shader compiler
	if (is_shark_online) shark_end();
#endif
}

void createDisplayRenderTarget(void) {
	// Populating sceGxmRenderTarget parameters
	SceGxmRenderTargetParams render_target_params;
	memset(&render_target_params, 0, sizeof(SceGxmRenderTargetParams));
	render_target_params.flags = 0;
	render_target_params.width = DISPLAY_WIDTH;
	render_target_params.height = DISPLAY_HEIGHT;
	render_target_params.scenesPerFrame = 1;
	render_target_params.multisampleMode = msaa_mode;
	render_target_params.multisampleLocations = 0;
	render_target_params.driverMemBlock = -1;

	// Creating render target for the display
	sceGxmCreateRenderTarget(&render_target_params, &gxm_render_target);
}

void destroyDisplayRenderTarget(void) {
	// Destroying render target for the display
	sceGxmDestroyRenderTarget(gxm_render_target);
}

void initDisplayColorSurfaces(void) {
	// Getting access to the shared framebuffer on system app mode
	while (system_app_mode) {
		shared_fb = sceSharedFbOpen(1);
		memset(&shared_fb_info, 0, sizeof(SceSharedFbInfo));
		sceSharedFbGetInfo(shared_fb, &shared_fb_info);
		if (shared_fb_info.index == 1)
			sceSharedFbClose(shared_fb);
		else {
			sceGxmMapMemory(shared_fb_info.fb_base, shared_fb_info.fb_size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
			gxm_color_surfaces_addr[0] = shared_fb_info.fb_base;
			gxm_color_surfaces_addr[1] = shared_fb_info.fb_base2;
			memset(&shared_fb_info, 0, sizeof(SceSharedFbInfo));
			break;
		}
	}

	vglMemType type = VGL_MEM_VRAM;
	int i;
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
		// Allocating color surface memblock
		if (!system_app_mode) {
			gxm_color_surfaces_addr[i] = gpu_alloc_mapped(
				ALIGN(4 * DISPLAY_STRIDE * DISPLAY_HEIGHT, 1 * 1024 * 1024),
				&type);
			memset(gxm_color_surfaces_addr[i], 0, DISPLAY_STRIDE * DISPLAY_HEIGHT);
		}

		// Initializing allocated color surface
		sceGxmColorSurfaceInit(&gxm_color_surfaces[i],
			SCE_GXM_COLOR_FORMAT_A8B8G8R8,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			msaa_mode == SCE_GXM_MULTISAMPLE_NONE ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			DISPLAY_WIDTH,
			DISPLAY_HEIGHT,
			DISPLAY_STRIDE,
			gxm_color_surfaces_addr[i]);

		// Creating a display sync object for the allocated color surface
		sceGxmSyncObjectCreate(&gxm_sync_objects[i]);
	}
}

void termDisplayColorSurfaces(void) {
	// Deallocating display's color surfaces and destroying sync objects
	int i;
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
		if (!system_app_mode)
			mempool_free(gxm_color_surfaces_addr[i], VGL_MEM_VRAM);
		sceGxmSyncObjectDestroy(gxm_sync_objects[i]);
	}
}

void initDepthStencilBuffer(uint32_t w, uint32_t h, SceGxmDepthStencilSurface *surface, void **depth_buffer, void **stencil_buffer, vglMemType *depth_type, vglMemType *stencil_type) {
	// Calculating sizes for depth and stencil surfaces
	unsigned int depth_stencil_width = ALIGN(w, SCE_GXM_TILE_SIZEX);
	unsigned int depth_stencil_height = ALIGN(h, SCE_GXM_TILE_SIZEY);
	unsigned int depth_stencil_samples = depth_stencil_width * depth_stencil_height;
	if (msaa_mode == SCE_GXM_MULTISAMPLE_2X)
		depth_stencil_samples = depth_stencil_samples * 2;
	else if (msaa_mode == SCE_GXM_MULTISAMPLE_4X)
		depth_stencil_samples = depth_stencil_samples * 4;

	// Allocating depth surface
	*depth_type = VGL_MEM_VRAM;
	*depth_buffer = gpu_alloc_mapped(4 * depth_stencil_samples, depth_type);

	// Allocating stencil surface
	*stencil_type = VGL_MEM_VRAM;
	*stencil_buffer = gpu_alloc_mapped(1 * depth_stencil_samples, stencil_type);

	// Initializing depth and stencil surfaces
	sceGxmDepthStencilSurfaceInit(surface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M_S8,
		SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR,
		msaa_mode == SCE_GXM_MULTISAMPLE_4X ? depth_stencil_width * 2 : depth_stencil_width,
		*depth_buffer,
		*stencil_buffer);
}

void initDepthStencilSurfaces(void) {
	vglMemType t1, t2;
	initDepthStencilBuffer(DISPLAY_WIDTH, DISPLAY_HEIGHT, &gxm_depth_stencil_surface, &gxm_depth_surface_addr, &gxm_stencil_surface_addr, &t1, &t2);
}

void termDepthStencilSurfaces(void) {
	// Deallocating depth and stencil surfaces memblocks
	mempool_free(gxm_depth_surface_addr, VGL_MEM_VRAM);
	mempool_free(gxm_stencil_surface_addr, VGL_MEM_VRAM);
}

void startShaderPatcher(void) {
	// Constants for shader patcher buffers
	static const unsigned int shader_patcher_buffer_size = 1024 * 1024;
	static const unsigned int shader_patcher_vertex_usse_size = 1024 * 1024;
	static const unsigned int shader_patcher_fragment_usse_size = 1024 * 1024;
	vglMemType type = VGL_MEM_VRAM;

	// Allocating Shader Patcher buffer
	gxm_shader_patcher_buffer_addr = gpu_alloc_mapped(
		shader_patcher_buffer_size, &type);

	// Allocating Shader Patcher vertex USSE buffer
	unsigned int shader_patcher_vertex_usse_offset;
	gxm_shader_patcher_vertex_usse_addr = gpu_vertex_usse_alloc_mapped(
		shader_patcher_vertex_usse_size, &shader_patcher_vertex_usse_offset);

	// Allocating Shader Patcher fragment USSE buffer
	unsigned int shader_patcher_fragment_usse_offset;
	gxm_shader_patcher_fragment_usse_addr = gpu_fragment_usse_alloc_mapped(
		shader_patcher_fragment_usse_size, &shader_patcher_fragment_usse_offset);

	// Populating shader patcher parameters
	SceGxmShaderPatcherParams shader_patcher_params;
	memset(&shader_patcher_params, 0, sizeof(SceGxmShaderPatcherParams));
	shader_patcher_params.userData = NULL;
	shader_patcher_params.hostAllocCallback = shader_patcher_host_alloc_cb;
	shader_patcher_params.hostFreeCallback = shader_patcher_host_free_cb;
	shader_patcher_params.bufferAllocCallback = NULL;
	shader_patcher_params.bufferFreeCallback = NULL;
	shader_patcher_params.bufferMem = gxm_shader_patcher_buffer_addr;
	shader_patcher_params.bufferMemSize = shader_patcher_buffer_size;
	shader_patcher_params.vertexUsseAllocCallback = NULL;
	shader_patcher_params.vertexUsseFreeCallback = NULL;
	shader_patcher_params.vertexUsseMem = gxm_shader_patcher_vertex_usse_addr;
	shader_patcher_params.vertexUsseMemSize = shader_patcher_vertex_usse_size;
	shader_patcher_params.vertexUsseOffset = shader_patcher_vertex_usse_offset;
	shader_patcher_params.fragmentUsseAllocCallback = NULL;
	shader_patcher_params.fragmentUsseFreeCallback = NULL;
	shader_patcher_params.fragmentUsseMem = gxm_shader_patcher_fragment_usse_addr;
	shader_patcher_params.fragmentUsseMemSize = shader_patcher_fragment_usse_size;
	shader_patcher_params.fragmentUsseOffset = shader_patcher_fragment_usse_offset;

	// Creating shader patcher instance
	sceGxmShaderPatcherCreate(&shader_patcher_params, &gxm_shader_patcher);
}

void stopShaderPatcher(void) {
	// Destroying shader patcher instance
	sceGxmShaderPatcherDestroy(gxm_shader_patcher);

	// Freeing shader patcher buffers
	mempool_free(gxm_shader_patcher_buffer_addr, VGL_MEM_VRAM);
	gpu_vertex_usse_free_mapped(gxm_shader_patcher_vertex_usse_addr);
	gpu_fragment_usse_free_mapped(gxm_shader_patcher_fragment_usse_addr);
}

void waitRenderingDone(void) {
	// Wait for rendering to be finished
	sceGxmDisplayQueueFinish();
	sceGxmFinish(gxm_context);
}

/*
 * ------------------------------
 * - IMPLEMENTATION STARTS HERE -
 * ------------------------------
 */

void vglSetParamBufferSize(uint32_t size) {
	gxm_param_buf_size = size;
}

void vglStartRendering(void) {
	// Starting drawing scene
	if (active_write_fb == NULL) { // Default framebuffer is used
		if (system_app_mode) {
			sceSharedFbBegin(shared_fb, &shared_fb_info);
			shared_fb_info.vsync = vblank;
			gxm_back_buffer_index = (shared_fb_info.index + 1) % 2;
		}
		sceGxmBeginScene(gxm_context, gxm_scene_flags, gxm_render_target,
			NULL, NULL,
			gxm_sync_objects[gxm_back_buffer_index],
			&gxm_color_surfaces[gxm_back_buffer_index],
			&gxm_depth_stencil_surface);
		gxm_scene_flags &= ~SCE_GXM_SCENE_VERTEX_WAIT_FOR_DEPENDENCY;
	} else {
		gxm_scene_flags |= SCE_GXM_SCENE_FRAGMENT_SET_DEPENDENCY;
		sceGxmBeginScene(gxm_context, gxm_scene_flags, active_write_fb->target,
			NULL, NULL, NULL,
			&active_write_fb->colorbuffer,
			&active_write_fb->depthbuffer);
		gxm_scene_flags |= SCE_GXM_SCENE_VERTEX_WAIT_FOR_DEPENDENCY;
		gxm_scene_flags &= ~SCE_GXM_SCENE_FRAGMENT_SET_DEPENDENCY;
	}

	// Setting back current viewport if enabled cause sceGxm will reset it at sceGxmEndScene call
	sceGxmSetViewport(gxm_context, x_port, x_scale, y_port, y_scale, z_port, z_scale);

	if (scissor_test_state)
		sceGxmSetRegionClip(gxm_context, SCE_GXM_REGION_CLIP_OUTSIDE, region.x, region.y, region.x + region.w - 1, region.y + region.h - 1);
	else
		sceGxmSetRegionClip(gxm_context, SCE_GXM_REGION_CLIP_OUTSIDE, 0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
}

void vglStopRenderingInit(void) {
	// Ending drawing scene
	sceGxmEndScene(gxm_context, NULL, NULL);
	if (system_app_mode && vblank)
		sceDisplayWaitVblankStart();
}

void vglStopRenderingTerm(void) {
	if (active_write_fb == NULL) { // Default framebuffer is used
		// Properly requesting a display update
		if (system_app_mode)
			sceSharedFbEnd(shared_fb);
		else {
			struct display_queue_callback_data queue_cb_data;
			queue_cb_data.addr = gxm_color_surfaces_addr[gxm_back_buffer_index];
			sceGxmDisplayQueueAddEntry(gxm_sync_objects[gxm_front_buffer_index],
				gxm_sync_objects[gxm_back_buffer_index], &queue_cb_data);
			gxm_front_buffer_index = gxm_back_buffer_index;
			gxm_back_buffer_index = (gxm_back_buffer_index + 1) % DISPLAY_BUFFER_COUNT;
		}
	}

	// Resetting vitaGL mempool
	gpu_pool_reset();
}

void vglStopRendering() {
	// Ending drawing scene
	vglStopRenderingInit();

	// Updating display and resetting vitaGL mempool
	vglStopRenderingTerm();
}

void vglUpdateCommonDialog() {
	// Populating SceCommonDialog parameters
	SceCommonDialogUpdateParam updateParam;
	memset(&updateParam, 0, sizeof(updateParam));
	updateParam.renderTarget.colorFormat = SCE_GXM_COLOR_FORMAT_A8B8G8R8;
	updateParam.renderTarget.surfaceType = SCE_GXM_COLOR_SURFACE_LINEAR;
	updateParam.renderTarget.width = DISPLAY_WIDTH;
	updateParam.renderTarget.height = DISPLAY_HEIGHT;
	updateParam.renderTarget.strideInPixels = DISPLAY_STRIDE;
	updateParam.renderTarget.colorSurfaceData = gxm_color_surfaces_addr[gxm_back_buffer_index];
	updateParam.renderTarget.depthSurfaceData = gxm_depth_surface_addr;
	updateParam.displaySyncObject = gxm_sync_objects[gxm_back_buffer_index];

	// Updating sceCommonDialog
	sceCommonDialogUpdate(&updateParam);
}

void glFinish(void) {
	// Waiting for GPU to finish drawing jobs
	sceGxmFinish(gxm_context);
}
