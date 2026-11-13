const { ipcRenderer } = require('electron');
ipcRenderer.on('volume:label', (_event, label) => {
  document.getElementById('label').textContent = label;
});
