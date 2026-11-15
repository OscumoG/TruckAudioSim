// SPDX-License-Identifier: MIT
export const EQ_BANDS = [80, 250, 1000, 3000, 8000];
export const EQ_LIMIT = 9;

export function normalizeEqualizer(raw) {
  return {
    enabled: raw?.enabled === true,
    gains: EQ_BANDS.map((_, i) => {
      const gain = Array.isArray(raw?.gains) ? raw.gains[i] : undefined;
      return Number.isFinite(gain) ? Math.min(EQ_LIMIT, Math.max(-EQ_LIMIT, gain)) : 0;
    }),
  };
}
