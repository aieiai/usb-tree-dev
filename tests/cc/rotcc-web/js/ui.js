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
  refreshParamControls();
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
  const popupSupported = ccPopupSupported === true;
  for (const btn of [$btnHandshake, $btnKeyCount, $btnSysReboot,
                     $btnSysReset, $btnSysResetAll,
                     $btnPopupSupport, $btnPopupRead]) {
    btn.disabled = busy || !ready;
  }
  for (const chk of [$chkResetKc, $chkResetRgb, $chkResetAdv]) {
    chk.disabled = busy || !ready;
  }
  $btnPopupWrite.disabled = busy || !ready || !popupSupported;
  $chkPopup.disabled = busy || !ready || !popupSupported;
}

function refreshParamControls() {
  const busy = isFirmwareActive() || kcBusy || sysBusy || ccQueryPromise !== null;
  const ready = Boolean(device?.opened) && links.length > 0;
  const tick = paramTab.id === 'tick';
  const hasParamSelection = paramSelected.size > 0;
  const hasAxisSelection = axisSelected.size > 0;

  $btnParamSelectAll.disabled =
    busy || paramSelected.size >= paramKeyEls.size;
  $btnParamClear.disabled = busy || !hasParamSelection;
  $btnParamRead.disabled =
    busy || !ready || (!tick && !hasParamSelection);
  $btnParamWrite.disabled =
    busy || !ready || !paramLoaded || (!tick && !hasParamSelection);
  $btnAxisSelectAll.disabled =
    busy || axisSelected.size >= axisKeyEls.size;
  $btnAxisClear.disabled = busy || !hasAxisSelection;
  $btnAxisList.disabled = busy || !ready;
  $btnAxisWrite.disabled =
    busy || !ready || !hasAxisSelection || axisSupport.length === 0;
  $axisValue.disabled = busy || !ready || axisSupport.length === 0;

  for (const btn of $paramTabs.children) {
    btn.disabled = busy;
  }
  for (const btn of $axisTabs.children) {
    btn.disabled = busy;
  }
}

