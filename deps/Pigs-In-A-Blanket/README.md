[![Development Time](https://wakatime.com/badge/github/SonicMastr/Pigs-In-A-Blanket.svg)](https://wakatime.com/badge/github/SonicMastr/Pigs-In-A-Blanket)

# Pigs In A Blanket | OpenGL ES 2.0 on the PS Vita by CBPS
#### A Piglet/ShaccCg Wrapper Library for OpenGL ES 2.0 Support on the Vita
#### Now with System App and Experimental MSAAx4 Support!
---
### Compiling
- Install [DolceSDK](https://github.com/DolceSDK/doc) (Has not been tested on VitaSDK as of yet)
- Run ```make -j4 install```
- Link libpib.a in your projects

Note when building your projects:

**ALWAYS COMPILE YOUR PROJECTS WITH UNSAFE**. Doing otherwise will result in PIB failing.

If using CMake, make sure to specify ```set(DOLCE_ELF_CREATE_FLAGS "${DOLCE_ELF_CREATE_FLAGS} -h 2097152")```<br>If using Makefile, make sure to specify ```dolce-elf-create -h 2097152```

Piglet needs an SceLibc heap size of at least 2MB to intialize. Without it, the module will fail to start. This heap needs to be larger if using ```-nostdlib``` as this becomes your main heap. More info on ```-nostdlib``` support in the headers.

These stubs are all linked automatically as they are combined with PIB after being built. Be sure to be aware of that.

- liblibScePiglet_stub.a (Yes, that's the name)
- libSceShaccCg_stub.a
- libtaihen_stub.a
- libSceGxmInternalForVsh_stub.a
- libSceGxmInternal_stub.a
- libSceSharedFb_stub.a
- libSceAppMgr_stub.a

### Check the "Samples" folder for examples of how to use PIB to initialize OpenGLES 2.0 with EGL or GLFW.
Yes. We have a GLFW3 port for the vita. You can use it by simply including it in your project as normal or build it here: https://github.com/SonicMastr/glfw-vita<br>Note: Button mapping is perfect and there's full touchscreen support.

To install Piglet on your Vita, just use the [Pigs in a Blanket Configuration Tool](https://github.com/SonicMastr/PIB-Configuration-Tool)

Documentation Provided in the Headers. I'm clean.

## What is Pigs in a Blanket?
This library is a developer focused wrapper which provides easy initialization and expandability, with some quality of life features, including supporting resolutions up to 1920x1080 native on the Playstion TV, and on the Vita with Sharpscale. Developers can choose make their applications with one resolution in mind, as all long as the code is made to scale by dimension, PIB will handle the rest with it's companion app, the [Pigs in a Blanket Configuration Tool](https://github.com/SonicMastr/PIB-Configuration-Tool), which allows the user to specify their own preference of resultion that will automatically override the original settings. For more information aobut Piglet, you can check out our information about it on our [forum post](https://forum.devchroma.nl/index.php/topic,294.msg902.html#msg902).

It library doubles as the heart of Piglet's Shader compiling abilities, removed in the standalone module. I spent the time to rewrite the shader compiler code according to PSM specifications to re-enable the ShaccCg support that was removed. This includes proper return codes and regular log output, so you never have to wonder what's going on with your shaders. Piglet **DOES NOT** support GLSL shaders though, so you'll need to convert the shaders to CG. Check the resources at the end of this README for converting your shaders.

This library also support EGL 1.5 `eglGetProcAddress` functionality using the `PIB_GET_PROC_ADDR_CORE` flag, as the orignal Piglet only returns extensions per EGL 1.4 standard. Thanks to dots-tb we were able to create a simple patch to support returning GLES functions as well.

### Custom Extension Support

This library will allow us to add extensions using native functions as we feel fit. We'll accept the requests of any developers who have ideas of what extensions to add utilizing PIB, and they will all be able to be accessed via `eglGetProcAddress`.

## Supported Extensions
|GL|EGL|
|:-:|:-:|
|GL_EXT_draw_instanced|EGL_SCE_piglet_sync|
|GL_EXT_instanced_arrays|EGL_SCE_piglet_vita_pre_swap_callback|
|GL_SCE_piglet_shader_binary|EGL_SCE_piglet_vita_vsync_callback|
|GL_SCE_texture_resource|
|GL_OES_texture_npot|
|GL_OES_rgb8_rgba8|
|GL_OES_depth_texture|
|GL_EXT_texture_format_BGRA8888|
|GL_EXT_read_format_bgra|
|GL_OES_texture_half_float|
|GL_OES_texture_half_float_linear|
|GL_OES_vertex_half_float|
|GL_OES_element_index_uint|
|GL_EXT_texture_compression_dxt1|
|GL_EXT_texture_compression_dxt3|
|GL_EXT_texture_compression_dxt5|
|GL_EXT_texture_compression_s3tc|
|GL_EXT_texture_storage|
|GL_IMG_texture_compression_pvrtc|

## Special Thanks
- **[GrapheneCt](https://github.com/GrapheneCt)** - Finding the Piglet Module and being a main part of the Project in reverse engineering and testing
- **[dots-tb](https://github.com/dots-tb)** - Initial idea of using PSM and efforts in getting all of the names for Piglet, as well as being a main part of the Project in reverse engineering and testing
- **[cuevavirus](https://github.com/cuevavirus)** - Help with debugging and sense of direction
- **[CreepNT](https://github.com/CreepNT)** - Help with debugging
- **[Princess-of-Sleeping](https://github.com/Princess-of-Sleeping)** - Dump tool and PrincessLog
- **[xyzz](https://github.com/xyzz)** - Initial deep dive into how ShaccCg works
- **[Zer0xFF](https://github.com/Zer0xFF) and [masterzorag](https://github.com/masterzorag)** - Their amazing work on the PS4 Piglet reverse engineering

## GLSL to CG Conversion Resources
Microsoft GLSL to HLSL: https://docs.microsoft.com/en-us/windows/uwp/gaming/glsl-to-hlsl-reference<br>Nvidia CG Standard Library: http://developer.download.nvidia.com/cg/index_stdlib.html<br>Nvidia CG GLSL Vert to CG: http://developer.download.nvidia.com/cg/glslv.html<br>Nvidia CG GLSL Frag to CG: http://developer.download.nvidia.com/cg/glslf.html

This requires both libshacccg.suprx and libScePiglet.suprx to be located in ur0:data/external
