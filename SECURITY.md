# Security Policy

## Reasonable expectations

RetroArch is a frontend for the libretro API. The main functionality is fulfilled by invoking other binary libraries ("cores") which are not restricted by RetroArch in any way. Cores are able to read/write/delete files, spawn processes, communicate over the network. Also, source for cores is not necessarily in control by libretro team, and core binaries / RetroArch binaries are not signed. For this reason, it is a bad idea to use RetroArch or any other libretro frontend on security critical systems.

Also, RetroArch and cores have been packaged in several ways. Content on the [official download site](https://buildbot.libretro.com/) is built from a direct mirror of the original RetroArch and core repositories, no binaries are reused. Note that source for the core repositories may be outside libretro team control.

## Supported Versions

For most delivery channels, libretro team does not have control over the version. The exceptions are:
- [official download site](https://buildbot.libretro.com/)
- Steam release
- Apple App Store release
- various Android app store releases
- note that Google Play Store version is years behind and can not be updated

You may report vulnerability against any recent version, but be reasonable.

## Reporting a Vulnerability

Please report security vulnerabilities at libretro@gmail.com

## Possible remediation

Due to the variety of delivery channels, RetroArch team can not recall any given version universally. Security fixes are accepted for next release, and notice may be posted in the channels controlled by RetroArch team, depending on the severity assessed by RetroArch team.
