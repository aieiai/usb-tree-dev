'use strict';

/* 每层一个按钮，层数由设备上报 */
function renderLayers() {
  $kcLayers.textContent = '';
  for (let layer = 0; layer < kcLayerCount; layer++) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = `kc-layer${layer === kcLayerIndex ? ' active' : ''}`;
    btn.textContent = `Fn${layer}`;
    btn.addEventListener('click', () => selectLayer(layer));
    $kcLayers.appendChild(btn);
  }
}

/* 读取设备的映射层数与当前活动层。 */
async function queryKeymapInfo() {
  const target = findCcTarget();
  if (!target || kcBusy || sysBusy || isFirmwareActive()) return false;

  let ok = false;
  kcBusy = true;
  refreshControls();
  try {
    await ccCall(target, async client => {
      const countPayload = await client.readPayload(
        CC.CMD_KC, CC.KC_LAYER_COUNT, 4,
        { label: '映射层数', timeoutMs: 3000 }
      );
      const count = readLe32(countPayload, 0);
      if (count > 0 && count <= 32) {
        kcLayerCount = count;
      }

      const activePayload = await client.readPayload(
        CC.CMD_KC, CC.KC_ACTIVE_LAYER, 4,
        { label: '活动映射层', timeoutMs: 3000 }
      );
      const active = readLe32(activePayload, 0);
      if (active < kcLayerCount) {
        kcLayerIndex = active;
      }
    });
    ok = true;
    appendLine(`[KC] 映射层数 ${kcLayerCount}，当前活动层 Fn${kcLayerIndex}`);
  } catch (error) {
    appendLine(`[KC] 读取层信息失败：${error.message || error}`, 'warn');
  } finally {
    if (kcLayerIndex >= kcLayerCount) kcLayerIndex = 0;
    kcBusy = false;
    renderLayers();
    syncKeycaps();
    refreshControls();
  }
  return ok;
}

async function selectLayer(layer) {
  if (kcBusy || sysBusy || isFirmwareActive()) return;
  if (layer === kcLayerIndex) {
    autoReadKeymap();
    return;
  }

  const target = findCcTarget();
  if (!target) {
    appendLine('[KC] 没有找到 CC IN/OUT 端点', 'err');
    return;
  }

  let switched = false;
  kcBusy = true;
  refreshControls();
  try {
    const payload = new Uint8Array(4);
    writeLe32(payload, 0, layer);
    await ccCall(target, client =>
      client.statusRequest(CC.CMD_KC, CC.KC_SET_ACTIVE_LAYER, {
        payload,
        label: `切换活动层 Fn${layer}`,
        timeoutMs: 3000
      })
    );

    kcLayerIndex = layer;
    for (const [index, btn] of [...$kcLayers.children].entries()) {
      btn.classList.toggle('active', index === layer);
    }
    kcSelected.clear();
    appendLine(`[KC] 活动层已切换为 Fn${layer}`);
    switched = true;
  } catch (error) {
    appendLine(`[KC] 切换活动层失败：${error.message || error}`, 'err');
  } finally {
    kcBusy = false;
    syncKeycaps();
    refreshControls();
  }

  /* 等 kcBusy 释放后再读，否则 readKeycodeLayer() 会直接返回。 */
  if (switched) autoReadKeymap();
}
