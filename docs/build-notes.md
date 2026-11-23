# Building the Original's firmware without the official toolchain

Arm's own download site (`developer.arm.com`) was not reachable from the cloud environment used for this project, so the compiler came from Ubuntu's archive instead. This is a recipe for repeating it, and the first measured numbers.

## Result (stock code, Original, nothing changed)
| Region | Used | Size | Used % |
|---|---|---|---|
| FLASH | 935,436 B | 1 MB | 89.21% |
| RAM | 181,084 B | 256 KB | 69.08% |
| EXTERNAL_RAM (SDRAM) | 15,686,472 B | 16 MB | 93.50% |

Compared with the baseline in `docs/memory_map.md` (flash about 880 KB used, about 144 KB free), this build uses about 55 KB more flash, leaving about 113 KB free. The cause is probably the compiler build differing from the official one and the assert workaround below, but that is a guess. The GitHub Actions build with the official toolchain would give the authoritative number. **Treat free flash as roughly 110 to 145 KB until that is checked.**

Do not flash the `.ptf` from this build. It was made with a different compiler build and a workaround. Use an official release for the recovery drill.

## Toolchain recipe (Ubuntu 24.04, no changes to the system)
Download these from `http://archive.ubuntu.com/ubuntu/pool/` and unpack each with `dpkg -x <deb> <private folder>`:
- `universe/g/gcc-arm-none-eabi/gcc-arm-none-eabi_10.3-2021.07-4_amd64.deb`
- `universe/b/binutils-arm-none-eabi/binutils-arm-none-eabi_2.38-3ubuntu1+15build1_amd64.deb`
- `universe/n/newlib/libnewlib-arm-none-eabi_3.3.0-1.3_all.deb` and `universe/n/newlib/libnewlib-dev_3.3.0-1.3_all.deb`
- `universe/libs/libstdc++-arm-none-eabi/libstdc++-arm-none-eabi-dev_10.3-2021.07-4+17_all.deb` and `...-newlib_10.3-2021.07-4+17_all.deb`

Then, inside the private folder:
- Link every `*.specs` file from `usr/lib/arm-none-eabi/newlib/` into `usr/lib/gcc/arm-none-eabi/10.3.1/`.
- Link `usr/lib/arm-none-eabi/lib` to `newlib`.

Set these in the shell (replace `ROOT` with the private folder):
```bash
export PATH=ROOT/usr/bin:$PATH
export CPLUS_INCLUDE_PATH=ROOT/usr/include/newlib/c++/10.3.1:ROOT/usr/include/newlib/c++/10.3.1/arm-none-eabi:ROOT/usr/include/newlib
export C_INCLUDE_PATH=ROOT/usr/include/newlib
export LIBRARY_PATH=ROOT/usr/lib/arm-none-eabi/newlib
```

## Build
```bash
make original -j4 TOOLCHAIN=ROOT/usr/bin/arm-none-eabi- CUSTOM_DEFINES="-include polyassert.h"
```
The `-include polyassert.h` is a workaround: with this library build, `core/source/mtStructs.cpp` fails with "assert was not declared", because it relies on a header that the official toolchain pulls in indirectly. It does not change the source. The build writes `build/PolyendTracker_CE_1.0.0.ptf`, which is ignored by git.
