# See build.sh: this harness links the shipping RetroArch objects with
# only main() replaced, so it needs the project's own build rather
# than a source list of its own.
TARGET := monitor_index_test

all:
	@sh ./build.sh

clean:
	rm -f $(TARGET) $(TARGET).exe retroarch_nomain.o retroarch_nomain.d \
	      harness_main.o harness_main.d

.PHONY: all clean
