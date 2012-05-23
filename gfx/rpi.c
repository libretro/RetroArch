#include <assert.h>
#include <math.h>
#include <bcm_host.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "../libretro.h"
#include "../general.h"

static inline unsigned get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

static const GLfloat default_vertices[] = {
   0, 0,
   0, 1,
   1, 1,
   1, 0
};

typedef struct rpi {
	EGLDisplay mDisplay;
	EGLSurface mSurface;
	EGLContext mContext;
	uint32_t mScreenWidth;
	uint32_t mScreenHeight;
	GLint mTextureWidth;
	GLint mTextureHeight;
	unsigned mRenderWidth;
	unsigned mRenderHeight;
	int mBpp;
	GLenum mTexType;
	GLuint mTexture;
	GLuint mPalette;
	GLuint mVertBuf;
	GLuint vshader;
	GLuint fshader;
	GLuint program;
	GLfloat mTexVertices[8];
	uint8_t *mEmptyBuf;
} rpi_t;

static uint16_t rgba1555_to_rgba5551[0x10000];

static void rpi_setup_palette(rpi_t *rpi)
{
	// because GLES doesn't have GL_UNSIGNED_SHORT_1_5_5_5_REV, we fake it with this shader
	static const GLchar *vertex_shader_src =
		"attribute vec4 vertex;\n"
		"varying vec2 tcoord;\n"
		"void main(void)\n"
		"{\n"
		"vec4 pos = vertex;\n"
		"gl_Position = vertex;\n"
		"tcoord = vertex.xy; // * 0.5 + 0.5;\n"
		"}";

	static const GLchar *fragment_shader_16_src =
		"uniform sampler2D pal;\n"
		"uniform sampler2D tex;\n"
		"varying vec2 tcoord;\n"
		"void main()\n"
		"{\n"
		"const float scale = 255.0 / 256.0;\n"
		"const float offset = 0.5 / 255.0 * scale;\n"
		"vec4 color = texture2D(tex, tcoord.xy);\n"
		"float r = color.r / 32.0;\n"
		"float g = color.g;\n"
		"float b = color.b / 32.0;\n"
		"float a = color.a;\n"
		"float pixelx = (a + r) * scale + offset;\n"
		"float pixely = (g + b) * scale + offset;\n"
		"vec2 coords = vec2(pixelx, pixely);\n"
		"gl_FragColor = texture2D(pal, coords);\n"
		"}";

	static const GLchar *fragment_shader_32_src =
		"uniform sampler2D pal;\n"
		"uniform sampler2D tex;\n"
		"varying vec2 tcoord;\n"
		"void main()\n"
		"{\n"
		"vec4 color = texture2D(tex, tcoord.xy);\n"
		"gl_FragColor = vec4(color.b, color.g, color.r, color.a);\n"
		"}";

	GLint compiled, linked;
	GLuint unif_pal, unif_tex;
	int i, a, r, g, b;

	for(i = 0; i < 0x10000; i++)
	{
		a = (i & 0x8000) >> 15;
		r = (i & 0x7C00) >> 9;
		g = (i & 0x03E0) << 1;
		b = (i & 0x001F) << 11;
		rgba1555_to_rgba5551[i] = a | r | g | b;
	}

	glActiveTexture(GL_TEXTURE1);
	glGenTextures(1, &rpi->mPalette);
	glBindTexture(GL_TEXTURE_2D, rpi->mPalette);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, rgba1555_to_rgba5551);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	rpi->vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(rpi->vshader, 1, &vertex_shader_src, 0);
	glCompileShader(rpi->vshader);

	glGetShaderiv ( rpi->vshader, GL_COMPILE_STATUS, &compiled );

	if ( !compiled )
	{
		GLint infoLen = 0;
		glGetShaderiv ( rpi->vshader, GL_INFO_LOG_LENGTH, &infoLen );

		if ( infoLen > 1 )
		{
			char* infoLog = malloc (sizeof(char) * infoLen );
			glGetShaderInfoLog ( rpi->vshader, infoLen, NULL, infoLog );
			printf ( "Error compiling shader:\n%s\n", infoLog );
			free ( infoLog );
		}

		glDeleteShader ( rpi->vshader );
		exit(1);
	}

	rpi->fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(rpi->fshader, 1, rpi->mBpp == 2 ? &fragment_shader_16_src : &fragment_shader_32_src, 0);
	glCompileShader(rpi->fshader);

	glGetShaderiv ( rpi->fshader, GL_COMPILE_STATUS, &compiled );

	if ( !compiled )
	{
		GLint infoLen = 0;
		glGetShaderiv ( rpi->fshader, GL_INFO_LOG_LENGTH, &infoLen );

		if ( infoLen > 1 )
		{
			char* infoLog = malloc (sizeof(char) * infoLen );
			glGetShaderInfoLog ( rpi->fshader, infoLen, NULL, infoLog );
			printf ( "Error compiling shader:\n%s\n", infoLog );
			free ( infoLog );
		}

		glDeleteShader ( rpi->fshader );
		exit(1);
	}

	rpi->program = glCreateProgram();
	glAttachShader(rpi->program, rpi->vshader);
	glAttachShader(rpi->program, rpi->fshader);
	glBindAttribLocation(rpi->program, 0, "vertex");
	glLinkProgram(rpi->program);

	glGetProgramiv ( rpi->program, GL_LINK_STATUS, &linked );
	if ( !linked )
	{
		GLint infoLen = 0;
		glGetProgramiv ( rpi->program, GL_INFO_LOG_LENGTH, &infoLen );

		if ( infoLen > 1 )
		{
			char* infoLog = malloc (sizeof(char) * infoLen );
			glGetProgramInfoLog ( rpi->program, infoLen, NULL, infoLog );
			printf( "Error linking program:\n%s\n", infoLog );
			free ( infoLog );
		}

		glDeleteProgram ( rpi->program );
		exit(1);
	}

