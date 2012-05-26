#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <bcm_host.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "../libretro.h"
#include "../general.h"
#include "../input/linuxraw_input.h"
#include "../driver.h"

typedef struct {
	EGLDisplay mDisplay;
	EGLSurface mSurface;
	EGLContext mContext;
	uint32_t mScreenWidth;
	uint32_t mScreenHeight;
	unsigned mTextureWidth;
	unsigned mTextureHeight;
	unsigned mRenderWidth;
	unsigned mRenderHeight;
	unsigned x1, y1, x2, y2;
	VGImageFormat mTexType;
	VGImage mImage;
	VGfloat mTransformMatrix[9];
	VGint scissor[4];
} rpi_t;

static void rpi_set_nonblock_state(void *data, bool state)
{
	rpi_t *rpi = (rpi_t*)data;
	eglSwapInterval(rpi->mDisplay, state ? 0 : 1);
}

static void *rpi_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
	int32_t success;
	EGLBoolean result;
	EGLint num_config;
	rpi_t *rpi = (rpi_t*)calloc(1, sizeof(rpi_t));
	*input = NULL;

	static EGL_DISPMANX_WINDOW_T nativewindow;

	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	DISPMANX_MODEINFO_T dispman_modeinfo;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	static const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	EGLConfig config;

	// get an EGL display connection
	rpi->mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(rpi->mDisplay != EGL_NO_DISPLAY);

	// initialize the EGL display connection
	result = eglInitialize(rpi->mDisplay, NULL, NULL);
	assert(result != EGL_FALSE);
	eglBindAPI(EGL_OPENVG_API);

	// get an appropriate EGL frame buffer configuration
	result = eglChooseConfig(rpi->mDisplay, attribute_list, &config, 1, &num_config);
	assert(result != EGL_FALSE);

	// create an EGL rendering context
	rpi->mContext = eglCreateContext(rpi->mDisplay, config, EGL_NO_CONTEXT, NULL);
	assert(rpi->mContext != EGL_NO_CONTEXT);

	// create an EGL window surface
	success = graphics_get_display_size(0 /* LCD */, &rpi->mScreenWidth, &rpi->mScreenHeight);
	assert(success >= 0);

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = rpi->mScreenWidth;
	dst_rect.height = rpi->mScreenHeight;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = rpi->mScreenWidth << 16;
	src_rect.height = rpi->mScreenHeight << 16;

	dispman_display = vc_dispmanx_display_open(0 /* LCD */);
	vc_dispmanx_display_get_info(dispman_display, &dispman_modeinfo);
	dispman_update = vc_dispmanx_update_start(0);

	dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
		0/*layer*/, &dst_rect, 0/*src*/,
		&src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, DISPMANX_NO_ROTATE);

	nativewindow.element = dispman_element;
	nativewindow.width = rpi->mScreenWidth;
	nativewindow.height = rpi->mScreenHeight;
	vc_dispmanx_update_submit_sync(dispman_update);

	rpi->mSurface = eglCreateWindowSurface(rpi->mDisplay, config, &nativewindow, NULL);
	assert(rpi->mSurface != EGL_NO_SURFACE);

	// connect the context to the surface
	result = eglMakeCurrent(rpi->mDisplay, rpi->mSurface, rpi->mSurface, rpi->mContext);
	assert(result != EGL_FALSE);

	rpi->mTexType = video->rgb32 ? VG_sABGR_8888 : VG_sARGB_1555;

	VGfloat clearColor[4] = {0, 0, 0, 1};
	vgSetfv(VG_CLEAR_COLOR, 4, clearColor);

	// set viewport for aspect ratio, taken from RetroArch
	if (video->force_aspect)
	{
		float desired_aspect = g_settings.video.aspect_ratio;
		float device_aspect = (float) dispman_modeinfo.width / dispman_modeinfo.height;

		// If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff),
		// assume they are actually equal.
		if (fabs(device_aspect - desired_aspect) < 0.0001)
		{
			rpi->x1 = 0;
			rpi->y1 = 0;
			rpi->x2 = rpi->mScreenWidth;
			rpi->y2 = rpi->mScreenHeight;
		}
		else if (device_aspect > desired_aspect)
		{
			float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
			rpi->x1 = rpi->mScreenWidth * (0.5 - delta);
			rpi->y1 = 0;
			rpi->x2 = 2.0 * rpi->mScreenWidth * delta + rpi->x1;
			rpi->y2 = rpi->mScreenHeight + rpi->y1;
		}
		else
		{
			float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
			rpi->x1 = 0;
			rpi->y1 = rpi->mScreenHeight * (0.5 - delta);
			rpi->x2 = rpi->mScreenWidth + rpi->x1;
			rpi->y2 = 2.0 * rpi->mScreenHeight * delta + rpi->y1;
		}
	}
	else
	{
		rpi->x1 = 0;
		rpi->y1 = 0;
		rpi->x2 = rpi->mScreenWidth;
		rpi->y2 = rpi->mScreenHeight;
	}

	rpi->scissor[0] = rpi->x1;
	rpi->scissor[1] = rpi->y1;
	rpi->scissor[2] = rpi->x2 - rpi->x1;
	rpi->scissor[3] = rpi->y2 - rpi->y1;

	vgSetiv(VG_SCISSOR_RECTS, 4, rpi->scissor);

	rpi->mTextureWidth = rpi->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;
	// We can't use the native format because there's no sXRGB_1555 type and
	// emulation cores can send 0 in the top bit. We lose some speed on
	// conversion but I doubt it has any real affect, since we are only drawing
	// one image at the end of the day. Still keep the alpha channel for ABGR.
	rpi->mImage = vgCreateImage(video->rgb32 ? VG_sABGR_8888 : VG_sXBGR_8888, rpi->mTextureWidth, rpi->mTextureHeight, video->smooth ? VG_IMAGE_QUALITY_BETTER : VG_IMAGE_QUALITY_NONANTIALIASED);
	rpi_set_nonblock_state(rpi, !video->vsync);

	if (isatty(0))
	{
		linuxraw_input_t *linuxraw_input = (linuxraw_input_t*)input_linuxraw.init();
		if (linuxraw_input)
		{
			*input = &input_linuxraw;
			*input_data = linuxraw_input;
		}
	}

	return rpi;
}

