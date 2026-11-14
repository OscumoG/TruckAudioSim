#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Run after the Linux CMake build, npm ci, and compilation of the two helpers.
set -euo pipefail
if [[ $# != 3 ]]; then
  echo "Usage: $0 BUILD_DIR PROTON_HELPERS_DIR OUTPUT_DIR" >&2
  exit 2
fi
build="$(realpath "$1")"
helpers="$(realpath "$2")"
output="$(realpath -m "$3")"
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$root/desktop"
version="$(node -p "require('./package.json').version")"
name="TruckAudioSim-v$version-Linux-x64"
mkdir -p -- "$output"
stage="$(mktemp -d)"
trap 'rm -rf -- "$stage"' EXIT
# Assemble the same Electron entrypoint and presets as the Windows desktop.
node_modules/.bin/electron-builder --linux dir --x64 \
  --config linux/electron-builder.json \
  --config.directories.output="$stage"
mv -- "$stage/linux-unpacked" "$stage/$name"
app="$stage/$name"
install -Dm755 "$build/tas_binaural" "$app/resources/engine/tas_binaural"
install -Dm644 "$root/validacion-x64/prueba_7_1_canales_48k.wav" "$app/resources/engine/prueba_7_1_canales_48k.wav"
install -Dm644 "$build/plugins/trucker_aux_telemetry.so" "$app/resources/plugins/trucker_aux_telemetry.so"
for program in tas_proton_launch tas_proton_bridge; do
  install -Dm644 "$helpers/$program.exe" "$app/resources/proton/$program.exe"
done
install -m755 linux/start.sh linux/proton.sh "$app/"
install -m644 "$root/docs/LINUX.md" "$root/LICENSE" "$app/"
install -Dm644 "$root/third_party/scs-sdk-plugin/LICENSE" "$app/resources/plugins/LICENSE-RenCloud.txt"
install -Dm644 "$root/third_party/scs-sdk-plugin/scs_sdk/sdk_license.txt" "$app/resources/plugins/LICENSE-SCS-SDK.txt"
tar -czf "$output/$name.tar.gz" -C "$stage" "$name"
