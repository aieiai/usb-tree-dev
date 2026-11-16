'use strict';

function refreshControls() {
  const active = isFirmwareActive() || kcBusy || sysBusy || ccQueryPromise !== null;

  for (const link of links) {
    if (link.claimButton) {
      link.claimButton.textContent = link.claimed ? '释放接口' : '占用接口';
      link.claimButton.disabled = !device || !device.opened || isFirmwareActive();
      link.claimState.textContent = link.claimed ? '已占用' : '未占用';
      link.claimState.className = `claim ${link.claimed ? 'ok' : ''}`;
    }
    for (const ep of link.alt.endpoints) {
      const ui = link.ui.get(endpointKey(ep));
      if (!ui) continue;
      if (ep.direction === 'in') {
        ui.start.disabled = !link.claimed || link.reads.has(endpointKey(ep));
        ui.start.textContent = link.reads.has(endpointKey(ep)) ? '监听中' : '开始监听';
        ui.state.textContent = link.reads.has(endpointKey(ep)) ? '监听中' : '未监听';
      } else if (ui.send) {
        ui.send.disabled = !link.claimed || isFirmwareActive();
        ui.input.disabled = isFirmwareActive();
      }
    }
  }
  refreshFirmwareControls();
  refreshDebugControls();
  refreshSystemControls();
}

function refreshFirmwareControls() {
  const active = isFirmwareActive();
  $firmwareFile.disabled = active;
  $chkAll.disabled = active;
  $btnFirmwareStart.disabled =
    active || kcBusy || sysBusy || ccQueryPromise !== null ||
    !device || !device.opened || !$firmwareFile.files[0];
  $btnFirmwareCancel.disabled = !firmwareSession;
}

function refreshDebugControls() {
  const busy = isFirmwareActive() || kcBusy || sysBusy || ccQueryPromise !== null;
  const ready = Boolean(device?.opened) && links.length > 0;
  $btnCcQuery.disabled = busy || !ready;
  for (const btn of $kcLayers.children) btn.disabled = busy;
}

function refreshSystemControls() {
  const busy = isFirmwareActive() || kcBusy || sysBusy || ccQueryPromise !== null;
  const ready = Boolean(device?.opened) && links.length > 0;
  for (const btn of [$btnHandshake, $btnKeyCount, $btnSysReboot,
                     $btnSysReset, $btnSysResetAll]) {
    btn.disabled = busy || !ready;
  }
  for (const chk of [$chkResetKc, $chkResetRgb, $chkResetAdv]) {
    chk.disabled = busy || !ready;
  }
}

