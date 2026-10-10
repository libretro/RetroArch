/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2026 - The RetroArch team
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
#include <string.h>
#include <dlfcn.h>

#include <libretro.h>
#include <glsym/glsym.h>
#include <formats/image_yuv_blit.h>

#include "../camera_driver.h"
#include "../../retroarch.h"
#include "../../gfx/video_driver.h"

/* Three ways a frame reaches the core, chosen once at init from what
 * the core accepts, cheapest first:
 *
 *   texture  A GL core samples the camera's SurfaceTexture directly.
 *            Only with a GL hardware context, whose EGL context owns
 *            the texture.
 *   planar   The capture buffer itself, mapped and handed over as
 *            planes. Camera2 through the NDK (API 24 and up) maps the
 *            HAL's buffer in place; the legacy Camera API below that
 *            delivers NV21 into a Java array, pinned for the callback.
 *   raw      XRGB8888, which no Android camera produces: the planar
 *            frame converted once into a buffer the driver keeps.
 *            Only for a core that accepts nothing else. */
enum android_camera_mode
{
   ANDROID_CAMERA_TEXTURE = 0,
   ANDROID_CAMERA_PLANAR,
   ANDROID_CAMERA_RAW
};

/* Camera2 NDK, resolved at runtime so the build keeps its API 21
 * floor. Only what is called is declared, laid out as the NDK headers
 * (NdkCameraManager.h, NdkCameraMetadata.h, NdkCameraDevice.h,
 * NdkCameraCaptureSession.h) lay it out; the handle types are opaque. */
typedef struct ACameraManager ACameraManager;
typedef struct ACameraDevice ACameraDevice;
typedef struct ACaptureRequest ACaptureRequest;
typedef struct ACameraCaptureSession ACameraCaptureSession;
typedef struct ACaptureSessionOutputContainer ACaptureSessionOutputContainer;
typedef struct ACaptureSessionOutput ACaptureSessionOutput;
typedef struct ACameraOutputTarget ACameraOutputTarget;
typedef struct ACameraMetadata ACameraMetadata;
typedef struct AImageReader AImageReader;
typedef struct AImage AImage;
typedef struct ANativeWindow ANativeWindow;

typedef struct
{
   int numCameras;
   const char **cameraIds;
} ACameraIdList;

typedef struct
{
   uint32_t tag;
   uint8_t type;
   uint32_t count;
   union
   {
      uint8_t *u8;
      int32_t *i32;
      float *f;
      int64_t *i64;
      double *d;
      void *r;
   } data;
} ACameraMetadata_entry;

/* Later NDKs append members; an older library reads only those it
 * knows, so the newest layout is declared and the rest left NULL */
typedef struct
{
   void *context;
   void (*onDisconnected)(void *context, ACameraDevice *device);
   void (*onError)(void *context, ACameraDevice *device, int error);
   void (*onClientSharedAccessPriorityChanged)(void *context,
         ACameraDevice *device, bool primary);
} ACameraDevice_StateCallbacks;

typedef struct
{
   void *context;
   void (*onClosed)(void *context, ACameraCaptureSession *session);
   void (*onReady)(void *context, ACameraCaptureSession *session);
   void (*onActive)(void *context, ACameraCaptureSession *session);
} ACameraCaptureSession_stateCallbacks;

#define ACAMERA_OK                        0
#define AIMAGE_FORMAT_YUV_420_888         0x23
#define ACAMERA_SENSOR_ORIENTATION        0x000e000e /* ACAMERA_SENSOR_START + 14 */
#define ACAMERA_LENS_FACING               0x00080005 /* ACAMERA_LENS_START + 5 */
#define ACAMERA_LENS_FACING_BACK          1
#define TEMPLATE_PREVIEW                  1

