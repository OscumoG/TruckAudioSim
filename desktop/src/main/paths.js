import path from 'node:path';

// Where Trucker AUX keeps layouts, settings and the Chromium profile.
// Development: app/data. Installed: data/ in the user's application data.
// Portable: data/ next to the executable
// (electron-builder portable builds report their folder in PORTABLE_EXECUTABLE_DIR).
export function resolveDataDir({ isPackaged, appPath, execPath, portableDir, installedRoot }) {
  if (!isPackaged) return path.join(appPath, 'data');
  return path.join(installedRoot || portableDir || path.dirname(execPath), 'data');
}
