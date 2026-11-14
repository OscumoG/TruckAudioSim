// SPDX-License-Identifier: MIT
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';

const execute = promisify(execFile);
const bits = new Map([
  ['front-left', 0x1], ['front-right', 0x2], ['front-center', 0x4], ['lfe', 0x8],
  ['rear-left', 0x10], ['rear-right', 0x20], ['side-left', 0x200], ['side-right', 0x400],
]);
const supported = new Set([0x3, 0xb, 0x3f, 0x60f, 0x63f]);
const virtualName = 'truckaudiosim_input';

export function linuxEndpoints(sinks, sources, defaultSink) {
  const monitors = new Map(sources.map((source) => [source.name, source]));
  return sinks.map((sink) => {
    const source = monitors.get(sink.monitor_source);
    const channels = source?.channel_map?.split(',').map((channel) => bits.get(channel.trim()) ?? 0) ?? [];
    const mask = channels.reduce((total, channel) => total | channel, 0);
    const valid = channels.every(Boolean) && channels.length === new Set(channels).size && supported.has(mask);
    return {
      deviceId: sink.name,
      label: sink.description || sink.name,
      virtual: sink.name === virtualName,
      defaultOutput: sink.name === defaultSink,
      monitor: valid ? source.name : null,
      mask: valid ? mask : null,
    };
  });
}

async function pactl(...args) {
  try {
    return (await execute('pactl', args, { timeout: 4000 })).stdout.trim();
  } catch (error) {
    if (error.code === 'ENOENT') throw Error('Install pulseaudio-utils to list PipeWire playback devices.');
    throw Error(error.stderr?.trim() || error.message);
  }
}

export async function listLinuxDevices() {
  let sinks = JSON.parse(await pactl('-f', 'json', 'list', 'sinks'));
  if (!sinks.some(({ name }) => name === virtualName)) {
    try {
      await pactl('load-module', 'module-null-sink', `sink_name=${virtualName}`,
        'sink_properties=device.description=TruckAudioSim_Input', 'rate=48000', 'channels=8',
        'channel_map=front-left,front-right,front-center,lfe,rear-left,rear-right,side-left,side-right');
    } catch {
      // A session may prohibit module loading. Existing monitor sources still work.
    }
    sinks = JSON.parse(await pactl('-f', 'json', 'list', 'sinks'));
  }
  const [sources, defaultSink] = await Promise.all([
    pactl('-f', 'json', 'list', 'sources').then(JSON.parse),
    pactl('get-default-sink').catch(() => ''),
  ]);
  return linuxEndpoints(sinks, sources, defaultSink);
}
