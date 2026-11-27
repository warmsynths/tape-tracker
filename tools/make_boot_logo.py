#!/usr/bin/env python3
"""Turns the design's boot logo (a 200 x 120 PNG) into the run-length coded L8 array the display code
decompresses at start-up: core/libs/images/ft_boot_logo_200x120_L8.{h,cpp}.
Usage: python3 tools/make_boot_logo.py docs/design/round-6-export/boot-logo-200x120.png"""
import sys
from PIL import Image

im = Image.open(sys.argv[1]).convert('RGBA')
w, h = im.size
assert (w, h) == (200, 120), 'the boot logo must be 200 x 120'
data = bytearray()
for y in range(h):
    for x in range(w):
        r, g, b, a = im.getpixel((x, y))
        data.append(((r * 299 + g * 587 + b * 114) // 1000) * a // 255)
pairs = []
i = 0
while i < len(data):
    v = data[i]
    n = 1
    while i + n < len(data) and data[i + n] == v and n < 255:
        n += 1
    pairs.append((v, n))
    i += n
name = 'ft_boot_logo_200x120_l8_compressed'
open('core/libs/images/ft_boot_logo_200x120_L8.h', 'w').write(
    '/* The Four-Track boot logo from the design export: 200 x 120, L8, stride 200, 24000 bytes.\n * Made by tools/make_boot_logo.py. */\n#include "NewDisplay/CompressionRLE.h"\n\nextern const CompressedData %s[%d];\n' % (name, len(pairs)))
open('core/libs/images/ft_boot_logo_200x120_L8.cpp', 'w').write(
    '#include "ft_boot_logo_200x120_L8.h"\n\nconst CompressedData %s[%d] = {%s};\n' % (name, len(pairs), ', '.join('{%d,%d}' % p for p in pairs)))
print(len(pairs), 'pairs')