typedef struct
{
   void *lib_camera;
   void *lib_media;
   ACameraManager *(*ACameraManager_create)(void);
   void (*ACameraManager_delete)(ACameraManager*);
   int (*ACameraManager_getCameraIdList)(ACameraManager*, ACameraIdList**);
   void (*ACameraManager_deleteCameraIdList)(ACameraIdList*);
   int (*ACameraManager_getCameraCharacteristics)(ACameraManager*, const char*, ACameraMetadata**);
   int (*ACameraMetadata_getConstEntry)(const ACameraMetadata*, uint32_t, ACameraMetadata_entry*);
   void (*ACameraMetadata_free)(ACameraMetadata*);
   int (*ACameraManager_openCamera)(ACameraManager*, const char*, ACameraDevice_StateCallbacks*, ACameraDevice**);
   int (*ACameraDevice_close)(ACameraDevice*);
   int (*ACameraDevice_createCaptureRequest)(const ACameraDevice*, int, ACaptureRequest**);
   void (*ACaptureRequest_free)(ACaptureRequest*);
   int (*ACameraOutputTarget_create)(ANativeWindow*, ACameraOutputTarget**);
   void (*ACameraOutputTarget_free)(ACameraOutputTarget*);
   int (*ACaptureRequest_addTarget)(ACaptureRequest*, const ACameraOutputTarget*);
   int (*ACaptureSessionOutputContainer_create)(ACaptureSessionOutputContainer**);
   void (*ACaptureSessionOutputContainer_free)(ACaptureSessionOutputContainer*);
   int (*ACaptureSessionOutput_create)(ANativeWindow*, ACaptureSessionOutput**);
   void (*ACaptureSessionOutput_free)(ACaptureSessionOutput*);
   int (*ACaptureSessionOutputContainer_add)(ACaptureSessionOutputContainer*, const ACaptureSessionOutput*);
   int (*ACameraDevice_createCaptureSession)(ACameraDevice*, const ACaptureSessionOutputContainer*, const ACameraCaptureSession_stateCallbacks*, ACameraCaptureSession**);
   int (*ACameraCaptureSession_setRepeatingRequest)(ACameraCaptureSession*, void*, int, ACaptureRequest**, int*);
   int (*ACameraCaptureSession_stopRepeating)(ACameraCaptureSession*);
   void (*ACameraCaptureSession_close)(ACameraCaptureSession*);
   int (*AImageReader_new)(int32_t, int32_t, int32_t, int32_t, AImageReader**);
   void (*AImageReader_delete)(AImageReader*);
   int (*AImageReader_getWindow)(AImageReader*, ANativeWindow**);
   int (*AImageReader_acquireLatestImage)(AImageReader*, AImage**);
   void (*AImage_delete)(AImage*);
   int (*AImage_getWidth)(const AImage*, int32_t*);
   int (*AImage_getHeight)(const AImage*, int32_t*);
   int (*AImage_getPlanePixelStride)(const AImage*, int, int32_t*);
   int (*AImage_getPlaneRowStride)(const AImage*, int, int32_t*);
   int (*AImage_getPlaneData)(const AImage*, int, uint8_t**, int*);
} android_camera2_api_t;

typedef struct android_camera
{
   /* Java side: the legacy Camera and the SurfaceTexture */
   jmethodID onCameraInit;
   jmethodID onCameraFree;
   jmethodID onCameraPoll;
   jmethodID onCameraStart;
   jmethodID onCameraStop;
   jmethodID onCameraSetTexture;
   jmethodID onCameraRawInit;
   jmethodID onCameraPollRaw;
   jmethodID onCameraRawDone;
   jmethodID onCameraRawWidth;
   jmethodID onCameraRawHeight;
   jmethodID onCameraRawRotation;

   /* Camera2 session */
   android_camera2_api_t api;
   ACameraManager *manager;
   ACameraDevice *device;
   AImageReader *reader;
   ANativeWindow *window;
   ACameraOutputTarget *target;
   ACaptureSessionOutputContainer *outputs;
   ACaptureSessionOutput *output;
   ACaptureRequest *request;
   ACameraCaptureSession *session;
   AImage *image;        /* the frame last handed over; back to the reader at the next poll */

   /* raw: the XRGB8888 frame, VIDEO_SCALE_AREA(rgb_dims) words */
   uint32_t *rgb;
   unsigned rgb_dims;

   unsigned dims;        /* requested size, VIDEO_SCALE_PACK; 0 axes free */
   unsigned rotation;
   GLuint tex;
   uint8_t mode;         /* enum android_camera_mode */
   uint8_t camera2;      /* frames come from the NDK session */
   uint8_t started;
} androidcamera_t;

static bool android_camera_hw_is_gl(void)
{
   struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
   if (!hwr)
      return false;
   switch (hwr->context_type)
   {
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         return true;
      default:
         break;
   }
   return false;
}

/* ---- Camera2 ---------------------------------------------------- */

