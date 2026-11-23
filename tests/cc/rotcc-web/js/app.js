'use strict';

renderLayers();
renderKeyboard();
renderQuickSelect();
renderPicker();
renderParamKeyboard();
renderParamTabs();
renderParamForm(null);
renderAxisKeyboard();
renderAxisTabs();
if (globalThis.ResizeObserver) {
  const keyboardObserver = new ResizeObserver(syncKeycapSize);
  for (const keyboard of [$keyboard, $paramKeyboard, $axisKeyboard]) {
    keyboardObserver.observe(keyboard.closest('.keyboard-card'));
  }
  new ResizeObserver(syncBoardSize).observe($pickerGroups);
}

const navButtons = [...document.querySelectorAll('.nav-btn')];
function selectView(name) {
  for (const btn of navButtons) {
    btn.classList.toggle('active', btn.dataset.view === name);
  }
  for (const view of ['remap', 'param', 'axis', 'upgrade', 'system', 'debug']) {
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
$btnHandshake.addEventListener('click', () => {
  setSysState('正在握手...');
  queryHandshake().catch(() => {});
});
$btnKeyCount.addEventListener('click', () => {
  queryKeyCount().catch(() => {});
});
$btnSysReboot.addEventListener('click', rebootDevice);
$btnPopupSupport.addEventListener('click', () => {
  queryPopupSupport().catch(() => {});
});
$btnPopupRead.addEventListener('click', () => {
  queryPopupState().catch(() => {});
});
$btnPopupWrite.addEventListener('click', writePopupState);
$btnSysReset.addEventListener('click', () => {
  let options = 0;
  if ($chkResetKc.checked) options |= SYS_RESET_KC;
  if ($chkResetRgb.checked) options |= SYS_RESET_RGB;
  if ($chkResetAdv.checked) options |= SYS_RESET_ADV;
  if (options === 0) {
    setSysState('请至少选择一个模块。', 'warn');
    return;
  }
  resetDevice(options, '恢复所选模块');
});
$btnSysResetAll.addEventListener('click', () => {
  resetDevice(SYS_RESET_ALL, '恢复出厂');
});
$btnParamSelectAll.addEventListener('click', selectAllParamKeys);
$btnParamClear.addEventListener('click', clearParamSelection);
$btnParamRead.addEventListener('click', readParamValues);
$btnParamWrite.addEventListener('click', writeParamValues);
$btnAxisSelectAll.addEventListener('click', selectAllAxisKeys);
$btnAxisClear.addEventListener('click', clearAxisSelection);
$btnAxisList.addEventListener('click', readAxisSupportList);
$btnAxisWrite.addEventListener('click', writeAxisToSelection);
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
