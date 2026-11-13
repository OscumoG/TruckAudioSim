# SPDX-License-Identifier: MIT
"""Generate the stereo tone fixtures used by the Windows audio diagnostics."""
import math
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RATE = 48000
OUT = ROOT / 'validacion-x64'


def write_wav(name, sample):
    path = OUT / name
    with wave.open(str(path), 'wb') as out:
        out.setparams((2, 2, RATE, 0, 'NONE', 'not compressed'))
        frames = bytearray()
        for index in range(4 * RATE):
            frames.extend(struct.pack('<hh', *sample(index / RATE)))
        out.writeframes(frames)
    print(path)


def stereo(t):
    amplitude = .22 * 32767 * min(1.0, t * 8, (4 - t) * 8)
    return tuple(int(amplitude * math.sin(2 * math.pi * hz * t)) for hz in (440, 660))


def pulse(t, start, duration):
    position = t - start
    if not 0 <= position < duration:
        return 0
    envelope = min(1.0, position / .02, (duration - position) / .02)
    return int(6500 * envelope * math.sin(2 * math.pi * 550 * position))


def left_right(t):
    return pulse(t, .1, .3), pulse(t, 1.54, .22) + pulse(t, 1.91, .22)


if __name__ == '__main__':
    OUT.mkdir(exist_ok=True)
    write_wav('demo_stereo_48k.wav', stereo)
    write_wav('prueba_izquierda_derecha_48k.wav', left_right)
