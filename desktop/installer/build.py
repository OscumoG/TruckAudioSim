"""Package an assembled Windows x64 distribution with NSIS."""
import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("payload", type=Path)
    parser.add_argument("setup", type=Path)
    parser.add_argument("--makensis", default="makensis")
    args = parser.parse_args()
    payload = args.payload.resolve()
    setup = args.setup.resolve()
    script = Path(__file__).with_name("TruckAudioSim.nsi").resolve()
    version = json.loads((script.parent.parent / "package.json").read_text())["version"]
    if not (payload / "TruckAudioSim.exe").is_file():
        parser.error("payload is missing TruckAudioSim.exe")
    files = sorted(file for file in payload.rglob("*") if file.is_file())
    directories = sorted({parent for file in files for parent in file.parents if payload in parent.parents}, reverse=True)
    size_kb = (sum(file.stat().st_size for file in files) + 1023) // 1024
    setup.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as directory:
        manifest = Path(directory) / "uninstall.nsh"
        lines = []
        for command, entries in (("Delete", files), ("RMDir", directories)):
            for entry in entries:
                name = str(entry.relative_to(payload)).replace("/", "\\").replace("$", "$$")
                lines.append(f'  {command} "$INSTDIR\\{name}"')
        manifest.write_text("\n".join(lines) + "\n", encoding="utf-8-sig")
        option = "/" if os.name == "nt" else "-"
        subprocess.run([
            args.makensis,
            f"{option}INPUTCHARSET", "UTF8",
            f"{option}DVERSION={version}",
            f"{option}DPAYLOAD={payload}",
            f"{option}DSETUP_OUT={setup}",
            f"{option}DUNINSTALL_LIST={manifest}",
            f"{option}DSIZE_KB={size_kb}",
            str(script),
        ], check=True)


if __name__ == "__main__":
    main()
