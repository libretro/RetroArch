TARGET   := elf2rpl

CXX      := g++
LD	 := g++

OBJS   := main.o
INCLUDES := -I.

CFLAGS   := -O2 -Wall --std=c++14
LDFLAGS  := -lz

all: $(TARGET)

clean:
	@rm -rf $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) $(INCLUDES)

$(TARGET): $(OBJS)
	$(LD) $(OBJS) $(CFLAGS) $(LDFLAGS) -o $@
