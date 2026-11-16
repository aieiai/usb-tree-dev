'use strict';

renderLayers();
renderKeyboard();
renderPicker();
if (globalThis.ResizeObserver) {
  new ResizeObserver(syncKeycapSize).observe($keyboard.closest('.keyboard-card'));
  new ResizeObserver(syncBoardSize).observe($pickerGroups);
}

const navButtons = [...document.querySelectorAll('.nav-btn')];
function selectView(name) {
  for (const btn of navButtons) {
    btn.classList.toggle('active', btn.dataset.view === name);
  }
  for (const view of ['remap', 'upgrade', 'debug']) {
    document.getElementById(`view-${view}`)
      .classList.toggle('active', view === name);
  }
}
for (const btn of navButtons) {
  btn.addEventListener('click', () => selectView(btn.dataset.view));
}

$firmwareFile.addEventListener('change', () => {
  resetFirmwareDuration();
  const file = $firmwareFile.files[0];
  if (!file) {
    setFirmwareState('请选择升级文件。');
    updateFirmwareProgress(0, 0);
    refreshFirmwareControls();
    return;
  }
  updateFirmwareProgress(0, file.size);
  setFirmwareState(`已选择 ${file.name}，等待开始升级。`);
  refreshFirmwareControls();
});
$btnFirmwareStart.addEventListener('click', startFirmwareUpgrade);
$btnFirmwareCancel.addEventListener('click', cancelFirmwareUpgrade);
$btnCcQuery.addEventListener('click', () => {
  queryCcDeviceInfo().catch(() => {});
});
btnConnect.addEventListener('click', connect);
btnDisconnect.addEventListener('click', disconnect);
document.getElementById('btnClear').addEventListener('click', () => {
  $log.textContent = '';
  rxBytes = 0;
  txBytes = 0;
  $traffic.textContent = 'RX 0 B · TX 0 B';
});
$chkLf.addEventListener('change', () => {
  for (const link of links) {
    for (const ep of link.alt.endpoints) {
      if (ep.direction !== 'out') continue;
      const input = link.ui.get(endpointKey(ep))?.input;
      if (input) input.placeholder = $inputMode.value === 'hex'
        ? '例如：01 02 0a ff'
        : '输入文本，按回车或点击发送';
    }
  }
});
$inputMode.addEventListener('change', () => {
  for (const link of links) {
    for (const ep of link.alt.endpoints) {
      if (ep.direction !== 'out') continue;
      const input = link.ui.get(endpointKey(ep))?.input;
      if (input) input.placeholder = $inputMode.value === 'hex'
        ? '例如：01 02 0a ff'
        : '输入文本，按回车或点击发送';
    }
  }
});
$chkAll.addEventListener('change', async () => {
  if (!$chkAll.checked || !device || !device.opened) return;
  for (const link of links) {
    if (!link.claimed) await claimLink(link);
  }
  refreshControls();
});

navigator.usb?.addEventListener('disconnect', event => {
  if (device && event.device === device) {
    appendLine('[设备已拔出]', 'err');
    disconnect();
  }
});
