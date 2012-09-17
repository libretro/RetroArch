#ifndef CGNV2RT_HEADER
#define CGNV2RT_HEADER

#include <stdio.h>
#include <vector>
#include <string.h>

#ifndef CGNV2ELF_VERSION
#define CGNV2ELF_VERSION 6365
#define CGNV2ELF_PRODUCT_STRING "cgnv2elf"
#define CGNV2ELF_VERSION_NOTE_TYPE 0
#endif

#define CNV2END(val) convert_endianness((val), elfEndianness)
#define ENDSWAP(val) convert_endianness((val), (host_endianness() == 1) ? 2 : 1)

int convertNvToElfFromFile(const char *sourceFile, int endianness, int constTableOffset, void **binaryShader, int *size, std::vector<char> &stringTable, std::vector<float> &defaultValues);
int convertNvToElfFromMemory(const void *sourceData, size_t size, int endianness, int constTableOffset, void **binaryShader, int *binarySize, std::vector<char> &stringTable, std::vector<float> &defaultValues);

int convertNvToElfFreeBinaryShader(void *binaryShader);

#endif
