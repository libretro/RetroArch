TARGET := progress_bounds_test

# The bundled rcheevos runtime's progress reader against blobs held in
# buffers of exactly their size: run under ASan, a read past one fails.
#    make && make check
REPO_ROOT := ../../..
RC        := $(REPO_ROOT)/deps/rcheevos
LRC       := $(REPO_ROOT)/libretro-common

CFLAGS += -Wall -std=gnu99 -g1 -O1 -I$(RC)/include -I$(LRC)/include -DRC_DISABLE_LUA

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring undefined,$(SANITIZER)),)
   CFLAGS  += -fno-sanitize-recover=undefined
   LDFLAGS += -fno-sanitize-recover=undefined
endif

SOURCES := progress_bounds_test.c \
   $(wildcard $(RC)/src/rcheevos/*.c) \
   $(RC)/src/rc_util.c \
   $(RC)/src/rc_compat.c \
   $(LRC)/utils/md5.c

OWNER_TARGET := client_owner_queue_test$(if $(findstring mingw,$(CC)),.exe,)
OWNER_SOURCES := client_owner_queue_test.c api_json_test.c $(filter-out progress_bounds_test.c,$(SOURCES)) \
   $(RC)/src/rc_client.c $(wildcard $(RC)/src/rapi/*.c) \
   $(LRC)/formats/json/rjson.c $(LRC)/string/rstrtod.c \
   $(LRC)/rthreads/rthreads.c $(LRC)/rthreads/retro_eventcount.c
OWNER_LIBS := -lpthread

all: $(TARGET) $(OWNER_TARGET)

$(OWNER_TARGET): $(OWNER_SOURCES) ../no_client_locks.h
	$(CC) $(CFLAGS) -include ../no_client_locks.h -DHAVE_THREADS -o $@ $(OWNER_SOURCES) $(LDFLAGS) $(OWNER_LIBS) -lm

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -lm

check: $(TARGET) $(OWNER_TARGET)
	./$(TARGET)
	./$(OWNER_TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe
	rm -f $(OWNER_TARGET)

.PHONY: all check clean
