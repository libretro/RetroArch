#!/usr/bin/env python3
"""Run the shipping capture shader on EGL/GLES; readback is test-only."""
import ctypes as C
import ctypes.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
os.environ.setdefault("EGL_PLATFORM", "surfaceless")
def library(name):
    path = C.util.find_library(name)
    if not path:
        raise SystemExit(77)
    try:
        return C.CDLL(path)
    except OSError:
        raise SystemExit(77)
egl, gl = library("EGL"), library("GLESv2")
def function(lib, name, result, args):
    f = getattr(lib, name)
    f.restype, f.argtypes = result, args
    return f
I, U, P = C.c_int, C.c_uint, C.c_void_p
get_display = function(egl, "eglGetDisplay", P, [P])
initialize = function(egl, "eglInitialize", U, [P, P, P])
choose = function(egl, "eglChooseConfig", U, [P, P, P, I, P])
bind = function(egl, "eglBindAPI", U, [U])
create_surface = function(egl, "eglCreatePbufferSurface", P, [P, P, P])
create_context = function(egl, "eglCreateContext", P, [P, P, P, P])
make_current = function(egl, "eglMakeCurrent", U, [P, P, P, P])
destroy_surface = function(egl, "eglDestroySurface", U, [P, P])
destroy_context = function(egl, "eglDestroyContext", U, [P, P])
terminate = function(egl, "eglTerminate", U, [P])
display = get_display(None)
if not display or not initialize(display, None, None):
    raise SystemExit(77)
config = P()
count = I()
attrs = (I * 15)(0x3033, 1, 0x3040, 0x40, 0x3024, 8, 0x3023, 8,
                  0x3022, 8, 0x3021, 8, 0x3038, 0, 0)
assert choose(display, attrs, C.byref(config), 1, C.byref(count)) and count.value
assert bind(0x30A0)
surface = create_surface(display, config, (I * 5)(0x3057, 4, 0x3056, 4, 0x3038))
context = create_context(display, config, None, (I * 3)(0x3098, 3, 0x3038))
assert surface and context and make_current(display, surface, surface, context)
get_string = function(gl, "glGetString", C.c_char_p, [U])
assert get_string(0x1F02).startswith(b"OpenGL ES 3")
gen = function(gl, "glGenTextures", None, [I, P])
bind_tex = function(gl, "glBindTexture", None, [U, U])
parameter = function(gl, "glTexParameteri", None, [U, U, I])
upload = function(gl, "glTexImage2D", None, [U, I, I, I, I, I, U, U, P])
read = function(gl, "glReadPixels", None, [I, I, I, I, U, U, P])
delete = function(gl, "glDeleteTextures", None, [I, P])
error = function(gl, "glGetError", U, [])
# The C ops struct contains only function pointers in this declared order.
names = ["CreateShader", "ShaderSource", "CompileShader", "GetShaderiv",
         "DeleteShader", "CreateProgram", "AttachShader", "LinkProgram",
         "GetProgramiv", "DeleteProgram", "GetUniformLocation", "GenVertexArrays",
         "DeleteVertexArrays", "BindVertexArray", "UseProgram", "Uniform1i",
         "ActiveTexture", "BindTexture", "BindFramebuffer", "Viewport",
         "Disable", "ColorMask", "DrawArrays", "GetError"]
ops = (P * len(names))(*(C.cast(getattr(gl, "gl" + n), P).value for n in names))
def expected(rgb, mode):
    if mode == 2:
        rgb = [sum(a*b for a,b in zip(rgb,row)) * 80 / 10000 for row in
               ((.627404,.329283,.043313),(.069097,.919540,.011362),
                (.016391,.088013,.895595))]
        rgb = [((3424/4096+(2413/128)*max(0,min(1,x))**(2610/16384)) /
                (1+(2392/128)*max(0,min(1,x))**(2610/16384)))**(2523/32)
               for x in rgb]
    return [round(max(0,min(1,x))*255) for x in rgb] + [255]
