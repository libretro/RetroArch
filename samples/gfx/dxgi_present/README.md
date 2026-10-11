Run `python3 samples/gfx/dxgi_present/run.py` from a configured checkout.
The test extracts the production pacing and submission functions and checks
GPU queue ordering, fence bookkeeping, occlusion recovery, failed presents,
VSync interval clamping and optional tearing with portable COM mocks.
The resize and teardown tests check resource retention, first-frame pacing,
queue draining, partial initialization and failures. Vulkan acquire tests
check that unavailable frames preserve a valid image index and command
buffers are allocated only once. Resize fence tests check queue locking,
submission and wait ordering, reuse, and failures.

For MinGW and Wine:

```
CC=x86_64-w64-mingw32-gcc TEST_RUNNER=wine python3 samples/gfx/dxgi_present/run.py
```

Hardware verification additionally requires Windows OpenGL external-memory
and semaphore extensions or Vulkan Win32 external-memory and semaphore
extensions, plus D3D12. Unsupported systems retain native presentation.
