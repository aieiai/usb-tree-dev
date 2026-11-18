'use strict';

/*
 * Rot-CC 上位机协议自测。
 * 运行：node tests/cc/rotcc-web/tests/protocol_test.js
 *
 * 用 JS 模拟设备端，覆盖协议文档中的全部命令：
 * 握手、设备信息、重启、重置、按键数量、升级信息/请求/数据、
 * 键码读写、层数、活动层。
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const zlib = require('zlib');

const WEB_ROOT = path.resolve(__dirname, '..');
const PROJECT_ROOT = path.resolve(WEB_ROOT, '..', '..', '..');

/* ------------------------------------------------------------ 加载上位机 */

const sandbox = {
  console,
  setTimeout,
  clearTimeout,
  CC_RESPONSE_TIMEOUT_MS: 200,
  CC_START_TIMEOUT_MS: 1000,
  CC_END_TIMEOUT_MS: 1000,
  CC_MAX_RETRIES: 2,
  asUint8Array(data) {
    if (data instanceof Uint8Array) return data;
    if (ArrayBuffer.isView(data)) {
      return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
    }
    return new Uint8Array(data);
  }
};
const context = vm.createContext(sandbox);

for (const file of [
  'js/protocol/rotgen.js',
  'js/protocol/rotcc.js',
  'js/cc/client.js',
  'js/cc/device-info.js'
]) {
  vm.runInContext(fs.readFileSync(path.join(WEB_ROOT, file), 'utf8'), context, {
    filename: file
  });
}

const api = vm.runInContext(
  `({
    CC, RG, CcClient, CcUpgradeClient,
    makeRotGenFrame, parseRotGenFrame, parseCcMessage,
    ccCrc32, ccMaxDataSize, ccStatusText,
    readLe16, readLe32, writeLe16, writeLe32,
    rgCrc8, rgCrc16Update,
    decodeDeviceInfo, decodeUpgradeInfo
  })`,
  context
);

const CC = api.CC;
const RG = api.RG;
const CC_PROTOCOL_VERSION = 1;

let passed = 0;
function test(name, fn) {
  return Promise.resolve()
    .then(fn)
    .then(() => {
      passed++;
      console.log(`  PASS  ${name}`);
    })
    .catch(error => {
      console.error(`  FAIL  ${name}`);
      throw error;
    });
}

/* ------------------------------------------------------------ 设备端模拟 */

function writeString(target, offset, text) {
  for (let i = 0; i < text.length; i++) target[offset + i] = text.charCodeAt(i);
}

function deviceFrame(payload) {
  const frame = new Uint8Array(RG.HEADER_SIZE + payload.length);
  frame[0] = RG.SOF;
  frame[1] = RG.VERSION;
  frame[2] = 0;
  frame[3] = RG.ADDR_KEYBOARD;
  frame[4] = RG.ADDR_CC;
  api.writeLe16(frame, 5, payload.length);
  api.writeLe16(frame, 7, 0);
  frame[9] = api.rgCrc8(frame.subarray(0, 9));
  frame.set(payload, RG.HEADER_SIZE);

  let crc = api.rgCrc16Update(0, frame.subarray(0, 10));
  crc = api.rgCrc16Update(crc, frame.subarray(RG.HEADER_SIZE));
  api.writeLe16(frame, 10, crc);
  return frame;
}

function ccPayload(cmd, subcmd, reserve, payload) {
  const out = new Uint8Array(CC.HEADER_SIZE + payload.length);
  out[0] = cmd;
  out[1] = subcmd;
  api.writeLe16(out, 2, reserve);
  out.set(payload, CC.HEADER_SIZE);
  return out;
}

class MockDevice {
  constructor() {
    this.protocolVersion = CC_PROTOCOL_VERSION;
    this.keyCount = 42;
    this.layerCount = 4;
    this.activeLayer = 0;
    this.keymap = Array.from({ length: 4 }, () => new Array(42).fill(0));
    this.resetRequests = [];
    this.rebooted = false;
    this.upgrade = null;
    this.dropNextDataResponse = false;
  }

  defaults() {
    return Array.from({ length: 4 }, () => new Array(42).fill(0));
  }

  deviceInfo() {
    const payload = new Uint8Array(45);
    const view = new DataView(payload.buffer);
    view.setUint16(0, 0x7989, true);
    view.setUint16(2, 0x0001, true);
    writeString(payload, 4, 'RAY-40');
    writeString(payload, 20, 'SN0001');
    const version = (1n << 56n) | (1n << 46n) | 0x12345678n;
    view.setBigUint64(36, version, true);
    payload[44] = 1;
    return payload;
  }

  upgradeInfo() {
    const payload = new Uint8Array(7);
    payload[0] = 0;
    payload[1] = 2;
    api.writeLe32(payload, 2, 256 * 1024);
    payload[6] = 0;
    return payload;
  }

