/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <assert.h>
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
   puts("PASS: real JNI hardware/profile admission, aliases, failures and "
        "10000 bounded local-frame iterations");
   return 0;
}
