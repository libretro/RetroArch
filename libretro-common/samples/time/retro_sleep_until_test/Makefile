include ../../sample_objdir.mk

CORE_DIR := ../../..
TARGET   := retro_sleep_until_test
SOURCES_C := retro_sleep_until_test.c \
	$(CORE_DIR)/time/rtime.c \
	$(CORE_DIR)/features/features_cpu.c \
	$(CORE_DIR)/string/stdstring.c \
	$(CORE_DIR)/compat/compat_strl.c \
	$(CORE_DIR)/encodings/encoding_utf.c
OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))
CFLAGS  += -Wall -I$(CORE_DIR)/include -D_GNU_SOURCE -O1 -g1
LDFLAGS += -lm -lpthread
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)
.PHONY: all clean

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

# features_cpu.c as OpenDingux GCW0's uClibc 0.9.33 builds it: a core
# that does not link librt has no clock_nanosleep() to resolve
uclibc-link-check:
	@mkdir -p $(SAMPLE_OBJDIR)
	$(CC) -c -o $(SAMPLE_OBJDIR)/features_cpu_uclibc.o \
		$(CORE_DIR)/features/features_cpu.c $(CFLAGS) \
		-D__UCLIBC__ -D__UCLIBC_MAJOR__=0
	@if nm -u $(SAMPLE_OBJDIR)/features_cpu_uclibc.o | grep -w clock_nanosleep; then \
		echo "features_cpu.c needs librt's clock_nanosleep under uClibc 0.9"; exit 1; fi
.PHONY: uclibc-link-check
