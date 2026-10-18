# D3D12 menu channel mapping

`python3 run.py` builds the shipping menu SRV helper and DXGI copy routine
into a GPU oracle. Set `CC=x86_64-w64-mingw32-gcc RUNNER=wine` on Linux.
A native Linux build checks portability and reports that D3D12 is unavailable.

The oracle checks every BGRA4444 color/alpha pattern through `Load` and
linear `SampleLevel`, using padded rows. Cases cover borrowed and copied
16-bit uploads, converted BGRA8 fallback, and native BGRA8 input.
`REQUIRE_NATIVE16=1` makes missing native 16-bit support a failure.

The checked-in DXBC isolates GPU sampling from the runtime HLSL compiler.
After editing `sample_cs.hlsl`, run `python run.py --regenerate` on Windows
with the system D3DCompiler_47, then rerun the GPU oracle. The runner rejects
a stale source hash. GPU waits and readback belong only to this test.
