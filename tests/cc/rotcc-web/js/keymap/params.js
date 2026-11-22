'use strict';

/* ------------------------------------------------------------ 按键参数 */

const PARAM_TABS = [
  { id: 'trg', name: '触发模式', get: CC.KC_TRG_GET, set: CC.KC_TRG_SET, size: 4 },
  { id: 'dz', name: '死区', get: CC.KC_DZ_GET, set: CC.KC_DZ_SET, size: 4 },
  { id: 'nom', name: '常规触发', get: CC.KC_NOM_GET, set: CC.KC_NOM_SET, size: 4 },
  { id: 'rt', name: '快速触发', get: CC.KC_RT_GET, set: CC.KC_RT_SET, size: 6 },
  { id: 'tick', name: 'tick 参数', get: CC.KC_TICK_GET, set: CC.KC_TICK_SET, size: 12 }
];

const KTRG_LABELS = ['常规', '快速触发', '快速触发(持续)'];
const TICK_LABELS = ['sclt', 'sclto', 'dcit', 'dclt', 'dclto'];

let paramTab = PARAM_TABS[0];
let paramKeyEls = new Map();
let paramFormRefs = null;
let paramLoaded = false;
let paramPrimaryKey = null;
let paramReadSeq = 0;
let paramReadPending = false;
let paramReading = false;
const paramSelected = new Set();

function setParamState(text, className = '') {
  $paramState.textContent = text;
  $paramState.className = className ? `muted ${className}` : 'muted';
}

/* 把排序后的按键索引合并为连续区间，未选中的键不会被写入。 */
function groupKeyRuns(indices) {
  const runs = [];
  for (const index of indices) {
    const last = runs[runs.length - 1];
    if (last && (last.end + 1 === index)) last.end = index;
    else runs.push({ start: index, end: index });
  }
  return runs;
}

function paramNumber(value) {
  const parsed = Number(value);
  return Number.isFinite(parsed)
    ? Math.min(65535, Math.max(0, Math.round(parsed)))
    : 0;
}

function renderParamKeyboard() {
  paramKeyEls = buildKeyboard($paramKeyboard, toggleParamKey);
  syncKeycaps();
  syncParamSelection();
}

function syncParamSelection() {
  for (const [index, el] of paramKeyEls) {
    el.root.classList.toggle('selected', paramSelected.has(index));
  }
  $paramSelection.textContent = `已选择 ${paramSelected.size} 个键位`;
  refreshControls();
}

function invalidateParamValues() {
  paramLoaded = false;
}

function paramPrimary() {
  if ((paramPrimaryKey !== null) && paramSelected.has(paramPrimaryKey)) {
    return paramPrimaryKey;
  }
  const indices = [...paramSelected].sort((a, b) => a - b);
  return indices.length > 0 ? indices[0] : null;
}

/* 选中键位或切换分类后自动读取参数；读取期间只保留最后一次请求。 */
function scheduleParamRead() {
  if ((paramTab.id !== 'tick') && (paramSelected.size === 0)) return;
  if (sysBusy || isFirmwareActive()) return;
  if (!Boolean(device?.opened)) {
    setParamState('请连接设备', 'warn');
    return;
  }
  if (paramReading) {
    paramReadPending = true;
    return;
  }
  if (kcBusy) {
    setParamState('设备忙，请稍后重新选择按键', 'warn');
    return;
  }
  readParamValues();
}

function toggleParamKey(index) {
  if (sysBusy || isFirmwareActive()) return;
  if (paramSelected.has(index)) {
    paramSelected.delete(index);
    if (paramPrimaryKey === index) paramPrimaryKey = null;
  } else {
    paramSelected.add(index);
    paramPrimaryKey = index;
  }

  if (paramTab.id !== 'tick') {
    paramReadSeq++;
    invalidateParamValues();
  }
  syncParamSelection();

  if (paramTab.id === 'tick') {
    setParamState('tick 参数为全键盘配置');
    if (!paramLoaded) scheduleParamRead();
    return;
  }
  if (paramSelected.size === 0) {
    renderParamForm(null);
    setParamState('未选择');
    return;
  }

  setParamState(`已选择 K${paramPrimaryKey}，读取中...`);
  scheduleParamRead();
}

function selectAllParamKeys() {
  if (sysBusy || isFirmwareActive()) return;
  paramSelected.clear();
  paramPrimaryKey = null;
  for (const index of paramKeyEls.keys()) {
    paramSelected.add(index);
    if (paramPrimaryKey === null) paramPrimaryKey = index;
  }
  if (paramTab.id !== 'tick') {
    paramReadSeq++;
    invalidateParamValues();
  }
  syncParamSelection();
  setParamState(`已选择 ${paramSelected.size} 个键位，读取中...`);
  scheduleParamRead();
}

function clearParamSelection() {
  if (sysBusy || isFirmwareActive()) return;
  paramSelected.clear();
  paramPrimaryKey = null;
  if (paramTab.id !== 'tick') {
    paramReadSeq++;
    invalidateParamValues();
    renderParamForm(null);
  }
  syncParamSelection();
  setParamState('未选择');
}

