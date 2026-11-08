CC ?= cc
CFLAGS ?= -O2
CPPFLAGS += -I../../../include
WARN = -std=c89 -Wall -Wextra -Werror -Wdeclaration-after-statement
ifneq ($(SANITIZER),)
CFLAGS += -g -fno-omit-frame-pointer -fsanitize=$(SANITIZER)
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET ?= rwma_test
SOURCES = rwma_test.c ../../../formats/wma/rwma_pro.c ../../../formats/wma/rwma_standard.c
REFERENCE_TARGET = rwma_reference

all: $(TARGET)

$(TARGET): $(SOURCES) ../../../formats/wma/rwma_internal.h ../../../formats/wma/rwma_mdct.h ../../../formats/wma/rwma_pro_data.h ../../../formats/wma/rwma_pro_prefix.h ../../../formats/wma/rwma_pro_gain.h ../../../include/formats/rwma.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(SOURCES) $(LDFLAGS) -lm -o $@

test: $(TARGET)
	./$(TARGET)

reference: $(REFERENCE_TARGET)

$(REFERENCE_TARGET): rwma_reference.c $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=gnu89 -Wdeclaration-after-statement rwma_reference.c ../../../formats/wma/rwma_pro.c ../../../formats/wma/rwma_standard.c $$(pkg-config --cflags --libs libavformat libavcodec libavutil) $(LDFLAGS) -lm -o $@

file-reference: rwma_file_reference

rwma_file_reference: rwma_file_reference.c $(SOURCES) ../../../formats/wma/rwma_stream.c ../../../formats/wma/rwma_xma_stream.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) rwma_file_reference.c ../../../formats/wma/rwma_pro.c ../../../formats/wma/rwma_standard.c ../../../formats/wma/rwma_stream.c $(LDFLAGS) -lm -o $@

clean:
	rm -f rwma_fast_math_test rwma_fast_math_test.exe
	rm -f $(TARGET) $(REFERENCE_TARGET) rwma_file_reference rwma_file_reference.exe

.PHONY: all test clean reference file-reference
