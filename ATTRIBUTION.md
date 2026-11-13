# Attribution and provenance

- TruckAudioSim v0.1 prototype source is independently authored under MIT.
- Included example `profiles/Scania_S_7_1_Fidelidad.json` originated in the
  user's prior conversation as a custom profile using the Trucker AUX JSON format.
- The original desktop interface, editor, preset collection and game integration
  are adapted from [Trucker AUX v0.4.0](https://github.com/SergeyDoes/trucker-aux/releases/tag/v0.4.0),
  commit `90d7718441db92d92b95fc48a75d02ba360f7a8f`, copyright (c) 2026 SergeyDoes, MIT.
  See `desktop/LICENSE-Trucker-AUX.txt`. The Windows bundle uses the official Electron
  executable and its notices. The optional upstream game plugins retain their own notices.
- Microsoft Windows SDK headers and libraries must be acquired from Microsoft by
  the builder. No Dolby, DTS or other proprietary technology is bundled.

- `driver/sysvad/` includes modified Microsoft SysVAD sources from
  [Windows-driver-samples at d49bb7d](https://github.com/microsoft/Windows-driver-samples/tree/d49bb7d72f3651952b2b6e4ad28d14fe2247eb69/audio/sysvad),
  under the Microsoft Public License. The complete license is included in
  `driver/sysvad/LICENSE-MS-PL.txt` and covers the derived driver binary.
  TruckAudioSim user-mode code is licensed separately under MIT.
