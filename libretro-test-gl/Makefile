
ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

ifeq ($(platform), unix)
   TARGET := libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
   GL_LIB := -lGL
else ifeq ($(platform), osx)
   TARGET := libretro.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
   GL_LIB := -framework OpenGL
else
   CC = gcc
   TARGET := retro.dll
   SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=link.T -Wl,--no-undefined
   GL_LIB := -lopengl32
   CFLAGS += -I..
endif

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
else
   CFLAGS += -O3
endif

OBJECTS := libretro-test.o
CFLAGS += -std=gnu99 -Wall -pedantic $(fpic)

ifeq ($(GLES), 1)
   CFLAGS += -DGLES
   LIBS += -lGLESv2
else
   LIBS += $(GL_LIB)
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(fpic) $(SHARED) $(INCLUDES) -o $@ $(OBJECTS) $(LIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean

