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
//#include "../input/linuxraw_input.h"
// SDL include messing with some defines
typedef struct linuxraw_input linuxraw_input_t;
#include "../driver.h"

#ifdef HAVE_FREETYPE
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include "../file.h"
#endif


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

#ifdef HAVE_FREETYPE
	char *mLastMsg;
	uint32_t mFontHeight;
	FT_Library ftLib;
	FT_Face ftFace;
	VGFont mFont;
	bool mFontsOn;
	VGuint mMsgLength;
	VGuint mGlyphIndices[1024];
	VGPaint mPaintFg;
	VGPaint mPaintBg;
#endif
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
		const linuxraw_input_t *linuxraw_input = (const linuxraw_input_t *) input_linuxraw.init();
		if (linuxraw_input)
		{
			*input = (const input_driver_t *) &input_linuxraw;
			*input_data = linuxraw_input;
		}
	}

#ifdef HAVE_FREETYPE

	static const char *font_paths[] = {
		"/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
		"/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
		"osd-font.ttf", // Magic font to search for, useful for distribution.
	};

	const char *font = g_settings.video.font_path;

	if (!g_settings.video.font_enable)
		goto fail;

	if (!path_file_exists(font))
	{
		font = NULL;

		for (int i = 0; i < sizeof(font_paths) / sizeof(font_paths[0]); i++)
		{
			if (path_file_exists(font_paths[i]))
			{
				font = font_paths[i];
				break;
			}
		}
	}

	if (!font)
	{
		RARCH_WARN("No font found, messages disabled...\n");
		goto fail;
	}

	if (FT_Init_FreeType(&rpi->ftLib)) {
		RARCH_WARN("failed to initialize freetype\n");
		goto fail;
	}

	if (FT_New_Face(rpi->ftLib, font, 0, &rpi->ftFace)) {
		RARCH_WARN("failed to load %s\n", font);
		goto fail;
	}

	if (FT_Select_Charmap(rpi->ftFace, FT_ENCODING_UNICODE)) {
		RARCH_WARN("failed to select an unicode charmap\n");
		goto fail;
	}

	uint32_t size = g_settings.video.font_size * (g_settings.video.font_scale ? (float) rpi->mScreenWidth / 1280.0f : 1.0f);
	rpi->mFontHeight = size;

	if (FT_Set_Pixel_Sizes(rpi->ftFace, size, size))
		RARCH_WARN("failed to set pixel sizes\n");

	rpi->mFont = vgCreateFont(0);

	if (rpi->mFont)
		rpi->mFontsOn = true;

	rpi->mPaintFg = vgCreatePaint();
	rpi->mPaintBg = vgCreatePaint();
	VGfloat paintFg[] = { g_settings.video.msg_color_r, g_settings.video.msg_color_g, g_settings.video.msg_color_b, 1.0f };
	VGfloat paintBg[] = { g_settings.video.msg_color_r / 2.0f, g_settings.video.msg_color_g / 2.0f, g_settings.video.msg_color_b / 2.0f, 0.5f };

	vgSetParameteri(rpi->mPaintFg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(rpi->mPaintFg, VG_PAINT_COLOR, 4, paintFg);

	vgSetParameteri(rpi->mPaintBg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(rpi->mPaintBg, VG_PAINT_COLOR, 4, paintBg);

//	vgSetPaint(myFillPaint, VG_FILL_PATH);
//	vgSetPaint(myStrokePaint, VG_STROKE_PATH);

	fail:
#endif

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

#ifdef HAVE_FREETYPE

// mostly adapted from example code in Mesa-3d

static int path_append(VGPath path, VGubyte segment, const FT_Vector **vectors)
{
	VGfloat coords[6];
	int i, num_vectors;

	switch (segment)
	{
		case VG_MOVE_TO:
		case VG_LINE_TO:
			num_vectors = 1;
			break;
		case VG_QUAD_TO:
			num_vectors = 2;
			break;
		case VG_CUBIC_TO:
			num_vectors = 3;
			break;
		default:
			return -1;
			break;
	}

	for (i = 0; i < num_vectors; i++)
	{
		coords[2 * i + 0] = (float) vectors[i]->x / 64.0f;
		coords[2 * i + 1] = (float) vectors[i]->y / 64.0f;
	}

	vgAppendPathData(path, 1, &segment, (const void *) coords);

	return 0;
}

static int decompose_move_to(const FT_Vector *to, void *user)
{
	VGPath path = (VGPath) (VGuint) user;

	return path_append(path, VG_MOVE_TO, &to);
}

static int decompose_line_to(const FT_Vector *to, void *user)
{
	VGPath path = (VGPath) (VGuint) user;

	return path_append(path, VG_LINE_TO, &to);
}

static int decompose_conic_to(const FT_Vector *control, const FT_Vector *to, void *user)
{
	VGPath path = (VGPath) (VGuint) user;
	const FT_Vector *vectors[2] = { control, to };

	return path_append(path, VG_QUAD_TO, vectors);
}

static int decompose_cubic_to(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user)
{
	VGPath path = (VGPath) (VGuint) user;
	const FT_Vector *vectors[3] = { control1, control2, to };

	return path_append(path, VG_CUBIC_TO, vectors);
}

static VGHandle convert_outline_glyph(rpi_t *rpi)
{
	FT_GlyphSlot glyph = rpi->ftFace->glyph;
	FT_Outline_Funcs funcs = {
		decompose_move_to,
		decompose_line_to,
		decompose_conic_to,
		decompose_cubic_to,
		0, 0
	};

	VGHandle path;

	path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, glyph->outline.n_points, VG_PATH_CAPABILITY_ALL);

	if (FT_Outline_Decompose(&glyph->outline, &funcs, (void *) (VGuint) path))
	{
		vgDestroyPath(path);
		path = VG_INVALID_HANDLE;
	}

	return path;
}

static VGint glyph_string_add_path(rpi_t *rpi, VGPath path, const VGfloat origin[2], VGfloat escapement[2])
{
	if (rpi->mMsgLength >= 1024)
		return -1;

	if (rpi->mFont != VG_INVALID_HANDLE)
	{
		vgSetGlyphToPath(rpi->mFont, rpi->mMsgLength, path, VG_TRUE, origin, escapement);
		return rpi->mMsgLength++;
	}

	return -1;
}

static VGHandle convert_bitmap_glyph(rpi_t *rpi)
{
	FT_GlyphSlot glyph = rpi->ftFace->glyph;
	VGImage image;
	VGint width, height, stride;
	unsigned char *data;
	int i, j;

	switch (glyph->bitmap.pixel_mode)
	{
		case FT_PIXEL_MODE_MONO:
		case FT_PIXEL_MODE_GRAY:
			break;
		default:
			return VG_INVALID_HANDLE;
			break;
	}

	data = glyph->bitmap.buffer;
	width = glyph->bitmap.width;
	height = glyph->bitmap.rows;
	stride = glyph->bitmap.pitch;

	/* mono to gray, and flip if needed */
	if (glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		data = malloc(width * height);
		if (!data)
			return VG_INVALID_HANDLE;

		for (i = 0; i < height; i++)
		{
			unsigned char *dst = &data[width * i];
			const unsigned char *src;

			if (stride > 0)
				src = glyph->bitmap.buffer + stride * (height - i - 1);
			else
				src = glyph->bitmap.buffer - stride * i;

			for (j = 0; j < width; j++)
			{
				if (src[j / 8] & (1 << (7 - (j % 8))))
					dst[j] = 0xff;
				else
					dst[j] = 0x0;
			}
		}
		stride = -width;
	}

	image = vgCreateImage(VG_A_8, width, height, VG_IMAGE_QUALITY_NONANTIALIASED);

	if (stride < 0)
	{
		stride = -stride;
		vgImageSubData(image, data, stride, VG_A_8, 0, 0, width, height);
	}
	else
	{
		/* flip vertically */
		for (i = 0; i < height; i++)
		{
			const unsigned char *row = data + stride * i;

			vgImageSubData(image, row, stride, VG_A_8, 0, height - i - 1, width, 1);
		}
	}

	if (data != glyph->bitmap.buffer)
		free(data);

	return (VGHandle) image;
}

static VGint glyph_string_add_image(rpi_t *rpi, VGImage image, const VGfloat origin[2], VGfloat escapement[2])
{
	if (rpi->mMsgLength >= 1024)
		return -1;

	if (rpi->mFont != VG_INVALID_HANDLE)
	{
		vgSetGlyphToImage(rpi->mFont, rpi->mMsgLength, image, origin, escapement);
		return rpi->mMsgLength++;
	}

	return -1;
}

static void rpi_draw_message(rpi_t *rpi, const char *msg)
{
	if (!rpi->mLastMsg || strcmp(rpi->mLastMsg, msg))
	{
		if (rpi->mLastMsg)
			free(rpi->mLastMsg);

		rpi->mLastMsg = strdup(msg);

		if(rpi->mMsgLength)
			while (--rpi->mMsgLength)
				vgClearGlyph(rpi->mFont, rpi->mMsgLength);

		rpi->mMsgLength = 0;

		for (int i = 0; msg[i]; i++)
		{
			VGfloat origin[2], escapement[2];
			VGHandle handle;

			/*
			 * if a character appears more than once, it will be loaded and converted
			 * again...
			 */
			if (FT_Load_Char(rpi->ftFace, msg[i], FT_LOAD_DEFAULT))
			{
				RARCH_WARN("failed to load glyph '%c'\n", msg[i]);
				goto fail;
			}

			escapement[0] = (VGfloat) rpi->ftFace->glyph->advance.x / 64.0f;
			escapement[1] = (VGfloat) rpi->ftFace->glyph->advance.y / 64.0f;

			switch (rpi->ftFace->glyph->format)
			{
				case FT_GLYPH_FORMAT_OUTLINE:
					handle = convert_outline_glyph(rpi);
					origin[0] = 0.0f;
					origin[1] = 0.0f;
					glyph_string_add_path(rpi, (VGPath) handle, origin, escapement);
					break;
				case FT_GLYPH_FORMAT_BITMAP:
					handle = convert_bitmap_glyph(rpi);
					origin[0] = (VGfloat) (-rpi->ftFace->glyph->bitmap_left);
					origin[1] = (VGfloat) (rpi->ftFace->glyph->bitmap.rows - rpi->ftFace->glyph->bitmap_top);
					glyph_string_add_image(rpi, (VGImage) handle, origin, escapement);
					break;
				default:
					break;
			}
		}
		for (int i = 0; i < rpi->mMsgLength; i++)
			rpi->mGlyphIndices[i] = i;
	}

	fail:

	vgSeti(VG_SCISSORING, VG_FALSE);

	VGfloat origins[] = { rpi->mScreenWidth * g_settings.video.msg_pos_x - 2.0f, rpi->mScreenHeight * g_settings.video.msg_pos_y - 2.0f };
	vgSetfv(VG_GLYPH_ORIGIN, 2, origins);
	vgSetPaint(rpi->mPaintBg, VG_FILL_PATH);
	vgDrawGlyphs(rpi->mFont, rpi->mMsgLength, rpi->mGlyphIndices, NULL, NULL, VG_FILL_PATH, VG_TRUE);
	origins[0] += 2.0f;
	origins[1] += 2.0f;
	vgSetfv(VG_GLYPH_ORIGIN, 2, origins);
	vgSetPaint(rpi->mPaintFg, VG_FILL_PATH);
	vgDrawGlyphs(rpi->mFont, rpi->mMsgLength, rpi->mGlyphIndices, NULL, NULL, VG_FILL_PATH, VG_TRUE);

	vgSeti(VG_SCISSORING, VG_TRUE);
}

#endif

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

#ifdef HAVE_FREETYPE
	if (msg && rpi->mFontsOn)
		rpi_draw_message(rpi, msg);
#endif

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
