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

/*
 * CC 接收分发：通知直接交给全局处理函数，其余响应交给当前客户端。
 * 分发器常驻，避免 sensor 上报在两次请求之间被当成原始数据刷屏。
 */
let ccRxClient = null;

function ccRxDispatch(data) {
  const frame = parseRotGenFrame(data);
  if (!frame || (frame.options & RG.OPT_REQUEST) !== 0) return;
  const msg = parseCcMessage(frame.payload);
  if (!msg) return;

  if (msg.cmd === CC.CMD_NOTIFY) {
    ccDispatchNotification(msg);
    return;
  }
  if (ccRxClient) ccRxClient.receive(data);
}

function ccEnsureRx(target) {
  const key = endpointKey(target.epIn);
  if (!target.link.rxHandlers.has(key)) {
    target.link.rxHandlers.set(key, ccRxDispatch);
  }
  if (!target.link.reads.has(key)) {
    startRead(target.link, target.epIn);
  }
}

function ccAttachRxClient(target, client) {
  ccEnsureRx(target);
  ccRxClient = client;
}

function ccDetachRxClient(client) {
  if (ccRxClient === client) ccRxClient = null;
}

/* 一次性请求：共用常驻分发器，结束只摘客户端，不摘分发器。 */
async function ccCall(target, handler) {
  if (ccRxClient) {
    throw new Error('CC 接收通道正在被其他功能使用');
  }

  let client = null;
  let result = null;
  let failure = null;

  try {
    client = await makeCcClient(target);
    ccAttachRxClient(target, client);
    result = await handler(client);
  } catch (error) {
    failure = error;
  } finally {
    if (client) {
      ccDetachRxClient(client);
      client.cancel();
    }
  }

  if (failure) throw failure;
  return result;
}