static void android_camera2_unload(android_camera2_api_t *api)
{
   if (api->lib_camera)
      dlclose(api->lib_camera);
   if (api->lib_media)
      dlclose(api->lib_media);
   memset(api, 0, sizeof(*api));
}

#define ANDROID_CAMERA2_SYM(lib, name) \
   do { \
      *(void**)&api->name = dlsym(lib, #name); \
      if (!api->name) \
         goto fail; \
   } while (0)

static bool android_camera2_load(android_camera2_api_t *api)
{
   api->lib_camera = dlopen("libcamera2ndk.so", RTLD_NOW | RTLD_LOCAL);
   api->lib_media  = dlopen("libmediandk.so",   RTLD_NOW | RTLD_LOCAL);
   if (!api->lib_camera || !api->lib_media)
      goto fail;
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraManager_create);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraManager_delete);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraManager_getCameraIdList);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraManager_deleteCameraIdList);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraManager_getCameraCharacteristics);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraMetadata_getConstEntry);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraMetadata_free);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraManager_openCamera);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraDevice_close);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraDevice_createCaptureRequest);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureRequest_free);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraOutputTarget_create);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraOutputTarget_free);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureRequest_addTarget);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureSessionOutputContainer_create);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureSessionOutputContainer_free);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureSessionOutput_create);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureSessionOutput_free);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACaptureSessionOutputContainer_add);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraDevice_createCaptureSession);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraCaptureSession_setRepeatingRequest);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraCaptureSession_stopRepeating);
   ANDROID_CAMERA2_SYM(api->lib_camera, ACameraCaptureSession_close);
   ANDROID_CAMERA2_SYM(api->lib_media, AImageReader_new);
   ANDROID_CAMERA2_SYM(api->lib_media, AImageReader_delete);
   ANDROID_CAMERA2_SYM(api->lib_media, AImageReader_getWindow);
   ANDROID_CAMERA2_SYM(api->lib_media, AImageReader_acquireLatestImage);
   ANDROID_CAMERA2_SYM(api->lib_media, AImage_delete);
   ANDROID_CAMERA2_SYM(api->lib_media, AImage_getWidth);
   ANDROID_CAMERA2_SYM(api->lib_media, AImage_getHeight);
   ANDROID_CAMERA2_SYM(api->lib_media, AImage_getPlanePixelStride);
   ANDROID_CAMERA2_SYM(api->lib_media, AImage_getPlaneRowStride);
   ANDROID_CAMERA2_SYM(api->lib_media, AImage_getPlaneData);
   return true;
fail:
   android_camera2_unload(api);
   return false;
}

static void android_camera2_on_disconnected(void *context, ACameraDevice *device)
{
   (void)context; (void)device;
}

static void android_camera2_on_error(void *context, ACameraDevice *device, int error)
{
   (void)context; (void)device; (void)error;
}

static void android_camera2_on_session(void *context, ACameraCaptureSession *session)
{
   (void)context; (void)session;
}

static void android_camera2_close(androidcamera_t *cam)
{
   android_camera2_api_t *api = &cam->api;
   if (!api->lib_camera)
      return;
   if (cam->image)
      api->AImage_delete(cam->image);
   if (cam->session)
   {
      api->ACameraCaptureSession_stopRepeating(cam->session);
      api->ACameraCaptureSession_close(cam->session);
   }
   if (cam->request)
      api->ACaptureRequest_free(cam->request);
   if (cam->target)
      api->ACameraOutputTarget_free(cam->target);
   if (cam->output)
      api->ACaptureSessionOutput_free(cam->output);
   if (cam->outputs)
      api->ACaptureSessionOutputContainer_free(cam->outputs);
   if (cam->device)
      api->ACameraDevice_close(cam->device);
   if (cam->reader)
      api->AImageReader_delete(cam->reader);
   if (cam->manager)
      api->ACameraManager_delete(cam->manager);
   cam->image   = NULL;
   cam->session = NULL;
   cam->request = NULL;
   cam->target  = NULL;
   cam->output  = NULL;
   cam->outputs = NULL;
   cam->device  = NULL;
   cam->reader  = NULL;
   cam->window  = NULL;
   cam->manager = NULL;
}

/* The back camera when there is one, else the first; a reader of the
 * requested size (640x480 when none was asked) and a preview request
 * into it, ready to repeat. False leaves nothing open. */
