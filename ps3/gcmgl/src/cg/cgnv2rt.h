#ifndef CGNV2RT_HEADER
#define CGNV2RT_HEADER

#include <stdio.h>
#include <vector>

#define CNV2END(val) convert_endianness((val), elfEndianness)
#define ENDSWAP(val) convert_endianness((val), (host_endianness() == 1) ? 2 : 1)

int convertNvToElfFromMemory(const void *sourceData, size_t size, int endianness, int constTableOffset, void **binaryShader, int *binarySize, 
	std::vector<char> &stringTable, std::vector<float> &defaultValues);

int convertNvToElfFreeBinaryShader(void *binaryShader);

#endif