  handle(frameBytes) {
    const frame = api.parseRotGenFrame(frameBytes);
    assert(frame, '设备收到无法解析的帧');
    assert(frame.options & 0x08, '请求帧必须带 request 位');
    const msg = api.parseCcMessage(frame.payload);
    assert(msg, '设备收到无法解析的 CC 消息');

    const reply = payload => deviceFrame(
      ccPayload(msg.cmd, msg.subcmd, msg.reserve, payload)
    );
    const status = code => {
      const payload = new Uint8Array(4);
      api.writeLe32(payload, 0, code >>> 0);
      return reply(payload);
    };
    const u32 = value => {
      const payload = new Uint8Array(4);
      api.writeLe32(payload, 0, value >>> 0);
      return reply(payload);
    };

    if (msg.cmd === CC.CMD_SYS) {
      switch (msg.subcmd) {
      case CC.SYS_HANDSHAKE:
        return u32(this.protocolVersion);
      case CC.SYS_INFO:
        return reply(this.deviceInfo());
      case CC.SYS_REBOOT:
        this.rebooted = true;
        return status(0);
      case CC.SYS_RESET: {
        const options = api.readLe32(msg.payload, 0);
        this.resetRequests.push(options >>> 0);
        if ((options & 0x1) !== 0) {
          this.keymap = this.defaults();
          this.activeLayer = 0;
        }
        return status(0);
      }
      case CC.SYS_KEY_COUNT:
        return u32(this.keyCount);
      default:
        return status(4); /* rotsta_not_supported */
      }
    }

    if (msg.cmd === CC.CMD_KC) {
      const layer = msg.reserve >> 8;
      const key = msg.reserve & 0xff;
      switch (msg.subcmd) {
      case CC.KC_GET:
        return u32(this.keymap[layer][key]);
      case CC.KC_SET:
        this.keymap[layer][key] = api.readLe32(msg.payload, 0);
        return status(0);
      case CC.KC_LAYER_COUNT:
        return u32(this.layerCount);
      case CC.KC_ACTIVE_LAYER:
        return u32(this.activeLayer);
      case CC.KC_SET_ACTIVE_LAYER: {
        const value = api.readLe32(msg.payload, 0);
        if (value >= this.layerCount) return status(3); /* invalid argument */
        this.activeLayer = value;
        return status(0);
      }
      default:
        return status(4);
      }
    }

    if (msg.cmd === CC.CMD_UPGRADE) {
      switch (msg.subcmd) {
      case CC.UPGRADE_INFO:
        return reply(this.upgradeInfo());
      case CC.UPGRADE_REQUEST: {
        const reqTy = api.readLe32(msg.payload, 0);
        if (reqTy === CC.UPGRADE_REQ_START) {
          this.upgrade = {
            size: api.readLe32(msg.payload, 4),
            crc32: api.readLe32(msg.payload, 8),
            data: [],
            finished: false
          };
          return status(0);
        }
        if (reqTy === CC.UPGRADE_REQ_STOP) {
          this.upgrade = null;
          return status(0);
        }
        if (reqTy === CC.UPGRADE_REQ_FINISH) {
          if (!this.upgrade) return status(7); /* incomplete */
          const size = this.upgrade.data.reduce((sum, c) => sum + c.length, 0);
          const bytes = new Uint8Array(size);
          let offset = 0;
          for (const chunk of this.upgrade.data) {
            bytes.set(chunk, offset);
            offset += chunk.length;
          }
          const crc = api.ccCrc32(bytes);
          const ok = this.upgrade.size === size && this.upgrade.crc32 === crc;
          this.upgrade.finished = ok;
          return status(ok ? 0 : 8); /* checksum mismatch */
        }
        return status(3);
      }
      case CC.UPGRADE_DATA: {
        if (!this.upgrade) return status(7);
        this.upgrade.data.push(new Uint8Array(msg.payload));
        if (this.dropNextDataResponse) {
          this.dropNextDataResponse = false;
          return null;
        }
        return status(0);
      }
      default:
        return status(4);
      }
    }

    return status(4);
  }
}

function makeClient(device) {
  let client = null;
  const log = [];
  const write = (frame) => {
    const reply = device.handle(frame);
    if (reply) client.receive(reply);
    return Promise.resolve();
  };
  client = new api.CcClient({
    packetSize: 64,
    write,
    log: (text, kind) => log.push([kind, text])
  });
  return { client, log };
}

function makeUpgradeClient(device, bytes, onProgress = () => {}) {
  let client = null;
  const log = [];
  const write = (frame) => {
    const reply = device.handle(frame);
    if (reply) client.receive(reply);
    return Promise.resolve();
  };
  client = new api.CcUpgradeClient({
    bytes,
    crc32: api.ccCrc32(bytes),
    packetSize: 64,
    write,
    log: (text, kind) => log.push([kind, text]),
    onProgress
  });
  return { client, log };
}

