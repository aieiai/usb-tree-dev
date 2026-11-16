'use strict';

/* ----------------------------------------------------------- CC 客户端 */

class CcAbortError extends Error {
  constructor(message = 'CC 传输已取消') {
    super(message);
    this.name = 'CcAbortError';
  }
}

class CcClient {
  constructor({ packetSize, write, log }) {
    if (!Number.isInteger(packetSize) || packetSize < CC.MIN_FRAME_SIZE) {
      throw new Error(`CC 端点包大小无效：${packetSize}`);
    }
    this.packetSize = packetSize;
    this.write = write;
    this.log = log;
    this.responses = [];
    this.waiter = null;
    this.cancelled = false;
  }

  receive(data) {
    if (this.cancelled) return;
    const frame = parseRotGenFrame(data);
    if (!frame) {
      this.log(`忽略格式或 CRC 无效的响应：${toHex(data)}`, 'warn');
      return;
    }
    if ((frame.options & RG.OPT_REQUEST) !== 0) return;
    const msg = parseCcMessage(frame.payload);
    if (!msg) {
      this.log('忽略缺少 Rot-CC 头的响应', 'warn');
      return;
    }
    this.responses.push(msg);
    this.pump();
  }

  cancel() {
    if (this.cancelled) return;
    this.cancelled = true;
    this.settleWaiter(new CcAbortError());
  }

  settleWaiter(error, value) {
    const waiter = this.waiter;
    if (!waiter) return;
    clearTimeout(waiter.timer);
    this.waiter = null;
    if (error) waiter.reject(error);
    else waiter.resolve(value);
  }

  pump() {
    const waiter = this.waiter;
    if (!waiter) return;

    while (this.responses.length > 0) {
      const msg = this.responses.shift();
      if (msg.cmd !== waiter.cmd || msg.subcmd !== waiter.subcmd) {
        this.log(
          `忽略过期响应 cmd=0x${msg.cmd.toString(16)} ` +
          `subcmd=0x${msg.subcmd.toString(16)}`,
          'warn'
        );
        continue;
      }
      this.settleWaiter(null, msg);
      return;
    }
  }

  waitResponse(cmd, subcmd, timeoutMs, label) {
    if (this.cancelled) return Promise.reject(new CcAbortError());
    if (this.waiter) {
      return Promise.reject(new Error('CC 接收状态冲突：已有等待中的响应'));
    }

    return new Promise((resolve, reject) => {
      const waiter = { cmd, subcmd, label, resolve, reject, timer: 0 };
      this.waiter = waiter;
      waiter.timer = setTimeout(() => {
        if (this.waiter !== waiter) return;
        this.waiter = null;
        reject(new Error(`${label}超时（${timeoutMs / 1000} 秒）`));
      }, timeoutMs);
      this.pump();
    });
  }

  checkCancelled() {
    if (this.cancelled) throw new CcAbortError();
  }

  async request(cmd, subcmd, options = {}) {
    const {
      reserve = 0,
      payload = new Uint8Array(),
      label = `cmd=0x${cmd.toString(16)} subcmd=0x${subcmd.toString(16)}`,
      timeoutMs = CC_RESPONSE_TIMEOUT_MS
    } = options;

    const frame = makeRotGenFrame(packCcMessage(cmd, subcmd, reserve, payload));
    if (frame.length > this.packetSize) {
      throw new Error(`${label} 帧长 ${frame.length} 超过端点 MPS ${this.packetSize}`);
    }

    let lastError = null;
    for (let attempt = 1; attempt <= CC_MAX_RETRIES; attempt++) {
      this.checkCancelled();
      await this.write(frame, `${label}（第 ${attempt} 次）`);
      try {
        return await this.waitResponse(cmd, subcmd, timeoutMs, `${label}响应`);
      } catch (error) {
        if (this.cancelled || error instanceof CcAbortError) throw error;
        lastError = error;
        this.log(
          `${label} 失败，重试 ${attempt}/${CC_MAX_RETRIES}：` +
          `${error.message || error}`,
          'warn'
        );
      }
    }
    throw lastError || new Error(`${label} 发送失败`);
  }

  async statusRequest(cmd, subcmd, options = {}) {
    const response = await this.request(cmd, subcmd, options);
    if (response.payload.length !== 4) {
      throw new Error(`${options.label || '请求'}响应长度异常`);
    }
    const status = readLe32(response.payload, 0);
    if (status !== 0) {
      throw new Error(
        `${options.label || '请求'}失败：${ccStatusText(status)}`
      );
    }
    return response;
  }

  async readPayload(cmd, subcmd, minLength, options = {}) {
    const response = await this.request(cmd, subcmd, options);
    if (response.payload.length >= minLength) {
      return response.payload;
    }
    if (response.payload.length === 4) {
      throw new Error(
        `${options.label || '请求'}失败：` +
        `${ccStatusText(readLe32(response.payload, 0))}`
      );
    }
    throw new Error(`${options.label || '请求'}响应长度异常`);
  }
}

class CcUpgradeClient extends CcClient {
  constructor({ bytes, sessionId, packetSize, write, log, onProgress }) {
    super({ packetSize, write, log });
    this.bytes = bytes;
    this.sessionId = sessionId;
    this.maxDataSize = ccMaxDataSize(packetSize);
    if (this.maxDataSize < 1) {
      throw new Error(`端点 MPS ${packetSize} 不足以承载升级数据帧`);
    }
    this.onProgress = onProgress;
  }

  async run() {
    const startPayload = new Uint8Array(8);
    writeLe32(startPayload, 0, this.bytes.length);
    writeLe32(startPayload, 4, this.sessionId);

    this.log(
      `开始升级会话 0x${this.sessionId.toString(16).padStart(8, '0')}，` +
      `${this.bytes.length} 字节；端点 MPS=${this.packetSize} B，` +
      `单帧数据=${this.maxDataSize} B`
    );
    await this.statusRequest(CC.CMD_UPGRADE, CC.UPGRADE_START, {
      payload: startPayload,
      label: 'START',
      timeoutMs: CC_START_TIMEOUT_MS
    });

    let offset = 0;
    let seq = 0;
    while (offset < this.bytes.length) {
      this.checkCancelled();
      const end = Math.min(offset + this.maxDataSize, this.bytes.length);
      const chunk = this.bytes.subarray(offset, end);
      const payload = new Uint8Array(2 + chunk.length);
      writeLe16(payload, 0, seq);
      payload.set(chunk, 2);

      await this.statusRequest(CC.CMD_UPGRADE, CC.UPGRADE_DATA, {
        payload,
        label: `DATA seq=${seq} @ ${offset}`
      });
      offset = end;
      this.onProgress(offset, this.bytes.length);
      seq = (seq + 1) & 0xffff;
    }

    this.log('文件数据已全部确认，请求设备校验并提交');
    await this.statusRequest(CC.CMD_UPGRADE, CC.UPGRADE_END, {
      payload: new Uint8Array(),
      label: 'END',
      timeoutMs: CC_END_TIMEOUT_MS
    });
    this.onProgress(this.bytes.length, this.bytes.length);
    this.log('设备校验完成，即将重启');
  }
}

