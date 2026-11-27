#!/bin/sh
# Runs every host test: the shared Four-Track core with a fake machine, the audio maths, and a
# check of the button registrations. No hardware or toolchain needed (clang++ and python3).
set -e
cd "$(dirname "$0")/.."
OUT=${TMPDIR:-/tmp}/ft-tests
mkdir -p "$OUT"
INC="-Icore/ft -Icore/ft/dsp -Icore/ft/dsp/third_party -Icore/source -Icore/libs/Audio -Itests/host"
RES=core/ft/dsp/third_party/plaits/resources.cpp
CXX=${CXX:-clang++}
for t in four_track_session loop_bars loop_fill loop_mix; do
  [ -f tests/host/${t}_test.cpp ] || continue
  $CXX -std=c++17 $INC tests/host/${t}_test.cpp core/source/FourTrackSession.cpp -o "$OUT/$t" 2>/dev/null || $CXX -std=c++17 $INC tests/host/${t}_test.cpp -o "$OUT/$t"
  echo "== $t"; "$OUT/$t"
done
echo "== ft_dsp"
$CXX -std=c++17 $INC tests/host/ft_dsp_test.cpp core/ft/dsp/Dsp.cpp core/ft/dsp/SendFx.cpp -o "$OUT/ft_dsp" && "$OUT/ft_dsp"
echo "== ft_latency"
$CXX -std=c++17 $INC tests/host/ft_latency_test.cpp core/ft/Latency.cpp -o "$OUT/ft_latency" && "$OUT/ft_latency"
echo "== ft_sendfx"
$CXX -std=c++17 $INC tests/host/ft_sendfx_test.cpp core/ft/dsp/SendFx.cpp core/ft/dsp/Dsp.cpp -o "$OUT/ft_sendfx" && "$OUT/ft_sendfx"
echo "== ft_drums"
$CXX -std=c++17 $INC tests/host/ft_drums_test.cpp core/ft/dsp/Drums.cpp $RES -o "$OUT/ft_drums" && "$OUT/ft_drums"
echo "== ft_roles"
$CXX -std=c++17 $INC tests/host/ft_roles_test.cpp core/ft/Roles.cpp core/ft/Chords.cpp -o "$OUT/ft_roles" && "$OUT/ft_roles"
echo "== ft_art"
$CXX -std=c++17 $INC tests/host/ft_art_test.cpp core/ft/SoundArt.cpp core/ft/dsp/SynthPatch.cpp -o "$OUT/ft_art" && "$OUT/ft_art"
echo "== ft_chords"
$CXX -std=c++17 $INC tests/host/ft_chords_test.cpp core/ft/Chords.cpp -o "$OUT/ft_chords" && "$OUT/ft_chords"
echo "== ft_synth"
$CXX -std=c++17 $INC tests/host/ft_synth_test.cpp core/ft/dsp/Synth.cpp core/ft/dsp/Drums.cpp core/ft/dsp/SynthPatch.cpp $RES -o "$OUT/ft_synth" && "$OUT/ft_synth"
echo "== ft_app"
$CXX -std=c++17 $INC tests/host/ft_app_test.cpp core/ft/*.cpp core/ft/dsp/*.cpp $RES core/source/FourTrackSession.cpp -o "$OUT/ft_app" && "$OUT/ft_app"
echo "== build id"
python3 tests/check_build_id.py
echo "== button bindings"
python3 tests/check_button_bindings.py
echo "all host tests passed"
