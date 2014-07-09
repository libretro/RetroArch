typedef struct ios_camera
{
  void *empty;
} ioscamera_t;

static void *ios_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   ioscamera_t *ioscamera;
    
   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE)) == 0)
   {
      RARCH_ERR("ioscamera returns OpenGL texture.\n");
      return NULL;
   }

   ioscamera = (ioscamera_t*)calloc(1, sizeof(ioscamera_t));
   if (!ioscamera)
      return NULL;

   [[RAGameView get] onCameraInit];

   return ioscamera;
}

static void ios_camera_free(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
    
   [[RAGameView get] onCameraFree];

   if (ioscamera)
      free(ioscamera);
   ioscamera = NULL;
}

static bool ios_camera_start(void *data)
{
   (void)data;

   [[RAGameView get] onCameraStart];

   return true;
}

static void ios_camera_stop(void *data)
{
   [[RAGameView get] onCameraStop];
}

static bool ios_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
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

const camera_driver_t camera_ios = {
   ios_camera_init,
   ios_camera_free,
   ios_camera_start,
   ios_camera_stop,
   ios_camera_poll,
   "ios",
};
