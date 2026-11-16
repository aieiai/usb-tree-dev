'use strict';

/* ---------------------------------------------------------- 接口与收发 */

function makeButton(text, className = '') {
  const button = document.createElement('button');
  button.textContent = text;
  if (className) button.className = className;
  return button;
}

function renderInterfaces() {
  $interfaces.textContent = '';
  if (!links.length) {
    const empty = document.createElement('div');
    empty.className = 'interface';
    empty.innerHTML = '<div class="interface-head"><strong>尚无接口数据</strong></div>';
    $interfaces.appendChild(empty);
    return;
  }

  /* 已占用的接口排在前面，其余按接口号顺序。 */
  const ordered = [...links].sort(
    (a, b) => (b.claimed ? 1 : 0) - (a.claimed ? 1 : 0) || a.number - b.number
  );

  for (const link of ordered) {
    link.ui.clear();
    const hint = INTERFACE_HINTS[link.number] || {};
    const card = document.createElement('section');
    card.className = 'interface';

    const head = document.createElement('div');
    head.className = 'interface-head';
    const title = document.createElement('strong');
    title.textContent = `Interface ${link.number} · ${hint.name || '未命名接口'}`;
    head.appendChild(title);

    const cls = document.createElement('span');
    cls.className = 'grow';
    cls.textContent = `class=0x${link.interfaceClass.toString(16).padStart(2, '0')}`;
    cls.style.color = '#8b98a5';
    head.appendChild(cls);

    const claimButton = makeButton(link.claimed ? '释放接口' : '占用接口',
                                   link.claimed ? 'secondary' : '');
    claimButton.disabled = !device || !device.opened || isFirmwareActive();
    claimButton.addEventListener('click', async () => {
      if (isFirmwareActive()) return;
      if (link.claimed) await releaseLink(link);
      else await claimLink(link);
      refreshControls();
    });
    head.appendChild(claimButton);

    const claimState = document.createElement('span');
    claimState.className = `claim ${link.claimed ? 'ok' : ''}`;
    claimState.textContent = link.claimed ? '已占用' : '未占用';
    head.appendChild(claimState);
    card.appendChild(head);

    link.claimButton = claimButton;
    link.claimState = claimState;

    if (hint.note) {
      const hintNode = document.createElement('div');
      hintNode.className = 'hint';
      hintNode.textContent = hint.note;
      card.appendChild(hintNode);
    }

    const endpoints = document.createElement('div');
    endpoints.className = 'endpoints';
    if (!link.alt.endpoints.length) {
      const none = document.createElement('div');
      none.className = 'hint';
      none.textContent = '该接口没有端点';
      endpoints.appendChild(none);
    }

    for (const ep of link.alt.endpoints) {
      const row = document.createElement('div');
      row.className = 'endpoint';

      const line = document.createElement('div');
      line.className = 'endpoint-line';
      const name = document.createElement('span');
      name.className = 'endpoint-name';
      name.innerHTML = `<b>${endpoinTag(ep)}</b> · ` +
                       `${endpointType(ep.type)} · ${ep.packetSize} B`;
      line.appendChild(name);

      if (ep.direction === 'in') {
        const state = document.createElement('span');
        state.className = 'read-state';
        state.textContent = link.reads.has(endpointKey(ep)) ? '监听中' : '未监听';
        const start = makeButton('开始监听', 'secondary');
        start.disabled = !link.claimed || link.reads.has(endpointKey(ep));
        start.addEventListener('click', () => {
          startRead(link, ep);
          refreshControls();
        });
        line.appendChild(start);
        line.appendChild(state);
        link.ui.set(endpointKey(ep), { state, start });
      } else {
        const note = document.createElement('span');
        note.className = 'read-state';
        note.textContent = link.number === 3 ? '日志只读' : '可发送';
        line.appendChild(note);
        link.ui.set(endpointKey(ep), { note });
      }
      row.appendChild(line);

      if (ep.direction === 'out' && link.number !== 3) {
        const sendRow = document.createElement('div');
        sendRow.className = 'send-row';
        const input = document.createElement('input');
        input.placeholder = $inputMode.value === 'hex'
          ? '例如：01 02 0a ff'
          : '输入文本，按回车或点击发送';
        const send = makeButton('发送');
        send.disabled = !link.claimed || isFirmwareActive();
        input.disabled = isFirmwareActive();
        send.addEventListener('click', () => sendPacket(link, ep, input));
        input.addEventListener('keydown', event => {
          if (event.key === 'Enter') sendPacket(link, ep, input);
        });
        sendRow.appendChild(input);
        sendRow.appendChild(send);
        row.appendChild(sendRow);
        link.ui.get(endpointKey(ep)).send = send;
        link.ui.get(endpointKey(ep)).input = input;
      }

      endpoints.appendChild(row);
    }
    card.appendChild(endpoints);
    $interfaces.appendChild(card);
  }
}


