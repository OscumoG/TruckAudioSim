import matches from './vehicle-matches.json' with { type: 'json' };

const words = (text) => ` ${String(text ?? '').normalize('NFD').replace(/\p{M}|['’]/gu, '')
  .toLowerCase().replace(/[^a-z0-9]+/g, ' ').trim()} `;

export function matchedVehicleKey(game, id, brand, name) {
  const fields = [words(id), words(name), words(`${brand} ${name}`)];
  return matches[game]?.find((model) => model.id === id
    || model.names.some((alias) => fields.some((field) => field.includes(words(alias)))))?.id;
}
