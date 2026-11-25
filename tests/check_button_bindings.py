#!/usr/bin/env python3
"""The Tracker keeps ONE handler per physical button: registering a second one (for example a
release handler after a press handler) silently replaces the first. This check fails if a screen
file registers the same button twice inside one function. Use the form that receives the state
(`setButtonObj(button, handler(uint8_t state))`) to get both press and release."""
import re, sys, glob

bad = 0
for path in glob.glob('core/interface/fourTrack/*.cpp'):
    src = open(path, encoding='utf-8', errors='ignore').read()
    # split into function bodies at top-level closing braces (good enough for these files)
    for body in re.split(r'\n}\n', src):
        seen = {}
        for m in re.finditer(r'setButtonObj\(\s*(interfaceButton\w+)\s*,([^;]*)\)\s*;', body):
            button = m.group(1)
            seen.setdefault(button, []).append(m.group(0).strip())
        for button, calls in seen.items():
            if len(calls) > 1:
                bad += 1
                print(f'{path}: {button} is registered {len(calls)} times in one function:')
                for c in calls:
                    print('   ', c)
if bad:
    print(f'\n{bad} button(s) registered more than once: the last one wins and the others are lost.')
    sys.exit(1)
print('button bindings: ok')
