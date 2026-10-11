Run `python3 samples/gfx/dxgi_present/run.py` from a configured checkout.
The test extracts the production pacing and submission functions and checks
GPU queue ordering, fence bookkeeping, occlusion recovery, failed presents,
VSync interval clamping and optional tearing with portable COM mocks.
The teardown test checks that presentation drains before resource release,
including partial initialization and queue or event failures.

For MinGW and Wine:

```
CC=x86_64-w64-mingw32-gcc TEST_RUNNER=wine python3 samples/gfx/dxgi_present/run.py
```

Hardware verification additionally requires Windows OpenGL external-memory
and semaphore extensions or Vulkan Win32 external-memory and semaphore
extensions, plus D3D12. Unsupported systems retain native presentation.
