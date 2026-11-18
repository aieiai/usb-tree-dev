'use strict';

/* ------------------------------------------------------------ 设备信息 */

function formatFwVersion64(value) {
  const major = Number((value >> 56n) & 0xffn);
  const minor = Number((value >> 46n) & 0x3ffn);
  const patch = Number((value >> 36n) & 0x3ffn);
  const stage = Number((value >> 32n) & 0x0fn);
  const hash = Number(value & 0xffffffffn);
  const stages = ['release', 'dev', 'alpha', 'beta', 'rc'];
  const stageName = stages[stage] || `stage${stage}`;
  return `v${major}.${minor}.${patch}-${stageName}+` +
    hash.toString(16).padStart(8, '0');
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
    hwVersion: payload[44],
    buildDate: text(45, 16)
  };
}

function decodeUpgradeInfo(payload) {
  const view = new DataView(
    payload.buffer, payload.byteOffset, payload.byteLength
  );
  return {
    slotAct: payload[0],
    slotTag: payload[1],
    slotCount: payload[2],
    encrypt: payload[3] !== 0,
    remap: payload[4] !== 0,
    bootOta: payload[5] !== 0,
    otpAddressMatch: payload[6] !== 0,
    otpLengthMatch: payload[7] !== 0,
    slotValid: [payload[8] !== 0, payload[9] !== 0],
    slotRetired: [payload[10] !== 0, payload[11] !== 0],
    slotVersion: [view.getUint16(12, true), view.getUint16(14, true)],
    slotSize: view.getUint32(16, true),
    imageBaseOffset: view.getUint32(20, true),
    fileBootOffset: view.getUint32(24, true),
    otpOffset: view.getUint32(28, true),
    otpSize: view.getUint32(32, true),
    otpSecImgOffset: view.getUint16(36, true),
    otpMaxImgLen: view.getUint16(38, true),
    firmwareVersion: view.getBigUint64(40, true)
  };
}

function formatSlotState(index, info) {
  if (!info.slotValid[index]) return `APP${index + 1} 无镜像`;
  const flags = [];
  if (index === info.slotAct) flags.push('运行');
  if (info.slotRetired[index]) flags.push('已退役');
  const suffix = flags.length ? ` (${flags.join('/')})` : '';
  return `APP${index + 1} SW ${info.slotVersion[index]}${suffix}`;
}

function upgradeInfoLines(info) {
  if (!info) return [];
  const slots = [];
  for (let i = 0; i < info.slotCount; i++) {
    slots.push(formatSlotState(i, info));
  }
  const match = (ok) => (ok ? '匹配' : '不匹配');
  return [
    `升级 运行 APP${info.slotAct + 1} · 目标 APP${info.slotTag + 1} · ` +
      `槽位 ${info.slotCount} × ${Math.round(info.slotSize / 1024)} KiB · ` +
      `加密 ${info.encrypt ? '是' : '否'} · remap ${info.remap ? '是' : '否'} · ` +
      `固件 ${formatFwVersion64(info.firmwareVersion)}`,
    `槽位版本 ${slots.join(' · ')}`,
    `OTP SEC_IMG_OFFSET=${info.otpSecImgOffset} ` +
      `(${Math.round(info.otpOffset / 1024)} KiB, ${match(info.otpAddressMatch)}) · ` +
      `MAX_IMG_LEN=${info.otpMaxImgLen === 0 ? '默认' : info.otpMaxImgLen} ` +
      `(${Math.round(info.otpSize / 1024)} KiB, ${match(info.otpLengthMatch)}) · ` +
      `BootOTA ${info.bootOta ? '启用' : '未启用'}`,
    `镜像 base=0x${info.imageBaseOffset.toString(16)} · ` +
      `boot head=0x${info.fileBootOffset.toString(16)}`
  ];
}

function applyCcDeviceInfo(deviceInfo, upgradeInfo) {
  const head =
    `PN ${deviceInfo.pn || '--'} · SN ${deviceInfo.sn || '--'} · ` +
    `FW ${formatFwVersion64(deviceInfo.fwVersion)} · ` +
    `VID 0x${deviceInfo.vid.toString(16).padStart(4, '0')} ` +
    `PID 0x${deviceInfo.pid.toString(16).padStart(4, '0')} · ` +
    `HW ${deviceInfo.hwVersion} · ` +
    `BUILD ${deviceInfo.buildDate || '--'}`;
  const lines = upgradeInfoLines(upgradeInfo);

  $ccDeviceInfo.textContent = [head, ...lines].join('\n');
  appendLine(`[CC INFO] ${head}`);
  for (const line of lines) {
    appendLine(`[CC INFO] ${line}`);
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
        CC.CMD_SYS, CC.SYS_INFO, 61, { label: '设备信息' }
      );
      const deviceInfo = decodeDeviceInfo(devicePayload);

      let upgradeInfo = null;
      try {
        const payload = await client.readPayload(
          CC.CMD_UPGRADE, CC.UPGRADE_INFO, CC.UPGRADE_INFO_LEN,
          { label: '升级信息' }
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

