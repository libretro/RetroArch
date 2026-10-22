CC ?= cc
CFLAGS += -DHAVE_THREADS -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# boolean.h uses the compiler _Bool spelling in C89 builds.
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: test ndk_test thread_test
test: test.c ../../../gfx/common/android_image_queue.c ../../../gfx/common/android_image_queue.h
	$(CC) $(CFLAGS) test.c ../../../gfx/common/android_image_queue.c -o $@ $(LDFLAGS)
ndk_test: ndk_test.c android_image_ndk_test_types.h ../../../gfx/common/android_image_ndk.c ../../../gfx/common/android_image_queue.c
	$(CC) $(CFLAGS) -DHAVE_DYLIB -DANDROID_IMAGE_NDK_TEST -I. ndk_test.c ../../../gfx/common/android_image_ndk.c ../../../gfx/common/android_image_queue.c -o $@ $(LDFLAGS)
thread_test: thread_test.c test.c ../../../gfx/common/android_image_queue.c
	$(CC) $(CFLAGS) -pthread thread_test.c ../../../gfx/common/android_image_queue.c -o $@ $(LDFLAGS)
check: all
	./test
	./ndk_test
	./thread_test
clean:
	rm -f test ndk_test thread_test
.PHONY: all check clean
