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
const SENSOR_FREQ_MAX = 8000;
const SENSOR_RENDER_MS = 100;
const SENSOR_HISTORY_MS = 3000;
/* 8kHz 上报下保留完整 3s 窗口需要 24000 个采样点。 */
const SENSOR_HISTORY_MAX = 24000;
const SENSOR_Y_MAX_UM = 4000;
const SENSOR_PRESSED_COLOR = '#4c9aff';
const SENSOR_RELEASED_COLOR = '#ff7ab6';
const SENSOR_TRANSITION_PRESS_COLOR = '#48bb78';
const SENSOR_TRANSITION_RELEASE_COLOR = '#e53e3e';
const SENSOR_ZOOM_MIN_MS = 10;
const SENSOR_PAD_L = 46;
const SENSOR_PAD_R = 12;
const SENSOR_PAD_T = 16;
const SENSOR_PAD_B = 20;

let sensorRptEnabled = false;
let sensorFreqHz = SENSOR_FREQ_DEFAULT;
let sensorRenderTimer = 0;
let sensorPacketCount = 0;
let sensorNotifyCount = 0;
let sensorRateCount = 0;
let sensorRateStamp = 0;
let sensorRateHz = 0;
let sensorLastException = null;
let sensorSelectedKey = null;
let sensorHistory = [];
let sensorChartRaf = 0;
let sensorPaused = false;
let sensorPausedAtMs = 0;
let sensorViewSpanMs = SENSOR_HISTORY_MS;
let sensorViewEndMs = 0;
let sensorHover = null;
let sensorChartEdges = [];
let sensorDrag = null;
let sensorViewCache = null;
const sensorValues = new Map();

