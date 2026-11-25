#!/bin/sh
# Runs every host test: the shared Four-Track core with a fake machine, the audio maths, and a
# check of the button registrations. No hardware or toolchain needed (clang++ and python3).
set -e
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/ft-tests
mkdir -p "$OUT"
INC="-Icore/ft -Icore/ft/dsp -Icore/source -Icore/libs/Audio -Itests/host"
CXX=${CXX:-clang++}
for t in four_track_session loop_bars loop_fill loop_mix; do
  [ -f tests/host/${t}_test.cpp ] || continue
  $CXX -std=c++17 $INC tests/host/${t}_test.cpp core/source/FourTrackSession.cpp -o "$OUT/$t" 2>/dev/null || $CXX -std=c++17 $INC tests/host/${t}_test.cpp -o "$OUT/$t"
  echo "== $t"; "$OUT/$t"
done
echo "== ft_dsp"
$CXX -std=c++17 $INC tests/host/ft_dsp_test.cpp core/ft/dsp/Dsp.cpp -o "$OUT/ft_dsp" && "$OUT/ft_dsp"
echo "== ft_app"
$CXX -std=c++17 $INC tests/host/ft_app_test.cpp core/ft/*.cpp core/ft/dsp/*.cpp core/source/FourTrackSession.cpp -o "$OUT/ft_app" && "$OUT/ft_app"
echo "== button bindings"
python3 tests/check_button_bindings.py
echo "all host tests passed"
