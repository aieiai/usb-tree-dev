'use strict';


const VID = 0x7989;
const PID = 0x0001;
const MAX_LOG_CHARS = 300000;


const CC_RESPONSE_TIMEOUT_MS = 15000;
const CC_START_TIMEOUT_MS = 60000;
const CC_END_TIMEOUT_MS = 60000;
const CC_MAX_RETRIES = 10;
const UPGRADE_IMAGE_MAGIC = 0xbeaf5aa5;
const UPGRADE_DEVICE_ID = 0x6750;
const CC_UI_VERSION = 6;


const INTERFACE_HINTS = {
  0: {
    name: 'KBD 键鼠',
    auto: false,
    note: 'HID 接口。Windows 键鼠驱动通常占用；尝试 claim 可能会暂时影响键鼠输入。'
  },
  1: {
    name: 'CC Control Center',
    auto: true,
    note: '调试与固件升级接口，IN 0x82 / OUT 0x02；帧长按端点描述符自动适配。'
  },
  2: {
    name: 'Lighting',
    auto: false,
    note: 'HID 灯效接口，描述符只暴露 IN 端点；输出通常走 HID SET_REPORT。'
  },
  3: {
    name: 'DT Diagnostic Tracking',
    auto: true,
    note: '诊断日志接口，IN 0x84 / OUT 0x04；设备协议不经过此接口。'
  }
};

const $status = document.getElementById('status');
const $interfaces = document.getElementById('interfaces');
const $log = document.getElementById('log');
const $traffic = document.getElementById('traffic');
const $displayMode = document.getElementById('displayMode');
const $inputMode = document.getElementById('inputMode');
const $chkLf = document.getElementById('chkLf');
const $chkAll = document.getElementById('chkAllInterfaces');
const $firmwareFile = document.getElementById('firmwareFile');
const $btnFirmwareStart = document.getElementById('btnFirmwareStart');
const $btnFirmwareCancel = document.getElementById('btnFirmwareCancel');
const $firmwareState = document.getElementById('firmwareState');
const $firmwareProgress = document.getElementById('firmwareProgress');
const $firmwareMeta = document.getElementById('firmwareMeta');
const $firmwareDuration = document.getElementById('firmwareDuration');
const $ccPacketInfo = document.getElementById('ccPacketInfo');
const $ccDeviceInfo = document.getElementById('ccDeviceInfo');
const $btnCcQuery = document.getElementById('btnCcQuery');
const $kcLayers = document.getElementById('kcLayers');
const $keyboard = document.getElementById('keyboard');
const $pickerTabs = document.getElementById('pickerTabs');
const $pickerGroups = document.getElementById('pickerGroups');
const btnConnect = document.getElementById('btnConnect');
const btnDisconnect = document.getElementById('btnDisconnect');

let device = null;
let links = [];
let session = 0;
let rxBytes = 0;
let txBytes = 0;
let firmwareSession = null;
let firmwareStarting = false;
let firmwareProgressUpdatedAt = 0;
let firmwareStartedAt = 0;
let firmwareDurationTimer = 0;
let firmwareLastDurationMs = 0;
let ccQueryPromise = null;
let kcBusy = false;
let kcSelected = null;
let kcLayerIndex = 0;
const KC_LAYER_COUNT = 4;
const kcValues = new Map();
const kcKeyEls = new Map();


function nowText() {
  return new Date().toLocaleTimeString('zh-CN', { hour12: false });
}

function appendLine(text, className = '') {
  const line = document.createElement('span');
  if (className) line.className = className;
  line.textContent = `${nowText()} ${text}\n`;
  $log.appendChild(line);
  if ($log.textContent.length > MAX_LOG_CHARS) {
    $log.textContent = $log.textContent.slice(-MAX_LOG_CHARS);
  }
  $log.scrollTop = $log.scrollHeight;
}

function asUint8Array(data) {
  if (data instanceof Uint8Array) return data;
  if (ArrayBuffer.isView(data)) {
    return new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
  }
  return new Uint8Array(data);
}

function toHex(data) {
  return Array.from(asUint8Array(data))
    .map(v => v.toString(16).padStart(2, '0'))
    .join(' ');
}

function formatData(data) {
  if ($displayMode.value === 'hex') return toHex(data);
  return new TextDecoder('utf-8').decode(data);
}

function parseHex(text) {
  const normalized = text
    .replace(/0x/gi, '')
    .replace(/[\s,;:_-]+/g, '');
  if (!normalized || normalized.length % 2 !== 0 || !/^[0-9a-f]+$/i.test(normalized)) {
    throw new Error('HEX 数据必须由偶数个十六进制字符组成');
  }
  const out = new Uint8Array(normalized.length / 2);
  for (let i = 0; i < out.length; i++) {
    out[i] = parseInt(normalized.slice(i * 2, i * 2 + 2), 16);
  }
  return out;
}

function encodeInput(text) {
  let data = $inputMode.value === 'hex'
    ? parseHex(text)
    : new TextEncoder().encode($chkLf.checked ? text + '\n' : text);
  if (data.length === 0) throw new Error('发送内容不能为空');
  return data;
}

function endpointType(type) {
  return type === 'interrupt' ? '中断' : type === 'bulk' ? '批量' : type;
}

function endpointKey(ep) {
  return `${ep.direction}:${ep.endpointNumber}`;
}

function endpoinTag(ep) {
  const address = ep.endpointNumber | (ep.direction === 'in' ? 0x80 : 0x00);
  return `${ep.direction.toUpperCase()} 0x${address.toString(16).padStart(2, '0')}`;
}

