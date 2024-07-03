#!/bin/bash

if [[ "${1,,}" == "clean" ]]; then 
  rm -rf build/ release/
fi

MISSING_DEPS=0
if [ ! $(which cmake) ]; then
  echo "Missing cmake!"
  MISSING_DEPS=1
fi
if [ ! $(which arm-none-eabi-gcc) ]; then
  echo "missing arm gcc!"
  MISSING_DEPS=1
fi
if [ $(python3 -c "from elftools.elf import elffile") ]; then
  echo "missing elftools!"
  MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
  exit 1
fi

USE_CCACHE=""
if [ $(which ccache) ]; then
  USE_CCACHE="-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache"
fi

pushd deps/micropython/ports/rp2
  make BOARD=PICO submodules
popd

pushd $(dirname -- "${BASH_SOURCE[0]}")
  mkdir -p build/ release/
  pushd build
    # cmake .. -DCMAKE_INSTALL_PREFIX=$(pwd)/../release
    cmake ../deps/micropython/ports/rp2 "-DPICO_SDK_PATH_OVERRIDE=$(pwd)/../deps/micropython/lib/pico-sdk" "-DUSER_C_MODULES=$(realpath $(pwd)/../CMakeLists.txt)" -DCMAKE_INSTALL_PREFIX=$(realpath $(pwd)/../release) $USE_CCACHE
    cmake --build .
    cmake --install . 
  popd
popd
