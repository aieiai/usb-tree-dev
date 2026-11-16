'use strict';

/* ------------------------------------------------------------ 调试请求 */

async function writeCcFrame(target, data, label) {
  if (!device || !device.opened || !target.link.claimed) {
    throw new Error(`Interface ${target.link.number} 不可用`);
  }
  if (data.length > target.packetSize) {
    throw new Error(`CC 帧超过端点 MPS ${target.packetSize} 字节`);
  }
  const result = await device.transferOut(target.epOut.endpointNumber, data);
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

  const msg = parseCcMessage(data.subarray(RG.HEADER_SIZE));
  const isUpgradeData = msg &&
    msg.cmd === CC.CMD_UPGRADE &&
    msg.subcmd === CC.UPGRADE_DATA;
  if (!isUpgradeData) {
    appendLine(
      `[CC TX IF${target.link.number} ${endpoinTag(target.epOut)} ` +
      `${data.length}B] ${label}`
    );
  }
}

async function makeCcClient(target, { bytes = null, crc32 = 0, onProgress = null } = {}) {
  const write = (data, label) => writeCcFrame(target, data, label);
  const log = (text, className = '') => appendLine(`[CC] ${text}`, className);

  if (!target.link.claimed) {
    const claimed = await claimLink(target.link);
    if (!claimed || !target.link.claimed) {
      throw new Error(`Interface ${target.link.number} 占用失败`);
    }
  }

  if (bytes) {
    return new CcUpgradeClient({
      bytes,
      crc32,
      packetSize: target.packetSize,
      write,
      log,
      onProgress
    });
  }
  return new CcClient({ packetSize: target.packetSize, write, log });
}

/* 一次性请求：临时挂接收、发完即摘。 */
async function ccCall(target, handler) {
  const key = endpointKey(target.epIn);
  if (target.link.rxHandlers.has(key)) {
    throw new Error('CC 接收通道正在被其他功能使用');
  }

  let client = null;
  let result = null;
  let failure = null;
  const forward = data => {
    if (client) client.receive(data);
  };

  target.link.rxHandlers.set(key, forward);
  if (!target.link.reads.has(key)) {
    startRead(target.link, target.epIn);
  }

  try {
    client = await makeCcClient(target);
    result = await handler(client);
  } catch (error) {
    failure = error;
  } finally {
    if (client) client.cancel();
    if (target.link.rxHandlers.get(key) === forward) {
      target.link.rxHandlers.delete(key);
    }
  }

  if (failure) throw failure;
  return result;
}

