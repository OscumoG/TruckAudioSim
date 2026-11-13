#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate captured Windows loopback against TruckAudioSim's original test WAV.

No third-party packages. This is an acceptance test for a *future* virtual endpoint:
if SysVAD substitutes its example sine wave, the check MUST fail.

Generate with tools/make-demo-surround.py, play that file via the endpoint
under test and capture via tas_endpoint_capture --save-wav.
This checks that each channel contains its own pilot frequency without
bleeding into the other channel positions. It does NOT validate HRTF quality.
"""
from __future__ import annotations
import argparse
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

FREQUENCIES = {
    "2_1": (3, 0x000B, [320, 500, 60]),
    "5_1_side": (6, 0x060F, [320, 500, 650, 60, 830, 990]),
    "5_1_back": (6, 0x003F, [320, 500, 650, 60, 830, 990]),
    "7_1": (8, 0x063F, [320, 500, 650, 60, 830, 990, 1140, 1280]),
}

@dataclass
class Audio:
    samples: bytes
    channels: int
    mask: int
    frames: int
    format: str


def read_wav(path: str | Path) -> Audio:
    data = Path(path).read_bytes()
    if len(data) < 44 or data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("Expected ordinary RIFF WAVE (not RF64)")
    start = 12
    found_fmt = found_data = None
    while start + 8 <= len(data):
        tag = data[start:start + 4]
        size = struct.unpack_from("<I", data, start + 4)[0]
        begin = start + 8
        end = begin + size
        if end > len(data):
            raise ValueError("Truncated WAV chunk")
        if tag == b"fmt ":
            found_fmt = data[begin:end]
        elif tag == b"data":
            found_data = data[begin:end]
        start = end + (size & 1)
    if found_fmt is None or found_data is None or len(found_fmt) < 16:
        raise ValueError("Missing data/fmt")
    fmt, channels, rate, avg, align, bits = struct.unpack_from("<HHIIHH", found_fmt)
    if fmt == 0xFFFE:
        if len(found_fmt) < 40:
            raise ValueError("Short extensible fmt")
        mask, subformat = struct.unpack_from("<II", found_fmt, 20)
        if found_fmt[28:40] != bytes.fromhex("00001000800000aa00389b71"):
            raise ValueError("Unknown extensible subtype GUID")
        if found_fmt[24:28] != b"\x01\x00\x00\x00" and found_fmt[24:28] != b"\x03\x00\x00\x00":
            raise ValueError("Unknown extensible subtype")
        fmt = subformat
    else:
        mask = 0x0003 if channels == 2 else 0
    if rate != 48000 or channels not in (2, 3, 6, 8):
        raise ValueError("Expected 48 kHz and 2/3/6/8 channels")
    if (fmt, bits) not in ((1, 16), (3, 32)) or align != channels * bits // 8:
        raise ValueError("Expected PCM16 or float32 (no automatic conversion)")
    if avg != rate * align or len(found_data) % align:
        raise ValueError("Bad WAV block alignment")
    return Audio(found_data, channels, mask, len(found_data) // align,
                 "pcm16" if fmt == 1 else "float32")


def amplitudes(audio: Audio, start_frame: int, hz: list[int], window_seconds=1) -> list[list[float]]:
    """Amplitude of each known pilot in every channel, using 4:1 decimation.
    All test frequencies are below 1500 Hz, far below the decimated Nyquist.
    Since they are integral Hz, a one-second window avoids spectral leakage.
    """
    sample_rate = 48000
    step = 4
    n = int(window_seconds * sample_rate) // step
    if start_frame < 0 or start_frame + n * step > audio.frames:
        raise ValueError("Not enough captured audio for a one-second measurement")
    # Decode each sample once instead of once per frequency.
    values = [[0.0] * n for _ in range(audio.channels)]
    fmt = "<h" if audio.format == "pcm16" else "<f"
    width = 2 if audio.format == "pcm16" else 4
    for k in range(n):
        base = (start_frame + k * step) * audio.channels * width
        for c in range(audio.channels):
            value = struct.unpack_from(fmt, audio.samples, base + c * width)[0]
            values[c][k] = float(value) / 32768 if width == 2 else float(value)
    results = []
    for channel in values:
        amplitudes_channel = []
        for frequency in hz:
            omega = 2 * math.pi * frequency * step / sample_rate
            # Oscillator recurrence avoids trigonometry for every sample.
            cos_omega, sin_omega = math.cos(omega), math.sin(omega)
            real, imag = 1.0, 0.0
            re_sum = im_sum = 0.0
            for v in channel:
                re_sum += v * real
                im_sum -= v * imag
                real, imag = real * cos_omega - imag * sin_omega, imag * cos_omega + real * sin_omega
            amplitudes_channel.append(2.0 * math.hypot(re_sum, im_sum) / n)
        results.append(amplitudes_channel)
    return results


def assess(amps: list[list[float]], min_dbfs=-44.0, max_bleed_db=-14.0) -> tuple[bool, list[str]]:
    lines = []
    passed = True
    for channel, row in enumerate(amps):
        own = row[channel]
        strongest_other = max((a for i, a in enumerate(row) if i != channel), default=0)
        own_db = 20 * math.log10(max(own, 1e-12))
        bleed_db = 20 * math.log10(max(strongest_other, 1e-12) / max(own, 1e-12))
        dominant = max(range(len(row)), key=lambda i: row[i])
        ok = own_db >= min_dbfs and bleed_db <= max_bleed_db and dominant == channel
        passed &= ok
        lines.append(f"ch {channel + 1}: pilot={own_db:+.1f} dBFS, other={bleed_db:+.1f} dBc, "
                     f"dominant pilot=ch {dominant + 1} => {'OK' if ok else 'FAIL'}")
    return passed, lines


def analyze(file: str, mode: str, min_dbfs: float, max_bleed_db: float) -> tuple[bool, list[str]]:
    audio = read_wav(file)
    channels, mask, hz = FREQUENCIES[mode]
    if (audio.channels, audio.mask) != (channels, mask):
        return False, [f"ERROR: format mismatch: captured {audio.channels} channels mask=0x{audio.mask:x}, "
                       f"expected {channels} channels mask=0x{mask:x}"]
    # Capture might start before the test WAV plays. Find a one-second window
    # where the expected test frequencies (on any channel) have most energy.
    # Each trial uses the same sample rate; no audio output is opened.
    steps = range(0, max(1, audio.frames - 48000 + 1), 48000)
    best = None
    best_energy = -1.0
    for start in steps:
        table = amplitudes(audio, start, hz)
        energy = sum(v * v for ch in table for v in ch)
        if energy > best_energy:
            best_energy, best = energy, table
    if best is None:
        return False, ["ERROR: capture shorter than 1 second"]
    return assess(best, min_dbfs, max_bleed_db)


def main():
    parser = argparse.ArgumentParser(description="Validate 2.1/5.1/7.1 virtual audio loopback; no drivers installed")
    parser.add_argument("--captured", required=True, help="WAV saved by tas_endpoint_capture --save-wav")
    parser.add_argument("--mode", choices=FREQUENCIES, required=True)
    parser.add_argument("--min-dbfs", type=float, default=-44.0)
    parser.add_argument("--max-bleed-db", type=float, default=-14.0)
    args = parser.parse_args()
    try:
        passed, lines = analyze(args.captured, args.mode, args.min_dbfs, args.max_bleed_db)
    except (OSError, ValueError, struct.error) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    for line in lines:
        print(line)
    print("PASS: PCM por canal conservado" if passed else "FAIL: PCM perdido, canales cruzados o tono simulado")
    return 0 if passed else 1

if __name__ == "__main__":
    sys.exit(main())
