# D3D12 menu and frame channel mappings

`python3 run.py` builds the shipping menu SRV helper and DXGI copy routine
into a GPU oracle. Set `CC=x86_64-w64-mingw32-gcc RUNNER=wine` on Linux.
A native Linux build checks portability and reports that D3D12 is unavailable.

The oracle checks every BGRA4444 color/alpha pattern through `Load` and
linear `SampleLevel`, using padded rows. Cases cover borrowed and copied
16-bit uploads, converted BGRA8 fallback, and native BGRA8 input.
`REQUIRE_NATIVE16=1` makes missing native 16-bit support a failure.

Packed 10-bit cases exercise raw borrowed uploads, the shipping copied
upload helper (including inferred source pitch), and native DXGI channel/alpha
interpretation. Same-format software/native transitions must replace the SRV
mapping. Every 10-bit channel value and all four ignored X/alpha codes are
checked independently; software X is opaque, native alpha is preserved.

The checked-in DXBC isolates GPU sampling from the runtime HLSL compiler.
After editing `sample_cs.hlsl`, run `python run.py --regenerate` on Windows
with the system D3DCompiler_47, then rerun the GPU oracle. The runner rejects
a stale source hash. GPU waits and readback belong only to this test.
