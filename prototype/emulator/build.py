#!/usr/bin/env python3
"""Builds dist/four-track-emulator.html: one file with the page, the Tracker fonts, the
WebAssembly build of the firmware's Four-Track application and the audio worklet."""
import base64, os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
os.chdir(here)
subprocess.check_call(['sh', 'build_wasm.sh'])
subprocess.check_call([sys.executable, 'make_fonts.py'], stdout=subprocess.DEVNULL)
read = lambda p: open(p, encoding='utf-8').read()
page = read('src/index.template.html')
page = page.replace('/*CSS*/', read('src/style.css'))
page = page.replace('/*FONTS*/', read('fonts.json'))
page = page.replace('/*WASM*/', base64.b64encode(open('build/ft.wasm', 'rb').read()).decode())
page = page.replace('/*WORKLET*/', read('src/worklet.js'))
page = page.replace('/*APP*/', read('src/app.js'))
build_id = os.environ.get('FT_BUILD_ID') or re.search(r'"(FT[^"]*)"', read('../../core/ft/BuildId.h')).group(1)
page = page.replace('/*BUILDID*/', build_id)
os.makedirs('dist', exist_ok=True)
open('dist/four-track-emulator.html', 'w', encoding='utf-8').write(page)
print('dist/four-track-emulator.html', len(page) // 1024, 'KB')
