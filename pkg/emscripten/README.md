# RetroArch Web Player

The RetroArch Web Player is RetroArch compiled through [Emscripten](https://emscripten.org/).
The following outlines how to compile RetroArch using Emscripten, and running it in your browser.

## Compiling the single-threaded player

To compile RetroArch with Emscripten, you'll first have to
[download and install the Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) at 3.1.46:

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 3.1.46
./emsdk activate 3.1.46
. emsdk_env.sh
```

Other later versions of emsdk will function and may be needed,
but in general emscripten is in a constant state of development and you may run into other problems by not pinning to 3.1.46.
This is currently the version [web.libretro.com](https://web.libretro.com/) is built against.

After emsdk is installed you will need to build a libretro core, copy its static library (.bc or .a file) into Retroarch,
and use helper scripts to produce web ready assets.
In this example we will be building [fceumm](https://github.com/libretro/libretro-fceumm):

```
mkdir ~/retroarch
cd ~/retroarch
git clone https://github.com/libretro/libretro-fceumm.git
cd libretro-fceumm
emmake make -f Makefile.libretro platform=emscripten

git clone https://github.com/libretro/RetroArch.git ~/retroarch/RetroArch
cp ~/retroarch/libretro-fceumm/fceumm_libretro_emscripten.bc ~/retroarch/RetroArch/libretro_emscripten.bc

cd ~/retroarch
emmake make -f Makefile.emscripten LIBRETRO=fceumm -j all
cp fceumm_libretro.* pkg/emscripten/libretro
```

### Dependencies

The emscripten build in the retroarch tree does not contain the necessary web assets for a complete RetroArch installation.
You'll need the asset package from the latest emscripten nightly build (https://buildbot.libretro.com/nightly/emscripten/)
Take its `assets` folder and put it into `pkg/emscripten/libretro`.
This `assets` folder should contain a `frontend` directory and a `cores` directory.

If you're building your own frontend asset bundle (i.e. modifying `assets/frontend/bundle`), you'll need to turn the bundle into zipped partfiles.
Open a terminal in `assets/frontend` and run `zip -r9 bundle.zip bundle && split -b 30M bundle.zip bundle.zip.`
(This should work on Mac and Linux, please file a PR with instructions for Windows).

If you want to add more built-in core content files to `assets/cores`, you need to re-run the indexer script:

1. `chmod +x indexer`
2. run the indexer script (you need coffeescript installed) from a terminal opened at `assets/cores`: `../../indexer > .index-xhr`

### Usage

You need a web server. Nginx, apache, node's http-server, and python's http.server are all known to work.

Point your web server to the `pkg/emscripten/libretro` directory or unzipped nightly build
(or move those files to somewhere your web server can reach them), and everything should just work.

## Threaded cores

Some cores can be compiled with threading support to improve performance. (Some cores require threads.)
You will need to compile the core and frontend with special flags to support this and also serve the content
from an HTTPS endpoint with specific headers.

### Compiling the core (threaded)

In this example we will be building [melonDS](https://github.com/libretro/melonDS) with pthreads support.
We assume you allready have emsdk setup and are familiar with the build process.

First clone the repo:

```
git clone https://github.com/libretro/melonDS.git
cd melonDS
```

Next modify the Makefile to enable threads:

```
else ifeq ($(platform), emscripten)
   TARGET := $(TARGET_NAME)_libretro_emscripten.bc
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=$(CORE_DIR)/link.T -Wl
   HAVE_THREADS = 1
   CFLAGS += -pthread
   LDFLAGS += -pthread
   CXXFLAGS += -pthread
```

Build and move output to the frontend:

```
emmake make -f Makefile platform=emscripten
cp melonds_libretro_emscripten.bc ~/retroarch/RetroArch/libretro_emscripten.bc
```

Now build the frontend with the threads enabled:

```
cd ~/retroarch/RetroArch
emmake make -f Makefile.emscripten LIBRETRO=melonds HAVE_THREADS=1 -j all
cp melonds_libretro.* pkg/emscripten/libretro
```

Your resulting output will be located in:

```
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.js
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.wasm
```

### Setting up your web server (threaded)

To support multithreaded builds, you will need to serve the content from an HTTPS endpoint with a valid SSL certificate.
This is a security limitation imposed by web browsers.
Along with that you will need to set content control policies with special headers in your server:

In Nodejs with express:

```
app.use(function(req, res, next) {
  res.header("Cross-Origin-Embedder-Policy", "require-corp");
  res.header("Cross-Origin-Opener-Policy", "same-origin");
  res.header("Cross-Origin-Resource-Policy", "same-origin");
  next();
});
```

In NGINX: (site config under `server {`)

```
  add_header Cross-Origin-Opener-Policy same-origin;
  add_header Cross-Origin-Embedder-Policy require-corp;
  add_header Cross-Origin-Resource-Policy same-origin;
```

Node http-server:

```
http-server . -S \
  --header "Cross-Origin-Opener-Policy: same-origin" \
  --header "Cross-Origin-Embedder-Policy: require-corp" \
  --header "Cross-Origin-Resource-Policy: same-origin"
```

If your hosting provider does not allow you to set custom response headers,
client-side service workers such as [coi-serviceworker](https://github.com/gzuidhof/coi-serviceworker) can be used instead.

## Compiling the frontend with PROXY_TO_PTHREAD

To compile RetroArch with PROXY_TO_PTHREAD to make use of OPFS and other features,
you'll first have to [download and install the Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).
Currently, we need at least version 4.0.15:

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 4.0.15
./emsdk activate 4.0.15
. emsdk_env.sh
```

The steps for compiling a libretro core are the same as normal, the only difference being the compile settings in RetroArch:

```
mkdir ~/retroarch
cd ~/retroarch
git clone https://github.com/libretro/libretro-fceumm.git
cd libretro-fceumm
emmake make -f Makefile.libretro platform=emscripten

git clone https://github.com/libretro/RetroArch.git ~/retroarch/RetroArch
cp ~/retroarch/libretro-fceumm/fceumm_libretro_emscripten.bc ~/retroarch/RetroArch/libretro_emscripten.bc

cd ~/retroarch
emmake make -f Makefile.emscripten LIBRETRO=fceumm PROXY_TO_PTHREAD=1 HAVE_EXTRA_WASMFS=1 HAVE_AUDIOWORKLET=1 HAVE_RWEBAUDIO=0 -j all
cp fceumm_libretro.* pkg/emscripten/libretro-thread
```

### Dependencies

The emscripten build in the retroarch tree does not contain the necessary web assets for a complete RetroArch installation.
While it supports the regular desktop asset and content downloaders, we also provide a small bundle of UI assets for first launch.
You can obtain these files from the nightly Emscripten build on the buildbot, or make them yourself by running
`zip -r9 bundle-minimal.zip assets/ozone assets/pkg assets/sounds info`
(essentially, just the `assets/ozone`, `assets/pkg`, `assets/sounds`, and `info` folders from the regular asset package).

### Usage

Hosting the threaded web build is the same as for the multi-threaded cores above; SSL and proper CORS headers are required.

## Compile-time settings

Here is a non-exhaustive list of compile-time settings (makefile variables).

`HAVE_THREADS` (default: 0): Enables threads. Requires special headers detailed above to enable shared memory.

`PROXY_TO_PTHREAD` (default: 0): Runs RetroArch on a pthread instead of the browser thread; implies `HAVE_THREADS`.

`HAVE_OPENGLES3` (default: 0): Enables GLES 3.0 (WebGL 2). If left disabled, GLES 2.0 (WebGL 1) will be used.

`CLOSURE_COMPILER` (default: 0): Runs the closure compiler on the output JS file, typically reducing its size by around 50%.

`ASSERTIONS` (default: 0): Enables runtime assertions (always enabled in debug builds). Please use this when testing new emscripten-specific features!

### Audio settings

`HAVE_AUDIOWORKLET` (default: 0): Enables the AudioWorklet audio driver. Requires `HAVE_THREADS`.

`HAVE_RWEBAUDIO` (default: 1): Enables the RWebAudio audio driver. Currently incompatible with `PROXY_TO_PTHREAD`.

`HAVE_AL` (default: 0): Enables the OpenAL audio driver. Requires `ASYNC` or `PROXY_TO_PTHREAD`. Prefer another driver.

`ALLOW_AUDIO_BUSYWAIT` (default: 0): Deprecated; not recommended to use this.

### Async settings

`ASYNC` (default: 0): Enables asyncify, which enables sleeping on the browser thread and fibers.
This has a noticeable impact on compile time, output wasm size, and speed. See https://emscripten.org/docs/porting/asyncify.html for more info.

`MIN_ASYNC` (default: 0): Better performance than full asyncify, but sleeping on the browser thread is only possible in some places.

`JSPI` (default: 0): Enables JSPI, an experimental asyncify alternative. Currently (emscripten 4.0.15) this requires a patched emscripten toolchain.
Cores should be compiled with `-fwasm-exceptions`.

### Filesystem settings

`HAVE_WASMFS` (default: 0): Enables WasmFS, which is recommended when using `HAVE_THREADS`.

`HAVE_EXTRA_WASMFS` (default: 0): Enables OPFS (origin private file system) and FETCHFS, requires `PROXY_TO_PTHREAD` or `JSPI`.

`FS_DEBUG` (default: 0): Enables javascript filesystem tracking; currently incompatible with `HAVE_WASMFS`.

## Development

Most of the emscripten frontend code is contained in `frontend/drivers/platform_emscripten.c`.
`frontend/drivers/platform_emscripten.h` provides and documents several helper functions that are used across the input (rwebinput),
audio (rwebaudio/audioworklet), and video context drivers (emscripten webgl/egl).

When testing emscripten-specific changes, please use `ASSERTIONS=1` to provide better error checking.

## TODO

- Improve the "Development" section above
- Netplay
- Microphone driver (rwebaudio/audioworklet)
- Crash handling
- Async command interface?
