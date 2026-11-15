// SPDX-License-Identifier: MIT
export const ACOUSTIC_FIELDS = [
  { key: 'reflections', label: 'Early reflections', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'reverb', label: 'Reverberation', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'decayMs', label: 'Decay time', min: 40, max: 550, step: 5, unit: 'ms' },
  { key: 'absorption', label: 'Surface absorption', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'bodyLoss', label: 'Body attenuation', min: 0, max: 18, step: 1, unit: 'dB' },
  { key: 'bodyCutoff', label: 'Closed-body cutoff', min: 300, max: 4000, step: 50, unit: 'Hz' },
  { key: 'leftWindow', label: 'Left window open', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'rightWindow', label: 'Right window open', min: 0, max: 100, step: 1, unit: '%' },
  { key: 'deckHeight', label: 'Deck separation', min: -50, max: 280, step: 5, unit: 'cm' },
];

// The acoustic estimates use the same dimensions for Realista, 5.1 and 7.1.
export function acousticsFor({ acoustics: raw, bounds, vehicleSpace }) {
  const [width, height, length] = bounds.max.map((value, axis) => Math.max(0.2, value - bounds.min[axis]));
  const coach = vehicleSpace === 'coach';
  const splitDeck = raw?.splitDeck ?? (coach && height > 3.2);
  const volume = width * (splitDeck ? height / 2 : height) * length;
  const car = vehicleSpace === 'car';
  const pickup = vehicleSpace === 'pickup';
  const sleeper = !coach && !car && !pickup && (length >= 2.5 || volume >= 10);
  const result = {
    enabled: raw?.enabled === true,
    reflections: coach ? 44 : car ? 43 : pickup ? 39 : sleeper ? 34 : 43,
    reverb: coach ? 33 : sleeper ? 24 : 20,
    decayMs: 5 * Math.round(Math.min(coach ? 360 : 190, (coach ? 135 : 54) + 20 * Math.sqrt(volume)) / 5),
    absorption: coach ? 59 : sleeper ? 57 : car ? 47 : 43,
    bodyLoss: coach ? 10 : car ? 12 : pickup ? 10 : 9,
    bodyCutoff: coach ? 980 : car ? 860 : pickup ? 930 : 1050,
    leftWindow: 0, rightWindow: 0, splitDeck: splitDeck === true, deckHeight: 125,
  };
  for (const { key, min, max } of ACOUSTIC_FIELDS) {
    if (Number.isFinite(raw?.[key])) result[key] = Math.round(Math.min(max, Math.max(min, raw[key])));
  }
  return result;
}
