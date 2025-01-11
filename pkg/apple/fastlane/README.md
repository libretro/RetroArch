fastlane documentation
----

# Installation

Make sure you have the latest version of the Xcode command line tools installed:

```sh
xcode-select --install
```

For _fastlane_ installation instructions, see [Installing _fastlane_](https://docs.fastlane.tools/#installing-fastlane)

# Available Actions

## Mac

### mac build

```sh
[bundle exec] fastlane mac build
```

Build and optionally upload the app to App Store Connect.

Command-line options (all are optional):
- `version`: Override the marketing version string; otherwise read from version.all
- `dirty`: Pass `true` to allow building from a dirty git repo
- `branch`: The name of the branch to build from; default is current. Cannot be used with `dirty`
- `upload`: Pass `false` to prevent uploading to App Store Connect
- `public`: Pass `false` to prevent making the build available to TestFlight users (still uploads)


----


## iOS

### ios build

```sh
[bundle exec] fastlane ios build
```

Build and optionally upload the app to App Store Connect.

Command-line options (all are optional):
- `version`: Override the marketing version string; otherwise read from version.all
- `dirty`: Pass `true` to allow building from a dirty git repo
- `branch`: The name of the branch to build from; default is current. Cannot be used with `dirty`
- `upload`: Pass `false` to prevent uploading to App Store Connect
- `public`: Pass `false` to prevent making the build available to TestFlight users (still uploads)


----


## appletvos

### appletvos build

```sh
[bundle exec] fastlane appletvos build
```

Build and optionally upload the app to App Store Connect.

Command-line options (all are optional):
- `version`: Override the marketing version string; otherwise read from version.all
- `dirty`: Pass `true` to allow building from a dirty git repo
- `branch`: The name of the branch to build from; default is current. Cannot be used with `dirty`
- `upload`: Pass `false` to prevent uploading to App Store Connect
- `public`: Pass `false` to prevent making the build available to TestFlight users (still uploads)


----

This README.md is auto-generated and will be re-generated every time [_fastlane_](https://fastlane.tools) is run.

More information about _fastlane_ can be found on [fastlane.tools](https://fastlane.tools).

The documentation of _fastlane_ can be found on [docs.fastlane.tools](https://docs.fastlane.tools).
