/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "mediacodec_inventory.h"
#include "mediacodec_encoder_format.h"
#include <float.h>
#include <limits.h>
#include <string.h>

#define MC_CHECK(value) \
   do \
   { \
      if (!(value) || (*env)->ExceptionCheck(env)) \
         goto failed; \
   } while (0)
#define MC_VOID(call) \
   do \
   { \
      call; \
      if ((*env)->ExceptionCheck(env)) \
         goto failed; \
   } while (0)
static int mediacodec_inventory_select_internal(JNIEnv *env, const char *mime,
      int encoder, unsigned width, unsigned height, int profile, char *name,
      size_t capacity, const mediacodec_encoder_request_t *request)
{
   jclass version, lists, infos, formats, caps;
   jfieldID sdk;
   jmethodID construct, get_infos, is_encoder, is_alias, is_hw, is_sw, get_name,
         get_caps;
   jmethodID make_format, set_integer, supports;
   jobject list, format;
   jobjectArray codecs;
   jstring media, key;
   jsize i, count;
   int result = 0, inner = 0;
   if (name && capacity)
      name[0] = 0;
   if (!env || !name || capacity < 2 || capacity > 4096 || !mime || profile <= 0
         || (encoder != 0 && encoder != 1) || !width || !height || width > 16384
         || height > 16384)
      return 0;
   if (strcmp(mime, "video/avc") && strcmp(mime, "video/hevc")
         && strcmp(mime, "video/x-vnd.on2.vp9") && strcmp(mime, "video/av01"))
      return 0;
   if ((*env)->ExceptionCheck(env))
      return -1;
   if ((*env)->PushLocalFrame(env, 32) < 0)
   {
      (*env)->ExceptionClear(env);
      return -1;
   }
   version = (*env)->FindClass(env, "android/os/Build$VERSION");
   MC_CHECK(version);
   sdk = (*env)->GetStaticFieldID(env, version, "SDK_INT", "I");
   MC_CHECK(sdk);
   i = (*env)->GetStaticIntField(env, version, sdk);
   if ((*env)->ExceptionCheck(env))
      goto failed;
   if (i < 29)
      goto done;
   lists = (*env)->FindClass(env, "android/media/MediaCodecList");
   MC_CHECK(lists);
   infos = (*env)->FindClass(env, "android/media/MediaCodecInfo");
   MC_CHECK(infos);
   formats = (*env)->FindClass(env, "android/media/MediaFormat");
   MC_CHECK(formats);
   caps = (*env)->FindClass(
         env, "android/media/MediaCodecInfo$CodecCapabilities");
   MC_CHECK(caps);
   construct = (*env)->GetMethodID(env, lists, "<init>", "(I)V");
   MC_CHECK(construct);
   get_infos = (*env)->GetMethodID(
         env, lists, "getCodecInfos", "()[Landroid/media/MediaCodecInfo;");
   MC_CHECK(get_infos);
   is_encoder = (*env)->GetMethodID(env, infos, "isEncoder", "()Z");
   MC_CHECK(is_encoder);
   is_alias = (*env)->GetMethodID(env, infos, "isAlias", "()Z");
   MC_CHECK(is_alias);
   is_hw = (*env)->GetMethodID(env, infos, "isHardwareAccelerated", "()Z");
   MC_CHECK(is_hw);
   is_sw = (*env)->GetMethodID(env, infos, "isSoftwareOnly", "()Z");
   MC_CHECK(is_sw);
   get_name
         = (*env)->GetMethodID(env, infos, "getName", "()Ljava/lang/String;");
   MC_CHECK(get_name);
   get_caps = (*env)->GetMethodID(env, infos, "getCapabilitiesForType",
         "(Ljava/lang/String;)Landroid/media/"
         "MediaCodecInfo$CodecCapabilities;");
   MC_CHECK(get_caps);
   make_format = (*env)->GetStaticMethodID(env, formats, "createVideoFormat",
         "(Ljava/lang/String;II)Landroid/media/MediaFormat;");
   MC_CHECK(make_format);
   set_integer = (*env)->GetMethodID(
         env, formats, "setInteger", "(Ljava/lang/String;I)V");
   MC_CHECK(set_integer);
   supports = (*env)->GetMethodID(
         env, caps, "isFormatSupported", "(Landroid/media/MediaFormat;)Z");
   MC_CHECK(supports);
   list = (*env)->NewObject(env, lists, construct, 1);
   MC_CHECK(list); /* ALL_CODECS */
   codecs = (jobjectArray)(*env)->CallObjectMethod(env, list, get_infos);
   MC_CHECK(codecs);
   count = (*env)->GetArrayLength(env, codecs);
   if ((*env)->ExceptionCheck(env) || count < 0 || count > 4096)
      goto failed;
   media = (*env)->NewStringUTF(env, mime);
   MC_CHECK(media);
   format = (*env)->CallStaticObjectMethod(
         env, formats, make_format, media, (jint)width, (jint)height);
   MC_CHECK(format);
   key = (*env)->NewStringUTF(env, "profile");
   MC_CHECK(key);
   MC_VOID(
         (*env)->CallVoidMethod(env, format, set_integer, key, (jint)profile));
   if (request)
   {
      jfieldID surface;
      jmethodID set_float;
      jint color;
      surface = (*env)->GetStaticFieldID(env, caps, "COLOR_FormatSurface", "I");
      MC_CHECK(surface);
      color = (*env)->GetStaticIntField(env, caps, surface);
      if ((*env)->ExceptionCheck(env) || color <= 0)
         goto failed;
      set_float = (*env)->GetMethodID(
            env, formats, "setFloat", "(Ljava/lang/String;F)V");
      MC_CHECK(set_float);
      key = (*env)->NewStringUTF(env, "color-format");
      MC_CHECK(key);
      MC_VOID((*env)->CallVoidMethod(env, format, set_integer, key, color));
      key = (*env)->NewStringUTF(env, "bitrate");
      MC_CHECK(key);
      MC_VOID((*env)->CallVoidMethod(
            env, format, set_integer, key, (jint)request->bitrate));
      {
         static const char *const keys[]
               = {"level", "color-standard", "color-transfer", "color-range"};
         jint values[4];
         unsigned at;
         values[0] = (jint)request->level;
         values[1] = request->color ? 6 : 1;
         values[2] = request->color ? 6 : 3;
         values[3] = (jint)request->range;
         for (at = 0; at < 4; at++)
         {
            key = (*env)->NewStringUTF(env, keys[at]);
            MC_CHECK(key);
            MC_VOID((*env)->CallVoidMethod(
                  env, format, set_integer, key, values[at]));
         }
         if (request->color)
         {
            jclass buffers = (*env)->FindClass(env, "java/nio/ByteBuffer");
            jmethodID wrap, set_buffer;
            jbyteArray bytes;
            jobject buffer;
            MC_CHECK(buffers);
            wrap = (*env)->GetStaticMethodID(
                  env, buffers, "wrap", "([B)Ljava/nio/ByteBuffer;");
            MC_CHECK(wrap);
            set_buffer = (*env)->GetMethodID(env, formats, "setByteBuffer",
                  "(Ljava/lang/String;Ljava/nio/ByteBuffer;)V");
            MC_CHECK(set_buffer);
            bytes = (*env)->NewByteArray(env, 25);
            MC_CHECK(bytes);
            MC_VOID((*env)->SetByteArrayRegion(
                  env, bytes, 0, 25, (const jbyte *)request->hdr_static));
            buffer = (*env)->CallStaticObjectMethod(env, buffers, wrap, bytes);
            MC_CHECK(buffer);
            key = (*env)->NewStringUTF(env, "hdr-static-info");
            MC_CHECK(key);
            MC_VOID(
                  (*env)->CallVoidMethod(env, format, set_buffer, key, buffer));
         }
      }
      key = (*env)->NewStringUTF(env, "frame-rate");
      MC_CHECK(key);
      MC_VOID((*env)->CallVoidMethod(
            env, format, set_float, key, (jfloat)request->frame_rate));
   }
   for (i = 0; i < count; i++)
   {
      jobject info, capability;
      jstring codec_name;
      const char *utf;
      jsize length;
      jboolean value;
      if ((*env)->PushLocalFrame(env, 8) < 0)
         goto failed;
      inner = 1;
      info = (*env)->GetObjectArrayElement(env, codecs, i);
      MC_CHECK(info);
      value = (*env)->CallBooleanMethod(env, info, is_encoder);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (!!value != encoder)
         goto next;
      value = (*env)->CallBooleanMethod(env, info, is_alias);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (value)
         goto next;
      value = (*env)->CallBooleanMethod(env, info, is_hw);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (!value)
         goto next;
      value = (*env)->CallBooleanMethod(env, info, is_sw);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (value)
         goto next;
      capability = (*env)->CallObjectMethod(env, info, get_caps, media);
      if ((*env)->ExceptionCheck(env))
      {
         jthrowable exception = (*env)->ExceptionOccurred(env);
         jclass unsupported;
         (*env)->ExceptionClear(env);
         unsupported
               = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
         MC_CHECK(unsupported);
         /* Android throws this for an unsupported MIME on an otherwise
          * valid codec. Do not convert allocation/vendor failures to BUSY. */
         if (!exception || !(*env)->IsInstanceOf(env, exception, unsupported))
            goto failed;
         goto next;
      }
      MC_CHECK(capability);
      value = (*env)->CallBooleanMethod(env, capability, supports, format);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (!value)
         goto next;
      codec_name = (jstring)(*env)->CallObjectMethod(env, info, get_name);
      MC_CHECK(codec_name);
      length = (*env)->GetStringUTFLength(env, codec_name);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (length <= 0 || (size_t)length >= capacity)
         goto next;
      utf = (*env)->GetStringUTFChars(env, codec_name, NULL);
      MC_CHECK(utf);
      /* Codec component names are ASCII; refuse modified-UTF8/NUL tricks. */
      {
         jsize n;
         int ascii = 1;
         for (n = 0; n < length; n++)
            if ((unsigned char)utf[n] < 33 || (unsigned char)utf[n] > 126)
               ascii = 0;
         if (ascii)
         {
            memcpy(name, utf, (size_t)length);
            name[length] = 0;
            result = 1;
         }
      }
      (*env)->ReleaseStringUTFChars(env, codec_name, utf);
      if ((*env)->ExceptionCheck(env))
         goto failed;
      if (result)
         goto done;
   next:
      (*env)->PopLocalFrame(env, NULL);
      inner = 0;
   }
   goto done;
failed:
   (*env)->ExceptionClear(env);
   result = -1;
   name[0] = 0;
done:
   if (inner)
      (*env)->PopLocalFrame(env, NULL);
   (*env)->PopLocalFrame(env, NULL);
   return result;
}
#undef MC_CHECK
#undef MC_VOID

int mediacodec_inventory_select(JNIEnv *env, const char *mime, int encoder,
      unsigned width, unsigned height, int profile, char *name, size_t capacity)
{
   return mediacodec_inventory_select_internal(
         env, mime, encoder, width, height, profile, name, capacity, NULL);
}

int mediacodec_inventory_select_encoder(JNIEnv *env,
      const mediacodec_encoder_request_t *request, char *name, size_t capacity)
{
   if (name && capacity)
      name[0] = 0;
   if (!mediacodec_encoder_color_valid(request) || !request->bitrate
         || request->bitrate > INT_MAX
         || !(request->frame_rate > 0 && request->frame_rate <= FLT_MAX))
      return 0;
   return mediacodec_inventory_select_internal(env, request->mime, 1,
         request->width, request->height, request->profile, name, capacity,
         request);
}
