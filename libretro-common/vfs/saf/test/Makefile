# VfsImplementationSaf against an in-memory ExternalStorageProvider
# model and stub android.* classes (SafTest), then
# vfs_implementation_saf.c driving that class through a host JVM
# (jni_test), then on Linux the VFS copy with SAF ends over real
# descriptors (jni_copy_test), which must go through copy_file_range.
# Needs a JDK only; JAVA_HOME is found from javac if unset.
# Linux and Windows (MSYS2 MINGW64, JAVA_HOME set) alike: make check

JAVAC ?= javac
JAVA  ?= java
CC    ?= cc

ifeq ($(JAVA_HOME),)
JAVA_HOME := $(shell dirname "$$(dirname "$$(readlink -f "$$(command -v $(JAVAC))")")")
endif

ifeq ($(OS),Windows_NT)
JNI_PLATFORM := win32
JVM_LIBDIR   := $(JAVA_HOME)/lib
JVM_RUNDIR   := $(JAVA_HOME)/bin/server
JNI_TEST     := build/jni_test.exe
JNI_COPY_TEST :=
else
JNI_PLATFORM := linux
JVM_LIBDIR   := $(JAVA_HOME)/lib/server
JVM_RUNDIR   := $(JVM_LIBDIR)
JNI_TEST     := build/jni_test
JNI_COPY_TEST := build/jni_copy_test
endif

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	../src/com/libretro/common/vfs/VfsImplementationSaf.java \
	stubs/android/content/ContentResolver.java \
	stubs/android/database/Cursor.java \
	stubs/android/net/Uri.java \
	stubs/android/os/Build.java \
	stubs/android/os/ParcelFileDescriptor.java \
	stubs/android/provider/DocumentsContract.java \
	FakeProvider.java \
	FakeActivity.java \
	SafTest.java

C_SOURCES := jni_test.c $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation_saf.c

COPY_SOURCES := jni_copy_test.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation_saf.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS += -std=gnu99 -Wall -Wextra -Wno-unused-parameter -g1 \
	-DANDROID -DHAVE_SAF -I$(LIBRETRO_COMM_DIR)/include \
	-I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/$(JNI_PLATFORM)"

all: build/SafTest.class $(JNI_TEST) $(JNI_COPY_TEST)

build/SafTest.class: $(SOURCES)
	$(JAVAC) -Xlint:all -Werror -d build $(SOURCES)

$(JNI_TEST): $(C_SOURCES) $(LIBRETRO_COMM_DIR)/include/vfs/vfs_implementation_saf.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(C_SOURCES) -L"$(JVM_LIBDIR)" -ljvm

$(JNI_COPY_TEST): $(COPY_SOURCES) $(LIBRETRO_COMM_DIR)/include/vfs/vfs_implementation_saf.h
	@mkdir -p build
	$(CC) $(CFLAGS) -DVFS_COPY_DEBUG -o $@ $(COPY_SOURCES) -L"$(JVM_LIBDIR)" -ljvm

check: all
	$(JAVA) -cp build SafTest
	PATH="$(JVM_RUNDIR):$$PATH" LD_LIBRARY_PATH="$(JVM_RUNDIR)" ./$(JNI_TEST)
ifneq ($(JNI_COPY_TEST),)
	rm -rf build/backing && mkdir -p build/backing
	LD_LIBRARY_PATH="$(JVM_RUNDIR)" ./$(JNI_COPY_TEST) 2>build/copy.log
	@grep -q "copy_file_range: want" build/copy.log || { echo "[FAILED] SAF copies did not take the kernel path"; exit 1; }
	@! grep -q "portable:" build/copy.log || { echo "[FAILED] a SAF copy fell back to the portable path"; exit 1; }
	@echo "[SUCCESS] SAF copies took the kernel path"
endif

clean:
	rm -rf build

.PHONY: all check clean
