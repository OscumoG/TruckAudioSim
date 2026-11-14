// SPDX-License-Identifier: MIT
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const directory = path.join(os.homedir(), '.local', 'share', 'TruckAudioSim', 'telemetry');
const freshForMs = 2500;

export function prepareLinuxTelemetry() {
  if (process.platform !== 'linux') return;
  fs.mkdirSync(directory, { recursive: true, mode: 0o700 });
  const directoryInfo = fs.lstatSync(directory);
  if (!directoryInfo.isDirectory() || directoryInfo.uid !== process.getuid() || (directoryInfo.mode & 0o077)) {
    throw Error(`Expected a private telemetry directory (chmod 700): ${directory}`);
  }
}

function readFrame(file, size) {
  try {
    const fd = fs.openSync(path.join(directory, file), fs.constants.O_RDONLY | fs.constants.O_NOFOLLOW | fs.constants.O_NONBLOCK);
    try {
      const info = fs.fstatSync(fd);
      if (!info.isFile() || info.uid !== process.getuid() || info.size !== size || Date.now() - info.mtimeMs > freshForMs) return null;
      const buffer = Buffer.alloc(size);
      return fs.readSync(fd, buffer, 0, size) === size ? { buffer, time: info.mtimeMs } : null;
    } finally {
      fs.closeSync(fd);
    }
  } catch {
    return null;
  }
}

export function linuxSnapshot(files, size) {
  const read = () => {
    const frames = files.map((file) => readFrame(file, size)).filter(Boolean);
    return frames.reduce((latest, frame) => frame.time > latest.time ? frame : latest,
      { time: -Infinity, buffer: null }).buffer;
  };
  if (!read()) return null;
  return { close() {}, copy: (buffer) => {
    const frame = read();
    if (frame) frame.copy(buffer);
    return Boolean(frame);
  } };
}
