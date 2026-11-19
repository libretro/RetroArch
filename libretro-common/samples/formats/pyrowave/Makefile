include ../../sample_objdir.mk

TARGET := rpyrowave_test$(EXE)
STREAM_TARGET := rpyrowave_stream_test$(EXE)
TCP_TARGET := rpyrowave_stream_tcp_test$(EXE)
COLOR_TARGET := rpyrowave_gl_color_test$(EXE)
COMMON := ../../..
SOURCES := rpyrowave_test.c $(COMMON)/formats/pyrowave/rpyrowave.c \
           $(COMMON)/formats/pyrowave/rpyrowave_image.c \
           $(COMMON)/formats/pyrowave/rpyrowave_gpu.c
OBJS := $(call sample_objs,$(SOURCES:.c=.o))
STREAM_SOURCES := rpyrowave_stream_test.c $(COMMON)/formats/pyrowave/rpyrowave_stream.c \
                  $(COMMON)/formats/pyrowave/rpyrowave.c $(COMMON)/formats/pyrowave/rpyrowave_image.c
STREAM_OBJS := $(call sample_objs,$(STREAM_SOURCES:.c=.o))
SOCKET_OBJ := $(call sample_objs,$(COMMON)/net/rpyrowave_socket.o)
CFLAGS += -O2 -Wall -Wextra -Werror -pedantic -std=c89 -I$(COMMON)/include
LDLIBS += -lm
ifeq ($(EXE),.exe)
SOCKET_LIBS := -lws2_32
COLOR_LIBS := -lopengl32 -lgdi32 -luser32
COLOR_FLAGS := -I$(COMMON)/../gfx/include
else
COLOR_LIBS := -lEGL -lGL
endif

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(STREAM_TARGET) $(TCP_TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(STREAM_TARGET): $(STREAM_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

rpyrowave_stream_tcp_test.o: rpyrowave_stream_test.c
	$(CC) $(CFLAGS) -DRPYROWAVE_STREAM_SOCKET_TEST -c -o $@ $<

$(TCP_TARGET): rpyrowave_stream_tcp_test.o $(filter-out rpyrowave_stream_test.o,$(STREAM_OBJS)) $(SOCKET_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) $(SOCKET_LIBS)

$(COLOR_TARGET): gpu_gl_color.c gpu_gl_context.h gpu_vectors.h gpu_color_math.h \
      $(filter-out rpyrowave_test.c,$(SOURCES))
	$(CC) $(CFLAGS) $(COLOR_FLAGS) gpu_gl_color.c $(filter-out rpyrowave_test.c,$(SOURCES)) \
		$(LDFLAGS) $(LDLIBS) $(COLOR_LIBS) -o $@

color-build: $(COLOR_TARGET)

color-check: $(COLOR_TARGET)
	@test -n "$(COLOR_SHADER)" || (echo "Set COLOR_SHADER to the generated color compute shader"; exit 1)
	$(RUNNER) ./$(COLOR_TARGET) "$(COLOR_SHADER)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET) $(STREAM_TARGET) $(TCP_TARGET)
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(STREAM_TARGET)
	$(RUNNER) ./$(TCP_TARGET)

stream-check: $(STREAM_TARGET) $(TCP_TARGET)
	$(RUNNER) ./$(STREAM_TARGET)
	$(RUNNER) ./$(TCP_TARGET)

reference-check: $(TARGET)
	@test -n "$(REFERENCE_DIR)" || (echo "Set REFERENCE_DIR to external oracle captures"; exit 1)
	$(RUNNER) ./$(TARGET) --reference "$(REFERENCE_DIR)"

clean:
	rm -f rpyrowave_gl_color_test rpyrowave_gl_color_test.exe
	rm -f $(TARGET) $(STREAM_TARGET) $(TCP_TARGET) rpyrowave_test.o rpyrowave_stream_test.o rpyrowave_stream_tcp_test.o
	rm -f rpyrowave_test.exe rpyrowave_stream_test.exe rpyrowave_stream_tcp_test.exe
	rm -rf $(SAMPLE_OBJDIR)

.PHONY: all check stream-check reference-check color-build color-check clean
