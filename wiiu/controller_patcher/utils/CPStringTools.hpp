#ifndef _CPSTRING_TOOLS_H_
#define _CPSTRING_TOOLS_H_
#include <string>
#include <vector>

#include "wiiu/types.h"

class CPStringTools{
    public:
        static bool EndsWith(const std::string& a, const std::string& b);
        static std::vector<std::string> StringSplit(const std::string & inValue, const std::string & splitter);
        static std::string removeCharFromString(std::string& input,char toBeRemoved);
        static const char * byte_to_binary(s32 test);
        static std::string strfmt(const char * format, ...);
};

#endif /* _CPSTRING_TOOLS_H_ */