//	unif_pal = glGetUniformLocation(rpi->program, "pal");
//	unif_tex = glGetUniformLocation(rpi->program, "tex");

//	glUniform1i(unif_pal, 1);
//	glUniform1i(unif_tex, 0);

	// reset to screen texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rpi->mTexture);
	assert(glGetError() == 0);
}

static void rpi_set_nonblock_state(void *data, bool state)
{
	rpi_t *rpi = (rpi_t*)data;
	eglSwapInterval(rpi->mDisplay, state ? 1 : 0);
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

	rpi->mBpp = video->rgb32 ? 4 : 2;
	rpi->mTexType = video->rgb32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT_5_5_5_1;

	// Set background color and clear buffers
	glClearColor(0, 0, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);
	//glShadeModel(GL_SMOOTH);

	// set viewport for aspect ratio, taken from RetroArch
	if (video->force_aspect)
	{
		float desired_aspect = g_settings.video.aspect_ratio;
		float device_aspect = (float) dispman_modeinfo.width / dispman_modeinfo.height;

		// If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff),
		// assume they are actually equal.
		if (fabs(device_aspect - desired_aspect) < 0.0001)
		{
			glViewport(0, 0, rpi->mScreenWidth, rpi->mScreenHeight);
		}
		else if (device_aspect > desired_aspect)
		{
			float delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
			glViewport(rpi->mScreenWidth * (0.5 - delta), 0, 2.0 * rpi->mScreenWidth * delta, rpi->mScreenHeight);
			rpi->mScreenWidth = 2.0 * rpi->mScreenWidth * delta;
		}
		else
		{
			float delta = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
			glViewport(0, rpi->mScreenHeight * (0.5 - delta), rpi->mScreenWidth, 2.0 * rpi->mScreenHeight * delta);
			rpi->mScreenHeight = 2.0 * rpi->mScreenHeight * delta;
		}
	}
	else
	{
		glViewport(0, 0, rpi->mScreenWidth, rpi->mScreenHeight);
	}

	//glMatrixMode(GL_PROJECTION);
	//glLoadIdentity();

	//glOrthof(0, 1, 0, 1, -1, 1);
	//glMatrixMode(GL_MODELVIEW);
	//glLoadIdentity();

	rpi->mTextureWidth = rpi->mTextureHeight = video->input_scale * RARCH_SCALE_BASE;

	rpi->mEmptyBuf = calloc(rpi->mTextureWidth * rpi->mTextureHeight * rpi->mBpp, 1);

	glEnable(GL_TEXTURE_2D);
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &rpi->mTexture);
	glBindTexture(GL_TEXTURE_2D, rpi->mTexture);
	//glPixelStorei(GL_UNPACK_ROW_LENGTH, mTextureWidth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rpi->mTextureWidth, rpi->mTextureHeight, 0, GL_RGBA, rpi->mTexType, rpi->mEmptyBuf);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, video->smooth ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, video->smooth ? GL_LINEAR : GL_NEAREST);

	//glEnableClientState(GL_VERTEX_ARRAY);
	//glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	rpi->mTexVertices[0] = 0;
	rpi->mTexVertices[1] = 1;
	rpi->mTexVertices[2] = 0;
	rpi->mTexVertices[3] = 0;
	rpi->mTexVertices[4] = 1;
	rpi->mTexVertices[5] = 0;
	rpi->mTexVertices[6] = 1;
	rpi->mTexVertices[7] = 1;

	//glVertexPointer(2, GL_FLOAT, 0, default_vertices);
	//glTexCoordPointer(2, GL_FLOAT, 0, rpi->mTexVertices);

	rpi_setup_palette(rpi);
	rpi_set_nonblock_state(rpi, video->vsync);

	return rpi;
}

static void rpi_free(void *data)
{
	rpi_t *rpi = (rpi_t*)data;
	free(rpi->mEmptyBuf);

	// clear screen
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(rpi->mDisplay, rpi->mSurface);

	// Release OpenGL resources
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
		glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, rpi->mTexType, frame);
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram (rpi->program);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, default_vertices);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

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
