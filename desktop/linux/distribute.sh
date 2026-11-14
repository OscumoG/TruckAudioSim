#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Package an assembled Linux x64 directory as a DEB and an AppImage.
set -euo pipefail
if [[ $# != 2 ]]; then
  echo "Usage: $0 LINUX_APP_DIRECTORY OUTPUT_DIRECTORY" >&2
  exit 2
fi
original="$(realpath "$1")"
out="$(realpath -m "$2")"
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
version="$(node -p "require(process.argv[1]).version" "$root/desktop/package.json")"
stage="$(mktemp -d)"
trap 'rm -rf -- "$stage"' EXIT
debian="$stage/deb"
appdir="$stage/TruckAudioSim.AppDir"
mkdir -p -- "$out"
for dest in "$debian" "$appdir"; do
  mkdir -p "$dest/opt/truckaudiosim"
  cp -a "$original/." "$dest/opt/truckaudiosim/"
done

cat > "$stage/truckaudiosim.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=TruckAudioSim
Comment=Spatial audio and speaker editor for ETS2 and ATS
Exec=truckaudiosim
Icon=truckaudiosim
Terminal=false
Categories=AudioVideo;Audio;
StartupWMClass=TruckAudioSim
DESKTOP
convert "$root/desktop/src/main/truckaudiosim.ico[3]" "$stage/truckaudiosim.png"
mkdir -p "$debian/DEBIAN" "$debian/usr/bin"
cat > "$debian/usr/bin/truckaudiosim" <<'SH'
#!/bin/sh
exec /opt/truckaudiosim/start.sh "$@"
SH
chmod 755 "$debian/usr/bin/truckaudiosim"
chmod 4755 "$debian/opt/truckaudiosim/chrome-sandbox"
install -Dm644 "$stage/truckaudiosim.desktop" "$debian/usr/share/applications/truckaudiosim.desktop"
install -Dm644 "$stage/truckaudiosim.png" "$debian/usr/share/icons/hicolor/256x256/apps/truckaudiosim.png"
cat > "$debian/DEBIAN/control" <<CONTROL
Package: truckaudiosim
Version: $version
Section: sound
Priority: optional
Architecture: amd64
Maintainer: TruckAudioSim project <noreply@github.com>
Installed-Size: $(du -sk "$debian" | cut -f1)
Depends: libc6 (>= 2.34), libstdc++6 (>= 11), libgcc-s1, libatomic1, libpulse0, libmysofa1, pulseaudio-utils, libgtk-3-0t64 | libgtk-3-0, libatk1.0-0t64 | libatk1.0-0, libatk-bridge2.0-0t64 | libatk-bridge2.0-0, libatspi2.0-0t64 | libatspi2.0-0, libcups2t64 | libcups2, libglib2.0-0t64 | libglib2.0-0, libasound2t64 | libasound2, libnss3, libnspr4, libgbm1, libdbus-1-3, libpango-1.0-0, libcairo2, libx11-6, libxcomposite1, libxdamage1, libxext6, libxfixes3, libxrandr2, libxcb1, libxkbcommon0, libudev1, libexpat1
Recommends: pavucontrol, playerctl
Homepage: https://github.com/OscumoG/TruckAudioSim
Description: Spatial audio and speaker editor for ETS2 and ATS
 Configure virtual speakers and render multichannel PCM as binaural stereo
 for headphones through PipeWire or PulseAudio. Includes vehicle profiles
 and telemetry tools for native Linux and Proton.
CONTROL

install -m644 "$stage/truckaudiosim.desktop" "$stage/truckaudiosim.png" "$appdir/"
ln -s truckaudiosim.png "$appdir/.DirIcon"
app="$appdir/opt/truckaudiosim"
sofa_library="${SOFA_LIBRARY:-$(ldconfig -p | awk '/libmysofa.so.1 .*x86-64/ && !found { print $NF; found=1 }')}"
install -Dm644 "${SOFA_FILE:-/usr/share/libmysofa/default.sofa}" "$app/resources/engine/sofa/MIT_KEMAR_normal_pinna.sofa"
install -Dm644 "${SOFA_LICENSE:-/usr/share/doc/libmysofa1/copyright}" "$app/resources/engine/sofa/LICENSE-libmysofa-KEMAR.txt"
install -Dm644 "$sofa_library" "$app/resources/engine/lib/libmysofa.so.1"
mv "$app/resources/engine/tas_binaural" "$app/resources/engine/tas_binaural.bin"
cat > "$app/resources/engine/tas_binaural" <<'SH'
#!/bin/sh
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
export LD_LIBRARY_PATH="$here/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$here/tas_binaural.bin" "$@"
SH
chmod 755 "$app/resources/engine/tas_binaural"
cat > "$appdir/AppRun" <<'SH'
#!/bin/sh
set -eu
here="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
app="$here/opt/truckaudiosim"
if [ "${1-}" = "--proton" ]; then
    shift
    exec "$app/proton.sh" "$@"
fi
export TAS_HRTF_SOFA="${TAS_HRTF_SOFA:-$app/resources/engine/sofa/MIT_KEMAR_normal_pinna.sofa}"
exec "$app/start.sh" "$@"
SH
chmod 755 "$appdir/AppRun"
runtime=()
if [[ -n "${APPIMAGE_RUNTIME:-}" ]]; then runtime=(--runtime-file "$APPIMAGE_RUNTIME"); fi
dpkg-deb --root-owner-group -Z xz -z 4 --build "$debian" "$out/TruckAudioSim-v$version-Linux-amd64.deb" &
debpid=$!
result=0
ARCH=x86_64 "${APPIMAGETOOL:-appimagetool}" -n --comp zstd \
  --mksquashfs-opt '-processors' --mksquashfs-opt '4' "${runtime[@]}" \
  "$appdir" "$out/TruckAudioSim-v$version-Linux-x86_64.AppImage" || result=$?
wait "$debpid"
exit "$result"
