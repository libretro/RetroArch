LOCAL_PATH := $(call my-dir)

RARCH_DIR := ../../../..

HAVE_NEON   := 1
HAVE_LOGGER := 0
HAVE_VULKAN := 1

INCFLAGS    :=
DEFINES     :=

ifeq ($(TARGET_ARCH),arm)
   DEFINES += -DANDROID_ARM -marm
endif

ifeq ($(TARGET_ARCH),x86)
   DEFINES += -DANDROID_X86 -DHAVE_SSSE3
endif

DEFINES += -DRARCH_MOBILE -DHAVE_GRIFFIN -DANDROID -DHAVE_DYNAMIC \
			  -DHAVE_OPENGL -DHAVE_FBO -DHAVE_OVERLAY -DHAVE_OPENGLES \
			  -DHAVE_OPENGLES2 -DGLSL_DEBUG -DHAVE_DYLIB \
			  -DHAVE_EGL -DHAVE_GLSL -DHAVE_MENU -DHAVE_RGUI \
			  -DHAVE_ZLIB -DHAVE_RPNG -DINLINE=inline -DHAVE_THREADS \
			  -D__LIBRETRO__ -DHAVE_RSOUND -DHAVE_NETPLAY \
			  -DHAVE_NETWORKING -DRARCH_INTERNAL -DHAVE_FILTERS_BUILTIN \
			  -DHAVE_MATERIALUI -DHAVE_XMB -DHAVE_LIBRETRODB -DHAVE_STB_FONT
DEFINES += -DWANT_IFADDRS

ifeq ($(HAVE_VULKAN),1)
   DEFINES += -DHAVE_VULKAN
endif
DEFINES += -DHAVE_7ZIP
DEFINES += -DHAVE_CHEEVOS
DEFINES += -DHAVE_SL

# glslang
include $(CLEAR_VARS)
LOCAL_MODULE := glslang
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := $(RARCH_DIR)/deps/glslang/glslang.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/SPIRV/SpvBuilder.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/SPIRV/SPVRemapper.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/SPIRV/InReadableOrder.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/SPIRV/doc.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/SPIRV/GlslangToSpv.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/SPIRV/disassemble.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/OGLCompilersDLL/InitializeDll.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/GenericCodeGen/Link.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/GenericCodeGen/CodeGen.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/Intermediate.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/glslang_tab.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/Versions.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/RemoveTree.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/limits.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/intermOut.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/Initialize.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/SymbolTable.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/parseConst.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/ParseHelper.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/ShaderLang.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/IntermTraverse.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/InfoSink.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/Constant.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/Scan.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/reflection.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/linkValidate.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/PoolAlloc.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpMemory.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpSymbols.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent/preprocessor/Pp.cpp \
						 $(RARCH_DIR)/deps/glslang/glslang/glslang/OSDependent/Unix/ossource.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang \
						  $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/glslang/Public \
						  $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent \
						  $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/SPIRV \
                    $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include

# Permissive works around weird header issues in LLVM on x86.
LOCAL_CPPFLAGS := -std=gnu++11 -pthread -Wno-reorder -Wno-sign-compare -fpermissive $(DEFINES)
include $(BUILD_STATIC_LIBRARY)
#####

# retroarch-activity (C++ side)
include $(CLEAR_VARS)
LOCAL_MODULE := retroarch-activity-cpp
LOCAL_SRC_FILES += $(RARCH_DIR)/griffin/griffin_cpp.cpp
LOCAL_ARM_MODE := arm
LOCAL_CPPFLAGS := -std=gnu++11 -pthread $(DEFINES)
LOCAL_CPP_FEATURES += exceptions

ifeq ($(HAVE_VULKAN), 1)
   LOCAL_STATIC_LIBRARIES := glslang
endif

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include

ifeq ($(HAVE_VULKAN),1)
   LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(RARCH_DIR)/gfx/include \
                       $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang \
                       $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/glslang/Public \
                       $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent \
                       $(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/SPIRV \
                       $(LOCAL_PATH)/$(RARCH_DIR)/deps/spir2cross
endif

ifneq ($(SANITIZER),)
   LOCAL_CFLAGS   += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_CPPFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_LDFLAGS  += -fsanitize=$(SANITIZER)
endif

include $(BUILD_STATIC_LIBRARY)
#######

# retroarch-activity
include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
ifeq ($(HAVE_NEON),1)
   DEFINES += -D__ARM_NEON__
   LOCAL_SRC_FILES += $(RARCH_DIR)/audio/audio_utils_neon.S.neon
   LOCAL_SRC_FILES += $(RARCH_DIR)/audio/drivers_resampler/sinc_resampler_neon.S.neon
   LOCAL_SRC_FILES += $(RARCH_DIR)/audio/drivers_resampler/cc_resampler_neon.S.neon
endif
DEFINES += -DSINC_LOWER_QUALITY 
DEFINES += -DANDROID_ARM_V7
endif

ifeq ($(TARGET_ARCH),mips)
   DEFINES += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_MODULE := retroarch-activity
LOCAL_STATIC_LIBRARIES := retroarch-activity-cpp
LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES  +=	$(RARCH_DIR)/griffin/griffin.c

ifeq ($(HAVE_LOGGER), 1)
   DEFINES += -DHAVE_LOGGER
endif
LOGGER_LDLIBS := -llog

ifeq ($(GLES),3)
   GLES_LIB := -lGLESv3
   DEFINES += -DHAVE_OPENGLES3
else
   GLES_LIB := -lGLESv2
endif

LOCAL_CFLAGS   += -Wall -std=gnu99 -pthread -Wno-unused-function -fno-stack-protector -funroll-loops $(DEFINES)

# Let ndk-build set the optimization flags but remove -O3 like in cf3c3
LOCAL_CFLAGS := $(subst -O3,-O2,$(LOCAL_CFLAGS))

LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL $(GLES_LIB) $(LOGGER_LDLIBS) -ldl 
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include

ifeq ($(HAVE_VULKAN),1)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(RARCH_DIR)/gfx/include
endif

LOCAL_LDLIBS += -lOpenSLES -lz

ifneq ($(SANITIZER),)
   LOCAL_CFLAGS   += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_CPPFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_LDFLAGS  += -fsanitize=$(SANITIZER)
endif

include $(BUILD_SHARED_LIBRARY)