static void rpi_free(void *data)
{
	rpi_t *rpi = (rpi_t*)data;

	vgDestroyImage(rpi->mImage);

	// Release EGL resources
	eglMakeCurrent(rpi->mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(rpi->mDisplay, rpi->mSurface);
	eglDestroyContext(rpi->mDisplay, rpi->mContext);
	eglTerminate(rpi->mDisplay);

	free(rpi);
}

static bool rpi_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
	rpi_t *rpi = (rpi_t*)data;
	(void)msg;

	if (width != rpi->mRenderWidth || height != rpi->mRenderHeight)
	{
		rpi->mRenderWidth = width;
		rpi->mRenderHeight = height;
		vguComputeWarpQuadToQuad(
			rpi->x1, rpi->y1, rpi->x2, rpi->y1, rpi->x2, rpi->y2, rpi->x1, rpi->y2,
			// needs to be flipped, Khronos loves their bottom-left origin
			0, height, width, height, width, 0, 0, 0,
			rpi->mTransformMatrix);
		vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
		vgLoadMatrix(rpi->mTransformMatrix);
	}
	vgSeti(VG_SCISSORING, VG_FALSE);
	vgClear(0, 0, rpi->mScreenWidth, rpi->mScreenHeight);
	vgSeti(VG_SCISSORING, VG_TRUE);

	vgImageSubData(rpi->mImage, frame, pitch, rpi->mTexType, 0, 0, width, height);
	vgDrawImage(rpi->mImage);

	eglSwapBuffers(rpi->mDisplay, rpi->mSurface);

	return true;
}

static bool rpi_alive(void *data)
{
   (void)data;
   return true;
}

static bool rpi_focus(void *data)
{
   (void)data;
   return true;
}

static void rpi_set_rotation(void *data, unsigned rotation)
{
	(void)data;
	(void)rotation;
}

const video_driver_t video_rpi = {
   rpi_init,
   rpi_frame,
   rpi_set_nonblock_state,
   rpi_alive,
   rpi_focus,
   NULL,
   rpi_free,
   "rpi",
   rpi_set_rotation,
};
