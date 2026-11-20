# D3D11 PyroWave

`make check` runs the shipping C89 compute adapter against the committed
Shader Model 5 DXBC. On Linux, use `make check RUNNER=wine` under Xvfb.
The adapter requires feature level 11_0. No shader compiler is needed.

The oracle checks encoded packets against the CPU decoder, reads back GPU
decoded pixels, saturates the packet pool, and checks packet/image loans
through retirement and shutdown. Set `PYROWAVE_D3D11_DEBUG=1` on a Windows
system with the D3D11 debug layer installed to reject GPU validation errors.
Waits and pixel readback are confined to this offline executable.

The embedded frontend uses D3D11's immediate context on its render owner.
It samples the final composition without a pixel copy and lends compressed
readback buffers to I/O until release. Received textures remain on the GPU
through their final display query. The existing PyroWave device picker,
recording configuration and receiver also work with the `d3d11` driver.
