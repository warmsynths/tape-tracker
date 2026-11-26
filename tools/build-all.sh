#!/bin/sh
# Builds the web emulator and the firmware from the same sources and stamps both with the same
# build id. Usage: tools/build-all.sh 28   (writes firmware/PolyendTracker_FT28_full.ptf)
set -e
cd "$(dirname "$0")/.."
N=${1:?give the build number, for example 28}
HASH=$(python3 tools/shared_hash.py)
ID="FT$N-$HASH"
printf '#ifndef FT_BUILDID_H_\n#define FT_BUILDID_H_\n#define FT_BUILD_ID "%s"\n#endif\n' "$ID" > core/ft/BuildId.h
# the hash must not change because of the stamp itself
[ "$(python3 tools/shared_hash.py)" = "$HASH" ]
echo "build id $ID"
sh tests/run.sh > /dev/null
FT_BUILD_ID="$ID" python3 prototype/emulator/build.py
ROOT=${ARM_ROOT:-/tmp/arm/root}
if [ -d "$ROOT/usr/bin" ]; then
  export PATH=$ROOT/usr/bin:$PATH CPLUS_INCLUDE_PATH=$ROOT/usr/include/newlib/c++/10.3.1:$ROOT/usr/include/newlib/c++/10.3.1/arm-none-eabi:$ROOT/usr/include/newlib C_INCLUDE_PATH=$ROOT/usr/include/newlib LIBRARY_PATH=$ROOT/usr/lib/arm-none-eabi/newlib
  TC="TOOLCHAIN=$ROOT/usr/bin/arm-none-eabi-"
fi
make original -j8 $TC CUSTOM_DEFINES="-include polyassert.h" > /tmp/ft-firmware.log 2>&1 || { tail -20 /tmp/ft-firmware.log; exit 1; }
grep -A3 "Memory region" /tmp/ft-firmware.log | tail -3
OUT=firmware/PolyendTracker_FT${N}_full.ptf
ls firmware/PolyendTracker_FT*_full.ptf 2>/dev/null | grep -v "FT${N}_" | while read f; do git rm -q --cached "$f" 2>/dev/null || true; rm -f "$f"; done
cp build/PolyendTracker_CE_1.0.0.ptf "$OUT"
echo "firmware: $OUT"
echo "emulator: prototype/emulator/dist/four-track-emulator.html"
echo "both are $ID"
