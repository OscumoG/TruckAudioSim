#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate an original, deterministic 7.1 channel-isolation WAV (PCM16, 48 kHz).

8 sequential 1-second tones, exactly one nonzero input channel at a time.
The playback engine should emit 8 independent spatial objects, NOT an upmix.
"""
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'validacion-x64' / 'prueba_7_1_canales_48k.wav'
RATE = 48000
CHANNELS = ('FL', 'FR', 'FC', 'LFE', 'BL', 'BR', 'SL', 'SR')
MASK = 0x63F
FREQUENCIES = (390, 515, 650, 85, 740, 870, 990, 1120)
SECONDS_PER_CHANNEL = 1
FRAMES = RATE * SECONDS_PER_CHANNEL * len(CHANNELS)
BYTES = FRAMES * 2 * len(CHANNELS)
# WAVEFORMATEXTENSIBLE PCM GUID
fmt = struct.pack('<HHIIHHHHI', 0xFFFE, len(CHANNELS), RATE,
                  RATE * len(CHANNELS) * 2, len(CHANNELS) * 2,
                  16, 22, 16, MASK)
fmt += struct.pack('<I', 1) + bytes.fromhex('00001000800000aa00389b71')
assert len(fmt) == 40
OUT.parent.mkdir(parents=True, exist_ok=True)
with OUT.open('wb') as f:
    f.write(b'RIFF' + struct.pack('<I', 4 + 8 + 40 + 8 + BYTES) + b'WAVE')
    f.write(b'fmt ' + struct.pack('<I', len(fmt)) + fmt)
    f.write(b'data' + struct.pack('<I', BYTES))
    for tick in range(FRAMES):
        index = tick // RATE
        local = tick % RATE
        # 60ms fade edges + 70ms silence. No acoustic contamination.
        fade = min(1.0, local / (RATE * .06), (RATE - local) / (RATE * .06))
        envelope = max(0.0, min(1.0, fade)) if local < RATE * .93 else 0.0
        amplitude = int(32767 * .18 * envelope * math.sin(2 * math.pi * FREQUENCIES[index] * local / RATE))
        data = [0] * len(CHANNELS)
        data[index] = amplitude
        f.write(struct.pack('<8h', *data))
print(f'Generated {OUT.name}: 8s 48kHz PCM16 7.1 mask=0x{MASK:03X}, {OUT.stat().st_size} bytes')
print('Playback order:', ', '.join(f'{i+1}s {name}' for i, name in enumerate(CHANNELS)))
