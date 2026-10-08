# OpenXR™ Software Development Kit (SDK) Project

<!--
Copyright (c) 2017-2026 The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

This repository contains OpenXR headers, as well as source code and build scripts
for the OpenXR loader.
It contains all generated source files and headers pre-generated for minimum dependencies.

The authoritative public repository for this project is located at
<https://github.com/KhronosGroup/OpenXR-SDK>.

The public repository containing the scripts that generate the files in this repository
is located at
<https://github.com/KhronosGroup/OpenXR-SDK-Source>.
It hosts the public Issue tracker, and accepts patches (Pull Requests) from the
general public.
That repository is also where sample code (hello_xr) and API layer source can be found.

**Note that this repo is effectively read-only: changes to this repo should be made in the [OpenXR-SDK-Source](https://github.com/KhronosGroup/OpenXR-SDK-Source) repo instead**

## Directory Structure

<!-- REUSE-IgnoreStart -->
- `BUILDING.md` - Instructions for building the projects
- `README.md` - This file
- `COPYING.md` - Copyright and licensing information
- `CODE_OF_CONDUCT.md` - Code of Conduct
- `external/` - External code for projects in the repo
- `include/` - OpenXR header files
- `src/external/jsoncpp` - The jsoncpp project source code, an included dependency of the loader.
- `src/loader` - OpenXR loader code, **including generated code**
<!-- REUSE-IgnoreEnd -->

## Building

The project is set up to build using CMake.

### (Optional) Building the OpenXR Loader as a DLL

By default, the OpenXR loader is built as a static library on Windows and a dynamic library on other platforms.
To specify alternate behavior, define the CMake option `DYNAMIC_LOADER`,
e.g. by adding `-DDYNAMIC_LOADER=ON` or `-DDYNAMIC_LOADER=OFF` to your CMake command line.

### Windows

Building the OpenXR components in this tree on Windows is supported using
Visual Studio 2013 and newer.  Before beginning, make sure the appropriate
"msbuild.exe" is in your PATH.  Also, when generating the solutions/projects
using CMake, be sure to use the correct compiler version number.  The
following table is provided to help you:

| Visual Studio        | Version Number |
| -------------------- |:--------------:|
| Visual Studio 2017   |       15       |
| Visual Studio 2019   |       16       |
| Visual Studio 2022   |       17       |

Specific sample command lines for building follow.
If you're already familiar with the process of building a project with
CMake, you may skim or skip these instructions.

#### Windows 64-bit

First, generate the 64-bit solution and project files using CMake:

```cmd
mkdir build\win64
cd build\win64
cmake -G "Visual Studio [Version Number] Win64" ..\..
```

Finally, open the `build\win64\OPENXR.sln` in the Visual Studio to build the loader.

#### Windows 32-bit

First, generate the 32-bit solution and project files using CMake:

```cmd
mkdir build\win32
cd build\win32
cmake -G "Visual Studio [Version Number]" ..\..
```

Open the `build\win32\OPENXR.sln` in the Visual Studio to build the loader.

### Linux

The following set of Debian/Ubuntu packages provides all required libs for building for xlib or xcb with OpenGL and Vulkan support.

- `build-essential`
- `cmake` (of _somewhat_ recent vintage, 3.10+ known working)
- `libgl1-mesa-dev`
- `libvulkan-dev`
- `libx11-xcb-dev`
- `libxcb-dri2-0-dev`
- `libxcb-glx0-dev`
- `libxcb-icccm4-dev`
- `libxcb-keysyms1-dev`
- `libxcb-randr0-dev`
- `libxrandr-dev`
- `libxxf86vm-dev`
- `mesa-common-dev`

Specific sample command lines for building follow.
If you're already familiar with the process of building a project with
CMake, you may skim or skip these instructions.

#### Linux Debug

```sh
mkdir -p build/linux_debug
cd build/linux_debug
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make
```

#### Linux Release

```sh
mkdir -p build/linux_release
cd build/linux_release
cmake -DCMAKE_BUILD_TYPE=Release ../..
make
```

### macOS

Building the OpenXR components in this tree on macOS is supported using Xcode
14.0 and newer. You may need to install Xcode Command Line Tools and cmake.

First, generate the Xcode project file using CMake:

```cmd
mkdir -p build/macos
cd build/macos
cmake -G "Xcode" ../..
```

Finally, open the build/macos/OPENXR.xcodeproj in Xcode to build the samples.
