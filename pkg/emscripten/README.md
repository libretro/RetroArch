# RetroArch Web Player

The RetroArch Web Player is RetroArch compiled through [Emscripten](http://kripken.github.io/emscripten-site/). The following outlines how to compile RetroArch using Emscripten, and running it in your browser.

## Compiling

To compile RetroArch with Emscripten, you'll first have to [download and install the Emscripten SDK](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html). Once it's loaded in your shell, you'll run something like the following...

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
