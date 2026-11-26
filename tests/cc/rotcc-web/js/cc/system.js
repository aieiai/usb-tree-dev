'use strict';

/* ------------------------------------------------------------ 系统命令 */

const SYS_RESET_KC = 0x1;
const SYS_RESET_RGB = 0x2;
const SYS_RESET_ADV = 0x4;
const SYS_RESET_ALL = 0xffffffff;

function setSysState(text, className = '') {
  $sysState.textContent = text;
  $sysState.className = className ? `hint ${className}` : 'hint';
}

function sysBegin() {
  sysBusy = true;
  refreshControls();
}

function sysEnd() {
  sysBusy = false;
  refreshControls();
}

function sysCall(handler) {
  const target = findCcTarget();
  if (!target) {
    throw new Error('没有找到 CC IN/OUT 端点');
  }
  return ccCall(target, handler);
}

async function queryHandshake({ silent = false } = {}) {
  sysBegin();
  try {
    const payload = await sysCall(client =>
      client.readPayload(CC.CMD_SYS, CC.SYS_HANDSHAKE, 4, {
        label: '握手',
        timeoutMs: 3000
      })
    );
    const version = readLe32(payload, 0);
    $sysProtocol.textContent = `协议版本 v${version}`;
    setSysState(`握手成功，协议版本 v${version}。`);
    appendLine(`[SYS] 握手成功，Rot-CC 协议版本 v${version}`);
    return version;
  } catch (error) {
    if (!silent) setSysState(`握手失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function queryKeyCount({ silent = false } = {}) {
  sysBegin();
  try {
    const payload = await sysCall(client =>
      client.readPayload(CC.CMD_SYS, CC.SYS_KEY_COUNT, 4, {
        label: '按键数量',
        timeoutMs: 3000
      })
    );
    const count = readLe32(payload, 0);
    applyKeyboardLayout(count);
    const uiCount = kcKeyEls.size;
    $sysKeyCount.textContent = `按键数量 ${count}`;
    appendLine(`[SYS] 设备按键数量 ${count}`);
    if (uiCount > 0 && count !== uiCount) {
      appendLine(
        `[SYS] 按键数量与界面不一致（设备 ${count} / 界面 ${uiCount}）`,
        'warn'
      );
    }
    return count;
  } catch (error) {
    if (!silent) setSysState(`读取按键数量失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function queryPopupSupport() {
  sysBegin();
  try {
    const payload = await sysCall(client =>
      client.readPayload(CC.CMD_SYS, CC.SYS_POPUP_SUPPORT, 4, {
        label: '驱动弹窗支持',
        timeoutMs: 3000
      })
    );
    const supported = readLe32(payload, 0) !== 0;
    ccPopupSupported = supported;
    $sysPopup.textContent = supported ? '驱动弹窗 支持' : '驱动弹窗 不支持';
    setSysState(supported ? '设备支持驱动弹窗。' : '设备不支持驱动弹窗。',
      supported ? '' : 'warn');
    appendLine(`[SYS] 驱动弹窗支持：${supported ? '是' : '否'}`,
      supported ? '' : 'warn');
    return supported;
  } catch (error) {
    ccPopupSupported = null;
    $sysPopup.textContent = '驱动弹窗 --';
    setSysState(`查询驱动弹窗支持失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function queryPopupState() {
  sysBegin();
  try {
    const payload = await sysCall(client =>
      client.readPayload(CC.CMD_SYS, CC.SYS_POPUP_GET, 4, {
        label: '驱动弹窗状态',
        timeoutMs: 3000
      })
    );
    const enabled = readLe32(payload, 0) !== 0;
    $chkPopup.checked = enabled;
    $sysPopup.textContent = `驱动弹窗 ${enabled ? '开启' : '关闭'}`;
    setSysState(`驱动弹窗当前${enabled ? '开启' : '关闭'}。`);
    appendLine(`[SYS] 驱动弹窗状态：${enabled ? '开启' : '关闭'}`);
    return enabled;
  } catch (error) {
    setSysState(`读取驱动弹窗失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function writePopupState() {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  if (ccPopupSupported !== true) {
    setSysState('请先检查设备是否支持驱动弹窗。', 'warn');
    return;
  }

  const enabled = $chkPopup.checked;
  const payload = new Uint8Array(4);
  writeLe32(payload, 0, enabled ? 1 : 0);

  sysBegin();
  try {
    await sysCall(client =>
      client.statusRequest(CC.CMD_SYS, CC.SYS_POPUP_SET, {
        payload,
        label: '设置驱动弹窗',
        timeoutMs: 5000
      })
    );
    $sysPopup.textContent = `驱动弹窗 ${enabled ? '开启' : '关闭'}`;
    setSysState(
      `驱动弹窗已${enabled ? '开启' : '关闭'}，重新枚举后生效。`
    );
    appendLine(`[SYS] 驱动弹窗设置为${enabled ? '开启' : '关闭'}`);
  } catch (error) {
    setSysState(`设置驱动弹窗失败：${error.message || error}`, 'err');
  } finally {
    sysEnd();
  }
}

async function rebootDevice() {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  sysBegin();
  try {
    await sysCall(client =>
      client.statusRequest(CC.CMD_SYS, CC.SYS_REBOOT, {
        label: '重启设备',
        timeoutMs: 3000
      })
    );
    setSysState('设备已接受重启命令，正在复位...');
    appendLine('[SYS] 重启命令已发送');
  } catch (error) {
    setSysState(`重启失败：${error.message || error}`, 'err');
  } finally {
    sysEnd();
  }
}

async function resetDevice(options, label) {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  const payload = new Uint8Array(4);
  writeLe32(payload, 0, options >>> 0);

  let ok = false;
  sysBegin();
  try {
    await sysCall(client =>
      client.statusRequest(CC.CMD_SYS, CC.SYS_RESET, {
        payload,
        label,
        timeoutMs: 5000
      })
    );
    ok = true;
    setSysState(`${label}完成。`);
    appendLine(`[SYS] ${label}完成（options=0x${(options >>> 0).toString(16)}）`);
  } catch (error) {
    setSysState(`${label}失败：${error.message || error}`, 'err');
  } finally {
    sysEnd();
  }

  if (ok && (options & SYS_RESET_KC) !== 0) {
    kcValues.clear();
    kcSelected.clear();
    syncKeycaps();
    await queryKeymapInfo();
    autoReadKeymap();
  }
}
