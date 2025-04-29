# stable-retro lib
Library to be used with emulator frontends (such as RetroArch) to enable ML models to overide player input.
Warning: Still in early prototype version

## Build for Linux

```
sudo apt update
sudo apt install git cmake unzip libqt5opengl5-dev qtbase5-dev zlib1g-dev python3 python3-pip build-essential libopencv-dev
```

```
git clone https://github.com/MatPoliquin/stable-retro-scripts.git
```

Download pytorch C++ lib:
```
cd stable-retro-scripts/ef_lib/
wget https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-shared-with-deps-2.3.1%2Bcpu.zip
unzip libtorch-cxx11-abi-shared-with-deps-2.3.1+cpu.zip
```

Generate makefiles and compile
```
cmake . -DCMAKE_PREFIX_PATH=./libtorch
make
```

## Build for Windows

Clone stable-retro-scripts repo

Download pytorch C++ lib for Windows:
```
wget https://download.pytorch.org/libtorch/cpu/libtorch-win-shared-with-deps-2.3.1%2Bcpu.zip -o libtorch_win.zip
Expand-Archive libtorch_win.zip
```
Note: 2.3.1 might have missing intel MLK dll issue:
https://github.com/pytorch/pytorch/issues/124009
So you can use nightly build instead and it fixes the issue:
wget https://download.pytorch.org/libtorch/nightly/cpu/libtorch-win-shared-with-deps-latest.zip -o libtorch_win.zip

Download and Extract OpenCV for Windows:
```
https://sourceforge.net/projects/opencvlibrary/files/4.10.0/
```
The DLLs will be found here:
YourOpenCVFolder\opencv\build\x64\vc16\lib

Generate makefiles and compile
```
cd stable-retro-scripts
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=Absolute\path\to\libtorch_win -DOpenCV_DIR=Absolute\path\to\opencv\build\x64\vc16\lib
cmake --build . --config Release
```

## Test the lib

```
export LD_LIBRARY_PATH=/path/to/game_ai.so
./retroarch
```
