// SPDX-License-Identifier: MIT
export const ACOUSTIC_FIELDS = [
  { key: 'reflections', label: 'Early reflections', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'absorption', label: 'Surface absorption', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'bodyLoss', label: 'Body attenuation', min: 0, max: 18, step: 1, unit: 'dB' },
  { key: 'bodyCutoff', label: 'Closed-body cutoff', min: 300, max: 4000, step: 50, unit: 'Hz' },
  { key: 'leftWindow', label: 'Left window open', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'rightWindow', label: 'Right window open', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'deckHeight', label: 'Deck separation', min: -50, max: 280, step: 5, unit: 'cm' },
];

export function normalizeAcoustics(raw, splitDeck = false) {
  const result = {
    enabled: raw?.enabled === true,
    reflections: 18, absorption: 68, bodyLoss: 6, bodyCutoff: 1050,
    leftWindow: 0, rightWindow: 0, splitDeck: raw?.splitDeck ?? splitDeck, deckHeight: 125,
  };
  result.splitDeck = result.splitDeck === true;
  for (const { key, min, max } of ACOUSTIC_FIELDS) {
    if (Number.isFinite(raw?.[key])) result[key] = Math.round(Math.min(max, Math.max(min, raw[key])));
  }
  return result;
}
