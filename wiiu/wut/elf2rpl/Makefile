TARGET   := elf2rpl

CXX      := g++
LD	 := g++

OBJS   := main.o
INCLUDES := -I.

CFLAGS   := -O3 -Wall -std=c++14 -DNDEBUG
LDFLAGS  := -lz

ifneq ($(findstring Linux,$(shell uname -a)),)
else ifneq ($(findstring Darwin,$(shell uname -a)),)
else
   TARGET   := $(TARGET).exe
endif

all: $(TARGET)

clean:
	@rm -rf $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) $(INCLUDES)

$(TARGET): $(OBJS)
	$(LD) $(OBJS) $(CFLAGS) $(LDFLAGS) -o $@
