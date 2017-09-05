/* RetroArch - A frontend for libretro.
* Copyright (C) 2011-2017 - Daniel De Matteis
*
* RetroArch is free software: you can redistribute it and/or modify it under the terms
* of the GNU General Public License as published by the Free Software Found-
* ation, either version 3 of the License, or (at your option) any later version.
*
* RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
* PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with RetroArch.
* If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#define HAVE_SHADERS
#endif

#if defined(HAVE_ZLIB) || defined(HAVE_7ZIP)
#define HAVE_COMPRESSION
#endif

#if defined(_MSC_VER)
#include <compat/posix_string.h>
#endif

#ifdef WANT_GLSLANG
#ifdef HAVE_VULKAN
#include "../deps/glslang/glslang.cpp"
#include "../deps/glslang/glslang_tab.cpp"
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
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpMemory.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpScanner.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpSymbols.cpp"
#include "../deps/glslang/glslang/glslang/MachineIndependent/preprocessor/PpTokens.cpp"

#include "../deps/glslang/glslang/hlsl/hlslAttributes.cpp"
#include "../deps/glslang/glslang/hlsl/hlslGrammar.cpp"
#include "../deps/glslang/glslang/hlsl/hlslOpMap.cpp"
#include "../deps/glslang/glslang/hlsl/hlslParseables.cpp"
#include "../deps/glslang/glslang/hlsl/hlslParseHelper.cpp"
#include "../deps/glslang/glslang/hlsl/hlslScanContext.cpp"
#include "../deps/glslang/glslang/hlsl/hlslTokenStream.cpp"

#ifdef _WIN32
#include "../deps/glslang/glslang/glslang/OSDependent/Windows/ossource.cpp"
#endif

#if defined(__linux__) && !defined(ANDROID)
#include "../deps/glslang/glslang/glslang/OSDependent/Unix/ossource.cpp"
#endif
#endif
#endif

/*============================================================
AUDIO
============================================================ */
#ifdef HAVE_XAUDIO
#include "../audio/drivers/xaudio.cpp"
#endif

/*============================================================
MENU
============================================================ */
#ifdef HAVE_XUI
#include "../menu/drivers/xui.cpp"
#endif

#if defined(HAVE_D3D)
#include "../menu/drivers_display/menu_display_d3d.cpp"
#endif

/*============================================================
VIDEO CONTEXT
============================================================ */

#if defined(HAVE_D3D)
#include "../gfx/drivers_context/d3d_ctx.cpp"

#ifdef HAVE_HLSL
#include "../gfx/drivers_shader/shader_hlsl.cpp"
#endif
#endif

/*============================================================
UI
============================================================ */

#if defined(_WIN32) && !defined(_XBOX)
#include "../ui/drivers/win32/ui_win32_window.cpp"
#endif

#if defined(HAVE_QT)
#include "../ui/drivers/ui_qt.cpp"

#include "../ui/drivers/qt/ui_qt_window.cpp"
#include "../ui/drivers/qt/ui_qt_browser_window.cpp"
#include "../ui/drivers/qt/ui_qt_msg_window.cpp"
#include "../ui/drivers/qt/ui_qt_application.cpp"
#endif

#if defined(HAVE_QT_WRAPPER)
#include "../ui/drivers/ui_qt.cpp"
#endif

/*============================================================
VIDEO DRIVER
============================================================ */
#ifdef _XBOX
#include "../frontend/drivers/platform_xdk.cpp"
#endif

#if defined(HAVE_D3D)
#include "../gfx/common/d3d_common.cpp"
#include "../gfx/drivers/d3d.cpp"


#if defined(HAVE_D3D8)
#include "../gfx/drivers_renderchain/d3d8_renderchain.cpp"
#elif defined(HAVE_D3D9)

#ifdef HAVE_HLSL
#include "../gfx/drivers_renderchain/d3d9_hlsl_renderchain.cpp"
#endif

#ifdef HAVE_CG
#include "../gfx/drivers_renderchain/d3d9_cg_renderchain.cpp"
#endif

#endif

#endif

#ifdef HAVE_VULKAN
#include "../gfx/drivers_shader/shader_vulkan.cpp"
#include "../gfx/drivers_shader/glslang_util.cpp"
#include "../gfx/drivers_shader/slang_reflection.cpp"
#include "../deps/SPIRV-Cross/spirv_cross.cpp"
#include "../deps/SPIRV-Cross/spirv_cfg.cpp"
#endif

/*============================================================
FONTS
============================================================ */

#if defined(HAVE_D3D9) && !defined(_XBOX)
#include "../gfx/drivers_font/d3d_w32_font.cpp"
#endif

#if defined(_XBOX360)
#include "../gfx/drivers_font/xdk360_fonts.cpp"
#endif