static bool android_camera2_open(androidcamera_t *cam)
{
   android_camera2_api_t *api = &cam->api;
   ACameraDevice_StateCallbacks dev_cb;
   ACameraCaptureSession_stateCallbacks ses_cb;
   ACameraIdList *ids = NULL;
   ACameraMetadata *meta;
   ACameraMetadata_entry entry;
   const char *id     = NULL;
   unsigned w         = VIDEO_SCALE_W(cam->dims);
   unsigned h         = VIDEO_SCALE_H(cam->dims);
   int i;

   if (!w || !h)
   {
      w = 640;
      h = 480;
   }
   if (!(cam->manager = api->ACameraManager_create()))
      return false;
   if (api->ACameraManager_getCameraIdList(cam->manager, &ids) != ACAMERA_OK
         || !ids || ids->numCameras < 1)
      goto fail;
   id = ids->cameraIds[0];
   for (i = 0; i < ids->numCameras; i++)
   {
      bool back = false;
      if (api->ACameraManager_getCameraCharacteristics(cam->manager,
               ids->cameraIds[i], &meta) != ACAMERA_OK)
         continue;
      back =    api->ACameraMetadata_getConstEntry(meta,
                  ACAMERA_LENS_FACING, &entry) == ACAMERA_OK
             && entry.count && entry.data.u8[0] == ACAMERA_LENS_FACING_BACK;
      api->ACameraMetadata_free(meta);
      if (back)
      {
         id = ids->cameraIds[i];
         break;
      }
   }
   if (api->ACameraManager_getCameraCharacteristics(cam->manager, id,
            &meta) == ACAMERA_OK)
   {
      if (     api->ACameraMetadata_getConstEntry(meta,
                  ACAMERA_SENSOR_ORIENTATION, &entry) == ACAMERA_OK
            && entry.count)
         cam->rotation = (unsigned)entry.data.i32[0] % 360;
      api->ACameraMetadata_free(meta);
   }

   dev_cb.context        = cam;
   dev_cb.onDisconnected = android_camera2_on_disconnected;
   dev_cb.onError        = android_camera2_on_error;
   dev_cb.onClientSharedAccessPriorityChanged = NULL;
   if (api->ACameraManager_openCamera(cam->manager, id, &dev_cb,
            &cam->device) != ACAMERA_OK)
      goto fail;
   api->ACameraManager_deleteCameraIdList(ids);
   ids = NULL;

   /* Three images: one at the HAL, one just acquired, one being read */
   if (api->AImageReader_new((int32_t)w, (int32_t)h,
            AIMAGE_FORMAT_YUV_420_888, 3, &cam->reader) != ACAMERA_OK)
      goto fail;
   if (api->AImageReader_getWindow(cam->reader, &cam->window) != ACAMERA_OK)
      goto fail;
   if (api->ACameraDevice_createCaptureRequest(cam->device,
            TEMPLATE_PREVIEW, &cam->request) != ACAMERA_OK)
      goto fail;
   if (api->ACameraOutputTarget_create(cam->window, &cam->target) != ACAMERA_OK)
      goto fail;
   if (api->ACaptureRequest_addTarget(cam->request, cam->target) != ACAMERA_OK)
      goto fail;
   if (api->ACaptureSessionOutputContainer_create(&cam->outputs) != ACAMERA_OK)
      goto fail;
   if (api->ACaptureSessionOutput_create(cam->window, &cam->output) != ACAMERA_OK)
      goto fail;
   if (api->ACaptureSessionOutputContainer_add(cam->outputs, cam->output) != ACAMERA_OK)
      goto fail;
   ses_cb.context  = cam;
   ses_cb.onClosed = android_camera2_on_session;
   ses_cb.onReady  = android_camera2_on_session;
   ses_cb.onActive = android_camera2_on_session;
   if (api->ACameraDevice_createCaptureSession(cam->device, cam->outputs,
            &ses_cb, &cam->session) != ACAMERA_OK)
      goto fail;
   return true;
fail:
   if (ids)
      api->ACameraManager_deleteCameraIdList(ids);
   android_camera2_close(cam);
   return false;
}

/* The newest frame the HAL has, mapped where it lies. The previous
 * one goes back to the reader first: the core was done with it when
 * its callback returned. Never waits: no frame yet is false. */
