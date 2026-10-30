CC = cc
CFLAGS = -O2 -Wall -Wextra -std=gnu89 -Werror=declaration-after-statement
INCLUDES = -I../../libretro-common/include -I../.. -I../../gfx/include/ANGLE

all: egl_context_test angle_backend_test

egl_context_fragment.c: ../../gfx/common/egl_common.c
	sed -n '/^static void egl_report_error_code(/,/^}/p; /^static bool egl_display_has_extension(/,/^}/p; /^static void egl_keep_context_attribs(/,/^}/p; /^bool egl_create_context(/,/^}/p; /^bool egl_no_shared_context(/,/^}/p; /^bool egl_shared_context_new(/,/^}/p; /^void egl_release_current(/,/^}/p' $< > $@

egl_context_test: egl_context_test.c egl_context_fragment.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

angle_backend_fragment.c: ../../gfx/common/angle_common.c
	sed -n '/^static const EGLint backendD3D11\[\]/,/^};/p; /^static const EGLint backendD3D11_FL9_3\[\]/,/^};/p; /^static const EGLint backendD3D9\[\]/,/^};/p; /^static const EGLint backendWARP\[\]/,/^};/p; /^static const EGLint\* backendList\[\]/,/^};/p; /^static const char\* backendNamesList\[\]/,/^};/p; /^bool angle_init_context(/,/^}/p' $< > $@

angle_backend_test: angle_backend_test.c angle_backend_fragment.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

check: all
	./egl_context_test
	./angle_backend_test

clean:
	rm -f egl_context_test egl_context_test.exe egl_context_fragment.c
	rm -f angle_backend_test angle_backend_test.exe angle_backend_fragment.c

.PHONY: all check clean
