# See build.sh: this harness links the shipping RetroArch objects with
# only main() replaced, so it needs the project's own build rather than
# a source list of its own. MAKE_ARGS names the tree build to link.
TARGET := video_filter_menu_test

all:
	@MAKE_ARGS="$(MAKE_ARGS)" ./build.sh

check: all
	@./$(TARGET) > check.log 2>&1; rc=$$?; \
	 grep -vE '^\[(INFO|DEBUG|WARN|ERROR)\]' check.log; rm -f check.log; \
	 exit $$rc

clean:
	rm -rf $(TARGET) filter_core.so filters \
	      retroarch_nomain.o harness_main.o \
	      retroarch_nomain.d harness_main.d

.PHONY: all check clean