function destroyLinks() {
  session++;
  for (const link of links) {
    for (const state of link.reads.values()) state.active = false;
  }
  links = [];
  kcValues.clear();
  kcSelected = null;
  renderInterfaces();
  updateCcPacketInfo();
}

async function connect() {
  if (!navigator.usb) {
    appendLine('[错误] 当前浏览器不支持 WebUSB，请使用 localhost 或 HTTPS 下的 Chrome/Edge。', 'err');
    return;
  }

  try {
    if (device) await disconnect();
    device = await navigator.usb.requestDevice({
      filters: [{ vendorId: VID, productId: PID }]
    });
    await device.open();
    if (device.configuration === null) await device.selectConfiguration(1);

    const token = ++session;
    links = device.configuration.interfaces.map(interfaceObject => {
      const alt = interfaceObject.alternate;
      return {
        number: interfaceObject.interfaceNumber,
        interfaceClass: alt.interfaceClass,
        interfaceObject,
        alt,
        claimed: false,
        reads: new Map(),
        rxHandlers: new Map(),
        ui: new Map(),
        token
      };
    });
    updateCcPacketInfo();
    renderInterfaces();

    $status.innerHTML = `已连接：<b>${device.productName}</b> · ` +
      `configuration ${device.configuration.configurationValue} · ` +
      `${links.length} interfaces`;
    btnConnect.disabled = true;
    btnDisconnect.disabled = false;

    appendLine(`设备已打开：${device.manufacturer || ''} ${device.productName || ''}`);
    for (const link of links) {
      const endpoints = link.alt.endpoints
        .map(ep => `${endpoinTag(ep)} ${ep.packetSize}B`)
        .join(', ') || '无端点';
      appendLine(`Interface ${link.number}: class=0x${link.interfaceClass.toString(16).padStart(2, '0')}, ${endpoints}`);
    }

    for (const link of links) {
      const auto = Boolean(INTERFACE_HINTS[link.number]?.auto);
      if (auto || $chkAll.checked) await claimLink(link);
    }
    /* 连接后依次查询设备信息、握手、按键数量，再读取当前层键码 */
    (async () => {
      try {
        await queryCcDeviceInfo({ silent: true });
      } catch (error) {
        /* 设备信息失败不影响后续命令 */
      }
      try {
        await queryHandshake({ silent: true });
      } catch (error) {
        /* 旧固件可能不支持新协议 */
      }
      try {
        await queryKeyCount({ silent: true });
      } catch (error) {
        /* 按键数量仅用于展示/校验 */
      }
      await queryKeymapInfo();
      autoReadKeymap();
    })();
    refreshControls();
  } catch (error) {
    appendLine(`[连接失败] ${error.message || error}`, 'err');
    if (device && device.opened) await device.close().catch(() => {});
    device = null;
    destroyLinks();
    $status.textContent = '连接失败。';
    btnConnect.disabled = false;
    btnDisconnect.disabled = true;
    refreshControls();
  }
}

async function claimLink(link) {
  if (!device || !device.opened || link.claimed) return true;
  try {
    await device.claimInterface(link.number);
    link.claimed = true;
    appendLine(`Interface ${link.number} claim 成功`, '');
    for (const ep of link.alt.endpoints) {
      if (ep.direction === 'in') startRead(link, ep);
    }
    renderInterfaces();
    refreshControls();
    return true;
  } catch (error) {
    appendLine(`Interface ${link.number} claim 失败：${error.message || error}`, 'err');
    return false;
  }
}