function renderParamTabs() {
  $paramTabs.textContent = '';
  for (const tab of PARAM_TABS) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = `picker-tab${tab.id === paramTab.id ? ' active' : ''}`;
    btn.textContent = tab.name;
    btn.addEventListener('click', () => {
      if (sysBusy || isFirmwareActive()) return;
      paramTab = tab;
      paramReadSeq++;
      invalidateParamValues();
      renderParamTabs();
      renderParamForm(null);
      setParamState(paramSelected.size ? '读取中...' : '未选择',
                    paramSelected.size ? 'warn' : '');
      refreshControls();
      scheduleParamRead();
    });
    $paramTabs.appendChild(btn);
  }
}

function formField(labelText, input) {
  const field = document.createElement('label');
  field.className = 'param-field';
  const text = document.createElement('span');
  text.textContent = labelText;
  field.appendChild(text);
  field.appendChild(input);
  return field;
}

function formNumberInput(value) {
  const input = document.createElement('input');
  input.type = 'number';
  input.min = '0';
  input.max = '65535';
  input.value = String(value);
  return input;
}

function formCheckbox(checked) {
  const input = document.createElement('input');
  input.type = 'checkbox';
  input.checked = Boolean(checked);
  return input;
}

function formSelect(labels, value) {
  const select = document.createElement('select');
  labels.forEach((label, index) => {
    const option = document.createElement('option');
    option.value = String(index);
    option.textContent = label;
    select.appendChild(option);
  });
  select.value = String(Math.min(labels.length - 1, Math.max(0, value)));
  return select;
}

/* values 为空时显示默认值；读取后传入首个选中键的参数或 tick 数组。 */
function renderParamForm(values) {
  $paramForm.textContent = '';
  const grid = document.createElement('div');
  grid.className = 'param-form-grid';
  paramFormRefs = {};

  if (paramTab.id === 'tick') {
    paramFormRefs.tick = TICK_LABELS.map((label, i) => {
      const input = formNumberInput(values ? values[i] : 0);
      grid.appendChild(formField(label, input));
      return input;
    });
    $paramForm.appendChild(grid);
    return;
  }

  if (paramTab.id === 'trg') {
    const mode = formSelect(KTRG_LABELS, values ? values.trg : 0);
    paramFormRefs.trg = mode;
    grid.appendChild(formField('模式', mode));
  } else if ((paramTab.id === 'dz') || (paramTab.id === 'nom')) {
    const labels = paramTab.id === 'dz'
      ? ['顶部死区', '底部死区']
      : ['按下距离', '抬起距离'];
    const a = formNumberInput(values ? values.a : 0);
    const b = formNumberInput(values ? values.b : 0);
    paramFormRefs.a = a;
    paramFormRefs.b = b;
    grid.appendChild(formField(labels[0], a));
    grid.appendChild(formField(labels[1], b));
  } else {
    const en = formCheckbox(values ? values.en : false);
    const ctn = formCheckbox(values ? values.ctn : false);
    const a = formNumberInput(values ? values.a : 0);
    const b = formNumberInput(values ? values.b : 0);
    paramFormRefs = { en, ctn, a, b };
    grid.appendChild(formField('启用', en));
    grid.appendChild(formField('持续触发', ctn));
    grid.appendChild(formField('按下灵敏度', a));
    grid.appendChild(formField('抬起灵敏度', b));
  }

  $paramForm.appendChild(grid);
}

function parseParamEntry(tab, payload, index) {
  switch (tab.id) {
    case 'trg':
      return { trg: readLe32(payload, index * 4) };
    case 'dz':
    case 'nom':
      return {
        a: readLe16(payload, index * 4),
        b: readLe16(payload, index * 4 + 2)
      };
    default: {
      const flags = readLe16(payload, index * 6);
      return {
        en: (flags & 0x1) !== 0,
        ctn: (flags & 0x2) !== 0,
        a: readLe16(payload, index * 6 + 2),
        b: readLe16(payload, index * 6 + 4)
      };
    }
  }
}

function readParamForm() {
  if (paramTab.id === 'trg') {
    return { trg: Number(paramFormRefs.trg.value) || 0 };
  }
  if ((paramTab.id === 'dz') || (paramTab.id === 'nom')) {
    return {
      a: paramNumber(paramFormRefs.a.value),
      b: paramNumber(paramFormRefs.b.value)
    };
  }
  return {
    en: paramFormRefs.en.checked,
    ctn: paramFormRefs.ctn.checked,
    a: paramNumber(paramFormRefs.a.value),
    b: paramNumber(paramFormRefs.b.value)
  };
}

function writeParamEntry(tab, payload, offset, values) {
  if (tab.id === 'trg') {
    writeLe32(payload, offset, values.trg);
  } else if ((tab.id === 'dz') || (tab.id === 'nom')) {
    writeLe16(payload, offset, values.a);
    writeLe16(payload, offset + 2, values.b);
  } else {
    const flags = (values.en ? 0x1 : 0) | (values.ctn ? 0x2 : 0);
    writeLe16(payload, offset, flags);
    writeLe16(payload, offset + 2, values.a);
    writeLe16(payload, offset + 4, values.b);
  }
}

