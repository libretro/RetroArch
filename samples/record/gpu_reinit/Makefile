ROOT := ../../..
TARGET := gpu_reinit_test
TARGET_NOTHREADS := gpu_reinit_test_nothreads
CFLAGS += -O2 -g1 -std=c89 -pedantic -Wall -Wextra -Werror \
          -Wno-long-long -Wno-unused-parameter -Wframe-larger-than=2048 \
          -I$(ROOT) -I$(ROOT)/menu -I$(ROOT)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
all: $(TARGET) $(TARGET_NOTHREADS)
$(TARGET): gpu_reinit_test.c $(ROOT)/record/record_gpu_reinit.c $(ROOT)/record/record_driver.h
	$(CC) $(CFLAGS) -DHAVE_THREADS $< -o $@ $(LDFLAGS) -lm
$(TARGET_NOTHREADS): gpu_reinit_test.c $(ROOT)/record/record_gpu_reinit.c $(ROOT)/record/record_driver.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lm
check: all
	./$(TARGET)
	./$(TARGET_NOTHREADS)
clean:
	rm -f $(TARGET) $(TARGET).exe $(TARGET_NOTHREADS) $(TARGET_NOTHREADS).exe
.PHONY: all check clean
