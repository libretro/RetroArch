# Windows ARM64 release packaging

The Windows ARM64 workflow preserves its Debug/Release build checks and raw EXE
artifacts. Release builds additionally produce `RetroArch-WinARM64.zip` and its
SHA-256 sidecar after checking the PE architecture and running the menu for
180 frames with null video and disabled audio. This smoke check does not validate GPU/audio
hardware or game compatibility.

The portable ZIP contains the executable, Ozone assets, core information files,
relative directory configuration, license notices, and source/resource provenance.
No cores or copyrighted game content are bundled. Other menu themes, shaders,
databases and controller profiles remain optional downloads. The font notices
must be reviewed when updating the asset revision pinned in the workflow.

## Publishing

Publishing a GitHub Release triggers a build of its exact tag and attaches only
the ARM64 ZIP and checksum to that existing release. It does not create another
release or change the source tarball produced by SourceRelease.yml.

Maintainers can also run the workflow manually. Leave `release_tag` empty and
`publish_release` false for an artifact-only test of the selected workflow ref.
To backfill an existing release, provide its exact tag and set `publish_release`
to true. The build verifies that HEAD is the tag's commit before upload. The
workflow's packaging script is checked out separately so that old release tags
do not need to contain it. Re-running replaces only the two ARM64 attachments.

Pull requests and normal pushes have read-only repository permissions. Only the
separate upload job receives `contents: write`, and it runs solely for a published
release or an explicit manual publishing request. No Buildbot credentials are used.

## Remaining infrastructure integration

This workflow supplies GitHub release downloads. Publishing the same packages to
`buildbot.libretro.com`, adding an ARM64 download entry on the website, and serving
ARM64 cores through the online updater are separate maintainer/infrastructure tasks.
The GitLab RetroArch build matrix and public CI templates currently have no Windows
ARM64 release target. Do not advertise those endpoints until they exist and have
been verified. The package README documents manual ARM64 core installation.
