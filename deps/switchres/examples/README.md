# ANY OS - BASIC INFORMATION

## Build libswitchres

It supports cross compilation, and will build both dynamic and static libs as per the target OS
```bash
make libswitchres
```
## Basic usage as a client with examples
libswitchres can be called in 2 different ways (with example code):
  * `test_dlopen.c` -> by explicitely opening a .so/.dll, import the srlib object and call associated functions
  * `test_liblink.c` -> by simply linking libswitchres at build time

These options are generic whether you build for Linux or Windows
  * -I ../ (to get libswitchres_wrapper.h)
  * -L ../ or -L ./ (for win32, when the dll has been copied in the examples folder)
  * -lswitchres to link the lib if not manually opening it in the code

#please note#: static libs aven't been tested yet

# LINUX

You'll need a few extra parameters for gcc:
  * -ldl (will try later to find a way to statically link libdl.a)

When running, dont forget to add before the binary LD_LIBRARY_PATH=<libswitchres.so pass, even if it's ./>:$LD_LIBRARY_PATH

## Examples:
```bash
make libswitchres
cd examples
g++ -o linux_dl_test test_dlopen.cpp -I ../ -ldl
LD_LIBRARY_PATH=../:$LD_LIBRARY_PATH ./linux_dl_test

g++ -o linux_link_lib test_liblink.cpp -I ../ -L../ -lswitchres -ldl
LD_LIBRARY_PATH=../:$LD_LIBRARY_PATH ./linux_link_lib
```

# WINDOWS

Pretty much the same as Linux, but with mingw64. The resulting exe and dll can be tested with wine

## Examples (cross-building from windows)

```
make PLATFORM=NT CROSS_COMPILE=x86_64-w64-mingw32- libswitchres
(copy the dll to examples)

x86_64-w64-mingw32-g++-win32 test_dlopen.cpp -o w32_loaddll.exe -I ../ -static-libgcc -static-libstdc++
w32_loaddll.exe

x86_64-w64-mingw32-g++-win32 test_liblink.cpp -o w32_linkdll.exe -I ../ -static-libgcc -static-libstdc++ -L ./ -lswitchres
w32_linkdll.exe
```

Note that, when building w32_linkdll.exe, I couldn't point to another dir else than ./ with -L
