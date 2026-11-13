// SPDX-License-Identifier: MIT
import { normalizeLayout } from './layout.js';
import { isSilenced } from './dsp.js';
import { pluginWarning, turnsToDeg } from './pose.js';
import { exteriorGain, EXTERIOR_SILENCE_DISTANCE } from './camera.js';
import { MAX_VOLUME } from './settings.js';

const finite = (value, fallback = 0) => Number.isFinite(value) ? value : fallback;
const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

export function spatialProfile(layout, solo = null, mutedIds = []) {
  const normalized = normalizeLayout(layout);
  const muted = new Set(Array.isArray(mutedIds) ? mutedIds : []);
  return {
    truckerAuxPreset: 1,
    name: normalized.name,
    layout: {
      ...normalized,
      speakers: normalized.speakers.map((speaker) => ({
        ...speaker,
        enabled: !isSilenced(speaker, solo, muted),
      })),
    },
  };
}

export function spatialPose(pose, headX, silent, volume, exteriorDistance = null) {
  const head = pose?.sdkActive ? pose.head : null;
  const position = [finite(headX, -0.4) + finite(head?.x), finite(head?.y), finite(head?.z)];
  const distance = Math.hypot(...position);
  const scale = distance > EXTERIOR_SILENCE_DISTANCE ? EXTERIOR_SILENCE_DISTANCE / distance : 1;
  return {
    head: position.map((coordinate) => coordinate * scale),
    // The native yaw argument is positive to the right; SCS heading is positive left.
    yawDeg: -turnsToDeg(finite(head?.heading)),
    pitchDeg: turnsToDeg(finite(head?.pitch)),
    rollDeg: turnsToDeg(finite(head?.roll)),
    electronicsOn: !silent,
    volume: clamp(finite(volume, 1), 0, MAX_VOLUME),
    exteriorGain: Number.isFinite(exteriorDistance) ? exteriorGain(exteriorDistance) : 1,
  };
}

export function mediaTelemetry(pose, inWorld, player) {
  if (player === 'off' || !inWorld || !pose?.sdkActive || !pose.truck || pluginWarning(pose)
      || typeof pose.electricOn !== 'boolean') {
    return { player, vehicle: '', electricOn: null };
  }
  const { key, plate, variant, game } = pose.truck;
  return { player, vehicle: JSON.stringify([game, key, plate, variant]), electricOn: pose.electricOn };
}

export function parsePlaybackDevices(listing) {
  return Array.from(listing.matchAll(/^\s*\[\d+\][ \t]+([^\r\n]+)\r?\n[ \t]+ID:[ \t]*(\S+)/gm),
    ([, label, deviceId]) => ({ deviceId, label: label.trim() }));
}
