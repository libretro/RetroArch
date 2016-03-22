LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

RARCH_DIR := ../../../..

HAVE_NEON   := 1
HAVE_LOGGER := 0
HAVE_VULKAN := 1

INCFLAGS    :=
DEFINES     :=

include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH),arm)
	DEFINES += -DANDROID_ARM  -marm
   LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
   DEFINES += -DANDROID_X86 -DHAVE_SSSE3
endif

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

DEFINES  += -DRARCH_MOBILE -DHAVE_GRIFFIN -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_FBO -DHAVE_OVERLAY -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DGLSL_DEBUG -DHAVE_DYLIB -DHAVE_GLSL -DHAVE_MENU -DHAVE_RGUI -DHAVE_ZLIB -DHAVE_RPNG -DINLINE=inline -DHAVE_THREADS -D__LIBRETRO__ -DHAVE_RSOUND -DHAVE_NETPLAY -DHAVE_NETWORKING -DRARCH_INTERNAL -DHAVE_FILTERS_BUILTIN -DHAVE_MATERIALUI -DHAVE_XMB -std=gnu99 -DHAVE_LIBRETRODB -DHAVE_STB_FONT
DEFINES += -DWANT_IFADDRS

ifeq ($(HAVE_VULKAN),1)
DEFINES += -DHAVE_VULKAN
endif

DEFINES += -DHAVE_7ZIP
DEFINES += -DHAVE_CHEEVOS
DEFINES += -DDEBUG_ANDROID
DEFINES += -DHAVE_SL

LOCAL_CFLAGS += -Wall -std=gnu99 -pthread -Wno-unused-function -fno-stack-protector -funroll-loops $(DEFINES)
LOCAL_CPPFLAGS := -std=gnu++11 $(DEFINES)

ifeq ($(NDK_DEBUG),1)
LOCAL_CFLAGS += -O0 -g
else
LOCAL_CFLAGS += -O2 -DNDEBUG
endif

LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL $(GLES_LIB) $(LOGGER_LDLIBS) -ldl
LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include/

ifeq ($(HAVE_VULKAN),1)
INCFLAGS         += $(LOCAL_PATH)/$(RARCH_DIR)/gfx/include

LOCAL_C_INCLUDES += $(INCFLAGS)
LOCAL_CPPFLAGS   += -I$(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang \
						  -I$(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/glslang/Public \
						  -I$(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/glslang/MachineIndependent \
						  -I$(LOCAL_PATH)/$(RARCH_DIR)/deps/glslang/glslang/SPIRV
LOCAL_SRC_FILES += $(RARCH_DIR)/deps/glslang/glslang.cpp \
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
endif

LOCAL_LDLIBS += -lOpenSLES -lz

include $(BUILD_SHARED_LIBRARY)

