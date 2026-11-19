# VfsImplementationSaf against an in-memory ExternalStorageProvider
# model and stub android.* classes. Needs a JDK (javac, java) only;
# runs the same on Linux and on Windows (MSYS2).

JAVAC ?= javac
JAVA  ?= java

SOURCES := \
	../src/com/libretro/common/vfs/VfsImplementationSaf.java \
	stubs/android/content/ContentResolver.java \
	stubs/android/database/Cursor.java \
	stubs/android/net/Uri.java \
	stubs/android/os/Build.java \
	stubs/android/os/ParcelFileDescriptor.java \
	stubs/android/provider/DocumentsContract.java \
	FakeProvider.java \
	SafTest.java

all: build/SafTest.class

build/SafTest.class: $(SOURCES)
	$(JAVAC) -Xlint:all -Werror -d build $(SOURCES)

check: build/SafTest.class
	$(JAVA) -cp build SafTest

clean:
	rm -rf build

.PHONY: all check clean
