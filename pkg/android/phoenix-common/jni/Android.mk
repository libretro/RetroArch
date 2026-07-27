LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

RARCH_DIR := ../../../..

HAVE_NEON   := 1
HAVE_LOGGER := 0
HAVE_VULKAN := 1
HAVE_CHEEVOS := 1
HAVE_FILE_LOGGER := 1
HAVE_GFX_WIDGETS := 1
HAVE_SAF := 1
HAVE_BUILTINSMBCLIENT := 1
HAVE_BUILTINNFSCLIENT := 1

INCFLAGS    :=
DEFINES     :=

LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common
DEPS_DIR          := $(RARCH_DIR)/deps

GIT_VERSION := $(shell git rev-parse --short HEAD 2>/dev/null)
ifneq ($(GIT_VERSION),)
   DEFINES += -DHAVE_GIT_VERSION -DGIT_VERSION=$(GIT_VERSION)
endif

include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH),arm)
   DEFINES += -DANDROID_ARM -marm
   LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
   DEFINES += -DANDROID_X86 -DHAVE_SSSE3
endif

ifeq ($(TARGET_ARCH),x86_64)
   DEFINES += -DANDROID_X64
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)

ifeq ($(HAVE_NEON),1)
	DEFINES += -D__ARM_NEON__ -DHAVE_NEON
endif
DEFINES += -DANDROID_ARM_V7
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
   DEFINES += -DANDROID_AARCH64
endif

ifeq ($(TARGET_ARCH),mips)
   DEFINES += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_MODULE := retroarch-activity

LOCAL_SRC_FILES  +=	$(RARCH_DIR)/griffin/griffin.c \
							$(RARCH_DIR)/griffin/griffin_cpp.cpp

# libnfs cannot live inside griffin (symbol clashes); compile as separate TUs.
ifeq ($(HAVE_BUILTINNFSCLIENT),1)
LOCAL_SRC_FILES += \
   $(RARCH_DIR)/deps/libnfs/lib/init.c \
   $(RARCH_DIR)/deps/libnfs/lib/krb5-wrapper.c \
   $(RARCH_DIR)/deps/libnfs/lib/libnfs.c \
   $(RARCH_DIR)/deps/libnfs/lib/libnfs-sync.c \
   $(RARCH_DIR)/deps/libnfs/lib/libnfs-zdr.c \
   $(RARCH_DIR)/deps/libnfs/lib/multithreading.c \
   $(RARCH_DIR)/deps/libnfs/lib/nfs_v3.c \
   $(RARCH_DIR)/deps/libnfs/lib/nfs_v4.c \
   $(RARCH_DIR)/deps/libnfs/lib/pdu.c \
   $(RARCH_DIR)/deps/libnfs/lib/socket.c \
   $(RARCH_DIR)/deps/libnfs/mount/mount.c \
   $(RARCH_DIR)/deps/libnfs/mount/libnfs-raw-mount.c \
   $(RARCH_DIR)/deps/libnfs/nfs/nfs.c \
   $(RARCH_DIR)/deps/libnfs/nfs/nfsacl.c \
   $(RARCH_DIR)/deps/libnfs/nfs/libnfs-raw-nfs.c \
   $(RARCH_DIR)/deps/libnfs/nfs4/nfs4.c \
   $(RARCH_DIR)/deps/libnfs/nfs4/libnfs-raw-nfs4.c \
   $(RARCH_DIR)/deps/libnfs/nlm/nlm.c \
   $(RARCH_DIR)/deps/libnfs/nlm/libnfs-raw-nlm.c \
   $(RARCH_DIR)/deps/libnfs/nsm/nsm.c \
   $(RARCH_DIR)/deps/libnfs/nsm/libnfs-raw-nsm.c \
   $(RARCH_DIR)/deps/libnfs/portmap/portmap.c \
   $(RARCH_DIR)/deps/libnfs/portmap/libnfs-raw-portmap.c \
   $(RARCH_DIR)/deps/libnfs/rquota/rquota.c \
   $(RARCH_DIR)/deps/libnfs/rquota/libnfs-raw-rquota.c
endif

ifeq ($(HAVE_BUILTINSMBCLIENT),1)
   DEFINES += -DHAVE_BUILTINSMBCLIENT
   DEFINES += "-D_U_=__attribute__((unused))"
   DEFINES += -DHAVE_TIME_H -DHAVE_FCNTL_H -DHAVE_UNISTD_H
   DEFINES += -DHAVE_STDLIB_H -DSTDC_HEADERS
   DEFINES += -DHAVE_STRING_H
   DEFINES += -DHAVE_LINGER
   DEFINES += -DHAVE_SYS_UIO_H
   DEFINES += -DHAVE_POLL_H -DHAVE_NETDB_H
   DEFINES += -DHAVE_NETINET_TCP_H -DHAVE_NETINET_IN_H
   DEFINES += -DHAVE_SYS_SOCKET_H -DHAVE_ARPA_INET_H
   DEFINES += -DHAVE_SMBCLIENT