/* ------------------------------------------------------------------ 用例 */

async function main() {
  console.log('Rot-CC 协议自测');

  await test('固件头文件与上位机命令码一致', () => {
    const header = fs.readFileSync(
      path.join(PROJECT_ROOT, 'rot', 'proto', 'rotcc_proto.h'), 'utf8'
    );
    const enums = {};
    for (const match of header.matchAll(
      /\b(ROTCC_[A-Z0-9_]+)\s*=\s*(0x[0-9a-fA-F]+|\d+)/g
    )) {
      enums[match[1]] = Number(match[2]);
    }
    assert(Object.keys(enums).length > 15, '未解析到协议枚举');
    for (const [name, value] of Object.entries(enums)) {
      if (name.startsWith('ROTCC_RESET_')) continue;
      const key = name.slice('ROTCC_'.length);
      assert.strictEqual(
        CC[key], value,
        `${name}=${value} 与 CC.${key}=${CC[key]} 不一致`
      );
    }

    const defines = {};
    for (const match of header.matchAll(
      /#define\s+(ROTCC_[A-Z0-9_]+)\s+\(?(\d+)/g
    )) {
      defines[match[1]] = Number(match[2]);
    }
    assert.strictEqual(CC.HEADER_SIZE, defines.ROTCC_MSG_HEADER_SIZE);
    assert.strictEqual(defines.ROTCC_UPGRADE_INFO_LEN, 7);
    assert.strictEqual(defines.ROTCC_UPGRADE_REQUEST_LEN, 14);
  });

  await test('固件覆盖协议中的全部命令', () => {
    const source = fs.readFileSync(
      path.join(PROJECT_ROOT, 'rot', 'proto', 'rotcc_proto.c'), 'utf8'
    );
    const handlers = [
      'ROTCC_SYS_HANDSHAKE', 'ROTCC_SYS_INFO', 'ROTCC_SYS_REBOOT',
      'ROTCC_SYS_RESET', 'ROTCC_SYS_KEY_COUNT',
      'ROTCC_UPGRADE_INFO', 'ROTCC_UPGRADE_REQUEST', 'ROTCC_UPGRADE_DATA',
      'ROTCC_UPGRADE_REQ_START', 'ROTCC_UPGRADE_REQ_STOP',
      'ROTCC_UPGRADE_REQ_FINISH',
      'ROTCC_KC_GET', 'ROTCC_KC_SET', 'ROTCC_KC_LAYER_COUNT',
      'ROTCC_KC_ACTIVE_LAYER', 'ROTCC_KC_SET_ACTIVE_LAYER'
    ];
    for (const name of handlers) {
      const inTable = new RegExp(`\\{[^\\n]*\\b${name}\\b`).test(source);
      const inCase = source.includes(`case ${name}:`);
      assert(
        inTable || inCase,
        `固件缺少 ${name} 的分发处理`
      );
    }
  });

  await test('CRC-32 与 zlib 一致', () => {
    const sample = Buffer.from('123456789');
    assert.strictEqual(api.ccCrc32(sample), zlib.crc32(sample));
    const random = Buffer.from(
      Array.from({ length: 512 }, (_, i) => (i * 37 + 11) & 0xff)
    );
    assert.strictEqual(api.ccCrc32(random), zlib.crc32(random));
  });

  await test('Rot-Gen 帧自收自发', () => {
    const payload = ccPayload(CC.CMD_SYS, CC.SYS_HANDSHAKE, 0, new Uint8Array());
    const frame = api.makeRotGenFrame(payload);
    const parsed = api.parseRotGenFrame(frame);
    assert(parsed);
    const msg = api.parseCcMessage(parsed.payload);
    assert.strictEqual(msg.cmd, CC.CMD_SYS);
    assert.strictEqual(msg.subcmd, CC.SYS_HANDSHAKE);
  });

  const device = new MockDevice();
  const { client } = makeClient(device);

  await test('设备握手返回协议版本', async () => {
    const payload = await client.readPayload(CC.CMD_SYS, CC.SYS_HANDSHAKE, 4, {
      label: '握手'
    });
    assert.strictEqual(api.readLe32(payload, 0), device.protocolVersion);
  });

  await test('获取设备信息', async () => {
    const payload = await client.readPayload(CC.CMD_SYS, CC.SYS_INFO, 45, {
      label: '设备信息'
    });
    const info = api.decodeDeviceInfo(payload);
    assert.strictEqual(info.vid, 0x7989);
    assert.strictEqual(info.pid, 0x0001);
    assert.strictEqual(info.pn, 'RAY-40');
    assert.strictEqual(info.sn, 'SN0001');
    assert.strictEqual(info.fwVersion, (1n << 56n) | (1n << 46n) | 0x12345678n);
    assert.strictEqual(info.hwVersion, 1);
  });

  await test('获取按键数量', async () => {
    const payload = await client.readPayload(CC.CMD_SYS, CC.SYS_KEY_COUNT, 4, {
      label: '按键数量'
    });
    assert.strictEqual(api.readLe32(payload, 0), 42);
  });

  await test('读取/设置键码与层号', async () => {
    const reserve = (2 << 8) | 7;
    const code = new Uint8Array(4);
    api.writeLe32(code, 0, 0x0f000001);
    await client.statusRequest(CC.CMD_KC, CC.KC_SET, {
      reserve,
      payload: code,
      label: '写键码'
    });
    const payload = await client.readPayload(CC.CMD_KC, CC.KC_GET, 4, {
      reserve,
      label: '读键码'
    });
    assert.strictEqual(api.readLe32(payload, 0), 0x0f000001);
  });

  await test('层数与活动层读写', async () => {
    const count = await client.readPayload(CC.CMD_KC, CC.KC_LAYER_COUNT, 4);
    assert.strictEqual(api.readLe32(count, 0), 4);

    const payload = new Uint8Array(4);
    api.writeLe32(payload, 0, 2);
    await client.statusRequest(CC.CMD_KC, CC.KC_SET_ACTIVE_LAYER, {
      payload,
      label: '设置活动层'
    });
    const active = await client.readPayload(CC.CMD_KC, CC.KC_ACTIVE_LAYER, 4);
    assert.strictEqual(api.readLe32(active, 0), 2);
  });

  await test('重置改键并恢复活动层', async () => {
    const payload = new Uint8Array(4);
    api.writeLe32(payload, 0, 0x1);
    await client.statusRequest(CC.CMD_SYS, CC.SYS_RESET, {
      payload,
      label: '恢复改键'
    });
    assert.deepStrictEqual(device.resetRequests, [0x1]);
    assert.strictEqual(device.activeLayer, 0);
    assert.deepStrictEqual(device.keymap, device.defaults());
  });

  await test('恢复出厂下发 0xFFFFFFFF', async () => {
    const payload = new Uint8Array(4);
    api.writeLe32(payload, 0, 0xffffffff);
    await client.statusRequest(CC.CMD_SYS, CC.SYS_RESET, {
      payload,
      label: '恢复出厂'
    });
    assert.strictEqual(device.resetRequests[1], 0xffffffff);
  });

  await test('重启命令', async () => {
    await client.statusRequest(CC.CMD_SYS, CC.SYS_REBOOT, { label: '重启' });
    assert.strictEqual(device.rebooted, true);
  });

  await test('升级信息解码', async () => {
    const payload = await client.readPayload(CC.CMD_UPGRADE, CC.UPGRADE_INFO, 7, {
      label: '升级信息'
    });
    const info = api.decodeUpgradeInfo(payload);
    assert.strictEqual(info.slotCur, 0);
    assert.strictEqual(info.slotMax, 2);
    assert.strictEqual(info.slotSize, 256 * 1024);
    assert.strictEqual(info.encrypt, false);
  });

  await test('完整升级流程（START/DATA/FINISH）', async () => {
    const target = new MockDevice();
    const bytes = new Uint8Array(5000);
    for (let i = 0; i < bytes.length; i++) bytes[i] = (i * 131 + 7) & 0xff;
    let progress = 0;
    const { client: up } = makeUpgradeClient(target, bytes, done => {
      progress = done;
    });
    await up.run();

    assert(target.upgrade, '设备未建立升级会话');
    assert.strictEqual(target.upgrade.finished, true, '设备校验未通过');
    assert.strictEqual(target.upgrade.size, bytes.length);
    const received = new Uint8Array(target.upgrade.data.reduce(
      (sum, chunk) => sum + chunk.length, 0
    ));
    let offset = 0;
    for (const chunk of target.upgrade.data) {
      received.set(chunk, offset);
      offset += chunk.length;
    }
    assert.deepStrictEqual(Buffer.from(received), Buffer.from(bytes));
    assert.strictEqual(progress, bytes.length);
    assert.strictEqual(api.ccMaxDataSize(64), 48);
  });

  await test('DATA 响应丢失时不重发（避免重复写入）', async () => {
    const target = new MockDevice();
    target.dropNextDataResponse = true;
    const bytes = new Uint8Array(100).fill(0x5a);
    const { client: up } = makeUpgradeClient(target, bytes);
    await assert.rejects(up.run(), /超时/);
    assert.strictEqual(target.upgrade.data.length, 1, '首帧之后不应继续发送');
    assert.strictEqual(target.upgrade.data[0].length, 48);
  });

  console.log(`\n全部通过：${passed} 项`);
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
