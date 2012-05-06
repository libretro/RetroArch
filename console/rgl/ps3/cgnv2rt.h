#ifndef CGNV2RT_HEADER
#define CGNV2RT_HEADER

#include <stdio.h>
#include <vector>
#include <string.h>

#ifndef STL_NAMESPACE
#define STL_NAMESPACE ::std::
#endif

#ifndef CGNV2ELF_VERSION
#define CGNV2ELF_VERSION 6365
#define CGNV2ELF_PRODUCT_STRING "cgnv2elf"
#define CGNV2ELF_VERSION_NOTE_TYPE 0
#endif

#define CNV2END(val) convert_endianness((val), elfEndianness)
#define ENDSWAP(val) convert_endianness((val), (host_endianness() == 1) ? 2 : 1)

static unsigned int stringTableAdd( STL_NAMESPACE vector<char> &stringTable, const char* str )
{
	unsigned int ret = (unsigned int)stringTable.size();

	if ( ret == 0 )
	{
		stringTable.push_back('\0');
		ret = 1;
	}

	size_t stringLength = strlen(str) + 1;
	stringTable.resize(ret + stringLength);
	memcpy(&stringTable[0] + ret,str,stringLength);

	return ret;
}

static unsigned int stringTableFind( STL_NAMESPACE vector<char> &stringTable, const char* str  )
{
	const char* data = &stringTable[0];
	size_t size = stringTable.size();
	const char *end = data + size;

	size_t length = strlen(str);
	if (length+1 > size)
		return 0;
	data += length;

	const char *p = (char*)memchr(data,'\0',end-data);
	while (p && (end-data)>0)
	{
		if (!memcmp(p - length, str, length))
		{
			return (unsigned int)(p - length - &stringTable[0]);
		}
		data = p+1;	
		p = (char*)memchr(data,'\0',end-data);
	}
	return 0;
}

static unsigned int stringTableAddUnique( STL_NAMESPACE vector<char> &stringTable, const char* str )
{
	if ( stringTable.size() == 0 )
		stringTable.push_back('\0');
	unsigned int ret = stringTableFind(stringTable, str);
	if (ret == 0 && str[0] != '\0')
		ret = stringTableAdd(stringTable, str);
	return ret;
}

int convertNvToElfFromFile(const char *sourceFile, int endianness, int constTableOffset, void **binaryShader, int *size,
	STL_NAMESPACE vector<char> &stringTable, STL_NAMESPACE vector<float> &defaultValues);
int convertNvToElfFromMemory(const void *sourceData, size_t size, int endianness, int constTableOffset, void **binaryShader, int *binarySize, 
	STL_NAMESPACE vector<char> &stringTable, STL_NAMESPACE vector<float> &defaultValues);

int convertNvToElfFreeBinaryShader(void *binaryShader);

#endif