static bool android_camera2_frame(androidcamera_t *cam,
      struct retro_camera_planar_frame *f)
{
   android_camera2_api_t *api = &cam->api;
   AImage *img = NULL;
   int32_t w, h, ps, rs;
   uint8_t *p;
   int len, i;

   if (cam->image)
   {
      api->AImage_delete(cam->image);
      cam->image = NULL;
   }
   if (api->AImageReader_acquireLatestImage(cam->reader, &img) != ACAMERA_OK || !img)
      return false;
   if (     api->AImage_getWidth(img, &w) != ACAMERA_OK
         || api->AImage_getHeight(img, &h) != ACAMERA_OK)
      goto drop;
   for (i = 0; i < 3; i++)
   {
      if (     api->AImage_getPlaneData(img, i, &p, &len) != ACAMERA_OK
            || api->AImage_getPlaneRowStride(img, i, &rs) != ACAMERA_OK
            || api->AImage_getPlanePixelStride(img, i, &ps) != ACAMERA_OK)
         goto drop;
      f->planes[i]  = p;
      f->strides[i] = (size_t)rs;
      if (i == 1)
         f->chroma_step = (size_t)ps;
   }
   f->width    = (unsigned)w;
   f->height   = (unsigned)h;
   f->rotation = cam->rotation;
   f->flags    = 0;
   cam->image  = img;
   return true;
drop:
   api->AImage_delete(img);
   return false;
}

/* ---- legacy Camera, through Java --------------------------------- */

/* The newest NV21 preview buffer, pinned; @arr and @base come back
 * for android_java_frame_done once the core has read it. */
static bool android_java_frame(androidcamera_t *cam, JNIEnv *env,
      struct retro_camera_planar_frame *f, jbyteArray *arr, jbyte **base)
{
   struct android_app *android_app = (struct android_app*)g_android;
   jobject obj = NULL;
   jint w, h, rot;

   CALL_OBJ_METHOD(env, obj, android_app->activity->clazz, cam->onCameraPollRaw);
   if (!obj)
      return false;
   CALL_INT_METHOD(env, w,   android_app->activity->clazz, cam->onCameraRawWidth);
   CALL_INT_METHOD(env, h,   android_app->activity->clazz, cam->onCameraRawHeight);
   CALL_INT_METHOD(env, rot, android_app->activity->clazz, cam->onCameraRawRotation);
   *arr  = (jbyteArray)obj;
   *base = (*env)->GetByteArrayElements(env, *arr, NULL);
   if (!*base || w <= 0 || h <= 0)
   {
      if (*base)
         (*env)->ReleaseByteArrayElements(env, *arr, *base, JNI_ABORT);
      CALL_VOID_METHOD_PARAM(env, android_app->activity->clazz,
            cam->onCameraRawDone, obj);
      (*env)->DeleteLocalRef(env, obj);
      return false;
   }
   /* NV21: Y, then V,U interleaved */
   f->planes[0]   = (const uint8_t*)*base;
   f->planes[2]   = (const uint8_t*)*base + (size_t)w * (size_t)h;
   f->planes[1]   = f->planes[2] + 1;
   f->strides[0]  = (size_t)w;
   f->strides[1]  = (size_t)w;
   f->strides[2]  = (size_t)w;
   f->chroma_step = 2;
   f->width       = (unsigned)w;
   f->height      = (unsigned)h;
   f->rotation    = (unsigned)rot;
   f->flags       = 0;
   return true;
}

static void android_java_frame_done(androidcamera_t *cam, JNIEnv *env,
      jbyteArray arr, jbyte *base)
{
   struct android_app *android_app = (struct android_app*)g_android;
   (*env)->ReleaseByteArrayElements(env, arr, base, JNI_ABORT);
   CALL_VOID_METHOD_PARAM(env, android_app->activity->clazz,
         cam->onCameraRawDone, arr);
   (*env)->DeleteLocalRef(env, arr);
}

/* ---- the driver -------------------------------------------------- */

