// Help › Check for updates: the latest release on GitHub against this version, asked only when
// the menu item is pressed (nothing is checked by itself). A newer one: its page opens in the
// browser on request; the zip is unpacked over this folder by hand, data/ staying as it is.
import { app, dialog, net, shell } from 'electron';
import { isNewer } from '../shared/version.js';

export const RELEASES_URL = 'https://github.com/OscumoG/TruckAudioSim/releases';
const LATEST_API = 'https://api.github.com/repos/OscumoG/TruckAudioSim/releases/latest';
const TIMEOUT_MS = 10000;

export async function checkForUpdates(win) {
  const current = app.getVersion();
  let release;
  try {
    const response = await net.fetch(LATEST_API, {
      headers: { Accept: 'application/vnd.github+json', 'User-Agent': `TruckAudioSim ${current}` },
      signal: AbortSignal.timeout(TIMEOUT_MS),
    });
    if (!response.ok) throw new Error(`GitHub answered ${response.status}`);
    release = await response.json();
  } catch (err) {
    await dialog.showMessageBox(win, { type: 'warning', title: 'Check for updates', message: 'Could not check for updates.', detail: err.message });
    return;
  }
  const tag = String(release.tag_name ?? '');
  if (!isNewer(tag, current)) {
    await dialog.showMessageBox(win, { type: 'info', title: 'Check for updates', message: `TruckAudioSim ${current} is the latest version.` });
    return;
  }
  const { response } = await dialog.showMessageBox(win, {
    type: 'info',
    title: 'Check for updates',
    message: `TruckAudioSim ${tag.replace(/^v/, '')} is out (you have ${current}).`,
    detail: 'Close TruckAudioSim from the tray icon before updating. Download the installer from the release page.',
    buttons: ['Open the release page', 'Later'],
    defaultId: 0,
    cancelId: 1,
  });
  // Only a page of this repository's releases is opened.
  const page = String(release.html_url ?? '');
  if (response === 0) shell.openExternal(page.startsWith(`${RELEASES_URL}/`) ? page : RELEASES_URL);
}
