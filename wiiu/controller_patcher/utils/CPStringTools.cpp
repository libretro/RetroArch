#include "CPStringTools.hpp"
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <stdlib.h>

#include "wiiu/types.h"

bool CPStringTools::EndsWith(const std::string& a, const std::string& b) {
    if (b.size() > a.size()) return false;
    return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

std::vector<std::string> CPStringTools::StringSplit(const std::string & inValue, const std::string & splitter){
    std::string value = inValue;
    std::vector<std::string> result;
    while (true) {
        u32 index = value.find(splitter);
        if (index == std::string::npos) {
            result.push_back(value);
            break;
        }
        std::string first = value.substr(0, index);
        result.push_back(first);
        if (index + splitter.size() == value.length()) {
            result.push_back("");
            break;
        }
        if(index + splitter.size() > value.length()) {
            break;
        }
        value = value.substr(index + splitter.size(), value.length());
    }
    return result;
}

const char * CPStringTools::byte_to_binary(s32 x){
    static char b[9];
    b[0] = '\0';

    s32 z;
    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}

std::string CPStringTools::removeCharFromString(std::string& input,char toBeRemoved){
    std::string output = input;
    size_t position;
    while(1){
        position = output.find(toBeRemoved);
        if(position == std::string::npos)
            break;
        output.erase(position, 1);
    }
    return output;
}


std::string CPStringTools::strfmt(const char * format, ...){
	std::string str;
	char * tmp = NULL;

	va_list va;
	va_start(va, format);
	if((vasprintf(&tmp, format, va) >= 0) && tmp)
	{
		str = tmp;
	}
	va_end(va);

	if(tmp){
		free(tmp);
		tmp = NULL;
    }

	return str;
}
//CPStringTools
