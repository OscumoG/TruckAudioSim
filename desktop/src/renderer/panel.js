// The control panel on the left. Built once; update(view) refreshes values in place
// so a field the user is typing into keeps its focus.
import { LABEL_COLORS, LABEL_MAX, TYPES, TYPE_NAMES } from '../shared/layout.js';
import { speakerIcon } from './marks.js';
import { ACTIONS, ACTION_NAMES, VARIANT_ACTIONS, VOLUME_ACTIONS, bindingText } from '../shared/controls.js';
import { AXES, MAX_VOLUME } from '../shared/settings.js';

const CHANNEL_OPTIONS = [
  { value: 'L', label: 'Left' },
  { value: 'R', label: 'Right' },
  { value: 'M', label: 'Mono (L+R) / centre' },
  ...Object.entries({ FL: 'Front left', FR: 'Front right', FC: 'Front centre', LFE: 'LFE',
    SL: 'Side left', SR: 'Side right', BL: 'Rear left', BR: 'Rear right' })
    .map(([value, label]) => ({ value, label })),
];
const TYPE_OPTIONS = TYPES.map((value) => ({ value, label: TYPE_NAMES[value] }));
const TYPE_BADGE = { full: '', small: 'small', tweeter: 'tweeter', mid: 'mid', midbass: 'midbass', sub: 'sub' };
const MUTE_OPTIONS = [
  { value: 'never', label: 'Never' },
  { value: 'engine', label: 'Engine is off' },
  { value: 'electric', label: 'Electrics are off' },
];
const ELECTRIC_PLAYER_OPTIONS = [
  { value: 'off', label: 'Off (mute only)' },
  { value: 'spotify', label: 'Spotify' },
  { value: 'vlc', label: 'VLC (Windows media session)' },
];

// While the game is paused (settings.js PAUSE_BEHAVIORS).
const PAUSE_OPTIONS = [
  { value: 'active', label: 'Always active' },
  { value: 'muted', label: 'Always muted' },
  { value: 'vehicle', label: 'Active vehicle' },
  { value: 'preset', label: 'Pause menu preset' },
];

// Where the head comes from (settings.js HEAD_SOURCES), and what the Camera fieldset says.
const HEAD_OPTIONS = [
  { value: 'game', label: 'Raw Camera - Recommended' },
  { value: 'vr', label: 'Raw Telemetry - VR Recommended' },
  { value: 'telemetry', label: 'Adjusted Telemetry' },
];
const EMULATED = 'Set these as in the game\'s Accessibility options: the game turns its camera without telling the telemetry, so the sound is turned here. 100 % is 35° at full lock; a blinker turns it 20° to the driver\'s side or 40° across.';
const CAMERA_HINTS = {
  game: 'Sound follows the camera. Outside the cab the music fades with distance and is silent at 40 m.',
  fallback: `trucker_aux_camera.dll does not read the game's camera (it is missing, or does not know this game version), so the telemetry gives the head. ${EMULATED}`,
  telemetry: `The telemetry gives the head; the game's camera is not read (no free camera). ${EMULATED}`,
  vr: 'The telemetry gives the headset\'s pose as it is; the game turns nothing on its own in VR, so nothing is added. The game\'s camera is not read (no free camera).',
};

// The game's options for looking into turns on the reverse gear (settings.js TURN_LOOK_REVERSE).
const REVERSE_OPTIONS = [
  { value: 'off', label: 'Off' },
  { value: 'on', label: 'On' },
  { value: 'inverted', label: 'Inverted' },
];

function el(tag, props = {}, children = []) {
  const node = document.createElement(tag);
  Object.assign(node, props);
  node.append(...[].concat(children));
  return node;
}

function row(label, control) {
  return el('label', { className: 'row' }, [el('span', { textContent: label }), control]);
}

// options: { value, label } entries, or { group, options } for a group of them. An entry
// may have `full`: the text it shows while selected, as a closed list hides the group.
function fillSelect(select, options, value) {
  const key = JSON.stringify(options);
  if (select.dataset.key !== key) {
    const option = (o) => {
      const node = el('option', { value: o.value, textContent: o.label });
      node.dataset.label = o.label;
      if (o.full) node.dataset.full = o.full;
      return node;
    };
    select.replaceChildren(...options.map((o) => (o.group ? el('optgroup', { label: o.group }, o.options.map(option)) : option(o))));
    select.dataset.key = key;
  }
  select.value = value ?? '';
  for (const node of select.options) {
    const text = node.selected && node.dataset.full ? node.dataset.full : node.dataset.label;
    if (text !== undefined && node.textContent !== text) node.textContent = text;
  }
}

