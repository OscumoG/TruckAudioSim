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


def render(directory, yaw, speakers=None):
    pose = directory / "pose.json"
    pose.write_text(json.dumps({"head": [0, 0, 0], "yawDeg": yaw}), encoding="utf-8")
    output = directory / "output.wav"
    mode = ["--channel-test"]
    if speakers is not None:
        preset = directory / "preset.json"
        preset.write_text(json.dumps({
            "truckerAuxPreset": 1, "name": "Volume test", "layout": {"speakers": speakers},
        }), encoding="utf-8")
        mode = ["--preset", str(preset)]
    subprocess.run([engine, *mode, "--wav", wav, "--pose-file", str(pose),
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
    # Skip the edges, where roundoff from the next FFT block can leave a tiny residual.
    start = second * rate + rate // 10
    end = (second + 1) * rate - rate // 10
    data = stereo[2 * start + ear:2 * end:2]
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

    left = {"id": "l", "name": "Left", "channel": "FL", "type": "full",
            "gainDb": 0, "position": [-1, 0, -1]}
    right = {**left, "id": "r", "name": "Right", "channel": "FR", "position": [1, 0, -1]}
    baseline = render(Path(temporary), 0, [left, right])
    adjusted = render(Path(temporary), 0, [{**left, "volumePercent": 0}, {**right, "volumePercent": 50}])
    for ear in range(2):
        assert energy(baseline, 0, ear) > 0.01
        assert energy(adjusted, 0, ear) < 1e-12
        assert math.isclose(energy(adjusted, 1, ear), energy(baseline, 1, ear) * 0.25, rel_tol=1e-5)
