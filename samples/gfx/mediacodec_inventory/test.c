/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/mediacodec_inventory.h"
int main(void)
{
   JavaVM *vm;
   JNIEnv *env;
   JavaVMInitArgs args;
   JavaVMOption options[2];
   jclass list, version;
   jfieldID scenario, sdk;
   char name[256];
   unsigned i;
   memset(&args, 0, sizeof(args));
   options[0].optionString = "-Djava.class.path=classes";
   options[1].optionString = "-Xcheck:jni";
   args.version = JNI_VERSION_1_8;
   args.nOptions = 2;
   args.options = options;
   assert(JNI_CreateJavaVM(&vm, (void **)&env, &args) == JNI_OK);
   list = (*env)->FindClass(env, "android/media/MediaCodecList");
   assert(list);
   version = (*env)->FindClass(env, "android/os/Build$VERSION");
   assert(version);
   scenario = (*env)->GetStaticFieldID(env, list, "scenario", "I");
   assert(scenario);
   sdk = (*env)->GetStaticFieldID(env, version, "SDK_INT", "I");
   assert(sdk);
   assert(mediacodec_inventory_select(
                env, "video/hevc", 0, 1920, 1080, 2, name, sizeof(name))
         == 1);
   assert(!strcmp(name, "c2.test.hevc"));
   (*env)->SetStaticIntField(env, version, sdk, 28);
   assert(mediacodec_inventory_select(
                env, "video/hevc", 0, 1920, 1080, 2, name, sizeof(name))
               == 0
         && !*name);
   (*env)->SetStaticIntField(env, version, sdk, 29);
   assert(mediacodec_inventory_select(
                env, "video/hevc", 0, 1920, 1080, 99, name, sizeof(name))
         == 0);
   for (i = 3; i <= 10; i++)
   {
      int expected = i == 4 || i == 7 ? 1 : i == 5 || i == 6 ? 0 : -1;
      (*env)->SetStaticIntField(env, list, scenario, (jint)i);
      assert(mediacodec_inventory_select(
                   env, "video/hevc", i == 7, 1920, 1080, 2, name, sizeof(name))
            == expected);
      assert(!(*env)->ExceptionCheck(env));
      if (expected != 1)
         assert(!*name);
   }
   {
      mediacodec_encoder_request_t request;
      memset(&request, 0, sizeof(request));
      request.mime = "video/hevc";
      request.width = 1920;
      request.height = 1080;
      request.profile = 2;
      request.level = 4096;
      request.range = 2;
      request.bitrate = 10000000;
      request.frame_rate = 60000.0 / 1001.0;
      (*env)->SetStaticIntField(env, list, scenario, 11);
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            == 1);
      assert(!strcmp(name, "c2.test.hevc"));
      assert(mediacodec_inventory_select(env, request.mime, 1, request.width,
                   request.height, request.profile, name, sizeof(name))
            == 0);
      request.frame_rate = 120;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.frame_rate = 30;
      request.bitrate = 100000;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.bitrate = 20000000;
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            == 1);
      request.bitrate = 0;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.bitrate = UINT_MAX;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.bitrate = 10000000;
      request.frame_rate = DBL_MAX;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.frame_rate = 0;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.frame_rate = -1;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.frame_rate = 30;
      (*env)->SetStaticIntField(env, list, scenario, 12);
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
                  == -1
            && !*name);
      assert(!(*env)->ExceptionCheck(env));
      assert(!mediacodec_inventory_select_encoder(env, NULL, name, sizeof(name))
            && !*name);
      (*env)->SetStaticIntField(env, list, scenario, 13);
      request.profile = 4096;
      request.color = 1;
      request.hdr_static_size = 25;
      request.hdr_static[17] = 0xe8;
      request.hdr_static[18] = 3;
      for (i = 0; i < 10000; i++)
         assert(mediacodec_inventory_select_encoder(
                      env, &request, name, sizeof(name))
               == 1);
      request.range = 1;
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            == 1);
      request.mime = "video/av01";
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            == 1);
      request.mime = "video/x-vnd.on2.vp9";
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            == 1);
      request.mime = "video/avc";
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.mime = "video/hevc";
      request.hdr_static_size = 24;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.hdr_static_size = 25;
      request.hdr_static[0] = 1;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.hdr_static[0] = 0;
      request.level = 0;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.level = 4096;
      request.range = 3;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.range = 2;
      request.profile = 2;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.profile = 4096;
      (*env)->SetStaticIntField(env, list, scenario, 14);
      assert(mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
                  == -1
            && !*name);
      assert(!(*env)->ExceptionCheck(env));
      (*env)->SetStaticIntField(env, list, scenario, 15);
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.color = 0;
      assert(!mediacodec_inventory_select_encoder(
                   env, &request, name, sizeof(name))
            && !*name);
      request.profile = 2;
      request.hdr_static_size = 0;
      (*env)->SetStaticIntField(env, list, scenario, 11);
      for (i = 0; i < 10000; i++)
         assert(mediacodec_inventory_select_encoder(
                      env, &request, name, sizeof(name)) == 1);
   }
   (*env)->SetStaticIntField(env, list, scenario, 0);
   for (i = 0; i < 10000; i++)
      assert(mediacodec_inventory_select(
                   env, "video/hevc", 0, 1920, 1080, 2, name, sizeof(name))
            == 1);
   assert(mediacodec_inventory_select(
                env, "video/hevc", 0, 1920, 1080, 2, name, 4)
               == 0
         && !*name);
   {
      jclass error = (*env)->FindClass(env, "java/lang/IllegalStateException");
      assert(error);
      assert((*env)->ThrowNew(env, error, "preexisting") == JNI_OK);
      assert(mediacodec_inventory_select(
                   env, "video/hevc", 0, 1920, 1080, 2, name, sizeof(name))
            == -1);
      assert((*env)->ExceptionCheck(env));
      (*env)->ExceptionClear(env);
      (*env)->DeleteLocalRef(env, error);
   }
   assert((*vm)->DestroyJavaVM(vm) == JNI_OK);
   puts("PASS: real JNI decoder and Surface encoder format admission, "
        "rate/bitrate refusal, aliases, failures and 30000 bounded "
        "local-frame iterations");
   return 0;
}