// Sets a field unless the user is editing that very field.
function setValue(input, value) {
  if (document.activeElement !== input) input.value = value;
}

// The wheel scrolling the page carries on over a slider that comes under the pointer: a slider
// takes the wheel only after a pause in it (WHEEL_GAP_MS) and once the pointer has rested on it
// (WHEEL_DWELL_MS), so scrolling the panel, or the pointer passing over a slider, moves nothing.
const WHEEL_GAP_MS = 400;
const WHEEL_DWELL_MS = 250;
let pageWheelAt = -Infinity; // the last wheel event no slider took
document.addEventListener('wheel', (event) => {
  if (!event.defaultPrevented) pageWheelAt = performance.now();
}, { passive: true });

// The mouse wheel over a slider moves it by its step a notch (up: more), as dragging it would:
// its input and change events follow, and the page does not scroll. Not from a press of the
// left button on it until the pointer has left it, released or not: a wheel that slips while
// the slider is dragged, or just after, moves nothing.
function wheelSlider(input) {
  let held = false;
  let enteredAt = -Infinity;
  input.addEventListener('pointerenter', () => {
    enteredAt = performance.now();
  });
  const outside = (event) => {
    const box = input.getBoundingClientRect();
    return event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom;
  };
  input.addEventListener('pointerdown', (event) => {
    if (event.button === 0) held = true;
  });
  input.addEventListener('pointerleave', () => {
    held = false;
  });
  input.addEventListener('pointerup', (event) => {
    if (outside(event)) held = false; // let go after dragging off it
  });
  input.addEventListener('wheel', (event) => {
    if (input.disabled) return;
    const now = performance.now();
    if (now - pageWheelAt < WHEEL_GAP_MS || now - enteredAt < WHEEL_DWELL_MS) return; // the page scrolls on
    event.preventDefault();
    const delta = event.deltaY || event.deltaX;
    if (held || !delta) return;
    const step = Number(input.step) || 1;
    const moved = Number(input.value) + step * -Math.sign(delta);
    const value = Math.min(Number(input.max), Math.max(Number(input.min), Math.round(moved / step) * step));
    if (value === Number(input.value)) return;
    input.value = String(Number(value.toFixed(6)));
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new Event('change', { bubbles: true }));
  }, { passive: false });
}

function toggleButton(text, action, on, title) {
  const button = el('button', { textContent: text, title, className: on ? 'on' : '' });
  button.dataset.action = action;
  return button;
}

// boundsRoot: where the Bounds fieldset goes (under the 3D overview).
// A fieldset folds at a click on its legend (▾ / ▸), so the speakers can be brought up while
// editing; which ones are folded is kept per viewer, as the tree's width (localStorage), by the
// legend's first text.
const FOLDED_KEY = 'truckerAux.folded';
function readFolded() {
  try {
    const list = JSON.parse(localStorage.getItem(FOLDED_KEY) ?? '[]');
    return new Set(Array.isArray(list) ? list : []);
  } catch {
    return new Set();
  }
}
function foldable(fieldset) {
  const legend = fieldset.querySelector(':scope > legend');
  if (!legend) return;
  const id = legend.textContent;
  legend.classList.add('foldable');
  legend.title = 'Click to fold or unfold';
  fieldset.classList.toggle('folded', readFolded().has(id));
  legend.onclick = () => {
    const folded = readFolded();
    if (folded.has(id)) folded.delete(id);
    else folded.add(id);
    try {
      localStorage.setItem(FOLDED_KEY, JSON.stringify([...folded]));
    } catch {
      // not kept: it still folds now
    }
    fieldset.classList.toggle('folded', folded.has(id));
  };
}

