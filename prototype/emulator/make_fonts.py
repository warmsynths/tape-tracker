#!/usr/bin/env python3
"""Decodes the Tracker's built-in font bitmaps (core/libs/fonts/*.cpp) into fonts.json so the web
emulator draws text with exactly the same glyphs as the device.

Font format (FT81x custom font, 4 bits per pixel): 128 bytes of glyph advances, then the format,
line stride, width and height as 32-bit values at 128, 132, 136, 140, then the glyph bitmaps from
byte 148. Glyph n is the character (first character with an advance) + n.
"""
import base64, json, re, sys
FONTS = '../../core/libs/fonts/'
SETS = {
    'liberation': ['LiberationSans-Regular_14_L4', 'LiberationSans-Regular_17_L4', 'LiberationSans-Bold_14_L4',
                   'LiberationSans-Bold_17_L4', 'LiberationSans-Regular_12_L4'],
    'roboto': ['RobotoMono-Light_14_L4', 'RobotoMono-Light_17_L4', 'RobotoMono-Regular_14_L4',
               'RobotoMono-Bold_17_L4', 'RobotoMono-Regular_12_L4'],
}

def decode(name):
    src = open(FONTS + name + '.cpp').read()
    pairs = re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*\}', src)
    data = bytearray()
    for value, count in pairs:
        data += bytes([int(value)]) * int(count)
    adv = list(data[0:128])
    stride, width, height = data[132], data[136], data[140]
    first = next(i for i in range(128) if adv[i])
    glyphs = {}
    n = 0
    for ch in range(first, 128):
        if not adv[ch]:
            continue
        base = 148 + n * stride * height
        rows = []
        for y in range(height):
            row = data[base + y * stride: base + (y + 1) * stride]
            px = []
            for b in row:
                px.append(b >> 4)
                px.append(b & 15)
            rows.append(px[:width])
        flat = bytes(v for r in rows for v in r)
        glyphs[str(ch)] = base64.b64encode(flat).decode()
        n += 1
    return {'name': name, 'width': width, 'height': height, 'adv': adv, 'glyphs': glyphs}

out = {k: [decode(n) for n in v] for k, v in SETS.items()}
json.dump(out, open('fonts.json', 'w'), separators=(',', ':'))
for k, v in out.items():
    print(k, [(f['name'], f['width'], f['height'], len(f['glyphs'])) for f in v])
