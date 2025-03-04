# RetroArch Web Player

The RetroArch Web Player is RetroArch compiled through [Emscripten](https://emscripten.org/). The following outlines how to compile RetroArch using Emscripten, and running it in your browser.

## Compiling the Single-Threaded Player

To compile RetroArch with Emscripten, you'll first have to [download and install the Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) at 3.1.46:

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 3.1.46
./emsdk activate 3.1.46
source emsdk_env.sh
```

Other later versions of emsdk will function and may be needed, but in general emscripten is in a constant state of development and you may run into other problems by not pinning to 3.1.46. This is currently the version [https://web.libretro.com/](https://web.libretro.com/) is built against.

After emsdk is installed you will need to build an emulator core, move that output into Retroarch, and use helper scripts to produce web ready assets, in this example we will be building [https://github.com/libretro/libretro-fceumm](https://github.com/libretro/libretro-fceumm):

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
cp fceumm_libretro.{js,wasm} pkg/emscripten/libretro
```

## Dependencies

The emscripten build in the retroarch tree does not contain the necessary web assets for a complete RetroArch installation.  You'll need the asset package from the latest emscripten nightly build ( https://buildbot.libretro.com/nightly/emscripten/ ); take its `assets/` folder and put it into `pkg/emscripten/libretro`.  This `assets/` folder should contain a `frontend/` directory and a `cores/` directory.

If you're building your own frontend asset bundle (i.e. modifying `frontend/bundle/`), you'll need to turn the bundle into zipped partfiles.  Open a terminal in `assets/frontend/` and `zip -r9 bundle.zip bundle && cd .. && split -b 30M bundle.zip bundle.zip.` (this should work on Mac and Linux, please file a PR with instructions for Windows).

If you want to add more built-in core content files to `assets/cores`, you need to re-run the indexer script:

1. `chmod +x indexer`
2. run the indexer script (you need coffeescript installed) from a terminal opened at `assets/cores`: `../../indexer > .index-xhr`

## Usage

You need a web server.  Nginx, apache, node's http-server, and python's http.server are all known to work.

Point your webserver to the `pkg/emscripten/libretro/` directory or unzipped nightly build (or move those files to somewhere your webserver can reach them), and everything should Just Work.

# Threaded emulators

Some emulators can be compiled with `pthreads` support to increase performance. You will need to compile the core and frontend with special flags to support this and also serve the content from an HTTPS endpoint with specific headers.

## Compiling the code (Threaded)

In this example we will be building [melonDS](https://github.com/libretro/melonDS) with pthreads support. We assume you allready have emsdk setup and are familiar with the build process.

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

Now build the frontend with the pthreads env variable: (2 is the number of workers; this can be any integer, but many browsers limit the number of workers)

```
cd ~/retroarch/RetroArch
emmake make -f Makefile.emscripten LIBRETRO=melonds PTHREAD=2 && cp melonds_libretro.* pkg/emscripten/libretro
```

Your resulting output will be located in:

```
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.js
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.wasm
```

## Setting up your webserver (Threaded)

To support multithreaded builds, you will need to serve the content from an HTTPS endpoint with a valid SSL certificate. This is a security limitation imposed by the browser. Along with that you will need to set content control policies with special headers in your server:

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

# Compiling the Multi-Threaded Frontend

To compile the multi-threaded RetroArch frontend with Emscripten and make use of wasmfs and other features, you'll first have to [download and install the Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).  Currently, we need the "top of tree" or latest version of Emscripten:

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install tot
./emsdk activate tot
source emsdk_env.sh
```

After emsdk is installed you will need to build an emulator core, move that output into Retroarch, and use helper scripts to produce web ready assets, in this example we will be building [https://github.com/libretro/libretro-fceumm](https://github.com/libretro/libretro-fceumm):

```
mkdir ~/retroarch
cd ~/retroarch
git clone https://github.com/libretro/libretro-fceumm.git
cd libretro-fceumm
emmake make -f Makefile.libretro platform=emscripten
git clone https://github.com/libretro/RetroArch.git ~/retroarch/RetroArch
cp ~/retroarch/libretro-fceumm/fceumm_libretro_emscripten.bc ~/retroarch/RetroArch/libretro_emscripten.bc

cd ~/retroarch
emmake make -f Makefile.emscripten LIBRETRO=fceumm PROXY_TO_PTHREAD=1 PTHREAD=4 HAVE_WASMFS=1 ASYNC=0 HAVE_EGL=0 -j all
cp fceumm_libretro.{js,wasm} pkg/emscripten/libretro-thread
```

## Dependencies

The emscripten build in the retroarch tree does not contain the necessary web assets for a complete RetroArch installation.  While it supports the regular desktop asset and content downloaders, we also provide a small bundle of UI assets for first launch.  You can obtain these files from the nightly Emscripten build on the buildbot, or make them yourself by `zip -r9 bundle-minimal.zip bundle` (essentially, just the `assets/ozone`, `assets/pkg`, and `assets/sounds` folders from the regular asset package).

## Usage

Hosting the threaded web build is the same as for the multi-threaded emulators above; SSL and proper CORS headers are required.
