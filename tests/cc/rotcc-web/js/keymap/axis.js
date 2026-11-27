'use strict';

/* ------------------------------------------------------------ 按键轴体 */

const AXIS_NAMES = {
  0: 'BB',
  1: 'GDL',
  2: 'TTC'
};

const AXIS_TABS = [
  { id: 'list', name: '轴体列表' },
  { id: 'set', name: '轴体设置' }
];

let axisSupport = [];
let axisKeyEls = new Map();
let axisTab = AXIS_TABS[0];
let axisWriting = false;
let axisWritePending = false;
const axisSelected = new Set();
const axisValues = new Map();

function axisReady() {
  return !kcBusy && !sysBusy && ccQueryPromise === null &&
    !isFirmwareActive() && Boolean(device?.opened);
}

function axisLabel(type) {
  return AXIS_NAMES[type] || `轴体 ${type}`;
}

function setAxisState(text, className = '') {
  $axisState.textContent = text;
  $axisState.className = className ? `hint ${className}` : 'hint';
}

function applyAxisTab() {
  $axisListPanel.hidden = axisTab.id !== 'list';
  $axisSetPanel.hidden = axisTab.id !== 'set';
}

function renderAxisTabs() {
  $axisTabs.textContent = '';
  for (const tab of AXIS_TABS) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = `picker-tab${tab.id === axisTab.id ? ' active' : ''}`;
    btn.textContent = tab.name;
    btn.addEventListener('click', () => {
      if (kcBusy || sysBusy || isFirmwareActive()) return;
      axisTab = tab;
      renderAxisTabs();
      refreshControls();
      if ((axisSupport.length === 0) && axisReady()) {
        readAxisSupportList();
      }
    });
    $axisTabs.appendChild(btn);
  }
  applyAxisTab();
}

function renderAxisOptions() {
  $axisValue.textContent = '';
  for (const type of axisSupport) {
    const option = document.createElement('option');
    option.value = String(type);
    option.textContent = axisLabel(type);
    $axisValue.appendChild(option);
  }
  const text = axisSupport.length
    ? `支持 ${axisSupport.map(axisLabel).join(' / ')}`
    : '支持列表 --';
  $axisSupport.textContent = text;
  $axisListText.textContent = text;
}

function renderAxisKeyboard() {
  axisKeyEls = buildKeyboard($axisKeyboard, toggleAxisKey);
  syncKeycaps();
  syncAxisSelection();
}

function syncAxisSelection() {
  for (const [index, el] of axisKeyEls) {
    el.root.classList.toggle('selected', axisSelected.has(index));
  }
  $axisSelection.textContent = `已选择 ${axisSelected.size} 个键位`;
  refreshControls();
}

function toggleAxisKey(index) {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  if (axisSelected.has(index)) axisSelected.delete(index);
  else axisSelected.add(index);
  syncAxisSelection();
  setAxisState(axisSelected.size
    ? `已选择 ${axisSelected.size} 个键位`
    : '请选择要设置轴体的按键。');

  if ((axisSelected.size > 0) && (axisSupport.length === 0) &&
      !kcBusy && axisReady()) {
    readAxisSupportList();
  }
}

function selectAllAxisKeys() {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  axisSelected.clear();
  for (const index of axisKeyEls.keys()) axisSelected.add(index);
  syncAxisSelection();
  setAxisState(`已选择 ${axisSelected.size} 个键位`);
}

function clearAxisSelection() {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  axisSelected.clear();
  syncAxisSelection();
  setAxisState('请选择要设置轴体的按键。');
}

async function fetchAxisSupport(client) {
  const payload = await client.readPayload(CC.CMD_AXIS, CC.AXIS_LIST, 1, {
    label: '轴体支持列表',
    timeoutMs: 3000
  });
  axisSupport = [...payload];
  renderAxisOptions();
  appendLine(`[AXIS] 支持 ${axisSupport.map(axisLabel).join(', ')}`);
}

