# See build.sh: this harness links the shipping RetroArch objects with
# only main() replaced, so it needs the project's own build rather
# than a source list of its own.
TARGET := ozone_sublabel_scroll_test

all:
	@./build.sh

clean:
	rm -f $(TARGET) retroarch_nomain.o retroarch_nomain.d \
	      harness_main.o harness_main.d

.PHONY: all clean
