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
renderCalibKeyboard();
ccSetNotificationHandler(handleCcNotification);
if (globalThis.ResizeObserver) {
  const keyboardObserver = new ResizeObserver(syncKeycapSize);
  for (const keyboard of [$keyboard, $paramKeyboard, $axisKeyboard, $calibKeyboard]) {
    keyboardObserver.observe(keyboard.closest('.keyboard-card'));
  }
  new ResizeObserver(syncBoardSize).observe($pickerGroups);
}

const navButtons = [...document.querySelectorAll('.nav-btn')];
function selectView(name) {
  for (const btn of navButtons) {
    btn.classList.toggle('active', btn.dataset.view === name);
  }
  for (const view of ['remap', 'param', 'axis', 'calib', 'upgrade', 'system', 'debug']) {
    document.getElementById(`view-${view}`)
      .classList.toggle('active', view === name);
  }
  /* 日志在隐藏状态下无法滚动，切到调试页时补到底。 */
  if (name === 'debug') {
    scrollLogToBottom();
    requestAnimationFrame(scrollLogToBottom);
    setTimeout(scrollLogToBottom, 50);
    setTimeout(scrollLogToBottom, 200);
  }
  /* 校准页隐藏时画布尺寸为 0，显示后补画一次 sensor 曲线。 */
  if (name === 'calib') {
    requestAnimationFrame(() => {
      updateSensorChartTitle();
      drawSensorChart();
    });
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
$btnCalibRead.addEventListener('click', () => {
  queryCalibStatus().catch(() => {});
});
$btnCalibStart.addEventListener('click', () => {
  writeCalibStatus(CC.CALIB_STA_START).catch(() => {});
});
$btnCalibFinish.addEventListener('click', () => {
  writeCalibStatus(CC.CALIB_STA_DONE).catch(() => {});
});
$btnCalibStop.addEventListener('click', () => {
  writeCalibStatus(CC.CALIB_STA_STOP).catch(() => {});
});
$btnAutoCalibRead.addEventListener('click', () => {
  queryAutoCalib().catch(() => {});
});
$btnAutoCalibWrite.addEventListener('click', writeAutoCalib);
$btnSensorRptWrite.addEventListener('click', writeSensorReport);
$btnSensorFreqWrite.addEventListener('click', writeSensorFreq);
$btnSensorPause.addEventListener('click', () => setSensorPaused(!sensorPaused));
$btnSensorZoomIn.addEventListener('click', () => zoomSensorView(0.5));
$btnSensorZoomOut.addEventListener('click', () => zoomSensorView(2));
$btnSensorReset.addEventListener('click', resetSensorView);
$sensorChart.addEventListener('wheel', onSensorChartWheel, { passive: false });
$sensorChart.addEventListener('pointerdown', onSensorChartPointerDown);
$sensorChart.addEventListener('pointermove', onSensorChartPointerMove);
$sensorChart.addEventListener('pointerup', onSensorChartPointerUp);
$sensorChart.addEventListener('pointercancel', onSensorChartPointerUp);
$sensorChart.addEventListener('pointerleave', onSensorChartPointerLeave);
$sensorChart.addEventListener('dblclick', resetSensorView);
$btnParamSelectAll.addEventListener('click', selectAllParamKeys);
$btnParamClear.addEventListener('click', clearParamSelection);
$btnAxisRead.addEventListener('click', readAxisTypes);
$btnAxisSelectAll.addEventListener('click', selectAllAxisKeys);
$btnAxisClear.addEventListener('click', clearAxisSelection);
$axisValue.addEventListener('change', () => {
  writeAxisToSelection();
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
