# Embedded native shader contracts

Run `make check` on Windows with MinGW-w64, or
`xvfb-run -a make RUNNER=wine check` on Linux with the MinGW cross compiler,
Wine, and a D3D11-capable software or hardware device. The test includes the
production source headers with the host C compiler, so MSVC/MinGW string
preprocessing differences cannot silently fall back to runtime compilation.

It checks every committed stage, null-terminated and explicit-length source
keys, borrowed byte storage, COM reference counting, and native D3D11 object
creation for SM4/SM5. SM2/SM3 token profiles and SM6 DXIL profiles are checked
by the embedder. A second executable checks that the production catalog excludes unused
SM2/SM3 and pixel fallback assets while retaining native draw/compute paths.
No FXC/DXC is needed to build or run this test.
