# SPDX-License-Identifier: MIT
import importlib.util
import math
import struct
import sys
from pathlib import Path

source = Path(__file__).resolve().parents[1] / 'tools' / 'check-loopback.py'
spec = importlib.util.spec_from_file_location('loopback_check', source)
module = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = module
spec.loader.exec_module(module)

hz = module.FREQUENCIES['7_1'][2]
rate = 48000
n = rate

def make_audio(swap=False, substitute_tone=False, mute=False):
    # Intentionally use the same frequencies and gains as make-demo-surround.py.
    channels = []
    for i in range(8):
        if mute:
            values = [0.0] * n
        else:
            frequency = 440 if substitute_tone else hz[(i+1)%8 if swap else i]
            values = [0.16*math.sin(2*math.pi*frequency*k/rate) for k in range(n)]
        channels.append(values)
    raw = bytearray()
    for i in range(n):
        raw.extend(struct.pack('<8f',*(v[i] for v in channels)))
    return module.Audio(bytes(raw),8,0x63f,n,'float32')

for label, kwargs, expected in [('correct',{},True),('swapped',{'swap':True},False),
                                ('SysVAD-like tone',{'substitute_tone':True},False),
                                ('silence',{'mute':True},False)]:
    a = make_audio(**kwargs)
    amps = module.amplitudes(a, 0, hz)
    actual, lines = module.assess(amps)
    if actual != expected:
        raise RuntimeError(f'{label}: expected={expected}, actual={actual}; {lines}')
    print(f'{label}: {"PASS" if actual else "FAIL as expected"}')

# End-to-end RIFF/WAVEFORMATEXTENSIBLE decoding test, including correct sizes.
import tempfile
for content, expected in ((make_audio(), True), (make_audio(swap=True), False)):
    with tempfile.TemporaryDirectory() as td:
        file = Path(td) / 'capture.wav'
        fmt = struct.pack('<HHIIHHHHI', 0xFFFE, 8, 48000, 48000*8*4,
                          8*4, 32, 22, 32, 0x63F)
        fmt += struct.pack('<I', 3) + bytes.fromhex('00001000800000aa00389b71')
        raw = (b'RIFF' + struct.pack('<I', 60 + len(content.samples)) + b'WAVE'
               + b'fmt ' + struct.pack('<I', 40) + fmt
               + b'data' + struct.pack('<I', len(content.samples)) + content.samples)
        file.write_bytes(raw)
        assert module.read_wav(file).channels == 8
        passed, messages = module.analyze(str(file), '7_1', -44, -14)
        assert passed == expected, messages
print('RIFF WAV loopback analysis PASS')