async function readAxisSupportList() {
  if (!axisReady()) {
    setAxisState('请先连接设备。', 'warn');
    return;
  }
  const target = findCcTarget();
  if (!target) {
    setAxisState('没有找到 CC IN/OUT 端点。', 'err');
    return;
  }

  kcBusy = true;
  refreshControls();
  setAxisState('读取支持列表...');
  try {
    await ccCall(target, client => fetchAxisSupport(client));
    setAxisState('支持列表已读取');
    refreshControls();
  } catch (error) {
    setAxisState(`读取失败：${error.message || error}`, 'err');
    appendLine(`[AXIS] 支持列表读取失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    refreshControls();
  }
}

/* 读取选中键所在连续区间的轴体类型：查询帧为 {start, end, rsv, rsv}，每键 4 字节。 */
async function readAxisTypes() {
  if (!axisReady()) {
    setAxisState('请先连接设备。', 'warn');
    return;
  }
  if (axisSelected.size === 0) {
    setAxisState('请先在键盘上选择按键。', 'warn');
    return;
  }
  const target = findCcTarget();
  if (!target) {
    setAxisState('没有找到 CC IN/OUT 端点。', 'err');
    return;
  }

  const indices = [...axisSelected].sort((a, b) => a - b);
  const start = indices[0];
  const end = indices[indices.length - 1];

  kcBusy = true;
  refreshControls();
  setAxisState('读取轴体...');
  try {
    await ccCall(target, async client => {
      const entrySize = 4;
      const step = ccMaxEntries(client.packetSize, entrySize);
      for (let first = start; first <= end; first += step) {
        const last = Math.min(first + step - 1, end);
        const count = last - first + 1;
        const payload = await client.readPayload(CC.CMD_AXIS, CC.AXIS_GET, count * entrySize, {
          payload: Uint8Array.of(first & 0xff, last & 0xff, 0, 0),
          label: `读取轴体 K${first}-${last}`,
          timeoutMs: 3000
        });
        for (let i = 0; i < count; i++) {
          axisValues.set(first + i, readLe32(payload, i * entrySize));
        }
      }
    });

    const text = indices
      .map(index => `K${index}=${axisLabel(axisValues.get(index))}`)
      .join(' · ');
    setAxisState(text);
    appendLine(`[AXIS] ${text}`);
  } catch (error) {
    setAxisState(`读取失败：${error.message || error}`, 'err');
    appendLine(`[AXIS] 轴体读取失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    refreshControls();
  }
}

async function writeAxisToSelection() {
  if (axisWriting) {
    axisWritePending = true;
    return;
  }
  if (!axisReady()) {
    setAxisState('请先连接设备。', 'warn');
    return;
  }
  if (axisSelected.size === 0) {
    setAxisState('请先在键盘上选择按键。', 'warn');
    return;
  }
  if (axisSupport.length === 0) {
    setAxisState('请先读取支持列表。', 'warn');
    return;
  }
  const target = findCcTarget();
  if (!target) {
    setAxisState('没有找到 CC IN/OUT 端点。', 'err');
    return;
  }

  const axis = Number($axisValue.value);
  const indices = [...axisSelected].sort((a, b) => a - b);
  const runs = groupKeyRuns(indices);

  axisWriting = true;
  kcBusy = true;
  refreshControls();
  setAxisState(`写入${axisLabel(axis)}...`);
  try {
    await ccCall(target, async client => {
      for (const run of runs) {
        const payload = new Uint8Array(2 + 4);
        payload[0] = run.start;
        payload[1] = run.end;
        writeLe32(payload, 2, axis);
        await client.statusRequest(CC.CMD_AXIS, CC.AXIS_SET, {
          payload,
          label: `写入轴体 K${run.start}-${run.end}`,
          timeoutMs: 3000
        });
      }
      for (const index of indices) axisValues.set(index, axis);
    });

    setAxisState(`${axisLabel(axis)} 已写入 ${indices.length} 个键位`);
    appendLine(`[AXIS] ${axisLabel(axis)} 已写入 ${indices.length} 个键位`);
  } catch (error) {
    setAxisState(`写入失败：${error.message || error}`, 'err');
    appendLine(`[AXIS] 写入失败：${error.message || error}`, 'err');
  } finally {
    axisWriting = false;
    kcBusy = false;
    refreshControls();
    if (axisWritePending) {
      axisWritePending = false;
      writeAxisToSelection();
    }
  }
}
