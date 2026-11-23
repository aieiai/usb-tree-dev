'use strict';


function kcCacheKey(layer, index) {
  return `${layer}:${index}`;
}


/* 渲染 40% 配列键盘，返回 key index -> { root, value }。 */
function buildKeyboard(container, onKeyClick) {
  container.textContent = '';
  const keyEls = new Map();

  for (let rowIndex = 0; rowIndex < KEY_ROWS.length; rowIndex++) {
    let column = 0;
    for (const [label, index, width] of KEY_ROWS[rowIndex]) {
      const span = Math.round(width * 4);
      const keyEl = document.createElement('button');
      keyEl.type = 'button';
      keyEl.className = 'keycap';
      keyEl.dataset.index = String(index);
      keyEl.dataset.label = label;
      keyEl.setAttribute('aria-label', label);
      keyEl.style.gridRow = String(rowIndex + 1);
      keyEl.style.gridColumn = `${1 + column} / span ${span}`;

      const valueEl = document.createElement('span');
      valueEl.className = 'kc-value';
      valueEl.textContent = '--';

      keyEl.appendChild(valueEl);
      if (onKeyClick) {
        keyEl.addEventListener('click', () => onKeyClick(index));
      }

      container.appendChild(keyEl);
      keyEls.set(index, { root: keyEl, value: valueEl });
      column += span;
    }
  }
  return keyEls;
}

function renderKeyboard() {
  kcKeyEls.clear();
  for (const [index, el] of buildKeyboard($keyboard, selectKey)) {
    kcKeyEls.set(index, el);
  }
  syncKeycaps();
  syncKeycapSize();
}

/* 网格总宽 48 列（12U）、4 行；1U 取正方形，按卡片可用宽高收敛。 */
const keyboardUnits = new WeakMap();

function fitKeyboardSize(container) {
  const panel = container.parentElement;
  if (!panel) return;
  const style = getComputedStyle(panel);
  const width = panel.clientWidth -
    parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
  const height = panel.clientHeight -
    parseFloat(style.paddingTop) - parseFloat(style.paddingBottom);
  if (width <= 0 || height <= 0) return;

  const unit = Math.min(55, Math.max(30, Math.floor(Math.min(width / 12, height / 4))));
  if (keyboardUnits.get(container) === unit) return;
  keyboardUnits.set(container, unit);
  container.style.setProperty('--u', `${unit}px`);
}

function syncKeycapSize() {
  for (const keyboard of [$keyboard, $paramKeyboard, $axisKeyboard, $calibKeyboard]) {
    if (keyboard) fitKeyboardSize(keyboard);
  }
}

/* ------------------------------------------------------------ 快捷多选 */

const KC_QUICK_GROUPS = [
  { name: 'WASD', labels: ['W', 'A', 'S', 'D'] },
  { name: 'QWER', labels: ['Q', 'W', 'E', 'R'] },
  { name: 'ZXCV', labels: ['Z', 'X', 'C', 'V'] },
  { name: '全选', all: true },
  { name: '清空', clear: true }
];

function labelIndices(label) {
  const indices = [];
  for (const row of KEY_ROWS) {
    for (const [name, index] of row) {
      if (name === label) indices.push(index);
    }
  }
  return indices;
}

function renderQuickSelect() {
  $kcQuickSelect.textContent = '';

  for (const group of KC_QUICK_GROUPS) {
    const indices = group.all
      ? [...kcKeyEls.keys()]
      : group.clear
        ? []
        : group.labels.flatMap(labelIndices);
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'kc-layer';
    btn.textContent = group.name;
    btn.addEventListener('click', () => applyQuickGroup(group, indices));
    $kcQuickSelect.appendChild(btn);
  }
}

/* 再次点击同一快捷组可整组取消选择。 */
function applyQuickGroup(group, indices) {
  if (kcBusy || sysBusy || isFirmwareActive()) return;

  if (group.clear) {
    kcSelected.clear();
    syncKeycaps();
    return;
  }

  const allSelected = indices.length > 0 &&
    indices.every(index => kcSelected.has(index));
  const added = [];
  for (const index of indices) {
    if (allSelected) {
      kcSelected.delete(index);
    } else if (!kcSelected.has(index)) {
      kcSelected.add(index);
      added.push(index);
    }
  }
  syncKeycaps();

  const uncached = added.filter(
    index => !kcValues.has(kcCacheKey(kcLayerIndex, index))
  );
  if (uncached.length > 0) readKeycodes(uncached);
}

/* 参数/轴体页的键盘与按键映射共用同一份当前层键码。 */
function syncSharedKeyboardValues(keyEls) {
  const layer = kcLayerIndex;

  for (const [index, el] of keyEls) {
    const code = kcValues.get(kcCacheKey(layer, index));
    const label = el.root.dataset.label;
    el.value.textContent = code === undefined ? label : kcDescribe(code);
    el.value.classList.add('set');
    el.root.title = code === undefined
      ? `${label} · 索引 ${index}`
      : `${label} · 索引 ${index} · 0x${(code >>> 0).toString(16).padStart(8, '0')} · ${kcDescribe(code)}`;
  }
}

