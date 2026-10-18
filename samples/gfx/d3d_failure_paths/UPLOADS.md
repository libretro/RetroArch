# D3D11 upload regression

`python3 upload.py` extracts the shipping upload, texture load/update and
release routines. It checks idle/busy byte parity, independent packed 10-bit
channel and alpha expectations, padded and inferred pitches, native formats,
converted menu and RGB565 fallbacks, allocation refusal, cached growth/reuse,
map errors, image load/update parity, stream drops and teardown. A second
blocking Map is rejected by the API fixture. Set `CC` and `RUNNER` to run the
same test with MinGW and Wine.

`python3 gpu.py` uses the shipping upload routine on D3D11. The context proxy
forwards real GPU operations and can force only the initial staging Map busy.
The oracle checks every 10-bit channel value and all ignored X codes through
Load and linear SampleLevel, including padded source rows and cached reuse.
It shares the checked shader bytecode and source hash with the D3D12 sampling
oracle. Set `REQUIRE_D3D11=1` to require a compute-capable runtime. Native Linux
builds report that the D3D11 runtime is unavailable. GPU waits and readback are
test-only.
