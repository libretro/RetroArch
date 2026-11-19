# Native D3D12 asynchronous codec test

Build with MSVC C mode or MinGW and link d3d12, dxgi and dxguid. Native
execution requires a hardware D3D12 device and the Windows debug layer.
The Makefile can cross-compile on Linux; that alone is not a native pass.

With FXC on PATH, `make check` builds the three independently authored
compute kernels and runs the test. Alternatively, supply precompiled kernels
in color, DWT, packet order to `pyrowave_d3d12_test.exe`.
Set `RPY_D3D12_GPU_VALIDATION=1` to require GPU-based debug validation too.

The test runs code-value and signed scRGB packets, cropped source images,
held packet loans, source retirement across unsubmitted discards, display
fences, header rejection, ring/sequence wrap and 1080p. It checks decoded RGB
and alpha and CPU-decoded YCbCr against independent constant fixtures, checks
device-removal retention through an injected fence sentinel, and rejects debug-layer
errors. Its pixel readbacks and bounded waits are offline oracles; neither
exists in the shipping adapter's frame path. Frontend routing is unfinished.

The same executable also runs the shipping session controller through its
typed D3D12 binding: handshake, a GPU-produced packet over fragmented byte
I/O, receiver upload/decode, the RGB/display oracle, and EOS. This uses an
in-memory nonblocking wire and no audio device; physical A/V clock and
frontend connections remain unfinished.

`make check-player` repeats the native GPU session, RGB/scRGB, display-fence
and 1080p checks with the frontend's cold receiver factory. This loads the
independent HLSL through the common bounded VFS loader and compiles it through
D3DCompile. The test uses a direct queue, matching the frontend factory's queue
contract; the regular test continues to cover compute queues. Existing VFS
helpers build with their normal GNU dialect, while codec, factory and oracle
remain C89. This verifies the factory and native image loans; it does not
establish D3D12 frontend playback or hardware A/V synchronization.
