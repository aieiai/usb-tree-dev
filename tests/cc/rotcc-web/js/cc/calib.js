'use strict';

/* ------------------------------------------------------------ 按键校准 */

let calibKeyEls = new Map();

/* 校准页键盘用于选择要观察实时曲线的按键。 */
function renderCalibKeyboard() {
  calibKeyEls = buildKeyboard($calibKeyboard, toggleCalibKey);
  for (const el of calibKeyEls.values()) {
    el.value.textContent = el.root.dataset.label;
    el.value.classList.add('set');
    el.root.title = `${el.root.dataset.label} · 索引 ${el.root.dataset.index}`;
  }
  syncCalibSelection();
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

/* ------------------------------------------------------- sensor 数据上报 */

const SENSOR_FREQ_DEFAULT = 50;
const SENSOR_FREQ_MIN = 1;
const SENSOR_FREQ_MAX = 1000;
const SENSOR_RENDER_MS = 100;
const SENSOR_HISTORY_MS = 5000;
const SENSOR_HISTORY_MAX = 2400;
const SENSOR_Y_MAX_UM = 4000;
const SENSOR_PRESSED_COLOR = '#4c9aff';
const SENSOR_RELEASED_COLOR = '#ff7ab6';
const SENSOR_TRANSITION_PRESS_COLOR = '#48bb78';
const SENSOR_TRANSITION_RELEASE_COLOR = '#e53e3e';

let sensorRptEnabled = false;
let sensorFreqHz = SENSOR_FREQ_DEFAULT;
let sensorRenderTimer = 0;
let sensorPacketCount = 0;
let sensorLastException = null;
let sensorSelectedKey = null;
let sensorHistory = [];
let sensorChartRaf = 0;
const sensorValues = new Map();

function updateSensorState(text, className = 'muted') {
  if (text === undefined) {
    text = sensorRptEnabled
      ? `上报 ${sensorFreqHz} Hz · 已接收 ${sensorPacketCount} 条`
      : 'sensor 上报未开启';
  }
  $calibSensorState.textContent = text;
  $calibSensorState.className = className;
}

function handleCcNotification(msg) {
  if (msg.cmd !== CC.CMD_NOTIFY) return;

  if (msg.subcmd === CC.NOTIFY_SENSOR) {
    /* 一帧可携带多条 sensor_data_t，按 6 字节连续排布。 */
    const count = Math.floor(msg.payload.length / CC.SENSOR_DATA_LEN);
    for (let i = 0; i < count; i++) {
      const offset = i * CC.SENSOR_DATA_LEN;
      const ki = msg.payload[offset];
      const trg = msg.payload[offset + 1] !== 0;
      const val = readLe16(msg.payload, offset + 2);
      const dist = readLe16(msg.payload, offset + 4);
      sensorValues.set(ki, { val, dist, trg });
      if (ki === sensorSelectedKey) pushSensorSample(dist, trg);
    }
    sensorPacketCount += count;
    return;
  }

  if (msg.subcmd === CC.NOTIFY_EXCEPTION && msg.payload.length >= 4) {
    const sta = readLe32(msg.payload, 0);
    if (sta === sensorLastException) return;
    sensorLastException = sta;
    appendLine(`[CC 异常] 状态 0x${sta.toString(16).padStart(8, '0')}`, 'err');
  }
}

function toggleCalibKey(index) {
  sensorSelectedKey = (sensorSelectedKey === index) ? null : index;
  sensorHistory = [];
  const current = (sensorSelectedKey === null)
    ? null
    : sensorValues.get(sensorSelectedKey);
  if (current) {
    pushSensorSample(current.dist, current.trg);
  }
  syncCalibSelection();
  updateSensorChartTitle();
  drawSensorChart();
}

function syncCalibSelection() {
  for (const [index, el] of calibKeyEls) {
    const selected = (index === sensorSelectedKey);
    el.root.classList.toggle('selected', selected);
    if (selected) {
      el.root.title = `${el.root.dataset.label} · 索引 ${index} · 已选中`;
    }
  }
}

function updateSensorChartTitle() {
  const el = (sensorSelectedKey === null)
    ? null
    : calibKeyEls.get(sensorSelectedKey);
  if (!el) {
    $sensorChartTitle.textContent = 'sensor 曲线';
    $sensorChartValue.textContent = sensorRptEnabled
      ? '点击上方键盘选择按键'
      : '选择按键查看 dist';
    return;
  }

  $sensorChartTitle.textContent =
    `sensor 曲线 · K${sensorSelectedKey} ${el.root.dataset.label}`;
  const data = sensorValues.get(sensorSelectedKey);
  $sensorChartValue.textContent = data
    ? `dist ${(data.dist / 1000).toFixed(2)} mm · ` +
      `${data.trg ? '按下' : '释放'} · val ${data.val}`
    : '等待数据...';
}

function pushSensorSample(dist, trg) {
  sensorHistory.push({ t: performance.now(), dist, trg });
}

function trimSensorHistory(now) {
  const cutoff = now - SENSOR_HISTORY_MS;
  let drop = 0;
  while ((drop < sensorHistory.length) && (sensorHistory[drop].t < cutoff)) drop++;
  if (drop > 0) sensorHistory.splice(0, drop);
  if (sensorHistory.length > SENSOR_HISTORY_MAX) {
    sensorHistory.splice(0, sensorHistory.length - SENSOR_HISTORY_MAX);
  }
}

function drawChartMessage(ctx, width, height, text) {
  ctx.fillStyle = '#8b98a5';
  ctx.font = '12px Consolas, monospace';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(text, width / 2, height / 2);
}

function sensorChartPoint(sample, padL, padT, plotW, plotH, t0) {
  return {
    x: padL + ((sample.t - t0) / SENSOR_HISTORY_MS) * plotW,
    y: padT + plotH -
      (Math.min(Math.max(sample.dist, 0), SENSOR_Y_MAX_UM) / SENSOR_Y_MAX_UM) * plotH
  };
}

/* 选中按键后绘制最近 5 秒的 dist 曲线，纵轴固定 0~4 mm。 */
function drawSensorChart() {
  const canvas = $sensorChart;
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  if (!ctx) return;

  const cssW = canvas.clientWidth;
  const cssH = canvas.clientHeight;
  if ((cssW <= 0) || (cssH <= 0)) return;

  const dpr = globalThis.devicePixelRatio || 1;
  const pxW = Math.round(cssW * dpr);
  const pxH = Math.round(cssH * dpr);
  if ((canvas.width !== pxW) || (canvas.height !== pxH)) {
    canvas.width = pxW;
    canvas.height = pxH;
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  ctx.fillStyle = '#0a0e12';
  ctx.fillRect(0, 0, cssW, cssH);
  ctx.font = '11px Consolas, monospace';

  if (sensorSelectedKey === null) {
    drawChartMessage(ctx, cssW, cssH, '点击上方键盘选择要观察的按键');
    return;
  }
  if (!sensorRptEnabled) {
    drawChartMessage(ctx, cssW, cssH,
      `K${sensorSelectedKey} 已选中，开启 sensor 上报后显示曲线`);
    return;
  }

  const now = performance.now();
  trimSensorHistory(now);
  const t0 = now - SENSOR_HISTORY_MS;
  const padL = 46;
  const padR = 12;
  const padT = 16;
  const padB = 20;
  const plotW = Math.max(1, cssW - padL - padR);
  const plotH = Math.max(1, cssH - padT - padB);

  const yMax = SENSOR_Y_MAX_UM;

  ctx.lineWidth = 1;
  ctx.textAlign = 'right';
  ctx.textBaseline = 'middle';
  for (let y = 0; y <= yMax; y += 500) {
    const py = padT + plotH - (y / yMax) * plotH;
    ctx.strokeStyle = (y === 0) ? '#3a4754' : '#1d2630';
    ctx.beginPath();
    ctx.moveTo(padL, py);
    ctx.lineTo(padL + plotW, py);
    ctx.stroke();
    ctx.fillStyle = '#8b98a5';
    ctx.fillText((y / 1000).toFixed(1), padL - 6, py);
  }

  ctx.textAlign = 'center';
  ctx.textBaseline = 'top';
  for (let s = 0; s <= 5; s++) {
    const px = padL + (s / 5) * plotW;
    ctx.strokeStyle = '#1d2630';
    ctx.beginPath();
    ctx.moveTo(px, padT);
    ctx.lineTo(px, padT + plotH);
    ctx.stroke();
    ctx.fillStyle = '#8b98a5';
    ctx.fillText(s === 5 ? '现在' : `-${5 - s}s`, px, padT + plotH + 5);
  }

  if (sensorHistory.length > 0) {
    const transitions = [];

    /*
     * 相邻采样状态不同时，状态切换发生在两点之间：在时间/行程中点把线段
     * 分成两段，前半段用旧状态颜色，后半段用新状态颜色，切换点即打点位置。
     */
    for (let i = 1; i < sensorHistory.length; i++) {
      const prev = sensorHistory[i - 1];
      const curr = sensorHistory[i];
      const a = sensorChartPoint(prev, padL, padT, plotW, plotH, t0);
      const b = sensorChartPoint(curr, padL, padT, plotW, plotH, t0);

      ctx.lineWidth = 1;
      if (prev.trg === curr.trg) {
        ctx.strokeStyle = curr.trg
          ? SENSOR_PRESSED_COLOR
          : SENSOR_RELEASED_COLOR;
        ctx.beginPath();
        ctx.moveTo(a.x, a.y);
        ctx.lineTo(b.x, b.y);
        ctx.stroke();
        continue;
      }

      const mid = sensorChartPoint({
        t: (prev.t + curr.t) / 2,
        dist: (prev.dist + curr.dist) / 2
      }, padL, padT, plotW, plotH, t0);

      ctx.strokeStyle = prev.trg
        ? SENSOR_PRESSED_COLOR
        : SENSOR_RELEASED_COLOR;
      ctx.beginPath();
      ctx.moveTo(a.x, a.y);
      ctx.lineTo(mid.x, mid.y);
      ctx.stroke();

      ctx.strokeStyle = curr.trg
        ? SENSOR_PRESSED_COLOR
        : SENSOR_RELEASED_COLOR;
      ctx.beginPath();
      ctx.moveTo(mid.x, mid.y);
      ctx.lineTo(b.x, b.y);
      ctx.stroke();

      transitions.push({ point: mid, trg: curr.trg });
    }

    /* 状态切换点在颜色变化处打小点：按下绿点，释放红点。 */
    for (const item of transitions) {
      ctx.fillStyle = item.trg
        ? SENSOR_TRANSITION_PRESS_COLOR
        : SENSOR_TRANSITION_RELEASE_COLOR;
      ctx.beginPath();
      ctx.arc(item.point.x, item.point.y, 1.8, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  ctx.font = '11px Consolas, monospace';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'top';
  ctx.fillStyle = SENSOR_PRESSED_COLOR;
  ctx.fillText('■ 按下', cssW - 170, 4);
  ctx.fillStyle = SENSOR_RELEASED_COLOR;
  ctx.fillText('■ 释放', cssW - 116, 4);
  ctx.fillStyle = SENSOR_TRANSITION_PRESS_COLOR;
  ctx.fillText('● 按下', cssW - 62, 4);
  ctx.fillStyle = SENSOR_TRANSITION_RELEASE_COLOR;
  ctx.fillText('● 释放', cssW - 8, 4);

  ctx.fillStyle = '#8b98a5';
  ctx.textAlign = 'left';
  ctx.fillText('mm', 6, 4);
}

function sensorChartLoop() {
  drawSensorChart();
  sensorChartRaf = sensorRptEnabled ? requestAnimationFrame(sensorChartLoop) : 0;
}

function startSensorChart() {
  if (!sensorChartRaf) {
    sensorChartRaf = requestAnimationFrame(sensorChartLoop);
  }
}

function stopSensorChart() {
  if (sensorChartRaf) {
    cancelAnimationFrame(sensorChartRaf);
    sensorChartRaf = 0;
  }
  drawSensorChart();
}

/* 通知频率可能很高，DOM 只在固定间隔刷新，键帽显示行程（mm）。 */
function renderSensorValues() {
  if (!sensorRptEnabled) return;

  for (const [index, el] of calibKeyEls) {
    const label = el.root.dataset.label;
    const data = sensorValues.get(index);
    if (!data) {
      el.value.textContent = label;
      el.root.classList.remove('pressed');
      continue;
    }
    el.value.textContent = (data.dist / 1000).toFixed(2);
    el.value.classList.add('set');
    el.root.classList.toggle('pressed', data.trg);
    el.root.title = `${label} · 索引 ${index} · ${data.trg ? '按下' : '释放'} · ` +
      `val=${data.val} · dist=${data.dist} um`;
  }
  updateSensorState();
  updateSensorChartTitle();
}

function startSensorMonitor() {
  sensorValues.clear();
  sensorPacketCount = 0;
  sensorLastException = null;
  sensorHistory = [];
  if (sensorRenderTimer) clearInterval(sensorRenderTimer);
  sensorRenderTimer = setInterval(renderSensorValues, SENSOR_RENDER_MS);

  const target = findCcTarget();
  if (target) ccEnsureRx(target);
  startSensorChart();
  updateSensorState();
  updateSensorChartTitle();
}

function stopSensorMonitor() {
  if (sensorRenderTimer) {
    clearInterval(sensorRenderTimer);
    sensorRenderTimer = 0;
  }
  sensorValues.clear();
  sensorHistory = [];
  renderCalibKeyboard();
  stopSensorChart();
  updateSensorState();
  updateSensorChartTitle();
}

function resetSensorMonitor() {
  sensorRptEnabled = false;
  sensorFreqHz = SENSOR_FREQ_DEFAULT;
  sensorSelectedKey = null;
  sensorHistory = [];
  $chkSensorRpt.checked = false;
  $numSensorFreq.value = String(sensorFreqHz);
  if (sensorRenderTimer) {
    clearInterval(sensorRenderTimer);
    sensorRenderTimer = 0;
  }
  sensorValues.clear();
  sensorPacketCount = 0;
  sensorLastException = null;
  renderCalibKeyboard();
  stopSensorChart();
  updateSensorState();
  updateSensorChartTitle();
}

async function writeSensorReport() {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  const enabled = $chkSensorRpt.checked;
  sysBegin();
  try {
    await sysCall(client =>
      client.statusRequest(CC.CMD_CALIB, CC.CALIB_SENSOR_RPT_SET, {
        payload: Uint8Array.of(enabled ? 1 : 0, 0),
        label: enabled ? '开启 sensor 上报' : '关闭 sensor 上报',
        timeoutMs: 5000
      })
    );
    sensorRptEnabled = enabled;
    if (enabled) {
      startSensorMonitor();
    } else {
      stopSensorMonitor();
    }
    setCalibState(`sensor 上报已${enabled ? '开启' : '关闭'}。`);
    appendLine(`[CALIB] sensor 上报${enabled ? '开启' : '关闭'}`);
  } catch (error) {
    $chkSensorRpt.checked = sensorRptEnabled;
    setCalibState(`设置 sensor 上报失败：${error.message || error}`, 'err');
  } finally {
    sysEnd();
  }
}

async function writeSensorFreq() {
  if (sysBusy || kcBusy || isFirmwareActive()) return;

  const freq = Number.parseInt($numSensorFreq.value, 10);
  if (!Number.isInteger(freq) ||
      freq < SENSOR_FREQ_MIN || freq > SENSOR_FREQ_MAX) {
    setCalibState(
      `sensor 频率需在 ${SENSOR_FREQ_MIN}~${SENSOR_FREQ_MAX} Hz 之间。`,
      'err'
    );
    return;
  }

  sysBegin();
  try {
    const payload = new Uint8Array(4);
    writeLe16(payload, 0, freq);
    writeLe16(payload, 2, 0);
    await sysCall(client =>
      client.statusRequest(CC.CMD_CALIB, CC.CALIB_SENSOR_FREQ_SET, {
        payload,
        label: '设置 sensor 上报频率',
        timeoutMs: 5000
      })
    );
    sensorFreqHz = freq;
    updateSensorState();
    setCalibState(`sensor 上报频率已设为 ${freq} Hz。`);
    appendLine(`[CALIB] sensor 上报频率 ${freq} Hz`);
  } catch (error) {
    setCalibState(`设置 sensor 频率失败：${error.message || error}`, 'err');
  } finally {
    sysEnd();
  }
}