static void android_camera_deliver(androidcamera_t *cam,
      const struct retro_camera_planar_frame *f,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_planar_t frame_planar_cb)
{
   unsigned flags = 0;

   if (cam->mode == ANDROID_CAMERA_PLANAR)
   {
      if (frame_planar_cb)
         frame_planar_cb(f);
      return;
   }
   if (!frame_raw_cb)
      return;
   if (!cam->rgb || cam->rgb_dims != VIDEO_SCALE_PACK(f->width, f->height))
   {
      uint32_t *rgb = (uint32_t*)realloc(cam->rgb,
            (size_t)f->width * f->height * sizeof(uint32_t));
      if (!rgb)
         return;
      cam->rgb      = rgb;
      cam->rgb_dims = VIDEO_SCALE_PACK(f->width, f->height);
   }
   if (f->flags & RETRO_CAMERA_PLANAR_FULL_RANGE)
      flags |= IMAGE_YUV_FLAG_FULL_RANGE;
   if (f->flags & RETRO_CAMERA_PLANAR_BT709)
      flags |= IMAGE_YUV_FLAG_BT709;
   image_yuv_420_to_rgb32(cam->rgb, f->width,
         f->planes[0], (unsigned)f->strides[0],
         f->planes[1], (unsigned)f->strides[1],
         f->planes[2], (unsigned)f->strides[2],
         (unsigned)f->chroma_step, f->width, f->height, flags);
   frame_raw_cb(cam->rgb, f->width, f->height,
         (size_t)f->width * sizeof(uint32_t));
}

static void *android_camera_init(const char *device, uint64_t caps,
      unsigned dims)
{
   jclass class;
   jboolean ok                     = JNI_FALSE;
   androidcamera_t *cam            = NULL;
   JNIEnv *env                     = NULL;
   struct android_app *android_app = (struct android_app*)g_android;

   (void)device;

   if (!(cam = (androidcamera_t*)calloc(1, sizeof(*cam))))
      return NULL;
   cam->dims = dims;

   if (     (caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE))
         && android_camera_hw_is_gl())
      cam->mode = ANDROID_CAMERA_TEXTURE;
   else if (caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR))
      cam->mode = ANDROID_CAMERA_PLANAR;
   else if (caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER))
      cam->mode = ANDROID_CAMERA_RAW;
   else
   {
      RARCH_ERR("[Camera] No buffer type this core accepts is available.\n");
      goto dealloc;
   }

   if (!(env = jni_thread_getenv()))
      goto dealloc;

   GET_OBJECT_CLASS(env, class, android_app->activity->clazz);
   if (!class)
      goto dealloc;

   GET_METHOD_ID(env, cam->onCameraInit,        class, "onCameraInit",        "()Z");
   GET_METHOD_ID(env, cam->onCameraFree,        class, "onCameraFree",        "()V");
   GET_METHOD_ID(env, cam->onCameraSetTexture,  class, "onCameraSetTexture",  "(I)Z");
   GET_METHOD_ID(env, cam->onCameraStart,       class, "onCameraStart",       "()V");
   GET_METHOD_ID(env, cam->onCameraStop,        class, "onCameraStop",        "()V");
   GET_METHOD_ID(env, cam->onCameraPoll,        class, "onCameraPoll",        "()Z");
   GET_METHOD_ID(env, cam->onCameraRawInit,     class, "onCameraRawInit",     "(II)Z");
   GET_METHOD_ID(env, cam->onCameraPollRaw,     class, "onCameraPollRaw",     "()[B");
   GET_METHOD_ID(env, cam->onCameraRawDone,     class, "onCameraRawDone",     "([B)V");
   GET_METHOD_ID(env, cam->onCameraRawWidth,    class, "onCameraRawWidth",    "()I");
   GET_METHOD_ID(env, cam->onCameraRawHeight,   class, "onCameraRawHeight",   "()I");
   GET_METHOD_ID(env, cam->onCameraRawRotation, class, "onCameraRawRotation", "()I");
   if (     !cam->onCameraInit || !cam->onCameraFree || !cam->onCameraSetTexture
         || !cam->onCameraStart || !cam->onCameraStop || !cam->onCameraPoll
         || !cam->onCameraRawInit || !cam->onCameraPollRaw || !cam->onCameraRawDone
         || !cam->onCameraRawWidth || !cam->onCameraRawHeight
         || !cam->onCameraRawRotation)
      goto dealloc;

   /* The permission: asked for when missing, in which case there is
    * no camera for the core this time round */
   CALL_BOOLEAN_METHOD(env, ok, android_app->activity->clazz, cam->onCameraInit);
   if (!ok)
      goto dealloc;

   if (cam->mode != ANDROID_CAMERA_TEXTURE)
   {
      if (android_camera2_load(&cam->api) && android_camera2_open(cam))
         cam->camera2 = 1;
      else
      {
         android_camera2_unload(&cam->api);
         CALL_BOOLEAN_METHOD_PARAM(env, ok, android_app->activity->clazz,
               cam->onCameraRawInit,
               (jint)VIDEO_SCALE_W(dims), (jint)VIDEO_SCALE_H(dims));
         if (!ok)
            goto dealloc;
      }
   }
   return cam;

