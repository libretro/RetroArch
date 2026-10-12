## Video drivers and the threaded video wrapper

- The threaded wrapper and the drivers never block the caller: no
  sleeps, polling loops, wait-for-idle or GPU round trips on the frame
  path. Hand data across threads lock-free (see the Threads section of
  the root AGENTS.md).
- No logging and no diagnostic instrumentation in display pacing paths;
  it changes the timing being looked at.
- A platform-specific workaround costs nothing on other platforms: gate
  it (`#ifdef __APPLE__` and so on) and leave their code path as it was.
- A shader or video feature belongs in every backend that can support it
  (see "Shader and video backend changes" in the root AGENTS.md).
- Before presenting a change to gl2 or the wrapper, also build
  `make C89_BUILD=1` and a GLES2 build of gl2 with implicit declarations
  as errors.
