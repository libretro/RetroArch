# The D3D drivers' texture pokes under threaded video: every entry a
# caller may reach with threaded=true that touches the device's queue
# or descriptor heaps goes through video_thread_texture_handle(), the
# round trip onto the video thread. Checked on the compiled object,
# through its relocations: the entry's body names the marshal, or the
# lane fails. Cross-built with mingw-w64 like the compile matrix's D3D
# lanes; nothing runs. SANITIZER is taken and ignored.
MINGW_CC ?= x86_64-w64-mingw32-gcc
OBJDUMP  ?= $(patsubst %gcc,%objdump,$(MINGW_CC))

all:
	@true

check:
	@MINGW_CC="$(MINGW_CC)" OBJDUMP="$(OBJDUMP)" ./check.sh

clean:
	rm -f *.o

.PHONY: all check clean
