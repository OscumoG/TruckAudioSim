# SPDX-License-Identifier: MIT
"""Manual *simulation*, not a live ETS2 integration.
Run in another terminal while tas_spatial --pose-file camera-pose.json plays a WAV.
"""
import json
import math
import os
from pathlib import Path
import time
root=Path(__file__).resolve().parent.parent
out=root/'camera-pose.json'
tmp=root/'camera-pose.json.tmp'
print('Publishing simulated camera orientation to:',out)
try:
    start=time.monotonic()
    while time.monotonic()-start<30:
        t=time.monotonic()-start
        payload={'yawDeg':70*math.sin(t*0.9), 'head':[-0.4,0.0,0.0], 'electronicsOn': True}
        tmp.write_text(json.dumps(payload),encoding='utf8')
        os.replace(tmp,out)
        time.sleep(0.05)
finally:
    out.write_text(json.dumps({'yawDeg':0,'head':[-0.4,0.0,0.0], 'electronicsOn':True}),encoding='utf8')
