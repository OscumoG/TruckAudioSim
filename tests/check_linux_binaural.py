#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the offline renderer with the same 7.1 WAV shipped in the desktop."""
from array import array
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
from tempfile import TemporaryDirectory

engine, wav = sys.argv[1:]
rate = 48000


def render(directory, yaw):
    pose = directory / "pose.json"
    pose.write_text(json.dumps({"head": [0, 0, 0], "yawDeg": yaw}), encoding="utf-8")
    output = directory / "output.wav"
    subprocess.run([engine, "--channel-test", "--wav", wav, "--pose-file", str(pose),
                    "--render", str(output)], check=True)
    data = output.read_bytes()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    offset = 12
    while offset + 8 <= len(data):
        chunk, size = struct.unpack_from("<4sI", data, offset)
        offset += 8
        if chunk == b"fmt ":
            assert struct.unpack_from("<HI", data, offset + 2) == (2, rate)
        if chunk == b"data":
            samples = array("f", data[offset:offset + size])
            if sys.byteorder != "little":
                samples.byteswap()
            assert len(samples) == 16 * rate
            return samples
        offset += size + size % 2
    raise AssertionError("Missing output PCM")


def energy(stereo, second, ear):
    data = stereo[2 * rate * second + ear:2 * rate * (second + 1):2]
    return sum(sample * sample for sample in data)


with TemporaryDirectory() as temporary:
    straight = render(Path(temporary), 0)
    turned = render(Path(temporary), 90)
    assert all(math.isfinite(sample) and abs(sample) <= 0.981 for sample in straight)
    for index in range(8):
        assert energy(straight, index, 0) + energy(straight, index, 1) > 0.01, index
    assert energy(straight, 6, 0) > 1.4 * energy(straight, 6, 1)  # side left
    assert energy(straight, 7, 1) > 1.4 * energy(straight, 7, 0)  # side right
    assert energy(turned, 2, 0) > 1.4 * energy(turned, 2, 1)  # front centre becomes left
