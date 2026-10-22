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
ifeq ($(OS),Windows_NT)
CFLAGS += -DANDROID_IMAGE_QUEUE_TEST_NO_FENCES
endif
THREAD_CFLAGS := -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L -O1 -g1 -I../../../libretro-common/include
ifneq ($(SANITIZER),)
THREAD_CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
all: test ndk_test thread_test
test: test.c ../../../gfx/common/android_image_queue.c ../../../gfx/common/android_image_queue.h
	$(CC) $(CFLAGS) test.c ../../../gfx/common/android_image_queue.c -o $@ $(LDFLAGS)
ndk_test: ndk_test.c android_image_ndk_test_types.h ../../../gfx/common/android_image_ndk.c ../../../gfx/common/android_image_queue.c
	$(CC) $(CFLAGS) -DHAVE_DYLIB -DANDROID_IMAGE_NDK_TEST -I. ndk_test.c ../../../gfx/common/android_image_ndk.c ../../../gfx/common/android_image_queue.c -o $@ $(LDFLAGS)
rthreads.o: ../../../libretro-common/rthreads/rthreads.c
	$(CC) $(THREAD_CFLAGS) -c $< -o $@
thread_test: thread_test.c test.c ../../../gfx/common/android_image_queue.c rthreads.o
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -pthread thread_test.c ../../../gfx/common/android_image_queue.c rthreads.o -o $@ $(LDFLAGS)
check: all
	./test
	./ndk_test
	./thread_test
clean:
	rm -f test ndk_test thread_test rthreads.o
.PHONY: all check clean
