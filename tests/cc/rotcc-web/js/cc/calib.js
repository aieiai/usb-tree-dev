'use strict';

/* ------------------------------------------------------------ 按键校准 */

let calibKeyEls = new Map();

/* 校准页的键盘只展示物理布局，按键不参与选择。 */
function renderCalibKeyboard() {
  calibKeyEls = buildKeyboard($calibKeyboard, null);
  for (const el of calibKeyEls.values()) {
    el.value.textContent = el.root.dataset.label;
    el.value.classList.add('set');
    el.root.title = `${el.root.dataset.label} · 索引 ${el.root.dataset.index}`;
  }
  syncKeycapSize();
}

const CALIB_ACTION_LABELS = {
  [CC.CALIB_STA_START]: '开始校准',
  [CC.CALIB_STA_DONE]: '完成校准',
  [CC.CALIB_STA_STOP]: '停止校准'
};

function calibStatusLabel(sta) {
  switch (sta) {
    case CC.CALIB_STA_START:
      return '校准中';
    case CC.CALIB_STA_DONE:
      return '校准完成';
    case CC.CALIB_STA_STOP:
      return '已停止';
    default:
      return `未知状态 0x${Number(sta).toString(16)}`;
  }
}

function setCalibState(text, className = '') {
  $calibState.textContent = text;
  $calibState.className = className ? `hint ${className}` : 'hint';
}

async function readCalibStatus(client) {
  const payload = await client.readPayload(CC.CMD_CALIB, CC.CALIB_STATUS_GET, 2, {
    label: '校准状态',
    timeoutMs: 3000
  });
  return payload[0];
}

async function queryCalibStatus({ silent = false } = {}) {
  sysBegin();
  try {
    const sta = await sysCall(client => readCalibStatus(client));
    ccCalibSta = sta;
    $calibStatus.textContent = `校准 ${calibStatusLabel(sta)}`;
    setCalibState(`校准状态：${calibStatusLabel(sta)}。`);
    appendLine(`[CALIB] 状态：${calibStatusLabel(sta)}（0x${sta.toString(16)}）`);
    return sta;
  } catch (error) {
    if (!silent) setCalibState(`读取校准状态失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function writeCalibStatus(sta) {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  const label = CALIB_ACTION_LABELS[sta] || '校准操作';
  let current = sta;

  sysBegin();
  try {
    await sysCall(async client => {
      await client.statusRequest(CC.CMD_CALIB, CC.CALIB_STATUS_SET, {
        payload: Uint8Array.of(sta, 0),
        label,
        timeoutMs: 5000
      });
      current = await readCalibStatus(client);
    });

    ccCalibSta = current;
    $calibStatus.textContent = `校准 ${calibStatusLabel(current)}`;
    if (sta === CC.CALIB_STA_START) {
      setCalibState('校准已开始，请把每个按键按压/松开到底，然后点击“完成校准”。');
    } else if (sta === CC.CALIB_STA_DONE) {
      setCalibState(`校准已完成：${calibStatusLabel(current)}。`);
    } else {
      setCalibState('已停止校准，本次采样未保存。');
    }
    appendLine(`[CALIB] ${label}，状态：${calibStatusLabel(current)}`);
    return current;
  } catch (error) {
    setCalibState(`${label}失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function queryAutoCalib() {
  sysBegin();
  try {
    const payload = await sysCall(client =>
      client.readPayload(CC.CMD_CALIB, CC.CALIB_AUTO_GET, 2, {
        label: '自校准状态',
        timeoutMs: 3000
      })
    );
    const enabled = payload[0] !== 0;
    $chkAutoCalib.checked = enabled;
    setCalibState(`自校准当前${enabled ? '开启' : '关闭'}。`);
    appendLine(`[CALIB] 自校准：${enabled ? '开启' : '关闭'}`);
    return enabled;
  } catch (error) {
    setCalibState(`读取自校准失败：${error.message || error}`, 'err');
    throw error;
  } finally {
    sysEnd();
  }
}

async function writeAutoCalib() {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  const enabled = $chkAutoCalib.checked;
  sysBegin();
  try {
    await sysCall(client =>
      client.statusRequest(CC.CMD_CALIB, CC.CALIB_AUTO_SET, {
        payload: Uint8Array.of(enabled ? 1 : 0, 0),
        label: '设置自校准',
        timeoutMs: 5000
      })
    );
    setCalibState(`自校准已${enabled ? '开启' : '关闭'}。`);
    appendLine(`[CALIB] 自校准设置为${enabled ? '开启' : '关闭'}`);
  } catch (error) {
    setCalibState(`设置自校准失败：${error.message || error}`, 'err');
  } finally {
    sysEnd();
  }
}
