#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Steam launch options: "/path/to/proton.sh" %command%
set -euo pipefail

app_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
bridge_dir="$app_dir/resources/proton"
data_dir="$HOME/.local/share/TruckAudioSim/telemetry"

if [[ ! -f "$bridge_dir/tas_proton_launch.exe" || ! -f "$bridge_dir/tas_proton_bridge.exe" ]]; then
  echo "TruckAudioSim: the Proton helpers are missing from $bridge_dir" >&2
  exit 1
fi

(umask 077; mkdir -p -- "$data_dir")
if [[ -L "$data_dir" || ! -O "$data_dir" || $(stat -c %a -- "$data_dir") != 700 ]]; then
  echo "TruckAudioSim: expected a private telemetry directory at $data_dir (chmod 700)." >&2
  exit 1
fi

windows_path() {
  printf 'Z:%s' "${1//\//\\}"
}

command=("$@")
for index in "${!command[@]}"; do
  game="${command[$index]}"
  case "${game,,}" in
    eurotrucks2.exe|amtrucks.exe|*/eurotrucks2.exe|*/amtrucks.exe)
      if [[ "$game" != /* ]]; then game="$PWD/$game"; fi
      exec "${command[@]:0:index}" "$bridge_dir/tas_proton_launch.exe" \
        "$(windows_path "$bridge_dir/tas_proton_bridge.exe")" \
        "$(windows_path "$data_dir/proton.bin")" "$(windows_path "$data_dir/camera.bin")" \
        "$(windows_path "$game")" "${command[@]:index+1}"
      ;;
  esac
done
echo "TruckAudioSim: no ETS2/ATS Windows game in the Steam command. Enable Proton for this game." >&2
exit 1
