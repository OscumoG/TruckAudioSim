const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('aux', {
  load: () => ipcRenderer.invoke('store:load'),
  saveLayouts: (store) => ipcRenderer.invoke('store:save-layouts', store),
  saveSettings: (settings) => ipcRenderer.invoke('store:save-settings', settings),
  onPose: (listener) => ipcRenderer.on('pose', (_event, pose) => listener(pose)),
  // { collection, warnings } whenever presets/ changes (main/collection.js).
  onCollection: (listener) => ipcRenderer.on('collection', (_event, collection) => listener(collection)),
  // Asks where (Save As) and writes [{ fileName, data }]: one file, or a folder named name.
  exportPresets: (files, name) => ipcRenderer.invoke('collection:export', files, name),
  // Asks for preset files (Open) and reads them: { entries, warnings } or { canceled }.
  importPresets: () => ipcRenderer.invoke('collection:import'),
  // File › Import / Export of a data folder (main/data-transfer.js): kind 'all', 'presets' or
  // 'settings'; { ok } (then reload), { warning } or { canceled }.
  importData: (kind) => ipcRenderer.invoke('data:import', kind),
  exportData: () => ipcRenderer.invoke('data:export'),
  // The menu bar's File items, carried out here (app.js).
  onMenu: (listener) => ipcRenderer.on('menu', (_event, action) => listener(action)),
  // Whether there is something to undo and to redo, for Edit's items.
  menuHistory: (undo, redo) => ipcRenderer.send('menu:history', { undo, redo }),
  // Whether the Editor View shows, for View › Editor View's tick.
  menuEditor: (open) => ipcRenderer.send('menu:editor', open),
  // The window's width as the Editor View closes (the tree and the panel: width) or opens, and
  // while it is closed.
  editorWindow: (open, width) => ipcRenderer.send('window:editor', { open, width }),
  fitWindow: (width) => ipcRenderer.send('window:fit', width),
  // The cab's keys and controller buttons (main/controls.js): the bindings, a combination
  // pressed (ids, or null), the controllers' names, the actions as they fire, warnings.
  setControls: (bindings) => ipcRenderer.invoke('controls:set', bindings),
  captureControl: () => ipcRenderer.invoke('controls:capture'),
  cancelCapture: () => ipcRenderer.invoke('controls:cancel'),
  controlNames: () => ipcRenderer.invoke('controls:names'),
  onControl: (listener) => ipcRenderer.on('controls:action', (_event, action) => listener(action)),
  volumeFeedback: (percent, muted) => ipcRenderer.send('controls:volume-feedback', { percent, muted }),
  onControlWarning: (listener) => ipcRenderer.on('controls:warning', (_event, message) => listener(message)),
  spatialDevices: () => ipcRenderer.invoke('spatial:devices'),
  startSpatial: (options) => ipcRenderer.invoke('spatial:start', options),
  stopSpatial: () => ipcRenderer.invoke('spatial:stop'),
  updateSpatial: (update) => ipcRenderer.send('spatial:update', update),
  onSpatialStatus: (listener) => ipcRenderer.on('spatial:status', (_event, status) => listener(status)),
  openVolumeMixer: () => ipcRenderer.invoke('spatial:mixer'),
  installGamePlugins: () => ipcRenderer.invoke('game-plugins:install'),
});
