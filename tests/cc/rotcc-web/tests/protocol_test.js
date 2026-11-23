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
    decodeDeviceInfo, decodeUpgradeInfo, upgradeInfoLines
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
    this.attributes = Array.from({ length: 42 }, () => ({
      trg: 0,
      dz: { dst_t: 0, dst_b: 0 },
      nom: { dst_p: 0, dst_r: 0 },
      rt: { en: 0, ctn_en: 0, dst_p: 0, dst_r: 0 },
      axis: 0
    }));
    this.tick = { sclt: 0, sclto: 0, dcit: 0, dclt: 0, dclto: 0 };
    this.axisSupport = [0, 1]; /* KAXIS_BB, KAXIS_GDL */
    this.resetRequests = [];
    this.rebooted = false;
    this.upgrade = null;
    this.dropNextDataResponse = false;
  }

  defaults() {
    return Array.from({ length: 4 }, () => new Array(42).fill(0));
  }

  /* 解析 {start, end} + count * entrySize 的按键范围请求。 */
  keyRange(payload, entrySize) {
    if (payload.length < 2) return null;
    const start = payload[0];
    const end = payload[1];
    if (end < start || end >= this.keyCount) return null;
    const count = end - start + 1;
    if (payload.length !== 2 + count * entrySize) return null;
    return { start, end, count };
  }

  deviceInfo() {
    const payload = new Uint8Array(61);
    const view = new DataView(payload.buffer);
    view.setUint16(0, 0x7989, true);
    view.setUint16(2, 0x0001, true);
    writeString(payload, 4, 'RAY-40');
    writeString(payload, 20, 'SN0001');
    const version = (1n << 56n) | (1n << 46n) | 0x12345678n;
    view.setBigUint64(36, version, true);
    payload[44] = 1;
    writeString(payload, 45, '20260909151925');
    return payload;
  }

  upgradeInfo() {
    const payload = new Uint8Array(48);
    const view = new DataView(payload.buffer);
    payload[0] = 0;                 /* slot_act */
    payload[1] = 1;                 /* slot_tag */
    payload[2] = 2;                 /* slot_count */
    payload[3] = 0;                 /* encrypted */
    payload[4] = 0;                 /* remap_enabled */
    payload[5] = 1;                 /* boot_ota_enabled */
    payload[6] = 1;                 /* otp_address_match */
    payload[7] = 1;                 /* otp_length_match */
    payload[8] = 1;                 /* slot0 valid */
    payload[9] = 1;                 /* slot1 valid */
    payload[10] = 0;                /* slot0 retired */
    payload[11] = 1;                /* slot1 retired */
    view.setUint16(12, 290, true);  /* slot0 SW_VERSION */
    view.setUint16(14, 301, true);  /* slot1 SW_VERSION */
    view.setUint32(16, 256 * 1024, true);
    view.setUint32(20, 0x400, true);
    view.setUint32(24, 0xc00, true);
    view.setUint32(28, 256 * 1024, true);
    view.setUint32(32, 256 * 1024, true);
    view.setUint16(36, 16, true);
    view.setUint16(38, 0, true);
    view.setBigUint64(40, (1n << 56n) | (1n << 46n) | 0x12345678n, true);
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
      switch (msg.subcmd) {
      case CC.KC_GET: {
        const range = this.keyRange(msg.payload, 0);
        if (!range || layer >= this.layerCount) return status(3);
        const out = new Uint8Array(range.count * 4);
        for (let i = 0; i < range.count; i++) {
          api.writeLe32(out, i * 4, this.keymap[layer][range.start + i]);
        }
        return reply(out);
      }
      case CC.KC_SET: {
        const range = this.keyRange(msg.payload, 4);
        if (!range || layer >= this.layerCount) return status(3);
        for (let i = 0; i < range.count; i++) {
          this.keymap[layer][range.start + i] =
            api.readLe32(msg.payload, 2 + i * 4);
        }
        return status(0);
      }
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
      case CC.KC_TICK_GET: {
        if (msg.payload.length !== 0) return status(3);
        const out = new Uint8Array(12);
        const values = [
          this.tick.sclt, this.tick.sclto, this.tick.dcit,
          this.tick.dclt, this.tick.dclto, 0
        ];
        values.forEach((value, i) => api.writeLe16(out, i * 2, value));
        return reply(out);
      }
      case CC.KC_TICK_SET: {
        if (msg.payload.length !== 12) return status(3);
        this.tick = {
          sclt: api.readLe16(msg.payload, 0),
          sclto: api.readLe16(msg.payload, 2),
          dcit: api.readLe16(msg.payload, 4),
          dclt: api.readLe16(msg.payload, 6),
          dclto: api.readLe16(msg.payload, 8)
        };
        return status(0);
      }
      case CC.KC_TRG_GET: {
        const range = this.keyRange(msg.payload, 0);
        if (!range) return status(3);
        const out = new Uint8Array(range.count * 4);
        for (let i = 0; i < range.count; i++) {
          api.writeLe32(out, i * 4, this.attributes[range.start + i].trg);
        }
        return reply(out);
      }
      case CC.KC_TRG_SET: {
        const range = this.keyRange(msg.payload, 4);
        if (!range) return status(3);
        for (let i = 0; i < range.count; i++) {
          const value = api.readLe32(msg.payload, 2 + i * 4);
          if (value > 2) return status(3);
          this.attributes[range.start + i].trg = value;
        }
        return status(0);
      }
      case CC.KC_DZ_GET: {
        const range = this.keyRange(msg.payload, 0);
        if (!range) return status(3);
        const out = new Uint8Array(range.count * 4);
        for (let i = 0; i < range.count; i++) {
          const dz = this.attributes[range.start + i].dz;
          api.writeLe16(out, i * 4, dz.dst_t);
          api.writeLe16(out, i * 4 + 2, dz.dst_b);
        }
        return reply(out);
      }
      case CC.KC_DZ_SET: {
        const range = this.keyRange(msg.payload, 4);
        if (!range) return status(3);
        for (let i = 0; i < range.count; i++) {
          this.attributes[range.start + i].dz = {
            dst_t: api.readLe16(msg.payload, 2 + i * 4),
            dst_b: api.readLe16(msg.payload, 2 + i * 4 + 2)
          };
        }
        return status(0);
      }
      case CC.KC_NOM_GET: {
        const range = this.keyRange(msg.payload, 0);
        if (!range) return status(3);
        const out = new Uint8Array(range.count * 4);
        for (let i = 0; i < range.count; i++) {
          const nom = this.attributes[range.start + i].nom;
          api.writeLe16(out, i * 4, nom.dst_p);
          api.writeLe16(out, i * 4 + 2, nom.dst_r);
        }
        return reply(out);
      }
      case CC.KC_NOM_SET: {
        const range = this.keyRange(msg.payload, 4);
        if (!range) return status(3);
        for (let i = 0; i < range.count; i++) {
          this.attributes[range.start + i].nom = {
            dst_p: api.readLe16(msg.payload, 2 + i * 4),
            dst_r: api.readLe16(msg.payload, 2 + i * 4 + 2)
          };
        }
        return status(0);
      }
      case CC.KC_RT_GET: {
        const range = this.keyRange(msg.payload, 0);
        if (!range) return status(3);
        const out = new Uint8Array(range.count * 6);
        for (let i = 0; i < range.count; i++) {
          const rt = this.attributes[range.start + i].rt;
          const flags = (rt.en & 1) | ((rt.ctn_en & 1) << 1);
          api.writeLe16(out, i * 6, flags);
          api.writeLe16(out, i * 6 + 2, rt.dst_p);
          api.writeLe16(out, i * 6 + 4, rt.dst_r);
        }
        return reply(out);
      }
      case CC.KC_RT_SET: {
        const range = this.keyRange(msg.payload, 6);
        if (!range) return status(3);
        for (let i = 0; i < range.count; i++) {
          const flags = api.readLe16(msg.payload, 2 + i * 6);
          this.attributes[range.start + i].rt = {
            en: flags & 1,
            ctn_en: (flags >> 1) & 1,
            dst_p: api.readLe16(msg.payload, 2 + i * 6 + 2),
            dst_r: api.readLe16(msg.payload, 2 + i * 6 + 4)
          };
        }
        return status(0);
      }
      default:
        return status(4);
      }
    }

    if (msg.cmd === CC.CMD_AXIS) {
      switch (msg.subcmd) {
      case CC.AXIS_LIST:
        if (msg.payload.length !== 0) return status(3);
        return reply(new Uint8Array(this.axisSupport));
      case CC.AXIS_SET: {
        const range = this.keyRange(msg.payload, 1);
        if (!range) return status(3);
        for (let i = 0; i < range.count; i++) {
          const axis = msg.payload[2 + i];
          if (!this.axisSupport.includes(axis)) return status(3);
          this.attributes[range.start + i].axis = axis;
        }
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
    assert.strictEqual(defines.ROTCC_UPGRADE_INFO_LEN, CC.UPGRADE_INFO_LEN);
    assert.strictEqual(defines.ROTCC_UPGRADE_REQUEST_LEN, 14);
  });

  await test('固件覆盖协议中的全部命令', () => {
    const source = fs.readFileSync(
      path.join(PROJECT_ROOT, 'rot', 'proto', 'rotcc_proto.c'), 'utf8'
    );
    const handlers = [
      'ROTCC_SYS_HANDSHAKE', 'ROTCC_SYS_INFO', 'ROTCC_SYS_REBOOT',
      'ROTCC_SYS_RESET', 'ROTCC_SYS_KEY_COUNT',
      'ROTCC_SYS_POPUP_SUPPORT', 'ROTCC_SYS_POPUP_GET', 'ROTCC_SYS_POPUP_SET',
      'ROTCC_UPGRADE_INFO', 'ROTCC_UPGRADE_REQUEST', 'ROTCC_UPGRADE_DATA',
      'ROTCC_UPGRADE_REQ_START', 'ROTCC_UPGRADE_REQ_STOP',
      'ROTCC_UPGRADE_REQ_FINISH',
      'ROTCC_KC_GET', 'ROTCC_KC_SET', 'ROTCC_KC_LAYER_COUNT',
      'ROTCC_KC_ACTIVE_LAYER', 'ROTCC_KC_SET_ACTIVE_LAYER',
      'ROTCC_KC_TICK_GET', 'ROTCC_KC_TICK_SET',
      'ROTCC_KC_TRG_GET', 'ROTCC_KC_TRG_SET',
      'ROTCC_KC_DZ_GET', 'ROTCC_KC_DZ_SET',
      'ROTCC_KC_NOM_GET', 'ROTCC_KC_NOM_SET',
      'ROTCC_KC_RT_GET', 'ROTCC_KC_RT_SET',
      'ROTCC_AXIS_LIST', 'ROTCC_AXIS_SET'
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
    const payload = await client.readPayload(CC.CMD_SYS, CC.SYS_INFO, 61, {
      label: '设备信息'
    });
    const info = api.decodeDeviceInfo(payload);
    assert.strictEqual(info.vid, 0x7989);
    assert.strictEqual(info.pid, 0x0001);
    assert.strictEqual(info.pn, 'RAY-40');
    assert.strictEqual(info.sn, 'SN0001');
    assert.strictEqual(info.fwVersion, (1n << 56n) | (1n << 46n) | 0x12345678n);
    assert.strictEqual(info.hwVersion, 1);
    assert.strictEqual(info.buildDate, '20260909151925');
  });

  await test('获取按键数量', async () => {
    const payload = await client.readPayload(CC.CMD_SYS, CC.SYS_KEY_COUNT, 4, {
      label: '按键数量'
    });
    assert.strictEqual(api.readLe32(payload, 0), 42);
  });

  await test('按键范围内读取/设置键码', async () => {
    const reserve = (2 << 8);
    const range = Uint8Array.of(6, 8);
    const write = new Uint8Array(2 + 3 * 4);
    write.set(range, 0);
    api.writeLe32(write, 2, 0x0f000001);
    api.writeLe32(write, 6, 0x0f000001);
    api.writeLe32(write, 10, 0x04000000);
    await client.statusRequest(CC.CMD_KC, CC.KC_SET, {
      reserve,
      payload: write,
      label: '写键码范围'
    });

    const payload = await client.readPayload(CC.CMD_KC, CC.KC_GET, 12, {
      reserve,
      payload: range,
      label: '读键码范围'
    });
    assert.strictEqual(api.readLe32(payload, 0), 0x0f000001);
    assert.strictEqual(api.readLe32(payload, 4), 0x0f000001);
    assert.strictEqual(api.readLe32(payload, 8), 0x04000000);
  });

  await test('键码范围非法时返回参数错误', async () => {
    const reserve = (2 << 8);
    await assert.rejects(
      client.readPayload(CC.CMD_KC, CC.KC_GET, 8, {
        reserve,
        payload: Uint8Array.of(8, 6),
        label: '非法范围'
      }),
      /参数错误/
    );
    await assert.rejects(
      client.readPayload(CC.CMD_KC, CC.KC_GET, 8, {
        reserve,
        payload: Uint8Array.of(40, 50),
        label: '越界范围'
      }),
      /参数错误/
    );
    await assert.rejects(
      client.statusRequest(CC.CMD_KC, CC.KC_SET, {
        reserve,
        payload: Uint8Array.of(6, 8, 1, 2, 3, 4),
        label: '键码条目不足'
      }),
      /参数错误/
    );
  });

  await test('按键 tick 参数读写', async () => {
    const values = [100, 200, 300, 400, 500];
    const payload = new Uint8Array(12);
    values.forEach((value, i) => api.writeLe16(payload, i * 2, value));
    await client.statusRequest(CC.CMD_KC, CC.KC_TICK_SET, {
      payload,
      label: '写 tick'
    });

    const got = await client.readPayload(CC.CMD_KC, CC.KC_TICK_GET, 12, {
      label: '读 tick'
    });
    values.forEach((value, i) => {
      assert.strictEqual(api.readLe16(got, i * 2), value);
    });
    assert.strictEqual(api.readLe16(got, 10), 0);
  });

  await test('按键触发模式范围读写', async () => {
    const payload = new Uint8Array(2 + 3 * 4);
    payload[0] = 10;
    payload[1] = 12;
    [1, 2, 0].forEach((value, i) => api.writeLe32(payload, 2 + i * 4, value));
    await client.statusRequest(CC.CMD_KC, CC.KC_TRG_SET, {
      payload,
      label: '写触发模式'
    });

    const got = await client.readPayload(CC.CMD_KC, CC.KC_TRG_GET, 12, {
      payload: Uint8Array.of(10, 12),
      label: '读触发模式'
    });
    assert.strictEqual(api.readLe32(got, 0), 1);
    assert.strictEqual(api.readLe32(got, 4), 2);
    assert.strictEqual(api.readLe32(got, 8), 0);
  });

  await test('按键死区参数范围读写', async () => {
    const payload = new Uint8Array(2 + 2 * 4);
    payload[0] = 20;
    payload[1] = 21;
    api.writeLe16(payload, 2, 300);
    api.writeLe16(payload, 4, 400);
    api.writeLe16(payload, 6, 500);
    api.writeLe16(payload, 8, 600);
    await client.statusRequest(CC.CMD_KC, CC.KC_DZ_SET, {
      payload,
      label: '写死区'
    });

    const got = await client.readPayload(CC.CMD_KC, CC.KC_DZ_GET, 8, {
      payload: Uint8Array.of(20, 21),
      label: '读死区'
    });
    assert.strictEqual(api.readLe16(got, 0), 300);
    assert.strictEqual(api.readLe16(got, 2), 400);
    assert.strictEqual(api.readLe16(got, 4), 500);
    assert.strictEqual(api.readLe16(got, 6), 600);
  });

  await test('按键常规触发参数范围读写', async () => {
    const payload = new Uint8Array(2 + 2 * 4);
    payload[0] = 22;
    payload[1] = 23;
    api.writeLe16(payload, 2, 1200);
    api.writeLe16(payload, 4, 1300);
    api.writeLe16(payload, 6, 1400);
    api.writeLe16(payload, 8, 1500);
    await client.statusRequest(CC.CMD_KC, CC.KC_NOM_SET, {
      payload,
      label: '写常规触发'
    });

    const got = await client.readPayload(CC.CMD_KC, CC.KC_NOM_GET, 8, {
      payload: Uint8Array.of(22, 23),
      label: '读常规触发'
    });
    assert.strictEqual(api.readLe16(got, 0), 1200);
    assert.strictEqual(api.readLe16(got, 2), 1300);
    assert.strictEqual(api.readLe16(got, 4), 1400);
    assert.strictEqual(api.readLe16(got, 6), 1500);
  });

  await test('按键快速触发参数范围读写', async () => {
    const payload = new Uint8Array(2 + 2 * 6);
    payload[0] = 24;
    payload[1] = 25;
    api.writeLe16(payload, 2, 0x3);
    api.writeLe16(payload, 4, 150);
    api.writeLe16(payload, 6, 250);
    api.writeLe16(payload, 8, 0x1);
    api.writeLe16(payload, 10, 350);
    api.writeLe16(payload, 12, 450);
    await client.statusRequest(CC.CMD_KC, CC.KC_RT_SET, {
      payload,
      label: '写快速触发'
    });

    const got = await client.readPayload(CC.CMD_KC, CC.KC_RT_GET, 12, {
      payload: Uint8Array.of(24, 25),
      label: '读快速触发'
    });
    assert.strictEqual(api.readLe16(got, 0), 0x3);
    assert.strictEqual(api.readLe16(got, 2), 150);
    assert.strictEqual(api.readLe16(got, 4), 250);
    assert.strictEqual(api.readLe16(got, 6), 0x1);
    assert.strictEqual(api.readLe16(got, 8), 350);
    assert.strictEqual(api.readLe16(got, 10), 450);
  });

  await test('轴体列表与按键轴体设置', async () => {
    const list = await client.readPayload(CC.CMD_AXIS, CC.AXIS_LIST, 2, {
      label: '轴体列表'
    });
    assert.deepStrictEqual([...list], device.axisSupport);

    const payload = Uint8Array.of(30, 32, 1, 0, 1);
    await client.statusRequest(CC.CMD_AXIS, CC.AXIS_SET, {
      payload,
      label: '设置轴体'
    });
    assert.strictEqual(device.attributes[30].axis, 1);
    assert.strictEqual(device.attributes[31].axis, 0);
    assert.strictEqual(device.attributes[32].axis, 1);

    await assert.rejects(
      client.statusRequest(CC.CMD_AXIS, CC.AXIS_SET, {
        payload: Uint8Array.of(30, 30, 9),
        label: '设置非法轴体'
      }),
      /参数错误/
    );
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
    const payload = await client.readPayload(
      CC.CMD_UPGRADE, CC.UPGRADE_INFO, CC.UPGRADE_INFO_LEN,
      { label: '升级信息' }
    );
    const info = api.decodeUpgradeInfo(payload);
    assert.strictEqual(info.slotAct, 0);
    assert.strictEqual(info.slotTag, 1);
    assert.strictEqual(info.slotCount, 2);
    assert.strictEqual(info.slotSize, 256 * 1024);
    assert.strictEqual(info.encrypt, false);
    assert.strictEqual(info.slotVersion[0], 290);
    assert.strictEqual(info.slotVersion[1], 301);
    assert.strictEqual(info.slotValid[0], true);
    assert.strictEqual(info.slotValid[1], true);
    assert.strictEqual(info.slotRetired[0], false);
    assert.strictEqual(info.slotRetired[1], true);
    assert.strictEqual(info.otpAddressMatch, true);
    assert.strictEqual(info.otpLengthMatch, true);
    assert.strictEqual(info.otpOffset, 256 * 1024);
    assert.strictEqual(info.otpSize, 256 * 1024);
    assert.strictEqual(info.otpSecImgOffset, 16);
    assert.strictEqual(info.otpMaxImgLen, 0);
    assert.strictEqual(info.imageBaseOffset, 0x400);
    assert.strictEqual(info.fileBootOffset, 0xc00);
    assert.strictEqual(
      info.firmwareVersion, (1n << 56n) | (1n << 46n) | 0x12345678n
    );

    const lines = api.upgradeInfoLines(info);
    assert(lines.some(line => line.includes('运行 APP1')));
    assert(lines.some(line => line.includes('APP1 SW 290')));
    assert(lines.some(line => line.includes('APP2 SW 301')));
    assert(lines.some(line => line.includes('匹配')));
    assert(lines.some(line => line.includes('boot head=0xc00')));
    assert(lines.some(line => line.includes('v1.1.0-release+12345678')));
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
