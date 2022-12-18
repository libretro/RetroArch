[![Publish Docker Image](https://github.com/wiiu-env/libmocha/actions/workflows/push_image.yml/badge.svg)](https://github.com/wiiu-env/libmocha/actions/workflows/push_image.yml)

**This library is still WIP, may not work as expected or have breaking changes in the near future**

# libmocha
Requires the [MochaPayload](https://github.com/wiiu-env/MochaPayload) to be running via [EnvironmentLoader](https://github.com/wiiu-env/EnvironmentLoader).  
Requires [wut](https://github.com/devkitPro/wut) for building.
Install via `make install`.

## Usage
Make sure to add `-lmocha` to `LIBS` and `$(WUT_ROOT)/usr` to `LIBDIRS` in your makefile.

After that you can simply include `<mocha/mocha.h>` to get access to the mocha functions after calling `Mocha_InitLibrary()`.

## Use this lib in Dockerfiles.
A prebuilt version of this lib can found on dockerhub. To use it for your projects, add this to your Dockerfile.
```
[...]
COPY --from=wiiuenv/libmocha:[tag] /artifacts $DEVKITPRO
[...]
```
Replace [tag] with a tag you want to use, a list of tags can be found [here](https://hub.docker.com/r/wiiuenv/libmocha/tags). 
It's highly recommended to pin the version to the **latest date** instead of using `latest`.

## Format the code via docker

`docker run --rm -v ${PWD}:/src wiiuenv/clang-format:13.0.0-2 -r ./source ./include -i`
