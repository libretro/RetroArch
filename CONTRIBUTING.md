# Contributing to RetroArch

If you are a developer who wishes to contribute to the development of _RetroArch_; or if you have
found a bug and wish to submit a minor patch and/or bug report, please read this document.

Active discussions happen on our [Discord](https://discordapp.com/invite/27Xxm2h), mostly within
the _Programming_ channel category. We value discussions that happen in real time around
these contributions.

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

# Pull Requests
Outside contributions are generally only accepted in the form of a pull request. The process is very simple.
Fork RetroArch, make your changes, and issue a pull request on GitHub. This can all be done within the browser.
The changes are reviewed, and might be merged in. If the pull request isn't acceptable at the time,
note that it's possible to continue pushing up commits to your branch.

If you want to develop a larger feature,
we'd like to discuss this first (ideally on IRC) so that you don't risk developing something
that won't be merged. A pull request with a proof-of-concept is fine, but please indicate so.

## libretro API
If you wish to add functionality to libretro's API, it can take some time to merge in, because changes
to libretro API will affect other projects as well, and we highly value API/ABI stability.
Features will only be added when deemed *necessary* for a concrete libretro core to function properly.
Features will not be added on basis of hypothetical libretro implementations.

# Coding style
Having a consistent code style throughout the code base is highly valued.
Please look through the code to get a feel for the coding style.
A pull request may be asked to fix the coding style before submission.
In other cases, a pull request may be followed up with a "style nit commit".

## Non-obvious things:
  - Code should be both C89 and ISO C++ compatible. This dual requirement is for XBox360 and MSVC in general. Think of it as a C++ compatible subset of C99.
  - Warnings are not allowed (-Wall). Make sure your code is warning-free. Note that warning sensitivity differs a bit across compiler versions.
  - Using deprecated APIs is discouraged.

# Copyright Headers and AUTHORS
If you have contributed to a part of a source file (a chunk of code that's written by you),
you should add yourself to the copyright header in that file.
If you have contributed significantly
(a feature, a contribution you can "name", e.g. "Added audio driver foo"), you should add yourself to AUTHORS file.
We'd like your full name and email, and which features you have been part of.

# Commit Access
Contributors who show a track record of making good pull requests over time will eventually get commit access to the repo.
This typically happens when the "overhead" of looking through pull requests over time becomes a burden.
