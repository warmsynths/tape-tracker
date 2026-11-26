#!/usr/bin/env python3
"""Fails if the shared sources changed since the build id was stamped, which means the emulator and
the firmware may no longer be built from the same code. Run tools/build-all.sh to rebuild both."""
import re, subprocess, sys, os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
stamp = re.search(r'"FT\d+-([0-9a-f]{8})"', open(os.path.join(root, 'core/ft/BuildId.h')).read()).group(1)
now = subprocess.check_output([sys.executable, os.path.join(root, 'tools/shared_hash.py')]).decode().strip()
if stamp != now:
    print(f'build id is stale: stamped {stamp}, sources are {now}. Run tools/build-all.sh <number>.')
    sys.exit(1)
print('build id: ok', stamp)
