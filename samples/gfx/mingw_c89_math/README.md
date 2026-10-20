# MinGW strict C89 math

Run `python3 samples/gfx/mingw_c89_math/run.py` with MinGW GCC. On Linux,
set `CC=x86_64-w64-mingw32-gcc`; the executable runs under Wine. Native
Windows uses GDB with return-child-result. Volatile inputs and disabled
builtins require actual runtime calls, checking the float return ABI.
Seven negative compilations retain C90 grammar coverage. The Makefile
force-includes the scoped compatibility declarations only for its Win32
C89 lane; it does not change the language standard or global SDK.
