# Contributing to RetroArch

If you are a developer who wishes to contribute to the development of _RetroArch_; or if you have
found a bug and wish to submit a minor patch and/or bug report, please read this document.

Active discussions happen on our [Discord](https://discordapp.com/invite/27Xxm2h), mostly within
the _Programming_ channel category. We value discussions that happen in real time around
these contributions.

Please do note that contributors to _RetroArch_ do such contributions within their spare time.
We do prefer to keep a professional and non-aggressive atmosphere around the project, along
with any disagreements to be settled professionally without insults, name calling, or otherwise.
If there are any issues, we are willing to have discussions about it.

## Submitting Bug Reports

Bug reports in _RetroArch_ may fall into one of two categories:

 * _RetroArch_ itself, the user interface and API around all of the various cores.
 * Individual _Core_, of which interact with _RetroArch_.

When submitting a bug report, ensure that the report is submitted to the correct repository.
For _RetroArch_ itself, it is done by reporting a bug within the
[RetroArch](https://github.com/libretro/RetroArch) repository. For other cores, please use
the search function within the [libretro Organization](https://github.com/libretro) on
GitHub. Issues that are specific to a core and not _RetroArch_ are likely to be closed very
quickly. If an issue is suspected with _RetroArch_, please make sure to test with multiple
cores to be sure that is is not isolated.

If the issue occurs during runtime, please paste the verbose log output:

 * If using the _Pheonix_ interface, the log will be in _File_ -> _Show Log_.
 * If using the main interface, enable verbose logging with _Settings_ -> _Logging_ ->
   _Logging Verbosity_. Ensure both _Log to File_ and _Timestamp log Files_ is enabled.
 * Otherwise, run _RetroArch_ with the verbose (`-v`) flag.

If the error happens during compilation and/or building, paste the output of `./configure`
and `make` accordingly. If using an IDE, please paste any of the errors and log output.

## Submitting Pull Requests

Any and all contributions should be submitted through Pull Requests on
[GitHub](https://github.com/libretro/RetroArch/pulls). The process requires that you fork the
repository, make the appropriate changes, and then open a pull request on _GitHub_. If your
pull request is for a proof-of-concept then please indicate as such.

Your pull request will then be reviewed. There may be comments and requests for additional
changes to be made. It may also be possible that the changes will not be accepted. Otherwise, it
may be merged in when it is fully approved. The final approval of merge requests is at the
discretion of the project.

If you want to develop a larger feature or make broad changes, please do join our
[Discord](https://discordapp.com/invite/27Xxm2h) server to discuss. The discussion is
necessary to prevent the possibility of major work being done which will not be accepted at all.

## libretro API

If you wish to contribute additional functionality to _libretro_'s API, there are considerations
that must be accepted. Please note that because this API affects multiple different projects, we
highly value and require API and ABI stability and backwards-compatibility. Due to this
requirement, there will be additional scrutiny in reviews for this added functionality.

Any and all features will be added only when **necessary** for an existing _libretro_ core to
properly function. Hypothetical implementations of _libretro_ are not considered.

## Coding style

We highly value a consistent code style throughout the entire code base, please make sure you look
through the existing code to get a feel for the coding style. When submitting a pull request, it may
be asked to fix any coding style issues before submission. In other cases, there may be a follow-up
pull request making the code style consistent.

For full guidelines please see the [Coding Standards](https://docs.libretro.com/development/coding-standards/).

Some non-obvious things to be aware of:

  - Code should be both C89 and ISO C++ compatible. This is a requirement for XBox 360 and MSVC to
    properly build. Think of it as a C++ compatible subset of C99.
  - There must be no warnings in your code (enabled by `-Wall` for GCC compilers), do also note that
    different compilers may produce different warnings.
  - Avoid using deprecated APIs, these will be removed in the future at some point.

## Copyright Headers and AUTHORS

If you have contributed a chunk of source code that is written to you, you should add yourself to
the copyright header in the file. If you have made a significant contribution you should add
yourself to the `AUTHORS` file, adding your full name, e-mail, and the feature you worked on.

## Commit Access

Contributors who show a good track record of pull requests over time may eventually
get commit access to the repository. This may happen when looking through pull requests
over long amounts of time becomes a burden.