async function releaseLink(link) {
  if (!link.claimed) return;
  for (const state of link.reads.values()) state.active = false;
  link.reads.clear();
  try {
    await device.releaseInterface(link.number);
    appendLine(`Interface ${link.number} 已释放`);
  } catch (error) {
    appendLine(`Interface ${link.number} 释放失败：${error.message || error}`, 'err');
  }
  link.claimed = false;
  renderInterfaces();
  refreshControls();
}

async function startRead(link, ep) {
  const key = endpointKey(ep);
  if (!link.claimed || link.reads.has(key)) return;
  const state = { active: true, count: 0, bytes: 0 };
  link.reads.set(key, state);
  refreshControls();

  while (state.active && device && device.opened && link.claimed) {
    try {
      const result = await device.transferIn(ep.endpointNumber, ep.packetSize);
      if (!state.active) break;
      if (result.status !== 'ok' || !result.data || result.data.byteLength === 0) continue;
      state.count++;
      state.bytes += result.data.byteLength;
      rxBytes += result.data.byteLength;
      $traffic.textContent = `RX ${rxBytes} B · TX ${txBytes} B`;
      const ui = link.ui.get(key);
      if (ui) ui.state.textContent = `监听中 · ${state.count} 包 / ${state.bytes} B`;
      const rxHandler = link.rxHandlers.get(key);
      if (rxHandler) {
        try {
          await rxHandler(result.data);
        } catch (handlerError) {
          appendLine(`[CC RX 处理失败] ${handlerError.message || handlerError}`,
                     'err');
        }
      } else {
        appendLine(`[RX IF${link.number} ${endpoinTag(ep)} ` +
                   `${result.data.byteLength}B] ${formatData(result.data)}`);
      }
    } catch (error) {
      if (state.active) {
        appendLine(`[RX 错误 IF${link.number} ${endpoinTag(ep)}] ` +
                   `${error.message || error}`, 'err');
      }
      break;
    }
  }

  link.reads.delete(key);
  refreshControls();
}

async function sendPacket(link, ep, input) {
  if (!device || !device.opened) return;
  if (isFirmwareActive()) {
    appendLine('[发送失败] 固件升级期间不能发送手动数据', 'warn');
    return;
  }
  if (!link.claimed) {
    appendLine(`[发送失败] Interface ${link.number} 未占用`, 'err');
    return;
  }

  try {
    const data = encodeInput(input.value);
    if (data.length > ep.packetSize) {
      throw new Error(`单包最多 ${ep.packetSize} 字节，当前 ${data.length} 字节`);
    }
    const result = await device.transferOut(ep.endpointNumber, data);
    if (result.status !== 'ok') {
      throw new Error(`transferOut status=${result.status}`);
    }
    if (result.bytesWritten !== data.length) {
      throw new Error(
        `transferOut 只提交了 ${result.bytesWritten}/${data.length} 字节`
      );
    }
    txBytes += data.length;
    $traffic.textContent = `RX ${rxBytes} B · TX ${txBytes} B`;
    appendLine(`[TX IF${link.number} ${endpoinTag(ep)} ${data.length}B] ` +
               formatData(data));
    input.value = '';
  } catch (error) {
    appendLine(`[发送失败 IF${link.number} ${endpoinTag(ep)}] ` +
               `${error.message || error}`, 'err');
  }
}

async function disconnect() {
  btnConnect.disabled = true;
  btnDisconnect.disabled = true;
  if (firmwareSession) firmwareSession.cancel();
  for (const link of links) {
    for (const state of link.reads.values()) state.active = false;
  }
  if (device && device.opened) {
    for (const link of links) {
      if (link.claimed) {
        await device.releaseInterface(link.number).catch(() => {});
      }
    }
    await device.close().catch(() => {});
  }
  device = null;
  destroyLinks();
  $status.textContent = '已断开。';
  btnConnect.disabled = false;
  btnDisconnect.disabled = true;
  refreshControls();
}