function updateSensorState(text, className = 'muted') {
  if (text === undefined) {
    text = sensorRptEnabled
      ? `上报 ${sensorFreqHz} Hz · 实测 ${Math.round(sensorRateHz)} Hz · 已接收 ${sensorPacketCount} 条`
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
      if (ki === sensorSelectedKey) pushSensorSample(dist, trg, val);
    }
    sensorPacketCount += count;
    sensorNotifyCount++;
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
  sensorHover = null;
  if (sensorSelectedKey !== null) {
    sensorPaused = false;
    sensorPausedAtMs = 0;
    sensorViewSpanMs = SENSOR_HISTORY_MS;
    sensorViewEndMs = performance.now();
  }
  const current = (sensorSelectedKey === null)
    ? null
    : sensorValues.get(sensorSelectedKey);
  if (current) {
    pushSensorSample(current.dist, current.trg, current.val);
  }
  syncCalibSelection();
  updateSensorChartTitle();
  updateSensorZoomState();
  startSensorChart();
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

function pushSensorSample(dist, trg, val) {
  if (sensorPaused) return;
  sensorHistory.push({ t: performance.now(), dist, trg, val });
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

function formatSensorSpan(ms) {
  return ms >= 1000 ? `${(ms / 1000).toFixed(2)} s` : `${Math.round(ms)} ms`;
}

function formatSensorOffset(ms) {
  const abs = Math.abs(ms);
  if (abs >= 1000) return `${(ms / 1000).toFixed(2)} s`;
  if (abs >= 10) return `${Math.round(ms)} ms`;
  return `${ms.toFixed(2)} ms`;
}

/* 点位提示需要看到 0.125ms 级的采样间隔。 */
function formatSensorPointOffset(ms) {
  return Math.abs(ms) >= 1000
    ? `${(ms / 1000).toFixed(3)} s`
    : `${ms.toFixed(3)} ms`;
}

function updateSensorZoomState() {
  const ready = sensorRptEnabled && (sensorSelectedKey !== null);
  if ($sensorZoomState) {
    $sensorZoomState.textContent =
      `${sensorPaused ? '已暂停' : '实时'} · 窗口 ${formatSensorSpan(sensorViewSpanMs)}` +
      ' · 滚轮缩放 / 拖动平移';
  }
  $btnSensorPause.textContent = sensorPaused ? '继续' : '暂停';
  $btnSensorPause.disabled = !ready;
  $btnSensorZoomIn.disabled = !ready || (sensorViewSpanMs <= SENSOR_ZOOM_MIN_MS);
  $btnSensorZoomOut.disabled = !ready || (sensorViewSpanMs >= SENSOR_HISTORY_MS);
  $btnSensorReset.disabled = !ready;
}

function resetSensorViewState() {
  sensorPaused = false;
  sensorPausedAtMs = 0;
  sensorViewSpanMs = SENSOR_HISTORY_MS;
  sensorViewEndMs = performance.now();
  sensorHover = null;
  sensorDrag = null;
  sensorViewCache = null;
  updateSensorZoomState();
}

function setSensorPaused(paused) {
  if (paused === sensorPaused) return;
  sensorPaused = paused;
  sensorHover = null;
  if (paused) {
    sensorPausedAtMs = sensorHistory.length
      ? sensorHistory[sensorHistory.length - 1].t
      : performance.now();
    sensorViewEndMs = sensorPausedAtMs;
  } else {
    sensorPausedAtMs = 0;
    sensorHistory = [];
    sensorViewEndMs = performance.now();
    startSensorChart();
  }
  updateSensorZoomState();
  drawSensorChart();
}

function zoomSensorView(factor, anchorX) {
  if (!sensorRptEnabled || (sensorSelectedKey === null)) return;
  const next = Math.min(Math.max(sensorViewSpanMs * factor, SENSOR_ZOOM_MIN_MS),
                        SENSOR_HISTORY_MS);
  if (next === sensorViewSpanMs) return;
  if (!sensorPaused) setSensorPaused(true);

  let ratio = 1;
  if ((anchorX !== undefined) && sensorViewCache) {
    ratio = Math.min(Math.max((anchorX - SENSOR_PAD_L) / sensorViewCache.plotW, 0), 1);
  }
  const anchorT = sensorViewEndMs - sensorViewSpanMs + ratio * sensorViewSpanMs;
  sensorViewSpanMs = next;
  sensorViewEndMs = anchorT + (1 - ratio) * sensorViewSpanMs;
  sensorHover = null;
  updateSensorZoomState();
  drawSensorChart();
}

function resetSensorView() {
  if (!sensorRptEnabled || (sensorSelectedKey === null)) return;
  resetSensorViewState();
  startSensorChart();
  drawSensorChart();
}

function sensorChartLayout() {
  const cssW = $sensorChart.clientWidth;
  const cssH = $sensorChart.clientHeight;
  return {
    padL: SENSOR_PAD_L,
    padT: SENSOR_PAD_T,
    plotW: Math.max(1, cssW - SENSOR_PAD_L - SENSOR_PAD_R),
    plotH: Math.max(1, cssH - SENSOR_PAD_T - SENSOR_PAD_B)
  };
}

function findSensorHover(x, y) {
  const cache = sensorViewCache;
  if (!cache || (sensorHistory.length === 0) || !sensorPaused) return null;
  if ((x < cache.padL) || (x > cache.padL + cache.plotW) ||
      (y < cache.padT) || (y > cache.padT + cache.plotH)) {
    return null;
  }

  /* 优先吸附按下/释放切换点，保证绿点/红点也能定位查看数据。 */
  let edgeBest = null;
  let edgeDist = 49;
  for (const edge of sensorChartEdges) {
    const dx = x - edge.x;
    const dy = y - edge.y;
    const d = (dx * dx) + (dy * dy);
    if (d <= edgeDist) {
      edgeDist = d;
      edgeBest = edge;
    }
  }
  if (edgeBest) {
    return {
      edge: true,
      index: edgeBest.index,
      sample: {
        t: edgeBest.t,
        dist: edgeBest.dist,
        val: edgeBest.val,
        trg: edgeBest.trg
      },
      x: edgeBest.x,
      y: edgeBest.y
    };
  }

  const t = cache.viewStart + ((x - cache.padL) / cache.plotW) * cache.span;
  let lo = 0;
  let hi = sensorHistory.length - 1;
  while (lo < hi) {
    const mid = (lo + hi) >> 1;
    if (sensorHistory[mid].t < t) lo = mid + 1;
    else hi = mid;
  }
  let best = lo;
  if ((lo > 0) &&
      (Math.abs(sensorHistory[lo - 1].t - t) < Math.abs(sensorHistory[lo].t - t))) {
    best = lo - 1;
  }
  const sample = sensorHistory[best];
  const px = cache.padL + ((sample.t - cache.viewStart) / cache.span) * cache.plotW;
  return {
    edge: false,
    index: best,
    sample,
    x: px,
    y: cache.padT + cache.plotH -
      (Math.min(Math.max(sample.dist, 0), SENSOR_Y_MAX_UM) / SENSOR_Y_MAX_UM) * cache.plotH
  };
}

function updateSensorHover(x, y) {
  const hover = findSensorHover(x, y);
  const same = ((hover === null) && (sensorHover === null)) ||
    (hover && sensorHover &&
     (hover.edge === sensorHover.edge) &&
     (hover.index === sensorHover.index));
  if (same) return;
  sensorHover = hover;
  drawSensorChart();
}

function sensorCanvasPoint(ev) {
  const rect = $sensorChart.getBoundingClientRect();
  return { x: ev.clientX - rect.left, y: ev.clientY - rect.top };
}

function onSensorChartWheel(ev) {
  if (!sensorRptEnabled || (sensorSelectedKey === null)) return;
  ev.preventDefault();
  const { x } = sensorCanvasPoint(ev);
  zoomSensorView(ev.deltaY > 0 ? 1.25 : 0.8, x);
}

function onSensorChartPointerDown(ev) {
  if (!sensorRptEnabled || (sensorSelectedKey === null)) return;
  if (!sensorPaused) setSensorPaused(true);
  sensorDrag = { pointerId: ev.pointerId, x: sensorCanvasPoint(ev).x };
  $sensorChart.setPointerCapture(ev.pointerId);
  $sensorChart.style.cursor = 'grabbing';
}

function onSensorChartPointerMove(ev) {
  const { x, y } = sensorCanvasPoint(ev);
  if (sensorDrag && (ev.pointerId === sensorDrag.pointerId)) {
    const cache = sensorViewCache;
    if (cache) {
      const dt = ((x - sensorDrag.x) / cache.plotW) * sensorViewSpanMs;
      sensorViewEndMs -= dt;
      sensorDrag.x = x;
      sensorHover = null;
      drawSensorChart();
    }
    return;
  }
  updateSensorHover(x, y);
}

function onSensorChartPointerUp(ev) {
  if (!sensorDrag || (ev.pointerId !== sensorDrag.pointerId)) return;
  sensorDrag = null;
  try {
    $sensorChart.releasePointerCapture(ev.pointerId);
  } catch (error) {
    /* 指针已释放时忽略 */
  }
  $sensorChart.style.cursor = '';
}

function onSensorChartPointerLeave() {
  if (sensorDrag || !sensorHover) return;
  sensorHover = null;
  drawSensorChart();
}

function drawSensorHover(ctx, cache, cssW, cssH) {
  const hover = sensorHover;
  const sample = hover.sample;

  ctx.strokeStyle = '#5a6b7b';
  ctx.lineWidth = 1;
  ctx.setLineDash([3, 3]);
  ctx.beginPath();
  ctx.moveTo(hover.x, cache.padT);
  ctx.lineTo(hover.x, cache.padT + cache.plotH);
  ctx.stroke();
  ctx.setLineDash([]);

  ctx.fillStyle = sample.trg ? SENSOR_PRESSED_COLOR : SENSOR_RELEASED_COLOR;
  ctx.beginPath();
  ctx.arc(hover.x, hover.y, 3.2, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = '#e6fffb';
  ctx.stroke();

  const lines = [
    `#${hover.index + 1}${hover.edge ? ' 切换点' : ''}  ${formatSensorPointOffset(sample.t - sensorViewEndMs)}`,
    `dist ${(sample.dist / 1000).toFixed(3)} mm`,
    `val ${sample.val}`,
    sample.trg ? '按下' : '释放'
  ];
  if (hover.edge) {
    lines[3] = sample.trg ? '按下事件' : '释放事件';
  }
  ctx.font = '11px Consolas, monospace';
  let textW = 0;
  for (const line of lines) textW = Math.max(textW, ctx.measureText(line).width);
  const boxW = Math.ceil(textW) + 12;
  const boxH = lines.length * 14 + 8;
  let bx = hover.x + 8;
  if (bx + boxW > cssW - 4) bx = hover.x - boxW - 8;
  let by = hover.y - boxH - 8;
  if (by < 4) by = hover.y + 8;
  by = Math.min(Math.max(by, 4), Math.max(4, cssH - boxH - 4));
  bx = Math.min(Math.max(bx, 4), Math.max(4, cssW - boxW - 4));

  ctx.fillStyle = 'rgba(10, 16, 22, 0.94)';
  ctx.fillRect(bx, by, boxW, boxH);
  ctx.strokeStyle = '#2c3a47';
  ctx.strokeRect(bx + 0.5, by + 0.5, boxW - 1, boxH - 1);
  ctx.textAlign = 'left';
  ctx.textBaseline = 'top';
  for (let i = 0; i < lines.length; i++) {
    ctx.fillStyle = i === 0 ? '#e6fffb' : '#a7b4c2';
    ctx.fillText(lines[i], bx + 6, by + 4 + i * 14);
  }
}

/* 选中按键后绘制可见窗口内的 dist 曲线；暂停后支持滚轮缩放、拖动平移、悬停查看点位。 */
function drawSensorChart() {
  const canvas = $sensorChart;
  if (!canvas) return;
  sensorChartEdges = [];
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

  const now = sensorPaused ? (sensorPausedAtMs || performance.now()) : performance.now();
  if (!sensorPaused) {
    /* 窗口按墙钟匀速滚动：如果改成跟随最新采样时间，USB 到达抖动会让整条曲线
     * 一顿一顿地跳，看起来同样像闪频。 */
    trimSensorHistory(now);
    sensorViewEndMs = now;
  } else if (sensorHistory.length > 0) {
    const firstT = sensorHistory[0].t;
    const lastT = sensorHistory[sensorHistory.length - 1].t;
    const minEnd = firstT + sensorViewSpanMs;
    const maxEnd = Math.max(sensorPausedAtMs || now, lastT);
    sensorViewEndMs = Math.min(Math.max(sensorViewEndMs, minEnd), maxEnd);
  }

  const layout = sensorChartLayout();
  const padL = layout.padL;
  const padT = layout.padT;
  const plotW = layout.plotW;
  const plotH = layout.plotH;
  const span = sensorViewSpanMs;
  const viewStart = sensorViewEndMs - span;
  sensorViewCache = { padL, padT, plotW, plotH, span, viewStart };

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
  const tickCount = 4;
  for (let i = 0; i <= tickCount; i++) {
    const ratio = i / tickCount;
    const px = padL + ratio * plotW;
    const t = viewStart + ratio * span;
    ctx.strokeStyle = '#1d2630';
    ctx.beginPath();
    ctx.moveTo(px, padT);
    ctx.lineTo(px, padT + plotH);
    ctx.stroke();
    ctx.fillStyle = '#8b98a5';
    const label = (i === tickCount)
      ? (sensorPaused ? '暂停' : '现在')
      : formatSensorOffset(t - sensorViewEndMs);
    ctx.fillText(label, px, padT + plotH + 5);
  }

  if (sensorHistory.length > 0) {
    const xScale = plotW / span;
    const yScale = plotH / SENSOR_Y_MAX_UM;
    const clampDist = (d) => Math.min(Math.max(d, 0), SENSOR_Y_MAX_UM);

    /* 二分定位可见窗口 [startIdx, endIdx]，缩放后只处理可见采样点。 */
    let lo = 0;
    let hi = sensorHistory.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (sensorHistory[mid].t < viewStart) lo = mid + 1;
      else hi = mid;
    }
    const startIdx = Math.max(0, lo - 1);

    let loEnd = lo;
    let hiEnd = sensorHistory.length;
    while (loEnd < hiEnd) {
      const mid = (loEnd + hiEnd) >> 1;
      if (sensorHistory[mid].t <= sensorViewEndMs) loEnd = mid + 1;
      else hiEnd = mid;
    }
    const endIdx = Math.min(sensorHistory.length - 1, Math.max(startIdx, loEnd - 1));
    const visibleCount = endIdx - startIdx + 1;

    ctx.lineWidth = 1;

    if (visibleCount > plotW * 2) {
      /*
       * 高密度：按绝对时间桶取每桶最后一个采样。桶边界只由时间决定，与窗口
       * 滚动位置无关，代表点不会在相邻采样间换人，因此滚动时曲线不会闪；
       * 绘制量也从每像素几十个点降到约 2 个点。
       */
      const bucketMs = span / (plotW * 2);
      const transitions = [];
      let bucket = -1;
      let repT = 0;
      let repD = 0;
      let repTrg = false;
      let haveRep = false;
      let prevT = sensorHistory[startIdx].t;
      let prevD = sensorHistory[startIdx].dist;
      let prevTrg = sensorHistory[startIdx].trg;
      let px = 0;
      let py = 0;
      let pTrg = false;
      let pathOpen = false;
      let edgeT = 0;
      let edgeD = 0;
      let edgeTrg = false;
      let edgeVal = 0;
      let edgeIdx = 0;
      let hasEdge = false;

      const flushRep = () => {
        if (!haveRep) {
          return;
        }
        const x = padL + (repT - viewStart) * xScale;
        const y = padT + plotH - clampDist(repD) * yScale;
        if (!pathOpen) {
          ctx.strokeStyle = repTrg ? SENSOR_PRESSED_COLOR : SENSOR_RELEASED_COLOR;
          ctx.beginPath();
          ctx.moveTo(x, y);
          pathOpen = true;
        } else {
          if (repTrg !== pTrg) {
            /* 切换点本身就是代表点：先用旧颜色画到它，再从这里开始用新颜色，
             * 避免切换点之前的线段提前变色。 */
            ctx.lineTo(x, y);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(x, y);
            ctx.strokeStyle = repTrg ? SENSOR_PRESSED_COLOR : SENSOR_RELEASED_COLOR;
          } else {
            ctx.lineTo(x, y);
          }
        }
        px = x;
        py = y;
        pTrg = repTrg;
        haveRep = false;
      };

      for (let i = startIdx; i <= endIdx; i++) {
        const sample = sensorHistory[i];
        const b = Math.floor(sample.t / bucketMs);
        if (b !== bucket) {
          flushRep();
          bucket = b;
          hasEdge = false;
        }

        if (sample.trg !== prevTrg) {
          edgeT = (prevT + sample.t) / 2;
          edgeD = (prevD + sample.dist) / 2;
          edgeTrg = sample.trg;
          edgeVal = sample.val;
          edgeIdx = i;
          hasEdge = true;
          transitions.push({
            x: padL + (edgeT - viewStart) * xScale,
            y: padT + plotH - clampDist(edgeD) * yScale,
            t: edgeT,
            dist: edgeD,
            val: edgeVal,
            trg: edgeTrg,
            index: edgeIdx
          });
        }
        prevT = sample.t;
        prevD = sample.dist;
        prevTrg = sample.trg;

        if (hasEdge) {
          repT = edgeT;
          repD = edgeD;
          repTrg = edgeTrg;
        } else {
          repT = sample.t;
          repD = sample.dist;
          repTrg = sample.trg;
        }
        haveRep = true;
      }
      flushRep();
      if (pathOpen) {
        ctx.stroke();
      }

      for (const dot of transitions) {
        ctx.fillStyle = dot.trg
          ? SENSOR_TRANSITION_PRESS_COLOR
          : SENSOR_TRANSITION_RELEASE_COLOR;
        ctx.beginPath();
        ctx.arc(dot.x, dot.y, 1.8, 0, Math.PI * 2);
        ctx.fill();
      }
      sensorChartEdges = transitions;
    } else {
      /* 低密度：每个采样点都画，状态切换在中点分段并打点。 */
      const edges = [];
      let px = padL + (sensorHistory[startIdx].t - viewStart) * xScale;
      let py = padT + plotH - clampDist(sensorHistory[startIdx].dist) * yScale;
      let pTrg = sensorHistory[startIdx].trg;

      ctx.strokeStyle = pTrg ? SENSOR_PRESSED_COLOR : SENSOR_RELEASED_COLOR;
      ctx.beginPath();
      ctx.moveTo(px, py);

      for (let i = startIdx + 1; i <= endIdx; i++) {
        const sample = sensorHistory[i];
        const x = padL + (sample.t - viewStart) * xScale;
        const y = padT + plotH - clampDist(sample.dist) * yScale;

        if (sample.trg === pTrg) {
          ctx.lineTo(x, y);
        } else {
          const prevSample = sensorHistory[i - 1];
          const mx = (px + x) / 2;
          const my = (py + y) / 2;

          edges.push({
            x: mx,
            y: my,
            t: (prevSample.t + sample.t) / 2,
            dist: (prevSample.dist + sample.dist) / 2,
            val: sample.val,
            trg: sample.trg,
            index: i
          });

          ctx.lineTo(mx, my);
          ctx.stroke();

          ctx.fillStyle = sample.trg
            ? SENSOR_TRANSITION_PRESS_COLOR
            : SENSOR_TRANSITION_RELEASE_COLOR;
          ctx.beginPath();
          ctx.arc(mx, my, 1.8, 0, Math.PI * 2);
          ctx.fill();

          ctx.strokeStyle = sample.trg ? SENSOR_PRESSED_COLOR : SENSOR_RELEASED_COLOR;
          ctx.beginPath();
          ctx.moveTo(mx, my);
          ctx.lineTo(x, y);
          pTrg = sample.trg;
        }
        px = x;
        py = y;
      }
      ctx.stroke();

      /* 放大到采样点分得开时，补画每个采样点。 */
      let lastDotX = -Infinity;
      for (let i = startIdx; i <= endIdx; i++) {
        const sample = sensorHistory[i];
        const x = padL + (sample.t - viewStart) * xScale;
        if (x - lastDotX < 3) {
          continue;
        }
        lastDotX = x;
        const y = padT + plotH - clampDist(sample.dist) * yScale;
        ctx.fillStyle = sample.trg ? SENSOR_PRESSED_COLOR : SENSOR_RELEASED_COLOR;
        ctx.beginPath();
        ctx.arc(x, y, 1.6, 0, Math.PI * 2);
        ctx.fill();
      }
      sensorChartEdges = edges;
    }
  }

  if (sensorHover) {
    drawSensorHover(ctx, sensorViewCache, cssW, cssH);
  }

  ctx.font = '11px Consolas, monospace';
  ctx.textAlign = 'right';
  ctx.textBaseline = 'top';
  /* 从右往左排，文本变长也不会重叠。 */
  let legendX = cssW - 8;
  const legend = [
    ['● 释放事件', SENSOR_TRANSITION_RELEASE_COLOR],
    ['● 按下事件', SENSOR_TRANSITION_PRESS_COLOR],
    ['■ 释放', SENSOR_RELEASED_COLOR],
    ['■ 按下', SENSOR_PRESSED_COLOR]
  ];
  for (const [text, color] of legend) {
    ctx.fillStyle = color;
    ctx.fillText(text, legendX, 4);
    legendX -= ctx.measureText(text).width + 14;
  }

  ctx.fillStyle = '#8b98a5';
  ctx.textAlign = 'left';
  ctx.fillText('mm', 6, 4);
}

function sensorChartLoop() {
  /* 每帧都重绘：按 30fps 节流会让窗口一次跳 5~10px，看起来像闪频。
     24000 点的全量绘制实测约 1ms，60fps 完全负担得起。 */
  drawSensorChart();
  sensorChartRaf = (sensorRptEnabled && !sensorPaused)
    ? requestAnimationFrame(sensorChartLoop)
    : 0;
}

function startSensorChart() {
  if (!sensorChartRaf && sensorRptEnabled && !sensorPaused) {
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

  const now = performance.now();
  const elapsed = now - sensorRateStamp;
  if (elapsed >= 100) {
    sensorRateHz = ((sensorNotifyCount - sensorRateCount) * 1000) / elapsed;
    sensorRateCount = sensorNotifyCount;
    sensorRateStamp = now;
  }

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
  sensorNotifyCount = 0;
  sensorRateCount = 0;
  sensorRateStamp = performance.now();
  sensorRateHz = 0;
  sensorLastException = null;
  sensorHistory = [];
  resetSensorViewState();
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
  sensorRateHz = 0;
  resetSensorViewState();
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
  sensorNotifyCount = 0;
  sensorRateHz = 0;
  sensorLastException = null;
  resetSensorViewState();
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
