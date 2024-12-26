# RetroArch Web Player

The RetroArch Web Player is RetroArch compiled through [Emscripten](http://kripken.github.io/emscripten-site/). The following outlines how to compile RetroArch using Emscripten, and running it in your browser.

## Compiling

To compile RetroArch with Emscripten, you'll first have to [download and install the Emscripten SDK](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html) at 3.1.46:

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
emmake make -f Makefile.emscripten LIBRETRO=fceumm -j all
cp fceumm_libretro.{js,wasm} pkg/emscripten/libretro
```

## Dependencies

The emscripten build in the retroarch tree does not contain the necessary web assets for a complete RetroArch installation.  You'll need the asset package from the latest emscripten nightly build ( https://buildbot.libretro.com/nightly/emscripten/ ); take its `assets/` folder and put it into `pkg/emscripten/libretro`.  This `assets/` folder should contain a `frontend/` directory and a `cores/` directory.

If you're building your own frontend asset bundle (i.e. modifying `frontend/bundle/`), you'll need to turn the bundle into zipped partfiles.  Open a terminal in `assets/frontend` and `zip -r9 bundle.zip bundle && split -b 30M bundle.zip bundle.zip.` (this should work on Mac and Linux, please file a PR with instructions for Windows).

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

Now build the frontend with the pthreads env variable: (2 is the number of workers this can be any integer)

```
cd ~/retroarch/RetroArch
pthread=2 emmake make -f Makefile.emscripten LIBRETRO=melonds && cp melonds_libretro.* pkg/emscripten/libretro
```

Your resulting output will be located in:

```
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.js
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.wasm
~/retroarch/RetroArch/pkg/emscripten/libretro/melonds_libretro.worker.js
```

## Setting up your webserver (Threaded)

Unless loading from `localhost` you will need to server the content from an HTTPS endpoint with a valid SSL certificate. This is a security limitation imposed by the browser. Along with that you will need to set content control policies with special headers in your server: 

In Nodejs with express:

```
app.use(function(req, res, next) {
  res.header("Cross-Origin-Embedder-Policy", "require-corp");
  res.header("Cross-Origin-Opener-Policy", "same-origin");
  next();
});
```

In NGINX: (site config under `server {`)

```
  add_header Cross-Origin-Opener-Policy same-origin;
  add_header Cross-Origin-Embedder-Policy require-corp;
```
