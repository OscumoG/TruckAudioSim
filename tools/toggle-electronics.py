#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Manually toggle *simulated* ETS2 electronics; never reads the game.
Use with tas_media_control --pose-file camera-pose.json --app spotify --dry-run.
"""
import json
import os
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'camera-pose.json'
active = True

def save():
    pending = path.with_suffix('.json.tmp')
    pending.write_text(json.dumps({'yawDeg':0,'head':[-0.4,0,0],
                                   'electronicsOn':active}), encoding='utf-8')
    os.replace(pending, path)

save()
print('Simulador de electricidad: ON. Abre tas_media_control en otra consola.')
try:
    while True:
        input('Enter: cambiar ON/OFF; Ctrl+C para salir... ')
        active = not active
        save()
        print('Electrónica', 'ON' if active else 'OFF')
except (KeyboardInterrupt, EOFError):
    active = True
    save()
    print('\nRestaurado ON; no se modificó ETS2.')