function syncKeycaps() {
  const layer = kcLayerIndex;

  for (const [index, el] of kcKeyEls) {
    const code = kcValues.get(kcCacheKey(layer, index));
    const label = el.root.dataset.label;
    const selected = kcSelected.has(index);
    el.value.textContent = code === undefined ? '--' : kcDescribe(code);
    el.value.classList.toggle('set', code !== undefined);
    el.root.classList.toggle('selected', selected);
    const base = code === undefined
      ? `${label} · 索引 ${index}（未读取）`
      : `${label} · 索引 ${index} · 0x${(code >>> 0).toString(16).padStart(8, '0')} · ${kcDescribe(code)}`;
    el.root.title = selected ? `${base} · 再次点击取消选择` : base;
  }

  syncSharedKeyboardValues(paramKeyEls);
  syncSharedKeyboardValues(axisKeyEls);
}

function selectKey(index) {
  /* 多选：点击切换选中状态，再次点击已选中的键取消选择。 */
  if (kcSelected.has(index)) {
    kcSelected.delete(index);
    syncKeycaps();
    return;
  }
  kcSelected.add(index);
  syncKeycaps();
  readKeycode(index);
}

/* 按设备协议读取 [start, end] 范围内的键码：查询帧为 {start, end, rsv, rsv}。 */
async function readKeycodeRange(client, layer, start, end) {
  const count = end - start + 1;
  const query = Uint8Array.of(start & 0xff, end & 0xff, 0, 0);

  const payload = await client.readPayload(CC.CMD_KC, CC.KC_GET, count * 4, {
    reserve: (layer & 0xff) << 8,
    payload: query,
    label: `读取键码 Fn${layer} K${start}-${end}`,
    timeoutMs: 3000
  });

  for (let i = 0; i < count; i++) {
    kcValues.set(kcCacheKey(layer, start + i), readLe32(payload, i * 4));
  }
}

async function readKeycodes(indices) {
  const unique = [...new Set(indices)].sort((a, b) => a - b);
  if ((unique.length === 0) || kcBusy || sysBusy || isFirmwareActive()) return;
  const layer = kcLayerIndex;
  const target = findCcTarget();
  if (!target) {
    appendLine('[KC] 没有找到 CC IN/OUT 端点', 'err');
    return;
  }

  kcBusy = true;
  refreshControls();
  try {
    await ccCall(target, async client => {
      const start = unique[0];
      const end = unique[unique.length - 1];

      await readKeycodeRange(client, layer, start, end);
      for (const index of unique) {
        const code = kcValues.get(kcCacheKey(layer, index));
        appendLine(
          `[KC] Fn${layer} K${index} = 0x${code.toString(16).padStart(8, '0')} ` +
          `(${kcDescribe(code)})`
        );
      }
      syncKeycaps();
    });
  } catch (error) {
    appendLine(`[KC] 读取失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    syncKeycaps();
    refreshControls();
  }
}

function readKeycode(index) {
  return readKeycodes([index]);
}

async function readKeycodeLayer() {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  const layer = kcLayerIndex;
  const target = findCcTarget();
  if (!target) {
    appendLine('[KC] 没有找到 CC IN/OUT 端点', 'err');
    return;
  }

  const indices = [...kcKeyEls.keys()];
  if (indices.length === 0) return;
  const start = Math.min(...indices);
  const end = Math.max(...indices);

  kcBusy = true;
  refreshControls();
  try {
    await ccCall(target, async client => {
      await readKeycodeRange(client, layer, start, end);
      syncKeycaps();
    });
    appendLine(`[KC] 已读取 Fn${layer} 全部键码`);
  } catch (error) {
    appendLine(`[KC] 读取本层失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    syncKeycaps();
    refreshControls();
  }
}

/* 连接后自动读取当前层的全部键码，已缓存则跳过。 */
function autoReadKeymap() {
  if (!device?.opened || !links.length || sysBusy || isFirmwareActive()) return;
  const layer = kcLayerIndex;
  if (kcValues.has(kcCacheKey(layer, 0))) return;
  readKeycodeLayer();
}

/* 设置帧为 {start, end, kc_t}，同一个键码应用到整个范围。 */
async function writeKeycodeRange(client, layer, start, end, code) {
  const payload = new Uint8Array(2 + 4);
  payload[0] = start & 0xff;
  payload[1] = end & 0xff;
  writeLe32(payload, 2, code);

  await client.statusRequest(CC.CMD_KC, CC.KC_SET, {
    reserve: (layer & 0xff) << 8,
    payload,
    label: `写入键码 Fn${layer} K${start}-${end}`,
    timeoutMs: 3000
  });
}

/* 把选中的键码写到当前所有选中的按键上。 */
async function assignKeycode(code, label) {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  const layer = kcLayerIndex;
  const indices = [...kcSelected].sort((a, b) => a - b);
  if (indices.length === 0) {
    appendLine('[KC] 请先在键盘上点选按键（可多选），再选择键码', 'warn');
    return;
  }

  /* 连续按键合并成一帧，未选中的按键不会被覆盖。 */
  const runs = [];
  for (const index of indices) {
    const last = runs[runs.length - 1];
    if (last && (last.end + 1 === index)) last.end = index;
    else runs.push({ start: index, end: index });
  }

  kcBusy = true;
  refreshControls();
  try {
    const target = findCcTarget();
    if (!target) throw new Error('没有找到 CC IN/OUT 端点');

    await ccCall(target, async client => {
      for (const run of runs) {
        await writeKeycodeRange(client, layer, run.start, run.end, code);
      }
      for (const index of indices) {
        kcValues.set(kcCacheKey(layer, index), code);
      }
    });

    appendLine(
      `[KC] ${indices.length} 个按键 <= ${label} 0x` +
      `${code.toString(16).padStart(8, '0')} (${kcDescribe(code)})`
    );
  } catch (error) {
    appendLine(`[KC] 写入失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    syncKeycaps();
    refreshControls();
  }
}

