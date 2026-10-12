## libretro-common

This tree is shared: the standalone libretro-common repository and many
cores carry copies of it.

- Do not make it depend on RetroArch's own headers, globals or settings.
- 2 KiB is the stack frame ceiling here, enforced by tools/stack_budget.py
  (see CODING-GUIDELINES).
- C89 only, `/* */` comments, declarations at the top of a block.
- A large optional library is gated behind its own HAVE_ define (as
  HAVE_CRYPTO is) that the small-RAM consoles (GameCube, Wii, PS2) do not
  set.
- Tests live in samples/ and test/; they must build with a Linux toolchain
  and with mingw-w64. `tools/ci_local.py which` finds the workflows that
  run them.
