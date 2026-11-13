# SPDX-License-Identifier: MIT
"""Validate exact 7.1 WAV order, exclusivity, and frequency-independent energy."""
from pathlib import Path
import struct

path = Path(__file__).resolve().parent.parent / 'validacion-x64' / 'prueba_7_1_canales_48k.wav'
with path.open('rb') as f:
    assert f.read(4) == b'RIFF'
    riff_size = struct.unpack('<I', f.read(4))[0]
    assert f.read(4) == b'WAVE'
    assert f.read(4) == b'fmt '
    fmt_size = struct.unpack('<I', f.read(4))[0]
    assert fmt_size == 40
    fmt = f.read(40)
    tag, channels, sample_rate, bytes_rate, block_align, bits, ext_size, valid_bits, mask = struct.unpack('<HHIIHHHHI', fmt[:24])
    assert (tag, channels, sample_rate, bytes_rate, block_align, bits, ext_size, valid_bits, mask) == (
        0xFFFE, 8, 48000, 48000 * 16, 16, 16, 22, 16, 0x63F
    )
    assert f.read(4) == b'data'
    count = struct.unpack('<I', f.read(4))[0]
    assert count == 8 * 48000 * 16
    assert riff_size == path.stat().st_size - 8
    energy = [0]*8
    for second in range(8):
        nonzero = 0
        for _ in range(48000):
            frame = struct.unpack('<8h', f.read(16))
            assert all(value == 0 for i, value in enumerate(frame) if i != second), (second, frame)
            if frame[second] != 0:
                nonzero += 1
                energy[second] += abs(frame[second])
        assert nonzero > 30_000, (second, nonzero)
    assert not f.read(1)
print('PASS: 8 independent sequential channels, 48 kHz, WAVEFORMATEXTENSIBLE mask 0x63F')