export function createPanel(root, actions, boundsRoot) {
  // Two lines that never wrap (app.js statusText): the vehicle, then the angles.
  const statusName = el('div', { className: 'status-name', textContent: 'Starting…' });
  const statusDetail = el('div', { className: 'status-detail', textContent: '\u00a0' });
  const status = el('div', { className: 'status' }, [statusName, statusDetail]);
  const warnings = el('ul', { className: 'warnings' });

  const input = el('select');
  const output = el('select');
  const cableHint = el('p', { className: 'hint' });
  const audioStatus = el('p', { className: 'hint' });
  const start = el('button', { textContent: 'Start' });
  const stop = el('button', { textContent: 'Stop' });
  const test = el('button', { textContent: 'Test 7.1' });
  const mixer = el('button', { textContent: 'Windows mixer' });
  const refresh = el('button', { textContent: 'Refresh devices' });
  start.onclick = () => actions.startAudio();
  stop.onclick = () => actions.stopAudio();
  test.onclick = () => actions.testAudio();
  mixer.onclick = () => actions.openVolumeMixer();
  refresh.onclick = () => actions.refreshAudioDevices();
  const muteWhen = el('select', { title: 'Mute the music like a car radio while the vehicle is switched off in the game' });
  const electricPlayer = el('select', { title: 'Pause this Windows media player when the truck electrics switch off' });
  const mediaStatus = el('p', { className: 'hint' });
  const install = el('button', { textContent: 'Install ETS2 / ATS plugins…' });
  install.onclick = () => actions.installGamePlugins();
  const pauseBehavior = el('select', {
    title: 'While the game is paused: keep playing, mute, or play while the vehicle is on, as "Mute when" says; or play the preset on the tree\'s "Pause menu" key, in the game\'s menus too, the head at rest',
  });
  const volume = el('input', { type: 'range', min: 0, max: MAX_VOLUME * 100, step: 1, title: 'Spatial audio volume. Higher settings can clip loud signals.' });
  const volumeValue = el('span', { className: 'value' });
  const muteButton = el('button', { className: 'mute-button', textContent: 'Mute', title: 'Silence the music until pressed again or the volume changes (Volume Control Bindings: a key for it)' });
  muteButton.onclick = () => actions.toggleMusicMute();
  // The game's camera options, set as in the game (pose.js turnLook).
  const turnLookOn = el('input', { type: 'checkbox', title: 'The game\'s "look into turns"' });
  const turnLookPercent = el('input', {
    type: 'number', min: 0, max: 200, step: 5, className: 'narrow', title: 'As in the game: 100 % turns 35° at full lock',
  });
  const turnLookReverse = el('select', { title: '"Look into turns" on the reverse gear, as in the game' });
  const turnLookBlinkers = el('input', {
    type: 'checkbox', title: 'The game\'s "look toward the blinker": while a blinker is on, 20° toward the driver\'s side or 40° across (in a left-hand-drive cab 20° left, 40° right)',
  });

  // The cab's keys and controller buttons (shared/controls.js): a button per action showing its
  // binding, a click waits for a combination (app.js bindControl), × clears it. A row of its own,
  // not a label: a click on the name does not start a capture.
  const BIND_HINT = 'Click, then press a key, a controller button, or up to three together; let go to set it. Escape cancels';
  const controlRow = (action, name = ACTION_NAMES[action]) => {
    const bind = el('button', { className: 'bind' });
    const clear = el('button', { textContent: '×', title: 'Clear' });
    bind.onclick = () => actions.bindControl(action);
    clear.onclick = () => actions.clearControl(action);
    const line = el('div', { className: 'row' }, [el('span', { textContent: name }), el('div', { className: 'inline' }, [bind, clear])]);
    return { action, bind, clear, line };
  };
  const controlRows = ACTIONS.map((action) => controlRow(action));
  // A binding's button: what is bound, or waiting for a press.
  const showBinding = (r, ids, view) => {
    const waiting = view.capturing === r.action;
    const text = bindingText(ids, view.controlNames);
    r.bind.textContent = waiting ? 'Press… (Esc cancels)' : text;
    r.bind.title = `${text}\n${BIND_HINT}`; // a long controller name is cut on the button
    r.bind.classList.toggle('waiting', waiting);
    r.clear.disabled = !ids.length;
  };

  // The Editor View closed (app.js): a way back from Layout and Speakers.
  const openEditor = [0, 1].map(() => el('div', { className: 'inline' }, [el('button', {
    className: 'open-editor', textContent: 'Open Editor View', title: 'The projections, the 3D view and Bounds (View › Editor View)', onclick: () => actions.setEditorOpen(true),
  })]));

  const rowsOf = (list) => controlRows.filter((row) => list.includes(row.action)).map((row) => row.line);
  const controlsForm = el('fieldset', {}, [
    el('legend', { textContent: 'Variant Control Bindings' }),
    ...rowsOf(VARIANT_ACTIONS),
    el('p', {
      className: 'hint',
      textContent: 'They work while the game has the focus. Next and Prev. try the variants of the vehicle\'s model; Auto goes back to the one that fitted when you got in. Getting out keeps the one tried for that cab. The game sees these keys and buttons too: pick ones it does not use.',
    }),
  ]);
  const volumeControlsForm = el('fieldset', {}, [
    el('legend', { textContent: 'Volume Control Bindings' }),
    ...rowsOf(VOLUME_ACTIONS),
    el('p', {
      className: 'hint',
      textContent: 'They work while the game has the focus. Volume Up and Down move Audio › Volume in steps of 10 percentage points (hold to repeat); Mute silences the music until pressed again or the volume changes. The game sees these keys and buttons too: pick ones it does not use.',
    }),
  ]);

  const preset = el('select');
  // The truck in the game: what plays, what it applies to, and what can be done
  // (presets.js truckStatus). Buttons carry an action name.
  const cardTruck = el('span');
  const cardPlays = el('span');
  // Applies to: text, or the vehicle's ladder to move the preset that plays (Auto).
  const cardScopeText = el('span');
  const cardScopeSelect = el('select', { title: 'Move the preset that plays to a narrower or wider scope' });
  const cardScope = el('span', {}, [cardScopeText, cardScopeSelect]);
  const cardUseIn = el('select', { title: 'Put the preset picked in the list to use in this vehicle' });
  const cardNote = el('p', { className: 'hint card-note' });
  const cardUseInLabel = el('span', { className: 'cab-label', textContent: 'Use it in' });
  const cardButtons = el('div', { className: 'inline buttons' });
  // The model's variants, and your vehicle's own preset: presets.js variantChoice.
  const cardVariantLabel = el('span', { className: 'cab-label', textContent: 'Variant' });
  const cardVariant = el('select', { title: 'Which of the model\'s variants plays in this vehicle: its hook remembers it (your own vehicle: its plate). A hook new to you plays the one picked last' });
  const truckCard = el('div', { className: 'cab-card' }, [
    el('span', { className: 'cab-label', textContent: 'Vehicle' }), cardTruck,
    el('span', { className: 'cab-label', textContent: 'Plays' }), cardPlays,
    cardVariantLabel, cardVariant,
    el('span', { className: 'cab-label', textContent: 'Applies to' }), cardScope,
    cardNote,
    cardUseInLabel, cardUseIn,
    el('span'), cardButtons,
  ]);
  cardScopeSelect.onchange = () => actions.moveToScope(cardScopeSelect.value);
  cardUseIn.onchange = () => actions.useInScope(cardUseIn.value);
  cardVariant.onchange = () => actions.cardVariant(cardVariant.value);
  cardButtons.onclick = (event) => {
    const action = event.target.closest('button')?.dataset.action;
    if (action) actions.truckAction(action);
  };
  const presetName = el('input', { type: 'text', title: 'Name of this preset' });
  const presetLabel = el('input', {
    type: 'text', maxLength: LABEL_MAX, className: 'narrow-text', placeholder: 'short tag',
    title: 'A short label, shown beside the keys that use this preset in the preset map',
  });
  const presetLabelColor = el('select', { title: 'Colour of the label in the preset map' });
  const newPreset = el('button', {
    textContent: 'New preset',
    title: 'Copy the current layout into a new preset and switch to it. Auto never picks it: choose it in the list.',
  });
  const deletePreset = el('button', { textContent: 'Delete preset' });
  // Shared presets: one file each in presets/, listed under Collection (collection.js).
  const exportButton = el('button', { textContent: 'Export…', title: 'Save presets as files to share, this one ticked' });
  const speakerHint = el('p', { className: 'hint', textContent: 'Realista, 5.1 and 7.1 presets filter by speaker type. Surround speakers require multichannel input.' });
  const list = el('ul', { className: 'speakers' });
  const add = el('button', { textContent: '+ Speaker' });
  const addPair = el('button', { textContent: '+ Pair' });
  const duplicate = el('button', { textContent: 'Duplicate', title: 'Duplicate the selected speakers (Ctrl+D)' });
  const copy = el('button', { textContent: 'Copy', title: 'Copy the selected speakers, also to another preset (Ctrl+C)' });
  const paste = el('button', { textContent: 'Paste', title: 'Paste the copied speakers (Ctrl+V)' });

  const name = el('input', { type: 'text' });
  const channel = el('select');
  const type = el('select');
  const gain = el('input', { type: 'range', min: -60, max: 12, step: 0.5 });
  const gainValue = el('span', { className: 'value' });
  const pair = el('select');
  const coords = [0, 1, 2].map(() => el('input', { type: 'number', step: 1 }));
  const remove = el('button', { textContent: 'Delete speaker' });
  fillSelect(channel, CHANNEL_OPTIONS, 'L');
  fillSelect(type, TYPE_OPTIONS, 'full');

  const formTitle = el('legend', { textContent: 'Speaker' });
  const speakerForm = el('fieldset', {}, [
    formTitle,
    row('Name', name),
    row('Channel', channel),
    row('Type', type),
    row('Level', el('div', { className: 'inline' }, [gain, gainValue])),
    row('Mirror of', pair),
    row('X / Y / Z, cm', el('div', { className: 'coords' }, coords)),
    el('p', { className: 'hint', textContent: 'X right, Y up, Z back, from the zero point (0 in the views).' }),
    remove,
  ]);

  // The bounds: a box around the zero point, a guide for placing speakers. Symmetric about
  // the truck's axis, so one width; the other walls as positive distances from zero.
  const WALLS = [
    { label: 'Width', read: (b) => b.max[0] - b.min[0], write: (cm) => actions.setBoundsEdge(0, 'max', cm / 200) },
    { label: 'Bottom', read: (b) => -b.min[1], write: (cm) => actions.setBoundsEdge(1, 'min', -cm / 100) },
    { label: 'Top', read: (b) => b.max[1], write: (cm) => actions.setBoundsEdge(1, 'max', cm / 100) },
    { label: 'Front', read: (b) => -b.min[2], write: (cm) => actions.setBoundsEdge(2, 'min', -cm / 100) },
    { label: 'Back', read: (b) => b.max[2], write: (cm) => actions.setBoundsEdge(2, 'max', cm / 100) },
  ];
  const walls = WALLS.map((wall) => {
    const input = el('input', { type: 'number', step: 1, min: 0 });
    input.onchange = () => wall.write(Number(input.value));
    return { ...wall, input, label: el('label', {}, [wall.label, input]) };
  });
  const headNote = el('p', { className: 'hint' });
  // Where the head comes from: the game's camera (trucker_aux_camera.dll), or the telemetry, raw
  // (VR) or adjusted. The look into turns and toward the blinker are emulated only for Adjusted
  // Telemetry, or once the app has seen the game without the camera (view.cameraFallback,
  // shared/camera.js createFallbackWatch); never for Raw Telemetry. The axes locked are the mode's own.
  const headFrom = el('select', {
    title: 'Raw Camera: the game\'s camera through trucker_aux_camera.dll as it is (the telemetry when it does not read it). Raw Telemetry: the telemetry\'s head as it is, a VR headset\'s pose. Adjusted Telemetry: the telemetry with the game\'s look into turns and toward the blinker added, for when Raw Camera misbehaves',
  });
  headFrom.onchange = () => actions.setHeadFrom(headFrom.value);
  const locks = Object.fromEntries(AXES.map((axis) => {
    const box = el('input', { type: 'checkbox' });
    box.onchange = () => actions.setAxisLock(axis, box.checked);
    return [axis, box];
  }));
  const lockRow = row('Lock', el('div', {
    className: 'inline',
    title: 'A locked axis of the head stays straight in the cab: the sound does not follow it (the free camera\'s turns are its own)',
  }, AXES.map((axis) => el('label', { className: 'inline' }, [locks[axis], axis[0].toUpperCase() + axis.slice(1)]))));
  const emulation = el('div', {}, [
    row('Into turns', el('div', { className: 'inline' }, [
      turnLookOn,
      el('label', { className: 'inline' }, [turnLookPercent, '%']),
    ])),
    row('In reverse', turnLookReverse),
    row('Blinkers', el('div', { className: 'inline' }, [turnLookBlinkers, 'Look toward them'])),
  ]);
  const cameraHint = el('p', { className: 'hint' });
  const cameraForm = el('fieldset', {}, [
    el('legend', { textContent: 'Camera' }),
    install,
    row('Mode', headFrom),
    lockRow,
    emulation,
    cameraHint,
  ]);
  const boundsForm = el('fieldset', {}, [
    el('legend', { textContent: 'Bounds, cm' }),
    el('div', { className: 'walls' }, walls.map((w) => w.label)),
    el('p', { className: 'hint', textContent: 'The bounds do not change the sound: they only frame the views for placing speakers.' }),
    headNote,
  ]);

  root.replaceChildren(
    status,
    warnings,
    el('fieldset', {}, [
      el('legend', { textContent: 'Audio' }),
      row('Capture from', input),
      cableHint,
      row('Headphones / output', output),
      el('div', { className: 'inline buttons' }, [start, stop, test]),
      el('div', { className: 'inline buttons' }, [mixer, refresh]),
      audioStatus,
      row('Mute when', muteWhen),
      row('Pause player on electrics off', electricPlayer),
      mediaStatus,
      row('Pause behavior', pauseBehavior),
      row('Volume', el('div', { className: 'inline' }, [volume, volumeValue, muteButton])),
    ]),
    cameraForm,
    controlsForm,
    volumeControlsForm,
    el('fieldset', {}, [
      el('legend', { textContent: 'Layout' }),
      openEditor[0],
      row('Preset', preset),
      truckCard,
      row('Name', presetName),
      row('Label', el('div', { className: 'inline' }, [presetLabel, presetLabelColor])),
      el('div', { className: 'inline buttons' }, [newPreset, deletePreset, exportButton]),
      speakerHint,
    ]),
    el('fieldset', {}, [
      el('legend', { textContent: 'Speakers' }),
      openEditor[1],
      list,
      el('div', { className: 'inline buttons' }, [add, addPair, duplicate, copy, paste]),
      el('p', { className: 'hint', textContent: 'Ctrl+click or drag a box in a view to select several; Shift+click a range; Ctrl+A all.' }),
    ]),
    speakerForm,
  );
  boundsRoot.replaceChildren(boundsForm);
  const fieldsets = [...root.querySelectorAll(':scope > fieldset'), boundsForm];
  fieldsets.forEach(foldable);

  input.onchange = () => actions.selectInput(input.value);
  output.onchange = () => actions.selectOutput(output.value);
  muteWhen.onchange = () => actions.setMuteWhen(muteWhen.value);
  electricPlayer.onchange = () => actions.setElectricPlayer(electricPlayer.value);
  pauseBehavior.onchange = () => actions.setPauseBehavior(pauseBehavior.value);
  volume.oninput = () => actions.setVolume(Number(volume.value) / 100, false);
  volume.onchange = () => actions.setVolume(Number(volume.value) / 100);
  turnLookOn.onchange = () => actions.setTurnLook({ on: turnLookOn.checked });
  turnLookPercent.onchange = () => actions.setTurnLook({ percent: Number(turnLookPercent.value) });
  turnLookReverse.onchange = () => actions.setTurnLook({ reverse: turnLookReverse.value });
  turnLookBlinkers.onchange = () => actions.setTurnLook({ blinkers: turnLookBlinkers.checked });
  preset.onchange = () => actions.selectPreset(preset.value);
  deletePreset.onclick = () => actions.deleteCurrentPreset();
  exportButton.onclick = () => actions.exportPresets('playing');
  newPreset.onclick = () => {
    actions.newPreset();
    presetName.focus();
    presetName.select();
  };
  presetName.onchange = () => actions.renamePreset(presetName.value);
  presetLabel.onchange = () => actions.setPresetLabel(presetLabel.value);
  presetLabelColor.onchange = () => actions.setPresetLabelColor(presetLabelColor.value);
  presetLabel.onkeydown = (event) => {
    if (event.key === 'Enter') presetLabel.blur();
  };
  presetName.onkeydown = (event) => {
    if (event.key === 'Enter') presetName.blur();
  };
  for (const slider of [volume, gain]) wheelSlider(slider);
  add.onclick = () => actions.addSpeaker();
  addPair.onclick = () => actions.addPair();
  duplicate.onclick = () => actions.duplicateSelected();
  copy.onclick = () => actions.copySelected();
  paste.onclick = () => actions.paste();
  list.onmousedown = (event) => {
    if (event.shiftKey) event.preventDefault(); // no text selection on shift+click
  };
  list.onclick = (event) => {
    const item = event.target.closest('li[data-id]');
    if (!item) return;
    const { id } = item.dataset;
    const what = event.target.dataset.action;
    if (what === 'solo') actions.toggleSolo(id);
    else if (what === 'mute') actions.toggleMute(id);
    else actions.selectSpeaker(id, { toggle: event.ctrlKey || event.metaKey, range: event.shiftKey });
  };
  name.onchange = () => actions.updateSelected({ name: name.value });
  // With several speakers selected, a "Mixed" entry is shown until a real value is chosen.
  channel.onchange = () => channel.value && actions.updateSelected({ channel: channel.value });
  type.onchange = () => type.value && actions.updateSelected({ type: type.value });
  gain.oninput = () => actions.setLevel(Number(gain.value));
  pair.onchange = () => actions.setPair(pair.value || null);
  coords.forEach((coord, axis) => {
    coord.onchange = () => coord.value !== '' && actions.setCoordinate(axis, Number(coord.value) / 100);
  });
  remove.onclick = () => actions.deleteSelected();

  function speakerItem(s, view) {
    const item = el('li', { className: view.picked.includes(s.id) ? 'selected' : '' }, [
      speakerIcon(s, view.silentIds.includes(s.id)),
      el('span', { textContent: s.name }),
      el('span', { className: 'badge', textContent: TYPE_BADGE[s.type] }),
      toggleButton('S', 'solo', view.solo === s.id, 'Solo'),
      toggleButton('M', 'mute', view.muted.has(s.id), 'Mute'),
    ]);
    item.dataset.id = s.id;
    return item;
  }

  function update(view) {
    warnings.replaceChildren(...view.warnings.map((text) => el('li', { textContent: text })));
    const choices = [{ value: '', label: 'Select…' }, ...view.devices.map((device) => ({ value: device.deviceId, label: device.label }))];
    fillSelect(input, choices, view.inputId ?? '');
    fillSelect(output, choices, view.outputId ?? '');
    const sourceDevice = view.devices.find((device) => device.deviceId === view.inputId);
    cableHint.textContent = sourceDevice ? `Route the source app to “${sourceDevice.label}” in Windows Volume mixer.` : '';
    cableHint.hidden = !sourceDevice;
    input.disabled = output.disabled = refresh.disabled = Boolean(view.audioMode);
    start.disabled = Boolean(view.audioMode) || !view.inputId || !view.outputId || view.inputId === view.outputId;
    test.disabled = Boolean(view.audioMode) || !view.outputId;
    stop.disabled = !view.audioMode || view.pendingStart;
    audioStatus.textContent = [view.audioStatus.pcm || (view.audioMode ? 'Opening Windows Spatial Sound…' : 'Audio stopped'),
      view.audioStatus.capacity && view.audioStatus.capacity !== 'OK' ? view.audioStatus.capacity : ''].filter(Boolean).join(' · ');
    fillSelect(muteWhen, MUTE_OPTIONS, view.muteWhen);
    fillSelect(electricPlayer, ELECTRIC_PLAYER_OPTIONS, view.electricPlayer);
    mediaStatus.textContent = view.audioStatus.mediaStatus ?? '';
    mediaStatus.hidden = view.electricPlayer === 'off' || !mediaStatus.textContent;
    fillSelect(pauseBehavior, PAUSE_OPTIONS, view.pauseBehavior);
    setValue(volume, Math.round(view.volume * 100));
    volumeValue.textContent = `${Math.round(view.volume * 100)} %`;
    muteButton.classList.toggle('on', view.mutedByHand);
    const look = view.turnLook;
    fillSelect(headFrom, HEAD_OPTIONS, view.headFrom);
    for (const axis of AXES) locks[axis].checked = view.axisLocks[axis];
    const emulated = view.headFrom === 'telemetry' || (view.headFrom === 'game' && view.cameraFallback);
    emulation.hidden = !emulated;
    if (view.headFrom === 'game') cameraHint.textContent = emulated ? CAMERA_HINTS.fallback : CAMERA_HINTS.game;
    else cameraHint.textContent = CAMERA_HINTS[view.headFrom];
    turnLookOn.checked = look.on;
    setValue(turnLookPercent, look.percent);
    turnLookPercent.disabled = !look.on;
    fillSelect(turnLookReverse, REVERSE_OPTIONS, look.reverse);
    turnLookReverse.disabled = !look.on;
    turnLookBlinkers.checked = look.blinkers;
    for (const r of controlRows) showBinding(r, view.controls[r.action], view);
    for (const line of openEditor) line.hidden = view.editorOpen;
    fillSelect(preset, view.presets, view.preset);
    const { card } = view;
    cardTruck.textContent = card.truck;
    cardPlays.textContent = card.plays;
    cardVariantLabel.hidden = !view.variant;
    cardVariant.hidden = !view.variant;
    if (view.variant) fillSelect(cardVariant, view.variant.options, view.variant.value);
    cardScopeText.textContent = card.appliesTo;
    cardScopeText.hidden = Boolean(card.scope);
    cardScopeSelect.hidden = !card.scope;
    if (card.scope) fillSelect(cardScopeSelect, card.scope.options, card.scope.value);
    cardUseIn.hidden = !card.useIn;
    if (card.useIn) fillSelect(cardUseIn, card.useIn.options, '');
    cardNote.textContent = card.note ?? '';
    cardNote.hidden = !card.note;

    const key = JSON.stringify(card.buttons);
    if (cardButtons.dataset.key !== key) {
      cardButtons.replaceChildren(...card.buttons.map((b) => {
        const button = el('button', { textContent: b.label });
        button.dataset.action = b.action;
        return button;
      }));
      cardButtons.dataset.key = key;
    }
    cardButtons.hidden = !card.buttons.length;
    cardUseInLabel.hidden = !card.useIn;
    deletePreset.disabled = !view.canDelete;
    presetName.disabled = !view.canRename; // a shared file keeps its name
    setValue(presetName, view.presetName);
    presetLabel.disabled = !view.canRename;
    setValue(presetLabel, view.presetLabel);
    fillSelect(presetLabelColor, LABEL_COLORS.map((c) => ({ value: c, label: c[0].toUpperCase() + c.slice(1) })), view.presetLabelColor);
    presetLabelColor.disabled = !view.canRename || !view.presetLabel;
    presetLabelColor.className = `label-color tag-${view.presetLabelColor}`;
    for (const w of walls) setValue(w.input, Math.round(w.read(view.bounds) * 100));
    const headCm = Math.round(view.headX * 100);
    headNote.textContent = view.headFromGame
      ? `Driver's head: X ${headCm} cm, from the game.`
      : `Driver's head: X ${headCm} cm, typical (no vehicle in the game).`;

    list.replaceChildren(...view.speakers.map((s) => speakerItem(s, view)));
    add.disabled = view.speakers.length >= view.maxSpeakers;
    addPair.disabled = view.speakers.length + 2 > view.maxSpeakers;
    const atCamera = view.cameraSource === 'free';
    add.title = atCamera ? 'Adds a speaker where the game\'s free camera is' : '';
    addPair.title = atCamera ? 'Adds a pair: the speaker on the free camera\'s side where the camera is, the other mirrored' : '';
    duplicate.disabled = !view.canDuplicate;
    copy.disabled = !view.selected.length;
    paste.disabled = !view.canPaste;

    const chosen = view.selected;
    const s = view.primary;
    speakerForm.hidden = !s;
    if (!s) return;
    const many = chosen.length > 1;
    // A value shared by every selected speaker, or null when they differ.
    const common = (get) => (chosen.every((o) => get(o) === get(s)) ? get(s) : null);
    formTitle.textContent = many ? `${chosen.length} speakers` : 'Speaker';
    remove.textContent = many ? `Delete ${chosen.length} speakers` : 'Delete speaker';
    name.disabled = many;
    setValue(name, many ? '' : s.name);
    for (const [select, options, key] of [[channel, CHANNEL_OPTIONS, 'channel'], [type, TYPE_OPTIONS, 'type']]) {
      const value = common((o) => o[key]);
      fillSelect(select, value === null ? [{ value: '', label: 'Mixed' }, ...options] : options, value ?? '');
    }
    setValue(gain, s.gainDb);
    const level = common((o) => o.gainDb);
    gainValue.textContent = level === null ? `${s.gainDb.toFixed(1)} dB, mixed` : `${s.gainDb.toFixed(1)} dB`;
    gain.title = many ? 'Moves the levels of all selected speakers by the same amount' : '';
    pair.disabled = many;
    fillSelect(pair, [
      { value: '', label: 'None' },
      ...view.speakers.filter((o) => o.id !== s.id).map((o) => ({ value: o.id, label: o.name })),
    ], many ? '' : s.pair ?? '');
    coords.forEach((c, i) => {
      const value = common((o) => o.position[i]);
      c.placeholder = value === null ? 'mixed' : '';
      setValue(c, value === null ? '' : Math.round(value * 100));
    });
  }

  return {
    update,
    // View › Reset panel layout: every fieldset unfolded.
    resetLayout() {
      try {
        localStorage.removeItem(FOLDED_KEY);
      } catch {
        // nothing kept
      }
      for (const fieldset of fieldsets) fieldset.classList.remove('folded');
    },
    setStatus([name, detail]) {
      statusName.textContent = name;
      statusName.title = name;
      statusDetail.textContent = detail || '\u00a0'; // the line keeps its height
      statusDetail.title = detail;
    },
  };
}
