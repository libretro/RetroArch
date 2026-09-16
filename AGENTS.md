AGENTS.md

## Project

RetroArch is primarily written in C89. Preserve the existing language and style of the file being edited.

- New C code must be C89-compatible unless the surrounding code explicitly requires otherwise.
- Do not introduce C99/C11 features into C89 code.
- Existing files written in C++, Objective-C, or another language should remain in that language.
- Follow the conventions of surrounding code rather than introducing a new style.
- Prefer the smallest change that completely solves the task.
- Preserve existing architecture

RetroArch contains deliberately complex, cross-file systems. Do not simplify, consolidate, refactor, or redesign an existing subsystem merely because its structure appears unnecessarily complicated.

Complexity may exist for compatibility, platform, driver, or architectural reasons.

When implementing a feature, find an existing feature with similar behavior and follow its implementation pattern. The current source tree takes precedence over historical examples.

Do not make unrelated cleanup or refactoring changes.

## Comments

Keep comments as short as possible.

- Prefer no comment when the code is self-explanatory.
- When a comment is necessary, prefer a few words or a few lines at most.
- Explain why something is non-obvious, rather than describing what obvious code does.
- Do not add comments merely because code was changed.
- Do not describe the previous implementation or behavior unless that historical context is necessary to understand the current code.
- Do not use comments as miniature documentation or change logs.
- Prefer clear names and straightforward code over explanatory comments.

Comments should describe information that will remain useful after the current change is forgotten.

## Commit messages

Keep commit messages concise and focused.

- Explain what changed and why.
- Prefer a short subject line and a brief body when additional context is useful.
- Do not enumerate every changed file or implementation detail.
- Do not describe intermediate attempts, discarded approaches, or the development process.
- Do not explain the previous implementation unless it is necessary to understand the change.
- Keep the complete commit message below approximately 600 words.
- A commit message is not a changelog or development diary.

## Menu changes

Adding or modifying a menu option commonly requires changes in several parts of the codebase.

Do not assume that a menu option can be implemented by modifying only the file where it is displayed.

### Before adding a menu option:

- Find an existing menu option with similar behavior.
- Trace its complete implementation through the menu, settings, configuration, and message systems.
- Identify every part of that pattern that applies to the new option.
- Implement the new option consistently across all required layers.
- Do not modify unrelated menu code.

#### Useful reference commits:

- 6e9aa66aea2b2b3d0687585e41bac658ac9ae252 — SMB timeout/context settings exposed as menu options.
- 434f41b935232325ed13024786653f9dfef49643 — CRT menu additions.
- 23170b82ec58c2b283a7132b85fa73e18050a5ae — MIDI device dropdown menu items.
05faba73e3fed69a5e118df2cc28d31309089243 — menu implementation example.

These are reference implementations, not fixed lists of files to modify. The required files depend on the feature.

## Shader and video backend changes

Shader functionality frequently crosses shared shader code and multiple video backends.

### When adding or modifying shader functionality:

- Find an existing shader feature with similar behavior.
- Trace the feature through the shared shader infrastructure.
- Identify every applicable shader implementation and video backend.
- Keep backend implementations consistent where the feature is supported.
- Do not update only the backend currently being tested if the feature is expected to work elsewhere.
- Check platform/backend-specific differences rather than blindly duplicating code.

#### Useful reference commits:

- da5ecaa45a862e51783eca1280f46fbfb7984ede — OriginalAspect and OriginalAspectRot uniforms.
- 2a56a827e8a97a3838a9d455f1dca442c0ffbb07 — frametime/original-FPS shader uniforms.
- 37cfa4857d064464754e800bad89ce69e81230fe — sensor shader uniforms across multiple shader and video/input backends.

These commits demonstrate the breadth of changes that may be required. They are examples of implementation patterns, not instructions to reproduce their exact file lists.

## Historical commits

Historical commits may be used to understand established implementation patterns.

- Treat examples as references, not specifications.
- Prefer the current source tree when it differs from an historical commit.
- Do not copy historical changes mechanically.
- Do not reproduce obsolete workarounds merely because they appear in an old commit.
- Inspect surrounding current code before applying a historical pattern.

## Scope of changes

Keep changes focused.

### Do not:

- reformat unrelated code;
- rename unrelated symbols;
- modernize C89 code;
- replace established patterns with preferred patterns;
- refactor neighboring code without a direct need;
- add documentation or comments solely to explain the current change;
- make speculative improvements.

If a task requires a broad change, make the breadth necessary for the feature rather than artificially minimizing the number of files changed.

A change touching many files is not inherently a problem in RetroArch. Missing a required layer of an established multi-file system is a problem.
