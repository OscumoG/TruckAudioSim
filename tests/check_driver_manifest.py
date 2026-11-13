"""Static WDM packaging regression test, NOT a substitute for InfVerif/WDK."""
from pathlib import Path
import re

base = Path(__file__).resolve().parents[1] / 'driver' / 'sysvad'
proj = (base / 'TruckAudioSimVirtualAudio.vcxproj').read_text(encoding='utf-8')
inf = (base / 'TruckAudioSimVirtualAudio.inx').read_text(encoding='utf-8')
fmt = (base / 'taswavtable.h').read_text(encoding='utf-8')
assert '<DriverType>WDM</DriverType>' in proj, 'PortCls must build as WDM, not KMDF'
assert '<DriverType>KMDF</DriverType>' not in proj
assert '[Tas.NT.Wdf]' not in inf and 'KmdfService=' not in inf
assert 'Root\\TruckAudioSimVirtualAudio' in inf
assert 'PnpLockdown=1' in inf and 'CatalogFile=' in inf
assert all(x in fmt for x in (
    'TasPcmFormat(2, KSAUDIO_SPEAKER_STEREO)',
    'TasPcmFormat(6, KSAUDIO_SPEAKER_5POINT1_SURROUND)',
    'TasPcmFormat(6, KSAUDIO_SPEAKER_5POINT1)',
    'TasPcmFormat(8, KSAUDIO_SPEAKER_7POINT1_SURROUND)'))
assert not re.search(r'^\s*(?:CopyFiles|ServiceBinary)\s*=\s*[^\n]*[.]exe\b', inf, re.I | re.M)
print('PASS: WDM PortCls manifest & channel declarations (static only; WDK and InfVerif pending)')
