#!/usr/bin/env python3
"""Static ETS2 Windows quick-start package checks; not a native WASAPI test."""
from pathlib import Path
import json
import struct

root = Path(__file__).resolve().parents[1]
p = json.loads((root / 'profiles/ETS2_Estereo_Fiel.json').read_text(encoding='utf-8'))
speakers = p['layout']['speakers']
assert len(speakers) == 2
assert [s['channel'] for s in speakers] == ['FL', 'FR']
assert all(s['position'][2] < 0 for s in speakers)
assert all(s['gainDb'] <= 0 for s in speakers)
assert p['game'] == 'ets2'
launcher = (root / 'tools/ets2-live.ps1').read_text(encoding='utf-8-sig')
cmd = (root / 'Iniciar-ETS2.cmd').read_text(encoding='ascii')
for required in ('--list-devices', '--input-id', '--output-id',
                 'Steam Streaming Speakers', 'Current shared mix:',
                 'ETS2_Estereo_Fiel.json', 'ms-settings:apps-volume',
                 'Choose-Device', '48000'):
    assert required in launcher, required
for forbidden in ('pnputil', 'devgen', 'Set-DefaultAudioDevice', 'SetDefaultEndpoint',
                  'Start-Service', 'sc.exe', 'rundll32'):
    assert forbidden.lower() not in launcher.lower(), forbidden
assert '-ExecutionPolicy Bypass' in cmd and '-File' in cmd
for binary in ('tas_spatial.exe', 'tas_endpoint_probe.exe'):
    path = root / 'bin-x64' / binary
    if not path.is_file():
        continue
    b = path.read_bytes()
    assert b[:2] == b'MZ'
    pe_offset = struct.unpack_from('<I', b, 60)[0]
    assert b[pe_offset:pe_offset+4] == b'PE\0\0'
    machine = struct.unpack_from('<H', b, pe_offset+4)[0]
    assert machine == 0x8664  # AMD64
assert not list((root/'driver').rglob('*.sys')), 'No kernel driver ships in ETS2 beta'
print('PASS: ETS2 launcher, stereo profile and any staged Windows executables are valid')
