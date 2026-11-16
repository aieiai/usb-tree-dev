'use strict';

/* ------------------------------------------------------------ 设备信息 */

function formatSwVersion(version) {
  return `${version} (0x${version.toString(16).padStart(4, '0')})`;
}

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
  const flags = payload[18];
  const slotVersion = [readLe16(payload, 8), readLe16(payload, 10)];
  return {
    protocolVersion: payload[0],
    activeSlot: payload[1],
    slotValid: [payload[2] !== 0, payload[3] !== 0],
    slotRetired: [(flags & 0x08) !== 0, (flags & 0x10) !== 0],
    slotRecordValid: [(flags & 0x20) !== 0, (flags & 0x40) !== 0],
    version: readLe16(payload, 4),
    headerLength: readLe16(payload, 6),
    slotVersion,
    slotAppVersion: [readLe16(payload, 24), readLe16(payload, 26)],
    otpSecImgOffset: readLe16(payload, 12),
    otpMaxImgLen: readLe16(payload, 14),
    configuredSlotSize256k: readLe16(payload, 16),
    remapEnabled: (flags & 0x01) !== 0,
    bootOtaEnabled: (flags & 0x02) !== 0,
    bootLayoutMatches: (flags & 0x04) !== 0,
    exipEnabled: (flags & 0x80) !== 0,
    firmwareVersion: readLe32(payload, 20)
  };
}

function applyCcDeviceInfo(deviceInfo, upgradeInfo) {
  const slots = upgradeInfo ? [0, 1].map(index => {
    const name = `APP${index + 1}`;
    const notes = [];
    if (!upgradeInfo.slotValid[index]) notes.push('槽头无效');
    if (!upgradeInfo.slotRecordValid[index]) notes.push('应用记录无效');
    if (upgradeInfo.slotRetired[index]) notes.push('已退役');
    const status = notes.length ? `（${notes.join('，')}）` : '';
    return `${name} 应用版本 ${upgradeInfo.slotAppVersion[index]}，` +
      `SW_VERSION=${formatSwVersion(upgradeInfo.slotVersion[index])}${status}`;
  }) : [];

  const bootMode = !upgradeInfo
    ? '槽位信息不可用'
    : upgradeInfo.bootOtaEnabled
      ? (upgradeInfo.bootLayoutMatches
        ? `BootROM 双槽布局匹配 (${upgradeInfo.otpSecImgOffset} x 256 KiB)`
        : `BootROM 布局不匹配 (OTP ${upgradeInfo.otpSecImgOffset} x 256 KiB / ` +
          `配置 ${upgradeInfo.configuredSlotSize256k} x 256 KiB)`)
      : 'BootROM 双槽未启用 (OTP SEC_IMG_OFFSET=0)';

  const head =
    `PN ${deviceInfo.pn || '--'} · SN ${deviceInfo.sn || '--'} · ` +
    `FW ${formatFwVersion64(deviceInfo.fwVersion)} · ` +
    `VID 0x${deviceInfo.vid.toString(16).padStart(4, '0')} ` +
    `PID 0x${deviceInfo.pid.toString(16).padStart(4, '0')} · ` +
    `HW ${deviceInfo.hwVersion}`;
  const slotsText = upgradeInfo
    ? `当前 APP${upgradeInfo.activeSlot} · ` +
      `固件版本 ${upgradeInfo.firmwareVersion} · ` +
      `SW_VERSION ${formatSwVersion(upgradeInfo.version)}`
    : '';
  $ccDeviceInfo.textContent = [head, slotsText, ...slots, bootMode]
    .filter(text => text)
    .join(' · ');

  appendLine(
    `[CC INFO] ${head}`
  );
  if (upgradeInfo) {
    appendLine(
      `[CC INFO] 当前 APP${upgradeInfo.activeSlot}，查询协议=v` +
      `${upgradeInfo.protocolVersion}，SW_VERSION=` +
      `${formatSwVersion(upgradeInfo.version)}，${slots[0]}，${slots[1]}，` +
      `${bootMode}，remap=${upgradeInfo.remapEnabled ? 1 : 0} ` +
      `exip=${upgradeInfo.exipEnabled ? 1 : 0}`
    );
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
          CC.CMD_UPGRADE, CC.UPGRADE_QUERY, 24, { label: '槽位信息' }
        );
        upgradeInfo = decodeUpgradeInfo(payload);
      } catch (error) {
        appendLine(`[CC INFO] 槽位信息查询失败：${error.message || error}`, 'warn');
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

