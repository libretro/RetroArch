CC ?= cc
JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(command -v javac))))
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: test
classes/.stamp: $(wildcard java/android/media/*.java) java/android/os/Build.java
	mkdir -p classes
	javac -d classes java/android/media/*.java java/android/os/Build.java
	touch $@
test: test.c ../../../gfx/common/mediacodec_inventory.c classes/.stamp
	$(CC) $(CFLAGS) test.c ../../../gfx/common/mediacodec_inventory.c -o $@ -L$(JAVA_HOME)/lib/server -Wl,-rpath,$(JAVA_HOME)/lib/server -ljvm $(LDFLAGS)
check: all
	./test
clean:
	rm -f test classes/.stamp
.PHONY: all check clean