endif

ifeq ($(HAVE_BUILTINNFSCLIENT),1)
   DEFINES += -DHAVE_BUILTINNFSCLIENT -DHAVE_NFSCLIENT
   DEFINES += "-D_U_=__attribute__((unused))"
   DEFINES += -DHAVE_ARPA_INET_H -DHAVE_DLFCN_H -DHAVE_INTTYPES_H
   DEFINES += -DHAVE_MEMORY_H -DHAVE_NETDB_H -DHAVE_NETINET_IN_H
   DEFINES += -DHAVE_NETINET_TCP_H -DHAVE_NET_IF_H -DHAVE_POLL_H
   DEFINES += -DHAVE_PWD_H -DHAVE_SOCKADDR_STORAGE -DHAVE_STDINT_H
   DEFINES += -DHAVE_STDLIB_H -DHAVE_STDATOMIC_H -DHAVE_STRINGS_H
   DEFINES += -DHAVE_STRING_H -DHAVE_SYS_IOCTL_H -DHAVE_SYS_STATVFS_H
   DEFINES += -DHAVE_SYS_STAT_H -DHAVE_SYS_TIME_H -DHAVE_SYS_TYPES_H
   DEFINES += -DHAVE_SYS_UIO_H -DHAVE_UNISTD_H -DHAVE_UTIME_H
   DEFINES += -DHAVE_SIGNAL_H -DHAVE_SYS_UTSNAME_H -DSTDC_HEADERS
   DEFINES += -DHAVE_CLOCK_GETTIME -DHAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
   # Bionic provides major()/minor() via sys/sysmacros.h (needed by nfs_v3/nfs_v4).
   DEFINES += -DHAVE_SYS_SYSMACROS_H
endif

ifeq ($(HAVE_LOGGER), 1)
   DEFINES += -DHAVE_LOGGER
endif
LOGGER_LDLIBS := -llog

ifeq ($(GLES),3)
   GLES_LIB := -lGLESv3
   DEFINES += -DHAVE_OPENGLES3
else
   GLES_LIB := -lGLESv2
   DEFINES += -DHAVE_OPENGLES2
endif

DEFINES += -DRARCH_MOBILE \
	   -DHAVE_GRIFFIN \
	   -DHAVE_RVORBIS \
	   -DHAVE_LANGEXTRA \
	   -DANDROID \
	   -DHAVE_DYNAMIC \
	   -DHAVE_OPENGL \
	   -DHAVE_OVERLAY \
	   -DHAVE_OPENGLES \
	   -DGLSL_DEBUG \
	   -DHAVE_DYLIB \
	   -DHAVE_EGL \
	   -DHAVE_GLSL \
	   -DHAVE_MENU \
	   -DHAVE_CONFIGFILE \
	   -DHAVE_PATCH \
	   -DHAVE_DSP_FILTER \
	   -DHAVE_VIDEO_FILTER \
	   -DHAVE_SCREENSHOTS \
	   -DHAVE_REWIND \
	   -DHAVE_CHEATS \
	   -DHAVE_BSV_MOVIE \
	   -DHAVE_RZSTD \
	   -DZSTD_DISABLE_ASM \
	   -DHAVE_CHEEVOS_RVZ \
	   -DHAVE_RPNG \
	   -DHAVE_RWEBP \
	   -DHAVE_RDDS \
	   -DHAVE_RJPEG \
	   -DHAVE_RBMP \
	   -DHAVE_RTGA \
	   -DINLINE=inline \
	   -DHAVE_THREADS \
	   -DHAVE_THREAD_STORAGE \
	   -D__LIBRETRO__ \
	   -DHAVE_RSOUND \
	   -DHAVE_NETWORKGAMEPAD \
	   -DHAVE_NETWORKING \
	   -DHAVE_NETWORK_CMD \
	   -DHAVE_COMMAND \
	   -DHAVE_CLOUDSYNC \
	   -DHAVE_IFINFO \
	   -DHAVE_NETPLAYDISCOVERY \
	   -DRARCH_INTERNAL \
	   -DHAVE_FILTERS_BUILTIN \
	   -DHAVE_RGUI \
	   -DHAVE_MATERIALUI \
	   -DHAVE_XMB \
	   -DHAVE_OZONE \
	   -DHAVE_SHADERPIPELINE \
	   -DHAVE_LIBRETRODB \
	   -DHAVE_STB_FONT \
	   -DHAVE_IMAGEVIEWER \
	   -DHAVE_ONLINE_UPDATER \
	   -DHAVE_UPDATE_ASSETS \
	   -DHAVE_UPDATE_CORES \
	   -DHAVE_UPDATE_CORE_INFO \
	   -DHAVE_CC_RESAMPLER \
	   -DHAVE_KEYMAPPER \
	   -DHAVE_NETWORKGAMEPAD \
	   -DHAVE_RFLAC \
	   -DHAVE_RMP3 \
	   -DHAVE_CHD \
	   -DWANT_SUBCODE \
	   -DWANT_RAW_DATA_SECTOR \
	   -DHAVE_RUNAHEAD \
	   -DHAVE_AUDIOMIXER \
	   -DHAVE_RWAV \
	   -DHAVE_ACCESSIBILITY \
	   -DHAVE_TRANSLATE \
	   -DWANT_IFADDRS \
	   -DHAVE_XDELTA \
	   -DHAVE_CORE_INFO_CACHE \
	   -DHAVE_BUILTINMBEDTLS -DHAVE_SSL

ifeq ($(HAVE_GFX_WIDGETS),1)
DEFINES += -DHAVE_GFX_WIDGETS
endif

ifeq ($(HAVE_VULKAN),1)
DEFINES += -DHAVE_VULKAN \
	   -DHAVE_SLANG \
	   -DHAVE_GLSLANG \
	   -DHAVE_BUILTINGLSLANG \
	   -DHAVE_SPIRV_CROSS \
	   -DWANT_GLSLANG \
	   -D__STDC_LIMIT_MACROS
endif
DEFINES += -DHAVE_7ZIP \
	   \
	   -DHAVE_SL

ifeq ($(HAVE_CHEEVOS),1)
DEFINES += -DHAVE_CHEEVOS \
	   -DRC_DISABLE_LUA
endif

ifeq ($(HAVE_SAF),1)
   DEFINES += -DHAVE_SAF
endif

ifeq ($(HAVE_BUILTINSMBCLIENT),1)
   DEFINES += -DHAVE_SMBCLIENT
endif

LOCAL_CFLAGS   += -Wall -std=gnu99 -pthread -Wno-unused-function -fno-stack-protector -funroll-loops $(DEFINES)
LOCAL_CPPFLAGS := -fexceptions -fpermissive -std=gnu++11 -fno-rtti -Wno-reorder $(DEFINES)

# Let ndk-build set the optimization flags but remove -O3 like in cf3c3
LOCAL_CFLAGS := $(subst -O3,-O2,$(LOCAL_CFLAGS))

LOCAL_LDLIBS	 := -landroid -lEGL $(GLES_LIB) $(LOGGER_LDLIBS) -ldl
LOCAL_C_INCLUDES := \
		    $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include \
		    $(LOCAL_PATH)/$(RARCH_DIR)/deps \
		    $(LOCAL_PATH)/$(RARCH_DIR)/deps/stb \
		    $(LOCAL_PATH)/$(RARCH_DIR)/deps/zstd/lib

INCLUDE_DIRS     := \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/stb/ \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/7zip/ \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/zstd/lib/

ifeq ($(HAVE_CHEEVOS),1)
INCLUDE_DIRS += -I$(LOCAL_PATH)/$(DEPS_DIR)/rcheevos/include
endif

ifeq ($(HAVE_BUILTINNFSCLIENT),1)
   # Prefer libnfs slist.h over libsmb2 when both are on the include path.
   INCLUDE_DIRS += \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/include \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/include/nfsc \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/mount \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/nfs \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/nfs4 \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/nlm \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/nsm \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/portmap \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libnfs/rquota
endif

ifeq ($(HAVE_BUILTINSMBCLIENT),1)
   INCLUDE_DIRS += \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libsmb2/include \
      -I$(LOCAL_PATH)/$(DEPS_DIR)/libsmb2/include/smb2
endif

LOCAL_CFLAGS     += $(INCLUDE_DIRS)
LOCAL_CPPFLAGS   += $(INCLUDE_DIRS)
LOCAL_CXXFLAGS   += $(INCLUDE_DIRS)

ifeq ($(HAVE_VULKAN),1)
INCFLAGS         += $(LOCAL_PATH)/$(RARCH_DIR)/gfx/include

LOCAL_C_INCLUDES += $(INCFLAGS)
LOCAL_CPPFLAGS   += -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/glslang/Public \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/glslang/MachineIndependent \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/SPIRV \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/SPIRV-Cross

LOCAL_CFLAGS    += -Wno-sign-compare -Wno-unused-variable -Wno-parentheses
LOCAL_SRC_FILES += $(RARCH_DIR)/griffin/griffin_glslang.cpp
endif

LOCAL_LDLIBS += -lOpenSLES

ifneq ($(SANITIZER),)
   LOCAL_CFLAGS   += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_CPPFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_LDFLAGS  += -fsanitize=$(SANITIZER)
endif

ifneq ($(PLAY_STORE_BUILD),1)
   ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
      LOCAL_LDFLAGS += -Wl,-z,max-page-size=4096
   endif

   ifeq ($(TARGET_ARCH_ABI),x86_64)
      LOCAL_LDFLAGS += -Wl,-z,max-page-size=4096
   endif
endif

include $(BUILD_SHARED_LIBRARY)
