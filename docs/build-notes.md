# Building the Original's firmware without the official toolchain

Arm's own download site (`developer.arm.com`) was not reachable from the cloud environment used for this project, so the compiler came from Ubuntu's archive instead. This is a recipe for repeating it, and the first measured numbers.

## Result on v1.0.0 (the base we use)
| Build | Flash used | RAM used | SDRAM used |
|---|---|---|---|
| Stock v1.0.0, built here | 873,936 B (83.35%) | 131,984 B (50.35%) | 15,587,616 B (92.91%) |
| With the Four-Track screen (FT5) | 879,172 B (83.84%) | about 132,700 B (50.6%) | 15,587,616 B (92.91%) |

Free flash is about 170 KB on stock and about 165 KB with our screen. Free SDRAM is about 1.13 MiB. These come from the compiler recipe below, not Arm's exact release, so treat them as close, not exact.

## Earlier result on Polyend's unreleased main (not our base)
| Region | Used | Size | Used % |
|---|---|---|---|
| FLASH | 935,436 B | 1 MB | 89.21% |
| RAM | 181,084 B | 256 KB | 69.08% |
| EXTERNAL_RAM (SDRAM) | 15,686,472 B | 16 MB | 93.50% |

That build used Polyend's unreleased main, which has more code than v1.0.0. It is kept only for reference.

The test builds made with this recipe have been flashed and run on the owner's Original. For the recovery drill use an official release file.

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
