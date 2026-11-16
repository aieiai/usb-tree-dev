'use strict';

/* --------------------------------------------------------- 端点与目标 */

function ccEndpointLimit(ep) {
  if (!ep || !['interrupt', 'bulk'].includes(ep.type)) {
    throw new Error(`CC 端点类型不受支持：${ep?.type || '未知'}`);
  }
  if (device?.speed === 'full' || device?.speed === 'low') {
    return 64;
  }
  if (device?.speed === 'high') {
    return ep.type === 'bulk' ? 512 : 1024;
  }
  /* Some WebUSB implementations omit USBDevice.speed. In that case the
   * endpoint descriptor's packetSize is the authoritative capability. */
  return CC.MAX_FRAME_SIZE;
}

function validateCcEndpointPair(link, epOut, epIn) {
  if (!epOut || !epIn) {
    throw new Error('CC 调试需要同时具备 IN 和 OUT 端点');
  }

  for (const ep of [epIn, epOut]) {
    if (!Number.isInteger(ep.packetSize) || ep.packetSize <= 0) {
      throw new Error(`${endpoinTag(ep)} 的 packetSize 无效`);
    }
    if (ep.packetSize < CC.MIN_FRAME_SIZE) {
      throw new Error(
        `${endpoinTag(ep)} 只有 ${ep.packetSize} 字节，` +
        `无法容纳 ${CC.MIN_FRAME_SIZE} 字节 Rot-Gen 帧`
      );
    }
    if ((ep.packetSize & (ep.packetSize - 1)) !== 0) {
      throw new Error(
        `${endpoinTag(ep)} 的 packetSize=${ep.packetSize} 不是 2 的幂`
      );
    }
    if (ep.packetSize > CC.MAX_FRAME_SIZE) {
      throw new Error(
        `${endpoinTag(ep)} 的 packetSize=${ep.packetSize} 超过协议上限 ` +
        `${CC.MAX_FRAME_SIZE}`
      );
    }
    const limit = ccEndpointLimit(ep);
    if (ep.packetSize > limit) {
      throw new Error(
        `${endpoinTag(ep)} 的 packetSize=${ep.packetSize} 超过当前 ` +
        `${device?.speed || '未知'} USB 速度上限 ${limit}`
      );
    }
  }

  const packetSize = Math.min(epIn.packetSize, epOut.packetSize);
  return {
    link,
    epIn,
    epOut,
    packetSize,
    maxDataSize: ccMaxDataSize(packetSize)
  };
}

function findCcTarget() {
  const link = links.find(item => item.number === 1) ||
               links.find(item => item.alt.endpoints.some(
                 ep => ep.endpointNumber === 2 &&
                       (ep.direction === 'out' || ep.direction === 'in')
               ));
  if (!link) return null;

  const epOut = link.alt.endpoints.find(
    ep => ep.direction === 'out' && ep.endpointNumber === 2
  );
  const epIn = link.alt.endpoints.find(
    ep => ep.direction === 'in' && ep.endpointNumber === 2
  );
  return epOut && epIn ? validateCcEndpointPair(link, epOut, epIn) : null;
}

function updateCcPacketInfo() {
  if (!links.length) {
    $ccPacketInfo.textContent = '等待读取设备端点';
    return;
  }
  try {
    const target = findCcTarget();
    if (!target) {
      $ccPacketInfo.textContent = '未找到 CC IN/OUT 端点';
      return;
    }
    const speed = device?.speed === 'high'
      ? 'HS'
      : (device?.speed || 'speed=unknown');
    const pair = target.epIn.packetSize === target.epOut.packetSize
      ? `MPS ${target.packetSize} B`
      : `MPS ${target.epIn.packetSize}/${target.epOut.packetSize} B`;
    $ccPacketInfo.textContent =
      `Rot-Gen v${RG.VERSION} · UI v${CC_UI_VERSION} · ${speed} · ${pair}` +
      ` · DATA ${target.maxDataSize} B`;
  } catch (error) {
    $ccPacketInfo.textContent = `CC 端点不受支持：${error.message || error}`;
  }
}

