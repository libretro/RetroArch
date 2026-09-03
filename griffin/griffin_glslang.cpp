
#ifdef WANT_GLSLANG

/* Builtin glslang: RetroArch wrapper + vendored library sources.
 *
 * THIS TRANSLATION UNIT MUST STAY FREE OF <windows.h>.  The wrapper
 * (glslang.cpp) includes only the minimal glslang_compile.h C ABI
 * header for exactly this reason.  windows.h breaks the amalgamation
 * in either position: before the vendored sources, its function-like
 * min()/max() macros mangle glslang's std::numeric_limits uses;
 * after them, its BOOL/INT/UINT/FLOAT typedefs collide with the
 * global token enumerators of glslang's generated parser
 * (glslang_tab.cpp).  A tripwire at the end of this file enforces
 * the invariant at compile time on every Windows lane. */
#ifdef _MSC_VER
#include <compat/msvc.h>
#ifdef strtoull
#undef strtoull
#endif
#endif

#include "../gfx/drivers_shader/glslang.cpp"

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

#elif defined(HAVE_GLSLANG)

/* Prebuilt glslang: only compile RetroArch wrapper, link external library */
#include "../gfx/drivers_shader/glslang.cpp"

#endif

/* Tripwire for the invariant above: if any include chain in this
 * translation unit ever reaches <windows.h> again, fail the build
 * here with a diagnosis instead of hundreds of cascade errors. */
#if defined(_WINDOWS_) || defined(_MINWINDEF_) || defined(_WINDEF_)
#error "windows.h leaked into griffin_glslang.cpp; its min/max macros and BOOL/INT/UINT/FLOAT typedefs conflict with the vendored glslang sources. Keep RetroArch platform headers out of this TU (see header comment)."
#endif
