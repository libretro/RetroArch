
#ifdef WANT_GLSLANG

/* Builtin glslang: vendored library sources first, RetroArch wrapper last.
 *
 * ORDER IS LOAD-BEARING.  The wrapper (glslang.cpp) includes RetroArch
 * headers, and on Windows those reach <windows.h> (via
 * retro_miscellaneous.h), which defines function-like min()/max()
 * macros.  If any RetroArch header precedes the vendored library in
 * this amalgamation, those macros mangle every use of
 * std::numeric_limits<T>::max() and friends inside glslang and the
 * build fails on all MSVC lanes.  Keeping the vendored sources first
 * means they are always parsed with pristine macro state, no matter
 * what the wrapper includes now or in the future.  Do not reorder,
 * and do not add includes above the vendored block. */
#ifdef _MSC_VER
#include <compat/msvc.h>
#ifdef strtoull
#undef strtoull
#endif
#endif

#include "../deps/glslang/glslang/SPIRV/GlslangToSpv.cpp"
#include "../deps/glslang/glslang/SPIRV/InReadableOrder.cpp"
#include "../deps/glslang/glslang/SPIRV/Logger.cpp"
#include "../deps/glslang/glslang/SPIRV/SpvBuilder.cpp"
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

/* RetroArch wrapper: compiled last so its RetroArch includes cannot
 * affect the vendored sources above. */
#include "../gfx/drivers_shader/glslang.cpp"

#elif defined(HAVE_GLSLANG)

/* Prebuilt glslang: only compile RetroArch wrapper, link external library */
#include "../gfx/drivers_shader/glslang.cpp"

#endif