dealloc:
   free(cam);
   return NULL;
}

static void android_camera_free(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *cam            = (androidcamera_t*)data;
   JNIEnv *env                     = jni_thread_getenv();

   if (!cam)
      return;
   if (cam->camera2)
   {
      android_camera2_close(cam);
      android_camera2_unload(&cam->api);
   }
   else if (env)
      CALL_VOID_METHOD(env, android_app->activity->clazz, cam->onCameraFree);
   free(cam->rgb);
   free(cam);
}

static bool android_camera_start(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *cam            = (androidcamera_t*)data;
   JNIEnv *env;

   if (cam->camera2)
   {
      if (!cam->started && cam->api.ACameraCaptureSession_setRepeatingRequest(
               cam->session, NULL, 1, &cam->request, NULL) != ACAMERA_OK)
         return false;
      cam->started = 1;
      return true;
   }
   if (!(env = jni_thread_getenv()))
      return false;
   if (cam->mode == ANDROID_CAMERA_TEXTURE && !cam->tex)
   {
      jboolean ok = JNI_FALSE;
      glGenTextures(1, &cam->tex);
      glBindTexture(GL_TEXTURE_EXTERNAL_OES, cam->tex);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      CALL_BOOLEAN_METHOD_PARAM(env, ok, android_app->activity->clazz,
            cam->onCameraSetTexture, (jint)cam->tex);
      if (!ok)
      {
         glDeleteTextures(1, &cam->tex);
         cam->tex = 0;
         return false;
      }
   }
   CALL_VOID_METHOD(env, android_app->activity->clazz, cam->onCameraStart);
   cam->started = 1;
   return true;
}

static void android_camera_stop(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *cam            = (androidcamera_t*)data;
   JNIEnv *env;

   if (cam->camera2)
   {
      if (cam->started)
         cam->api.ACameraCaptureSession_stopRepeating(cam->session);
      cam->started = 0;
      return;
   }
   if (!(env = jni_thread_getenv()))
      return;
   CALL_VOID_METHOD(env, android_app->activity->clazz, cam->onCameraStop);
   if (cam->tex)
      glDeleteTextures(1, &cam->tex);
   cam->tex     = 0;
   cam->started = 0;
}

static bool android_camera_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb,
      retro_camera_frame_planar_t frame_planar_cb)
{
   struct android_app *android_app = (struct android_app*)g_android;
   androidcamera_t *cam            = (androidcamera_t*)data;
   struct retro_camera_planar_frame f;
   JNIEnv *env;

   if (!cam->started)
      return false;

   if (cam->camera2)
   {
      if (!android_camera2_frame(cam, &f))
         return false;
      android_camera_deliver(cam, &f, frame_raw_cb, frame_planar_cb);
      return true;
   }

   if (!(env = jni_thread_getenv()))
      return false;

   if (cam->mode == ANDROID_CAMERA_TEXTURE)
   {
      jboolean new_frame = JNI_FALSE;
      /* Identity: the SurfaceTexture's transform is not read yet */
      static const float affine[] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
      };
      CALL_BOOLEAN_METHOD(env, new_frame, android_app->activity->clazz,
            cam->onCameraPoll);
      if (!new_frame)
         return false;
      if (frame_gl_cb)
         frame_gl_cb(cam->tex, GL_TEXTURE_EXTERNAL_OES, affine);
      return true;
   }

   {
      jbyteArray arr = NULL;
      jbyte *base    = NULL;
      if (!android_java_frame(cam, env, &f, &arr, &base))
         return false;
      android_camera_deliver(cam, &f, frame_raw_cb, frame_planar_cb);
      android_java_frame_done(cam, env, arr, base);
   }
   return true;
}

camera_driver_t camera_android = {
   android_camera_init,
   android_camera_free,
   android_camera_start,
   android_camera_stop,
   android_camera_poll,
   "android",
};
