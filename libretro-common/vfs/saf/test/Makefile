# VfsImplementationSaf against an in-memory ExternalStorageProvider
# model and stub android.* classes (SafTest), then
# vfs_implementation_saf.c driving that class through a host JVM
# (jni_test).
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
else
JNI_PLATFORM := linux
JVM_LIBDIR   := $(JAVA_HOME)/lib/server
JVM_RUNDIR   := $(JVM_LIBDIR)
JNI_TEST     := build/jni_test
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


CFLAGS += -std=gnu99 -Wall -Wextra -Wno-unused-parameter -g1 \
	-DANDROID -DHAVE_SAF -I$(LIBRETRO_COMM_DIR)/include \
	-I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/$(JNI_PLATFORM)"

all: build/SafTest.class $(JNI_TEST)

build/SafTest.class: $(SOURCES)
	$(JAVAC) -Xlint:all -Werror -d build $(SOURCES)

$(JNI_TEST): $(C_SOURCES) $(LIBRETRO_COMM_DIR)/include/vfs/vfs_implementation_saf.h
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(C_SOURCES) -L"$(JVM_LIBDIR)" -ljvm

check: all
	$(JAVA) -cp build SafTest
	PATH="$(JVM_RUNDIR):$$PATH" LD_LIBRARY_PATH="$(JVM_RUNDIR)" ./$(JNI_TEST)

clean:
	rm -rf build

.PHONY: all check clean
