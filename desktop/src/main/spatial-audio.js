// SPDX-License-Identifier: MIT
import { execFile, spawn } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';
import { parsePlaybackDevices } from '../shared/spatial-profile.js';
import { writeJsonAtomic } from './store.js';
import { listLinuxDevices } from './linux-audio.js';
import { startLinuxMedia } from './linux-media.js';

const execute = promisify(execFile);

export function createSpatialAudio(engineDir, demoDir, notify) {
  const linux = process.platform === 'linux';
  const engine = path.join(engineDir, linux ? 'tas_binaural' : 'tas_spatial.exe');
  const mediaEngine = path.join(engineDir, 'tas_media_control.exe');
  const demo = path.join(demoDir, 'prueba_7_1_canales_48k.wav');
  let endpoints = [];
  let active = null;
  let queue = Promise.resolve();
  const run = (task) => {
    const result = queue.then(task);
    queue = result.catch(() => {});
    return result;
  };

  async function devices() {
    if (linux) {
      endpoints = await listLinuxDevices();
      return endpoints;
    }
    const { stdout } = await execute(engine, ['--list-devices'], { windowsHide: true, timeout: 5000 });
    endpoints = parsePlaybackDevices(stdout);
    return endpoints;
  }

  function write(session, name, value) {
    const file = path.join(session.dir, name);
    const text = JSON.stringify(value);
    if (session.written.get(name) === text) return;
    const warning = writeJsonAtomic(file, value);
    if (warning) throw Error(warning);
    session.written.set(name, text);
  }

  function close() {
    if (!active) return;
    const session = active;
    session.stopping = true;
    clearTimeout(session.retry);
    session.child.stdin?.end();
    // Diagnostic WAV playback has no control pipe. A stalled WASAPI stream has a short deadline.
    const kill = setTimeout(() => session.child.kill(), session.demo ? 0 : 800);
    return session.closed.finally(() => clearTimeout(kill));
  }

  function startMedia(session) {
    const child = spawn(mediaEngine, ['--telemetry-file', 'media.json'], {
      cwd: session.dir, windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'],
    });
    session.mediaChild = child;
    child.stdin.on('error', () => {});
    let pending = '';
    let failure = '';
    child.stdout.setEncoding('utf8').on('data', (chunk) => {
      pending += chunk;
      let end;
      while ((end = pending.indexOf('\n')) !== -1) {
        const line = pending.slice(0, end).trim();
        pending = pending.slice(end + 1);
        if (active === session && !session.stopping && line.startsWith('Media:')) {
          notify({ mediaStatus: line.slice('Media:'.length).trim() });
        }
      }
    });
    child.stderr.setEncoding('utf8').on('data', (chunk) => { failure = (failure + chunk).slice(-2000); });
    child.on('error', (error) => { failure = error.message; });
    session.mediaClosed = new Promise((resolve) => child.once('close', (code) => {
      session.mediaChild = null;
      if (code !== 0 && active === session && !session.stopping) {
        notify({ mediaStatus: failure.trim() || 'Windows media controller stopped.' });
      }
      resolve();
    }));
  }

  async function stopMedia(session) {
    const child = session.mediaChild;
    if (!child) return;
    child.stdin.end();
    const kill = setTimeout(() => child.kill(), 800);
    await session.mediaClosed.finally(() => clearTimeout(kill));
  }

  function start({ input, output, profile, pose, media, diagnostic = false, speakerPreview = false }) {
    const preview = diagnostic || speakerPreview;
    return run(async () => {
      const present = (id) => endpoints.some((entry) => entry.deviceId === id);
      if (!present(output)) throw Error(linux ? 'Choose an output for binaural audio.' : 'Choose a Windows output and enable Spatial Sound on it.');
      if (!preview && (!present(input) || input === output)) throw Error('Choose a capture device different from the output.');
      await close();
      const capture = endpoints.find((entry) => entry.deviceId === input);
      if (linux && !preview && !capture?.monitor) throw Error('Choose a playback device with a monitor source.');
      const session = { dir: fs.mkdtempSync(path.join(os.tmpdir(), 'TruckAudioSim-')), written: new Map(), demo: preview };
      try {
        const args = [linux ? '--output' : '--output-id', output];
        if (linux && process.env.TAS_HRTF_SOFA) args.push('--sofa', process.env.TAS_HRTF_SOFA);
        if (diagnostic) args.push('--channel-test', '--wav', demo);
        else {
          write(session, 'preset.json', profile);
          write(session, 'pose.json', pose);
          write(session, 'media.json', media ?? { player: 'off', vehicle: '', electricOn: null });
          args.push('--preset', 'preset.json', '--pose-file', 'pose.json');
          if (speakerPreview) {
            const wav = Buffer.alloc(44 + 48000 * 4);
            wav.write('RIFF', 0);
            wav.writeUInt32LE(wav.length - 8, 4);
            wav.write('WAVEfmt ', 8);
            wav.writeUInt32LE(16, 16);
            wav.writeUInt16LE(1, 20);
            wav.writeUInt16LE(2, 22);
            wav.writeUInt32LE(48000, 24);
            wav.writeUInt32LE(192000, 28);
            wav.writeUInt16LE(4, 32);
            wav.writeUInt16LE(16, 34);
            wav.write('data', 36);
            wav.writeUInt32LE(wav.length - 44, 40);
            fs.writeFileSync(path.join(session.dir, 'preview.wav'), wav);
            args.push('--wav', 'preview.wav');
          } else if (linux) args.push('--input', capture.monitor, '--mask', String(capture.mask));
          else args.push('--input-id', input, '--edit-live');
        }
        const child = spawn(engine, args, { cwd: session.dir, windowsHide: true, stdio: ['pipe', 'pipe', 'pipe'] });
        session.child = child;
        child.stdin.on('error', () => {});
        active = session;
        let failure = '';
        let pending = '';
        const report = (message) => { if (active === session && !session.stopping) notify(message); };
        child.stdout.setEncoding('utf8').on('data', (chunk) => {
          pending += chunk;
          let end;
          while ((end = pending.indexOf('\n')) !== -1) {
            const line = pending.slice(0, end).trim();
            pending = pending.slice(end + 1);
            if (line.startsWith('Desktop PCM:')) {
              if (active === session && !session.stopping && !session.demo && !session.mediaClosed) {
                if (linux) session.stopLinuxMedia ??= startLinuxMedia(session.dir, (status) => report({ mediaStatus: status }));
                else startMedia(session);
              }
              report({ pcm: line.slice('Desktop PCM:'.length).trim(), running: true });
            }
            if (line.startsWith('Desktop capacity:')) report({ capacity: line.slice('Desktop capacity:'.length).trim() });
            if (diagnostic && line.startsWith('Activated discrete objects:')) report({ pcm: line, running: true });
          }
        });
        child.stderr.setEncoding('utf8').on('data', (chunk) => { failure = (failure + chunk).slice(-4000); });
        child.on('error', (error) => { failure = error.message; });
        session.closed = new Promise((resolve) => child.once('close', async (code) => {
          const firstError = failure.split(/\r?\n/).find((line) => line.startsWith('Error:')) ?? failure.trim();
          const error = session.stopping || code === 0 ? '' : firstError || `Audio engine stopped (code ${code}).`;
          session.stopping = true;
          clearTimeout(session.retry);
          await session.stopLinuxMedia?.();
          await stopMedia(session);
          if (active === session) {
            active = null;
            notify({ running: false, pcm: '', capacity: '', mediaStatus: '', error });
          }
          try { fs.rmSync(session.dir, { recursive: true, force: true }); } catch {}
          resolve();
        }));
      } catch (error) {
        fs.rmSync(session.dir, { recursive: true, force: true });
        throw error;
      }
    });
  }

  function flush(session) {
    session.retry = null;
    if (session !== active || session.stopping) return;
    const { profile, pose, media } = session.update;
    try {
      write(session, 'pose.json', pose);
      if (media) write(session, 'media.json', media);
      write(session, 'preset.json', profile);
      if (session.updateWarning) {
        session.updateWarning = false;
        notify({ poseWarning: '' });
      }
    } catch (error) {
      // On Windows a native reader can briefly hold the old file open during rename.
      // Retry the newest complete update even if the camera stays still afterwards.
      if (!session.updateWarning && Date.now() - session.pendingSince >= 1000) {
        session.updateWarning = true;
        notify({ poseWarning: `Could not update the spatial engine: ${error.message}` });
      }
      session.retry = setTimeout(() => flush(session), 23);
    }
  }

  function update(values) {
    if (!active || active.stopping || active.demo) return;
    active.update = values;
    if (active.retry) return;
    active.pendingSince = Date.now();
    flush(active);
  }

  return { devices, start, update, stop: () => run(close), close };
}
