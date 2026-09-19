TARGET := metal_texture_update_test

SOURCES := metal_texture_update_test.m
OBJS    := $(SOURCES:.m=.o)

# Apple's clang with ARC: the sample keeps its own references and does
# not use the driver's RARCH_* ownership layer, so there is nothing to
# match between them.
CFLAGS  += -Wall -g -O1 -fobjc-arc
LDFLAGS += -framework Foundation -framework Metal

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.m
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
