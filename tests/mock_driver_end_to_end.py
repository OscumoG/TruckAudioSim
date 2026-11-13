#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise original PCM -> driver wire -> WaveRT history -> WAV capture.
This is a user-mode diagnostic, NOT a signed Windows virtual audio driver.
"""
from __future__ import annotations
import math
import pathlib
import struct
import subprocess
import sys
import tempfile


def fixture(path: pathlib.Path) -> None:
    channels, rate, seconds, mask = 8, 48000, 2, 0x63f
    frequencies = (320, 500, 650, 60, 830, 990, 1140, 1280)
    data_bytes = channels * rate * seconds * 2
    with path.open('wb') as out:
        out.write(b'RIFF' + struct.pack('<I', 60 + data_bytes) + b'WAVE')
        fmt = struct.pack('<HHIIHHHHI', 0xFFFE, channels, rate,
                          channels * rate * 2, channels * 2, 16, 22, 16, mask)
        fmt += struct.pack('<I', 1) + bytes.fromhex('00001000800000aa00389b71')
        out.write(b'fmt ' + struct.pack('<I', 40) + fmt)
        out.write(b'data' + struct.pack('<I', data_bytes))
        for frame in range(rate * seconds):
            t = frame / rate
            samples = tuple(round(0.15 * 32767 * math.sin(2 * math.pi * h * t))
                            for h in frequencies)
            out.write(struct.pack('<8h', *samples))


def main() -> int:
    if len(sys.argv) != 3:
        print('Usage: python mock_driver_end_to_end.py tas_mock_wavert tools/check-loopback.py')
        return 2
    mock = pathlib.Path(sys.argv[1]).resolve()
    checker = pathlib.Path(sys.argv[2]).resolve()
    with tempfile.TemporaryDirectory(prefix='tas-wavert-') as directory:
        root = pathlib.Path(directory)
        original = root / 'original_7_1.wav'
        loopback = root / 'loopback_7_1.wav'
        fixture(original)
        subprocess.run([str(mock), '--input', str(original), '--output', str(loopback)], check=True)
        subprocess.run([sys.executable, str(checker), '--captured', str(loopback),
                        '--mode', '7_1'], check=True)
    return 0

if __name__ == '__main__':
    sys.exit(main())