async function readParamValues() {
  if (paramReading) {
    paramReadPending = true;
    return;
  }

  const tab = paramTab;
  const tick = tab.id === 'tick';
  const primary = tick ? null : paramPrimary();
  if (!tick && (primary === null)) {
    setParamState('请先在键盘上选择按键。', 'warn');
    return;
  }
  if (!Boolean(device?.opened)) {
    setParamState('请先连接设备。', 'warn');
    return;
  }
  if (sysBusy || isFirmwareActive()) {
    setParamState('设备忙，请稍后再试。', 'warn');
    return;
  }
  const target = findCcTarget();
  if (!target) {
    setParamState('没有找到 CC IN/OUT 端点。', 'err');
    return;
  }

  const seq = ++paramReadSeq;
  paramReading = true;
  kcBusy = true;
  refreshControls();
  try {
    let values = null;

    if (tick) {
      setParamState('读取 tick...');
      await ccCall(target, async client => {
        const payload = await client.readPayload(CC.CMD_KC, CC.KC_TICK_GET, 12, {
          label: '读取 tick',
          timeoutMs: 3000
        });
        values = TICK_LABELS.map((_, i) => readLe16(payload, i * 2));
      });
    } else {
      setParamState(`读取${tab.name} K${primary}...`);
      await ccCall(target, async client => {
        const payload = await client.readPayload(CC.CMD_KC, tab.get, tab.size, {
          payload: Uint8Array.of(primary, primary),
          label: `读取${tab.name} K${primary}`,
          timeoutMs: 3000
        });
        values = parseParamEntry(tab, payload, 0);
      });
    }

    if (seq !== paramReadSeq) {
      return;
    }

    renderParamForm(values);
    paramLoaded = true;
    if (tick) {
      setParamState('tick 参数已读取');
      appendLine('[PARAM] tick 参数已读取');
    } else {
      setParamState(`${tab.name} K${primary} 已读取 · 写入 ${paramSelected.size} 个键位`);
      appendLine(`[PARAM] ${tab.name} K${primary} 已读取`);
    }
  } catch (error) {
    if (seq === paramReadSeq) {
      setParamState(`读取失败：${error.message || error}`, 'err');
      appendLine(`[PARAM] ${tab.name} 读取失败：${error.message || error}`, 'err');
    }
  } finally {
    paramReading = false;
    kcBusy = false;
    refreshControls();
    if (paramReadPending) {
      paramReadPending = false;
      scheduleParamRead();
    }
  }
}

async function writeParamValues() {
  const tab = paramTab;
  const tick = tab.id === 'tick';
  if (!tick && (paramSelected.size === 0)) {
    setParamState('请先在键盘上选择按键。', 'warn');
    return;
  }
  if (!paramLoaded) {
    setParamState('请先读取参数。', 'warn');
    return;
  }
  if (!Boolean(device?.opened)) {
    setParamState('请先连接设备。', 'warn');
    return;
  }
  if (kcBusy || sysBusy || isFirmwareActive()) {
    setParamState('设备忙，请稍后再试。', 'warn');
    return;
  }
  const target = findCcTarget();
  if (!target) {
    setParamState('没有找到 CC IN/OUT 端点。', 'err');
    return;
  }

  const values = tick ? null : readParamForm();
  const tickValues = tick
    ? paramFormRefs.tick.map(input => paramNumber(input.value))
    : null;
  const indices = [...paramSelected].sort((a, b) => a - b);
  const runs = groupKeyRuns(indices);

  kcBusy = true;
  refreshControls();
  try {
    if (tick) {
      const payload = new Uint8Array(12);
      tickValues.forEach((value, i) => {
        writeLe16(payload, i * 2, value);
      });
      setParamState('写入 tick...');
      await ccCall(target, client =>
        client.statusRequest(CC.CMD_KC, CC.KC_TICK_SET, {
          payload,
          label: '写入 tick',
          timeoutMs: 3000
        })
      );
      setParamState('tick 参数已写入');
      appendLine('[PARAM] tick 参数已写入');
      return;
    }

    setParamState(`写入${tab.name}...`);
    await ccCall(target, async client => {
      for (const run of runs) {
        const count = run.end - run.start + 1;
        const payload = new Uint8Array(2 + count * tab.size);
        payload[0] = run.start;
        payload[1] = run.end;
        for (let i = 0; i < count; i++) {
          writeParamEntry(tab, payload, 2 + i * tab.size, values);
        }
        await client.statusRequest(CC.CMD_KC, tab.set, {
          payload,
          label: `写入${tab.name} K${run.start}-${run.end}`,
          timeoutMs: 3000
        });
      }
    });

    setParamState(`${tab.name} 已写入 ${indices.length} 个键位`);
    appendLine(`[PARAM] ${tab.name} 已写入 ${indices.length} 个键位`);
  } catch (error) {
    setParamState(`写入失败：${error.message || error}`, 'err');
    appendLine(`[PARAM] ${tab.name} 写入失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    refreshControls();
  }
}
