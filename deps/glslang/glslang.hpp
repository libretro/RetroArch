#ifndef GLSLANG_COMPILER_HPP
#define GLSLANG_COMPILER_HPP

#include <vector>
#include <string>
#include <stdint.h>

namespace glslang
{
    enum Stage
    {
        StageVertex = 0,
        StageTessControl,
        StageTessEvaluation,
        StageGeometry,
        StageFragment,
        StageCompute
    };

    bool compile_spirv(const std::string &source, Stage stage, std::vector<uint32_t> *spirv);
}

#endif

