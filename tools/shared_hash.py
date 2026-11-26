#!/usr/bin/env python3
"""A short hash of every source file that the firmware and the web emulator share. The firmware and
the emulator are built from the same files; this hash is stamped into both so a mismatch is visible."""
import hashlib, glob, os, sys
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
patterns = ['core/ft/**/*.cpp', 'core/ft/**/*.h', 'core/source/FourTrackSession.*', 'core/source/loop_bars.h',
            'core/libs/Audio/play_loops.*', 'core/libs/Audio/loop_fill.h', 'core/interface/fourTrack/*']
files = set()
for p in patterns:
    files.update(glob.glob(os.path.join(root, p), recursive=True))
files = sorted(f for f in files if os.path.isfile(f) and not f.endswith('BuildId.h') and '/wasm/' not in f)
h = hashlib.sha1()
for f in files:
    h.update(os.path.relpath(f, root).encode())
    h.update(open(f, 'rb').read())
print(h.hexdigest()[:8])
