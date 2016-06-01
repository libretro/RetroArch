ifeq ($(GLES),3)
   ifndef NDK_GL_HEADER_VER
      APP_PLATFORM := android-18
   else
      APP_PLATFORM := $(NDK_GL_HEADER_VER)
   endif
else
   ifndef NDK_NO_GL_HEADER_VER
      APP_PLATFORM := android-9
   else
      APP_PLATFORM := $(NDK_NO_GL_HEADER_VER)
   endif
endif

ifndef TARGET_ABIS
   APP_ABI := armeabi-v7a mips x86
else
   APP_ABI := $(TARGET_ABIS)
endif

# Run $NDK/toolchains/llvm-3.6/prebuilt/linux-x86_64/bin/asan_device_setup once
# for each AVD you use, replacing llvm-3.6 with the latest you have and
# linux-x86_64 with your host ABI.
ifneq ($(SANITIZER),)
   # AddressSanitizer doesn't run on mips
   ifndef TARGET_ABIS
      APP_ABI   := armeabi-v7a x86
   else
      ifneq ($(findstring mips,$(TARGET_ABIS)),)
         $(error "AddressSanitizer does not support mips.")
      endif
   endif
   USE_CLANG := 1
endif

ifeq ($(USE_CLANG),1)
   NDK_TOOLCHAIN_VERSION := clang
   APP_CFLAGS   := -Wno-invalid-source-encoding
   APP_CPPFLAGS := -Wno-invalid-source-encoding
endif

APP_STL := c++_static
