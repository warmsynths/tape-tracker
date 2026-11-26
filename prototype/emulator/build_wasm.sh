#!/bin/sh
# Builds the Four-Track application to WebAssembly. Needs clang with the wasm32 target and wasm-ld.
set -e
cd "$(dirname "$0")/../.."
mkdir -p prototype/emulator/build
clang++ --target=wasm32 -std=c++17 -O2 -nostdlib -ffreestanding -fno-exceptions -fno-rtti -fno-threadsafe-statics \
  -Iprototype/emulator/wasm_shim -Icore/ft -Icore/ft/dsp -Icore/ft/dsp/third_party -Icore/source -Icore/libs/Audio \
  -Wl,--no-entry -Wl,--export-all -Wl,--initial-memory=33554432 -Wl,--max-memory=33554432 -Wl,-z,stack-size=262144 \
  -o prototype/emulator/build/ft.wasm \
  core/ft/wasm/ft_wasm.cpp core/ft/App.cpp core/ft/Pages.cpp core/ft/PageDraw.cpp core/ft/Canvas.cpp core/ft/View.cpp core/ft/dsp/Dsp.cpp core/ft/dsp/Synth.cpp core/ft/dsp/third_party/plaits/resources.cc core/source/FourTrackSession.cpp
ls -la prototype/emulator/build/ft.wasm
