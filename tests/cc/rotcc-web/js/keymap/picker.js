'use strict';

let pickerCategory = KC_PICKER[0].id;

function renderPicker() {
  $pickerTabs.textContent = '';
  for (const cat of KC_PICKER) {
    const btn = document.createElement('button');
    btn.className = `picker-tab${cat.id === pickerCategory ? ' active' : ''}`;
    btn.textContent = cat.name;
    btn.addEventListener('click', () => {
      pickerCategory = cat.id;
      renderPicker();
    });
    $pickerTabs.appendChild(btn);
  }

  $pickerGroups.textContent = '';
  const cat = KC_PICKER.find(item => item.id === pickerCategory) || KC_PICKER[0];
  $pickerGroups.className = cat.board ? 'picker-board' : 'picker-groups';

  if (cat.board) {
    renderPickerBoard($pickerGroups, cat.board);
    return;
  }

  for (const [title, type, keys] of cat.groups) {
    const group = document.createElement('div');
    if (title) {
      const heading = document.createElement('h4');
      heading.className = 'picker-group-title';
      heading.textContent = title;
      group.appendChild(heading);
    }

    const wrap = document.createElement('div');
    wrap.className = 'picker-keys';
    for (const [name, value] of keys) {
      const code = KC_BUILDERS[type](value);
      const btn = document.createElement('button');
      btn.className = 'pick-key';
      btn.textContent = name;
      btn.title = `0x${code.toString(16).padStart(8, '0')}`;
      btn.addEventListener('click', () => assignKeycode(code, name));
      wrap.appendChild(btn);
    }
    group.appendChild(wrap);
    $pickerGroups.appendChild(group);
  }
}

/* 按 1/4U 列网格渲染 104 配列键码面板，三段键区列偏移固定（列 0 起算）。 */
const BOARD_BASE = { main: 0, nav: 61, num: 74 };

function renderPickerBoard(root, rows) {
  const board = document.createElement('div');
  board.className = 'board';

  for (const row of rows) {
    for (const [section, keys] of [
      ['main', row.main],
      ['nav', row.nav],
      ['num', row.num],
    ]) {
      for (const [name, usage, quarter, span, rowSpan = 1] of keys) {
        const code = KC_BUILDERS.KBD(usage);
        const label = name.replace('\n', ' ');
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'board-key';
        btn.textContent = name;
        btn.title = `${label} · 0x${code.toString(16).padStart(8, '0')}`;
        btn.style.gridColumn = `${1 + BOARD_BASE[section] + quarter} / span ${span}`;
        /* 第 1 行后插入 0.5U 间隔行 */
        btn.style.gridRow = `${row.row === 1 ? 1 : row.row + 1} / span ${rowSpan}`;
        btn.addEventListener('click', () => assignKeycode(code, label));
        board.appendChild(btn);
      }
    }
  }

  root.appendChild(board);
  syncBoardSize();
}

/* 1U 键帽取正方形：按面板可用宽高（23U × 6.5U，含 F 区间隔）算出最大边长，过小时保底并允许滚动。 */
function syncBoardSize() {
  const board = $pickerGroups;
  if (!board.classList.contains('picker-board')) return;
  const style = getComputedStyle(board);
  const width = board.clientWidth -
    parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
  const height = board.clientHeight -
    parseFloat(style.paddingTop) - parseFloat(style.paddingBottom);
  if (width <= 0 || height <= 0) return;

  const unit = Math.min(55, Math.max(30, Math.floor(Math.min(width / 23, height / 6.5))));
  if (board.dataset.unit === String(unit)) return;
  board.dataset.unit = String(unit);
  board.style.setProperty('--key', `${unit}px`);
  board.style.setProperty('--q', `${unit / 4}px`);
  board.style.setProperty('--sg', `${unit / 2}px`);
}