with tempfile.TemporaryDirectory(prefix="capture-draw-") as tmp:
    binary = Path(tmp) / ("draw.dll" if os.name == "nt" else "draw.so")
    compiler = shlex.split(os.environ.get("CC", "cc"))
    command = compiler + [
        "-shared", "-fPIC", "-O2", "-std=c89", "-pedantic", "-Wall", "-Wextra",
        "-Werror", "-Wno-long-long", "-Wframe-larger-than=2048",
        "-I" + str(root / "gfx/include/ANGLE"),
        "-I" + str(root / "libretro-common/include"),
        str(root / "record/record_android_capture_draw.c"), "-o", str(binary)]
    if "clang" in subprocess.check_output(compiler + ["--version"], text=True).lower():
        command += ["-Wno-c99-extensions"]
    subprocess.run(command, check=True)
    lib = C.CDLL(str(binary))
    new = function(lib, "record_android_capture_draw_new", P, [P,U,U,U,C.c_bool])
    frame = function(lib, "record_android_capture_draw_frame", C.c_bool, [P,U])
    free = function(lib, "record_android_capture_draw_free", None, [P,C.c_bool])
    texture = U()
    gen(1, C.byref(texture))
    bind_tex(0x0DE1, texture.value)
    parameter(0x0DE1, 0x2801, 0x2600)
    parameter(0x0DE1, 0x2800, 0x2600)
    maximum = 0
    cases = 0
    for mode in range(3):
        for flip in (False, True):
            draw = new(ops, 4, 4, mode, flip)
            assert draw, (mode, flip)
            for step in range(48 if mode == 2 else 16):
                # Binary fractions are exact in RGBA16F, including HDR/negative.
                scale = (1, 16, 128)[step // 16] if mode == 2 else 1
                pixels = [[scale*((x+step)%17-1)/8, scale*((y+step)%17-1)/8,
                           scale*((x+y+step)%17-1)/8, .25]
                          for y in range(4) for x in range(4)]
                if mode == 0:
                    pixels = [[round(max(0,min(1,v))*255)/255 for v in p]
                              for p in pixels]
                    data = (C.c_ubyte * 64)(*(round(v*255) for p in pixels for v in p))
                    internal, kind = 0x8058, 0x1401
                elif mode == 1:
                    pixels = [[round(max(0,min(1,v))*1023)/1023 for v in p[:3]] + [1]
                              for p in pixels]
                    data = (U * 16)(*(round(p[0]*1023) | (round(p[1]*1023)<<10) |
                                      (round(p[2]*1023)<<20) | (3<<30) for p in pixels))
                    internal, kind = 0x8059, 0x8368
                else:
                    data = (C.c_float * 64)(*(v for pixel in pixels for v in pixel))
                    internal, kind = 0x881A, 0x1406
                bind_tex(0x0DE1, texture.value)
                upload(0x0DE1,0,internal,4,4,0,0x1908,kind,data)
                assert error() == 0
                assert frame(draw, texture.value)
                result = (C.c_ubyte * 64)()
                read(0,0,4,4,0x1908,0x1401,result)
                assert error() == 0
                for y in range(4):
                    for x in range(4):
                        source = pixels[(3-y if flip else y)*4+x]
                        reference = expected(source[:3], mode)
                        actual = list(result[(y*4+x)*4:(y*4+x+1)*4])
                        peak = max(abs(a-b) for a,b in zip(actual,reference))
                        maximum = max(maximum, peak)
                        assert peak <= 1, (mode,flip,step,x,y,actual,reference)
                cases += 1
            free(draw, False)
    delete(1, C.byref(texture))
    print(json.dumps(dict(cases=cases, maximum_8bit_error=maximum,
                          renderer=get_string(0x1F01).decode(),
                          source_formats=["RGBA8", "RGB10_A2", "RGBA16F"],
                          destination="RGBA8 test pbuffer", complete=True)))
assert make_current(display, None, None, None)
assert destroy_context(display, context) and destroy_surface(display, surface)
assert terminate(display)
