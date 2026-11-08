include ../../sample_objdir.mk
TARGET := rwma_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := rwma_test.c $(LIBRETRO_COMM_DIR)/formats/wma/rwma_pro.c $(LIBRETRO_COMM_DIR)/formats/wma/rwma_standard.c
OBJS := $(SAMPLE_OBJDIR)/rwma_test.o $(call sample_objs,$(filter-out rwma_test.o,$(SOURCES:.c=.o)))
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -Wno-unused-function -O2 -I$(LIBRETRO_COMM_DIR)/include $(EXTRA_CFLAGS)
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(OBJS): CFLAGS += -Dcalloc=rwma_test_calloc -Dfree=rwma_test_free
$(SAMPLE_OBJDIR)/rwma_test.o: CFLAGS := $(filter-out -Dcalloc=rwma_test_calloc -Dfree=rwma_test_free,$(CFLAGS))
$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm
reference: rwma_reference
rwma_reference: rwma_reference.c $(LIBRETRO_COMM_DIR)/formats/wma/rwma_pro.c $(LIBRETRO_COMM_DIR)/formats/wma/rwma_standard.c
	@mkdir -p $(SAMPLE_OBJDIR)/reference
	$(CC) $(CFLAGS) -c $(LIBRETRO_COMM_DIR)/formats/wma/rwma_pro.c -o $(SAMPLE_OBJDIR)/reference/pro.o
	$(CC) $(CFLAGS) -c $(LIBRETRO_COMM_DIR)/formats/wma/rwma_standard.c -o $(SAMPLE_OBJDIR)/reference/standard.o
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 -I$(LIBRETRO_COMM_DIR)/include $$(pkg-config --cflags libavcodec libavformat libavutil) -c rwma_reference.c -o $(SAMPLE_OBJDIR)/reference/oracle.o
	$(CC) -o $@ $(SAMPLE_OBJDIR)/reference/*.o $(LDFLAGS) $$(pkg-config --libs libavcodec libavformat libavutil) -lm
clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)
.PHONY: all clean reference
