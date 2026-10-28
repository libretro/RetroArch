ROOT   := ../../..
TARGET := sensor_subdevice_test
CC     ?= cc
# The frontend's core sensor wrappers, extracted from input_driver.c
# between their markers, so the test runs the shipping code.
CFLAGS += -std=gnu99 -O1 -g1 -Wall -DRARCH_INTERNAL \
          -I. -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
core_sensor_fragment.c: $(ROOT)/input/input_driver.c
	sed -n '/===== BEGIN input core sensor wrappers =====/,/===== END input core sensor wrappers =====/p' $< > $@
$(TARGET): sensor_subdevice_test.c core_sensor_fragment.c
	$(CC) $(CFLAGS) -o $@ sensor_subdevice_test.c $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe core_sensor_fragment.c
.PHONY: all check clean
