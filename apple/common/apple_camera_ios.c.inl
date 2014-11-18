typedef struct apple_camera
{
  void *empty;
} applecamera_t;

static void *apple_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   applecamera_t *applecamera;
    
   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE)) == 0)
   {
      RARCH_ERR("applecamera returns OpenGL texture.\n");
      return NULL;
   }

   applecamera = (applecamera_t*)calloc(1, sizeof(applecamera_t));
   if (!applecamera)
      return NULL;

   [[RAGameView get] onCameraInit];

   return applecamera;
}

static void apple_camera_free(void *data)
{
   applecamera_t *applecamera = (applecamera_t*)data;
    
   [[RAGameView get] onCameraFree];

   if (applecamera)
      free(applecamera);
   applecamera = NULL;
}

static bool apple_camera_start(void *data)
{
   (void)data;

   [[RAGameView get] onCameraStart];

   return true;
}

static void apple_camera_stop(void *data)
{
   [[RAGameView get] onCameraStop];
}

static bool apple_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{

   (void)data;
   (void)frame_raw_cb;

   if (frame_gl_cb && newFrame)
   {
	   // FIXME: Identity for now. Use proper texture matrix as returned by iOS Camera (if at all?).
	   static const float affine[] = {
		   1.0f, 0.0f, 0.0f,
		   0.0f, 1.0f, 0.0f,
		   0.0f, 0.0f, 1.0f
	   };
       
	   frame_gl_cb(outputTexture, GL_TEXTURE_2D, affine);
       newFrame = false;
   }

   return true;
}

camera_driver_t camera_apple = {
   apple_camera_init,
   apple_camera_free,
   apple_camera_start,
   apple_camera_stop,
   apple_camera_poll,
   "apple",
};
