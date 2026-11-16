'use strict';

/* ------------------------------------------------------------ 设备信息 */

function formatFwVersion64(value) {
  const major = Number((value >> 56n) & 0xffn);
  const minor = Number((value >> 46n) & 0x3ffn);
  const patch = Number((value >> 36n) & 0x3ffn);
  return `v${major}.${minor}.${patch}`;
}

function decodeDeviceInfo(payload) {
  const view = new DataView(
    payload.buffer, payload.byteOffset, payload.byteLength
  );
  const text = (offset, length) => {
    let out = '';
    for (let i = 0; i < length; i++) {
      const byte = payload[offset + i];
      if (byte === 0) break;
      out += String.fromCharCode(byte);
    }
    return out;
  };

  return {
    vid: view.getUint16(0, true),
    pid: view.getUint16(2, true),
    pn: text(4, 16),
    sn: text(20, 16),
    fwVersion: view.getBigUint64(36, true),
    hwVersion: payload[44]
  };
}

function decodeUpgradeInfo(payload) {
  return {
    slotCur: payload[0],
    slotMax: payload[1],
    slotSize: readLe32(payload, 2),
    encrypt: payload[6] !== 0
  };
}

function applyCcDeviceInfo(deviceInfo, upgradeInfo) {
  const head =
    `PN ${deviceInfo.pn || '--'} · SN ${deviceInfo.sn || '--'} · ` +
    `FW ${formatFwVersion64(deviceInfo.fwVersion)} · ` +
    `VID 0x${deviceInfo.vid.toString(16).padStart(4, '0')} ` +
    `PID 0x${deviceInfo.pid.toString(16).padStart(4, '0')} · ` +
    `HW ${deviceInfo.hwVersion}`;
  const slotsText = upgradeInfo
    ? `当前 APP${upgradeInfo.slotCur + 1} · ` +
      `槽位 ${upgradeInfo.slotMax} × ` +
      `${Math.round(upgradeInfo.slotSize / 1024)} KiB · ` +
      `加密 ${upgradeInfo.encrypt ? '是' : '否'}`
    : '';

  $ccDeviceInfo.textContent = [head, slotsText]
    .filter(text => text)
    .join(' · ');

  appendLine(`[CC INFO] ${head}`);
  if (slotsText) {
    appendLine(`[CC INFO] ${slotsText}`);
  }
}

async function queryCcDeviceInfo({ silent = false } = {}) {
  if (ccQueryPromise) return ccQueryPromise;
  if (!device || !device.opened) {
    throw new Error('请先连接设备');
  }
  if (isFirmwareActive()) {
    throw new Error('固件传输期间不能查询设备信息');
  }

  ccQueryPromise = (async () => {
    const target = findCcTarget();
    if (!target) throw new Error('没有找到 CC IN/OUT 端点');

    return ccCall(target, async client => {
      const devicePayload = await client.readPayload(
        CC.CMD_SYS, CC.SYS_INFO, 45, { label: '设备信息' }
      );
      const deviceInfo = decodeDeviceInfo(devicePayload);

      let upgradeInfo = null;
      try {
        const payload = await client.readPayload(
          CC.CMD_UPGRADE, CC.UPGRADE_INFO, 7, { label: '升级信息' }
        );
        upgradeInfo = decodeUpgradeInfo(payload);
      } catch (error) {
        appendLine(`[CC INFO] 升级信息查询失败：${error.message || error}`, 'warn');
      }

      applyCcDeviceInfo(deviceInfo, upgradeInfo);
      return { deviceInfo, upgradeInfo };
    });
  })();
  refreshControls();

  try {
    return await ccQueryPromise;
  } catch (error) {
    $ccDeviceInfo.textContent = `查询失败：${error.message || error}`;
    if (!silent) {
      appendLine(`[CC INFO 失败] ${error.message || error}`, 'err');
    }
    throw error;
  } finally {
    ccQueryPromise = null;
    refreshControls();
  }
}

