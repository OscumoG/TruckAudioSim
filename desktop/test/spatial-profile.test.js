import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';
import { coachExteriorDistance } from '../src/shared/camera.js';
import { parsePresetFile, presetFile } from '../src/shared/collection.js';
import { updateSpeaker } from '../src/shared/layout.js';
import { matchedVehicleKey } from '../src/shared/vehicle-matches.js';
import { mediaTelemetry, parsePlaybackDevices, spatialPose, spatialProfile } from '../src/shared/spatial-profile.js';

test('every factory truck model has one realistic, 5.1 and 7.1 profile at 0 dB', () => {
  const root = new URL('../presets/default/Variantes%20de%20cabina/', import.meta.url);
  const models = new Map();
  for (const game of ['ETS2', 'ATS']) {
    const folder = new URL(`${game}/`, root);
    for (const file of fs.readdirSync(folder)) {
      const raw = JSON.parse(fs.readFileSync(new URL(encodeURIComponent(file), folder)));
      assert.match(raw.vehicle, /^vehicle\.[^@#]+$/);
      const key = `${game}/${raw.vehicle}`;
      const modes = models.get(key) ?? [];
      modes.push(raw.layout.cabAudio);
      models.set(key, modes);
      assert.ok(raw.layout.speakers.every((speaker) => speaker.gainDb === 0));
    }
  }
  assert.equal(models.size, 50);
  for (const [model, modes] of models) assert.deepEqual(modes.sort(), ['5.1', '7.1', 'realistic'], model);
});

test('coach, Ford and Scout telemetry can resolve to the three factory profiles', () => {
  const models = [
    ['ets2', 'vehicle.scn.touring', 'Scania', 'Touring', 'vehicle.scania.touring'],
    ['ets2', '', 'Volvo', '9700 Double Decker', 'vehicle.volvo.9700_dd'],
    ['ets2', '', 'MAN', 'Lion’s Coach', 'vehicle.man.lions_coach'],
    ['ets2', 'vehicle.scout.extra_d', 'Scout', 'Superb', 'vehicle.skoda.superb'],
    ['ats', '', 'Škoda', 'Superb', 'vehicle.skoda.scout_ats'],
    ['ats', 'vehicle.ford.f150_23', 'Ford', 'F-150', 'vehicle.ford.f150_23'],
    ['ats', '', 'Ford', 'Mustang Fastback', 'vehicle.ford.mustang_67'],
    ['ats', 'vehicle.ford.bronco_23', 'Ford', 'Bronco', 'vehicle.ford.bronco_24'],
    ['ats', '', 'Ford', 'Crown Victoria', 'vehicle.ford.victoria_06'],
  ];
  for (const [game, id, brand, name, expected] of models) {
    assert.equal(matchedVehicleKey(game, id, brand, name), expected);
    const folder = new URL(`../presets/default/Variantes%20de%20cabina/${game.toUpperCase()}/`, import.meta.url);
    const files = fs.readdirSync(folder).map((file) => JSON.parse(fs.readFileSync(new URL(encodeURIComponent(file), folder))));
    assert.equal(files.filter((entry) => entry.vehicle === expected).length, 3);
  }
  assert.equal(matchedVehicleKey('ets2', 'vehicle.scania.s_2016', 'Scania', 'S'), undefined);
});

test('the Scania profile keeps eight discrete channels through import, export and live edits', () => {
  const filename = 'Variantes de cabina/ETS2/ETS2, Scania, Scania S - 7.1.json';
  const raw = JSON.parse(fs.readFileSync(new URL(`../presets/default/${filename}`, import.meta.url)));
  const { entry } = parsePresetFile(raw, filename);
  const first = entry.layout.speakers[0];
  first.position[0] = -0.5;
  const expected = ['FL', 'FR', 'FC', 'LFE', 'SL', 'SR', 'BL', 'BR'];
  const exported = presetFile(entry);
  assert.deepEqual(exported.layout.speakers.map((speaker) => speaker.channel), expected);
  const live = spatialProfile(entry.layout, first.id);
  assert.deepEqual(live.layout.speakers.map((speaker) => speaker.channel), expected);
  assert.equal(live.layout.speakers[0].position[0], -0.5);
  assert.equal(live.layout.speakers.filter((speaker) => speaker.enabled).length, 1);
  assert.equal(spatialProfile(entry.layout, null, [first.id]).layout.speakers[0].enabled, false);
});

test('speaker volume is independent of the mirrored speaker and survives preset export', () => {
  const filename = 'speaker-volume.json';
  const raw = {
    truckerAuxPreset: 1,
    name: 'Speaker volume',
    layout: { speakers: [
      { id: 'l', channel: 'L', gainDb: -3, pair: 'r' },
      { id: 'r', channel: 'R', gainDb: 2, pair: 'l' },
    ] },
  };
  const { entry } = parsePresetFile(raw, filename);
  const layout = updateSpeaker(entry.layout, 'l', { volumePercent: 0 });
  const exported = presetFile({ ...entry, layout });
  const imported = parsePresetFile(JSON.parse(JSON.stringify(exported)), filename).entry.layout;
  const [left, right] = spatialProfile(imported).layout.speakers;
  assert.equal(left.volumePercent, 0);
  assert.equal(left.gainDb, -3);
  assert.equal(right.volumePercent ?? 100, 100);
  assert.equal(right.gainDb, 2);
  assert.equal(updateSpeaker(layout, 'l', { volumePercent: 50 }).speakers[0].volumePercent, 50);
});

test('all nine coach profiles keep the back of the passenger cabin in the editor and engine', () => {
  const root = new URL('../presets/default/Variantes%20de%20cabina/ETS2/', import.meta.url);
  const files = fs.readdirSync(root).filter((file) => /^(Scania Touring|MAN Lion's Coach|Volvo 9700 DD) - /.test(file));
  assert.equal(files.length, 9);
  for (const file of files) {
    const raw = JSON.parse(fs.readFileSync(new URL(encodeURIComponent(file), root)));
    const { entry } = parsePresetFile(raw, file);
    const { layout } = entry;
    const rear = layout.speakers.reduce((a, b) => a.position[2] > b.position[2] ? a : b);
    assert.equal(layout.vehicleSpace, 'coach', file);
    assert.ok(rear.position[2] > 9 && layout.speakers.length <= 16, file);
    assert.equal(presetFile(entry).layout.vehicleSpace, 'coach', file);
    assert.deepEqual(spatialProfile(layout).layout.speakers.at(-1).position, layout.speakers.at(-1).position);
    const moved = updateSpeaker(layout, rear.id, { position: [rear.position[0], rear.position[1], 11] });
    assert.equal(moved.speakers.find(({ id }) => id === rear.id).position[2], 11);

    const free = { source: 'free', x: 0.4, y: 0.2, z: layout.bounds.max[2] - 0.1 };
    assert.equal(coachExteriorDistance(free, -0.4, layout.bounds), null);
    assert.equal(coachExteriorDistance({ ...free, source: 'outside' }, -0.4, layout.bounds), 0);
    assert.equal(coachExteriorDistance({ ...free, z: layout.bounds.max[2] + 1 }, -0.4, layout.bounds), 1);
    if (raw.vehicle === 'vehicle.volvo.9700_dd') {
      assert.ok(layout.speakers.some(({ position: [, y, z] }) => y < 1 && z > 7));
      assert.ok(layout.speakers.some(({ position: [, y, z] }) => y > 2 && z > 10));
      assert.ok(layout.speakers.some(({ type, channel }) => type === 'sub' && channel === 'LFE'));
    }
  }
});

test('the native pose follows SCS axes, volume and muting', () => {
  const pose = { sdkActive: true, head: { x: 0.2, y: 0.1, z: -0.4, heading: 0.25, pitch: 0.125, roll: -0.125 } };
  assert.deepEqual(spatialPose(pose, -0.4, true, 0.7), {
    head: [-0.2, 0.1, -0.4], yawDeg: -90, pitchDeg: 45, rollDeg: -45, electronicsOn: false, volume: 0.7, exteriorGain: 1,
  });
  assert.equal(spatialPose(null, -0.4, false, 4).volume, 4);
  assert.equal(spatialPose(null, -0.4, false, 5).volume, 5);
  assert.equal(spatialPose(null, -0.4, false, 11).volume, 10);
});

test('outside distance controls attenuation without changing the electrical state', () => {
  const pose = { sdkActive: true, head: { x: 30, y: 0, z: 40 } };
  const state = spatialPose(pose, 0, false, 1, 50);
  assert.deepEqual(state.head, [24, 0, 32]);
  assert.equal(state.exteriorGain, 0);
  assert.equal(state.electronicsOn, true);
  assert.equal(spatialPose(pose, 0, false, 0.8, 1).volume, 0.8);
  assert.equal(spatialPose(pose, 0, false, 5, 1).exteriorGain, 0.25);
});

test('media control receives raw electrics only from a live compatible vehicle', () => {
  const pose = { sdkActive: true, pluginRevision: 12, electricOn: false, truck: { key: 'Scania', game: 'ets2', plate: 'A', variant: '1.4' } };
  const state = mediaTelemetry(pose, true, 'spotify');
  assert.equal(state.electricOn, false);
  assert.notEqual(state.vehicle, mediaTelemetry({ ...pose, truck: { ...pose.truck, plate: 'B' } }, true, 'spotify').vehicle);
  assert.equal(mediaTelemetry({ ...pose, electricOn: true }, true, 'vlc').electricOn, true);
  assert.equal(mediaTelemetry({ ...pose, pluginRevision: 99 }, true, 'spotify').electricOn, null);
  assert.equal(mediaTelemetry(pose, false, 'spotify').electricOn, null);
  assert.equal(mediaTelemetry(pose, true, 'off').electricOn, null);
});

test('playback device names and stable endpoint IDs are parsed from Windows output', () => {
  const listing = 'Active Windows playback endpoints (index is temporary):\r\n  [0] Altavoces (Realtek)\r\n      ID: {0.0.0.00000000}.{123}\r\n  [1] Steam Streaming Speakers\r\n      ID: {0.0.0.00000000}.{456}\r\n';
  assert.deepEqual(parsePlaybackDevices(listing), [
    { label: 'Altavoces (Realtek)', deviceId: '{0.0.0.00000000}.{123}' },
    { label: 'Steam Streaming Speakers', deviceId: '{0.0.0.00000000}.{456}' },
  ]);
});

test('acoustics survive export and keep the independent window openings', () => {
  const raw = {
    truckerAuxPreset: 1, name: 'Cab acoustics',
    layout: { acoustics: { enabled: true, leftWindow: 37, rightWindow: 0, bodyCutoff: 80000 } },
  };
  const { entry } = parsePresetFile(raw, 'acoustics.json');
  const exported = presetFile(entry);
  const imported = parsePresetFile(exported, 'acoustics.json').entry;
  assert.deepEqual(spatialProfile(imported.layout).layout.acoustics, entry.layout.acoustics);
  assert.equal(imported.layout.acoustics.leftWindow, 37);
  assert.equal(imported.layout.acoustics.rightWindow, 0);
  assert.equal(imported.layout.acoustics.bodyCutoff, 4000);
  assert.equal(spatialPose(null, 0, false, 1, 0).exterior, true);
  assert.equal(spatialPose(null, 0, false, 1).exterior, undefined);
});
