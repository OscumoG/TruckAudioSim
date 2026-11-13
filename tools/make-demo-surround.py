# SPDX-License-Identifier: MIT
"""Generate ORIGINAL multichannel WAV test tones without third-party music.

3ch PCM 2.1, 5.1 side + back, 7.1, all 48kHz WAVEFORMATEXTENSIBLE.
A different test tone on each channel makes incorrect routing easier to detect.
"""
import math
import struct
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
RATE, SECONDS = 48000, 12
MODES = {
    "2_1": (3, 0xB, [320, 500, 60]),
    "5_1_side": (6, 0x60F, [320, 500, 650, 60, 830, 990]),
    "5_1_back": (6, 0x3F, [320, 500, 650, 60, 830, 990]),
    "7_1": (8, 0x63F, [320, 500, 650, 60, 830, 990, 1140, 1280]),
}
for label, (channels, mask, freq) in MODES.items():
    out = ROOT / f"demo_{label}_48k.wav"
    data_size = RATE * SECONDS * channels * 2
    fmt = struct.pack('<HHIIHHHHI', 0xFFFE, channels, RATE, RATE*channels*2,
                      channels*2, 16, 22, 16, mask)
    fmt += struct.pack('<I', 1) + bytes.fromhex('00001000800000aa00389b71')
    assert len(fmt) == 40
    with out.open('wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 4+8+40+8+data_size)+b'WAVE')
        f.write(b'fmt ' + struct.pack('<I',40)+fmt)
        f.write(b'data'+struct.pack('<I',data_size))
        for n in range(RATE*SECONDS):
            t=n/RATE
            ramp=min(1.0, t*10, (SECONDS-t)*10)
            samples=[int(32767*0.16*ramp*math.sin(2*math.pi*hz*t)) for hz in freq]
            f.write(struct.pack('<'+'h'*channels,*samples))
    print(f'{out.name}: {channels} channels, mask=0x{mask:03x}, {SECONDS}s')
