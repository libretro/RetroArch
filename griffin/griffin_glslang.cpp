
#ifdef WANT_GLSLANG

#ifdef _MSC_VER
#include <compat/msvc.h>
#ifdef strtoull
#undef strtoull
#endif
#endif

#include "../deps/glslang/glslang.cpp"
#include "../deps/glslang/glslang/SPIRV/disassemble.cpp"
#include "../deps/glslang/glslang/SPIRV/doc.cpp"
#include "../deps/glslang/glslang/SPIRV/GlslangToSpv.cpp"
#include "../deps/glslang/glslang/SPIRV/InReadableOrder.cpp"
#include "../deps/glslang/glslang/SPIRV/Logger.cpp"
#include "../deps/glslang/glslang/SPIRV/SpvBuilder.cpp"
#include "../deps/glslang/glslang/SPIRV/SPVRemapper.cpp"

#include "../deps/glslang/glslang/glslang/GenericCodeGen/CodeGen.cpp"
#include "../deps/glslang/glslang/glslang/GenericCodeGen/Link.cpp"

#include "../deps/glslang/glslang/OGLCompilersDLL/InitializeDll.cpp"

#include "../deps/glslang/glslang/glslang/MachineIndependent/attribute.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/Constant.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/glslang_tab.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/InfoSink.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/Initialize.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/Intermediate.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/intermOut.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/IntermTraverse.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/iomapper.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/limits.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/linkValidate.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/parseConst.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/ParseContextBase.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/ParseHelper.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/PoolAlloc.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/propagateNoContraction.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/reflection.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/RemoveTree.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/Scan.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/ShaderLang.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/SymbolTable.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/Versions.cpp"

#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/Pp.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpAtom.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpContext.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp"

#ifdef __APPLE__
#include "../deps/glslang/glslang/glslang/OSDependent/Unix/ossource.cpp"
#endif

#if defined(ENABLE_HLSL)
#include "../deps/glslang/glslang/hlsl/hlslAttributes.cpp"
#include "../deps/glslang/glslang/hlsl/hlslGrammar.cpp"
#include "../deps/glslang/glslang/hlsl/hlslOpMap.cpp"
#include "../deps/glslang/glslang/hlsl/hlslParseables.cpp"
#include "../deps/glslang/glslang/hlsl/hlslParseHelper.cpp"
#include "../deps/glslang/glslang/hlsl/hlslScanContext.cpp"
#include "../deps/glslang/glslang/hlsl/hlslTokenStream.cpp"
#endif

#endif
