'use strict';


function kcCacheKey(layer, index) {
  return `${layer}:${index}`;
}


function renderKeyboard() {
  $keyboard.textContent = '';
  kcKeyEls.clear();

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
      keyEl.addEventListener('click', () => selectKey(index));

      $keyboard.appendChild(keyEl);
      kcKeyEls.set(index, { root: keyEl, value: valueEl });
      column += span;
    }
  }
  syncKeycaps();
  syncKeycapSize();
}

/* 网格总宽 48 列（12U）、4 行；1U 取正方形，按卡片可用宽高收敛。 */
let keycapUnit = 0;

function syncKeycapSize() {
  const panel = $keyboard.parentElement;
  if (!panel) return;
  const style = getComputedStyle(panel);
  const width = panel.clientWidth -
    parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
  const height = panel.clientHeight -
    parseFloat(style.paddingTop) - parseFloat(style.paddingBottom);
  if (width <= 0 || height <= 0) return;

  const unit = Math.min(55, Math.max(30, Math.floor(Math.min(width / 12, height / 4))));
  if (unit === keycapUnit) return;
  keycapUnit = unit;
  $keyboard.style.setProperty('--u', `${unit}px`);
}

function syncKeycaps() {
  const layer = kcLayerIndex;

  for (const [index, el] of kcKeyEls) {
    const code = kcValues.get(kcCacheKey(layer, index));
    const label = el.root.dataset.label;
    el.value.textContent = code === undefined ? '--' : kcDescribe(code);
    el.value.classList.toggle('set', code !== undefined);
    el.root.classList.toggle('selected', kcSelected === index);
    el.root.title = code === undefined
      ? `${label} · 索引 ${index}（未读取）`
      : `${label} · 索引 ${index} · 0x${(code >>> 0).toString(16).padStart(8, '0')} · ${kcDescribe(code)}`;
  }
}

function selectKey(index) {
  kcSelected = index;
  syncKeycaps();
  readKeycode(index);
}

async function readKeycode(index) {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  const layer = kcLayerIndex;
  const target = findCcTarget();
  if (!target) {
    appendLine('[KC] 没有找到 CC IN/OUT 端点', 'err');
    return;
  }

  kcBusy = true;
  refreshControls();
  try {
    const payload = await ccCall(target, client =>
      client.readPayload(CC.CMD_KC, CC.KC_GET, 4, {
        reserve: ((layer & 0xff) << 8) | (index & 0xff),
        label: `读取键码 Fn${layer} K${index}`,
        timeoutMs: 3000
      })
    );

    const code = readLe32(payload, 0);
    kcValues.set(kcCacheKey(layer, index), code);
    appendLine(
      `[KC] Fn${layer} K${index} = 0x${code.toString(16).padStart(8, '0')} ` +
      `(${kcDescribe(code)})`
    );
  } catch (error) {
    appendLine(`[KC] 读取失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    syncKeycaps();
    refreshControls();
  }
}

async function readKeycodeLayer() {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
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
      for (const row of KEY_ROWS) {
        for (const [, index] of row) {
          const payload = await client.readPayload(CC.CMD_KC, CC.KC_GET, 4, {
            reserve: ((layer & 0xff) << 8) | (index & 0xff),
            label: `读取键码 Fn${layer} K${index}`,
            timeoutMs: 3000
          });
          kcValues.set(kcCacheKey(layer, index), readLe32(payload, 0));
        }
        syncKeycaps();
      }
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

/* 把选中的键码写到当前键盘选中的按键上。 */
async function assignKeycode(code, label) {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  const layer = kcLayerIndex;
  if (kcSelected === null) {
    appendLine('[KC] 请先在键盘上点选一个按键，再选择键码', 'warn');
    return;
  }
  const index = kcSelected;

  kcBusy = true;
  refreshControls();
  try {
    const target = findCcTarget();
    if (!target) throw new Error('没有找到 CC IN/OUT 端点');

    const payload = new Uint8Array(4);
    writeLe32(payload, 0, code);
    await ccCall(target, client =>
      client.statusRequest(CC.CMD_KC, CC.KC_SET, {
        reserve: ((layer & 0xff) << 8) | (index & 0xff),
        payload,
        label: `写入键码 Fn${layer} K${index}`,
        timeoutMs: 3000
      })
    );

    kcValues.set(kcCacheKey(layer, index), code);
    appendLine(
      `[KC] Fn${layer} K${index} <= ${label} 0x` +
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

