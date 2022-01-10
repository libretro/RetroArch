# RetroArch Web Player

The RetroArch Web Player is RetroArch compiled through [Emscripten](http://kripken.github.io/emscripten-site/). The following outlines how to compile RetroArch using Emscripten, and running it in your browser.

## Compiling

To compile RetroArch with Emscripten, you'll first have to [download and install the Emscripten SDK](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html) at 1.39.5:

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 1.39.5
./emsdk activate 1.39.5
source emsdk_env.sh
```

Other later versions of emsdk will function and may be needed (like Async support), but in general emscripten is in a constant state of development and you may run into other problems by not pinning to 1.39.5. This is currently the version [https://web.libretro.com/](https://web.libretro.com/) is built against.

After emsdk is installed you will need to build an emulator core, move that output into Retroarch, and use helper scripts to produce web ready assets, in this example we will be building [https://github.com/libretro/libretro-fceumm](https://github.com/libretro/libretro-fceumm):

```
mkdir ~/retroarch
cd ~/retroarch
git clone https://github.com/libretro/libretro-fceumm.git
cd libretro-fceumm
emmake make -f Makefile.libretro platform=emscripten
git clone https://github.com/libretro/RetroArch.git ~/retroarch/RetroArch
cp ~/retroarch/libretro-fceumm/fceumm_libretro_emscripten.bc ~/retroarch/RetroArch/dist-scripts/fceumm_libretro_emscripten.bc
cd ~/retroarch/RetroArch/dist-scripts
emmake ./dist-cores.sh emscripten
```

The resulting build output will be located in `~/retroarch/RetroArch/pkg/emscripten` as:

```
fceumm_libretro.js
fceumm_libretro.wasm
```

## Usage

Most of the magic happens on the browser so nothing really on that regard

I you want a self hosted version you need
- A web server, nginx/apache will do, download a build here: 
  https://buildbot.libretro.com/nightly/emscripten/
- Extract the build somewhere in your web-server 
  - Grab the asset bundle:
  https://buildbot.libretro.com/assets/frontend/bundle.zip
- Unzip it in the same dir you extracted the rest, inside **/assets/frontend/bundle**
- Create an **assets/cores** dir, you can put game data in that dir so it's available under **downloads**
- chmod +x the indexer script
- run the indexer script (you need coffeescript) like this:
```
cd ${ROOT_WWW_PATH}/assets/frontend/bundle
../../../indexer > .index-xhr
cd ${ROOT_WWW_PATH}/assets/cores
../../indexer > .index-xhr
```

That should be it, you can add more cores to the list by editing index.html

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
cp fceumm_libretro_emscripten.bc ~/retroarch/RetroArch/dist-scripts/fceumm_libretro_emscripten.bc
```

Now build the frontend with the pthreads env variable: (2 is the number of workers this can be any integer)

```
cd ~/retroarch/RetroArch/dist-scripts
pthread=2 emmake ./dist-cores.sh emscripten
```

Your resulting output will be located in:

```
~/retroarch/RetroArch/pkg/emscripten/melonds_libretro.js
~/retroarch/RetroArch/pkg/emscripten/melonds_libretro.wasm
~/retroarch/RetroArch/pkg/emscripten/melonds_libretro.worker.js
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
