// SPDX-License-Identifier: MIT
import { execFile } from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
import { promisify } from 'node:util';

const run = promisify(execFile);
const playerctl = async (...args) => (await run('playerctl', args, { timeout: 1500 })).stdout.trim();

export function startLinuxMedia(dir, notify) {
  let session = '';
  let previous;
  let owned = '';
  let timer;
  let stopped = false;
  let pending;
  let lastMessage = '';
  const status = (message) => {
    if (!stopped && message !== lastMessage) notify(message);
    lastMessage = message;
  };

  async function poll() {
    const { player, vehicle, electricOn } = JSON.parse(await fs.readFile(path.join(dir, 'media.json'), 'utf8'));
    if (session !== JSON.stringify([player, vehicle])) {
      session = JSON.stringify([player, vehicle]);
      previous = undefined;
      owned = '';
    }
    if (player === 'off' || typeof electricOn !== 'boolean') return status('');
    const names = await playerctl('--list-all').catch((error) => {
      if (error.code === 'ENOENT') throw error;
      return '';
    });
    const name = names.split('\n').find((entry) => entry === player || entry.startsWith(`${player}.`));
    if (!name) {
      owned = '';
      previous = undefined;
      return status(`Waiting for ${player} (MPRIS)`);
    }
    const state = await playerctl('--player', name, 'status');
    if (stopped) return;
    if (!electricOn && previous !== false && state === 'Playing') {
      await playerctl('--player', name, 'pause');
      owned = name;
    } else if (electricOn && previous === false && owned === name) {
      if (state === 'Paused') await playerctl('--player', name, 'play');
      owned = '';
    } else if (!electricOn && state === 'Playing') {
      owned = '';
    }
    previous = electricOn;
    status(owned ? 'Paused with truck electrics off' : `MPRIS: ${name}`);
  }

  const tick = () => {
    pending = poll().catch((error) => status(error.code === 'ENOENT'
      ? 'Install playerctl to pause Spotify or VLC on Linux.' : 'MPRIS player temporarily unavailable.')).finally(() => {
      if (!stopped) timer = setTimeout(tick, 250);
    });
  };
  tick();
  return () => {
    stopped = true;
    clearTimeout(timer);
    return pending;
  };
}
