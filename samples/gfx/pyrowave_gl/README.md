# PyroWave native OpenGL checks

`make check` generates the independent shaders and checks asynchronous
capture and decoding on a desktop compute-capable context. On Linux use
`EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1` for llvmpipe portability
checks. `make check SANITIZER=address,undefined` also instruments host code.
Windows builds use `CC=x86_64-w64-mingw32-gcc EXE=.exe` and run natively.

The player factory loads its own programs through the shipping bounded VFS
loader. The test feeds a packet into its lent input span, checks restored
renderer bindings, and compares decoded GPU RGB with the independent CPU
decoder. It checks refusal to free a borrowed image and retirement after
final presentation. Disabling the decoder dispatch makes this check fail.
The encoder, decoder, factory and oracle compile with strict C89 flags;
existing VFS dependencies compile separately with their normal GNU dialect.

GPU pixel readback and blocking completion are confined to this offline
oracle. Production APIs lend GPU textures and poll fences without waiting.
The factory is a receiver backend, not a standalone streaming client; frontend
playback controls and hardware audio/video scheduling remain integration work.
