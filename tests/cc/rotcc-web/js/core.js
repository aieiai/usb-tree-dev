'use strict';


const VID = 0x7989;
const PID = 0x0001;
const MAX_LOG_CHARS = 300000;


const CC_RESPONSE_TIMEOUT_MS = 15000;
const CC_START_TIMEOUT_MS = 60000;
const CC_END_TIMEOUT_MS = 60000;
const CC_MAX_RETRIES = 10;
const CC_UI_VERSION = 8;


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
const $kcQuickSelect = document.getElementById('kcQuickSelect');
const $keyboard = document.getElementById('keyboard');
const $pickerTabs = document.getElementById('pickerTabs');
const $pickerGroups = document.getElementById('pickerGroups');
const $sysProtocol = document.getElementById('sysProtocol');
const $sysKeyCount = document.getElementById('sysKeyCount');
const $sysPopup = document.getElementById('sysPopup');
const $sysState = document.getElementById('sysState');
const $btnHandshake = document.getElementById('btnHandshake');
const $btnKeyCount = document.getElementById('btnKeyCount');
const $btnSysReboot = document.getElementById('btnSysReboot');
const $btnSysReset = document.getElementById('btnSysReset');
const $btnSysResetAll = document.getElementById('btnSysResetAll');
const $btnPopupSupport = document.getElementById('btnPopupSupport');
const $btnPopupRead = document.getElementById('btnPopupRead');
const $btnPopupWrite = document.getElementById('btnPopupWrite');
const $chkPopup = document.getElementById('chkPopup');
const $calibStatus = document.getElementById('calibStatus');
const $calibState = document.getElementById('calibState');
const $btnCalibRead = document.getElementById('btnCalibRead');
const $btnCalibStart = document.getElementById('btnCalibStart');
const $btnCalibFinish = document.getElementById('btnCalibFinish');
const $btnCalibStop = document.getElementById('btnCalibStop');
const $btnAutoCalibRead = document.getElementById('btnAutoCalibRead');
const $btnAutoCalibWrite = document.getElementById('btnAutoCalibWrite');
const $chkAutoCalib = document.getElementById('chkAutoCalib');
const $chkResetKc = document.getElementById('chkResetKc');
const $chkResetRgb = document.getElementById('chkResetRgb');
const $chkResetAdv = document.getElementById('chkResetAdv');
const $paramKeyboard = document.getElementById('paramKeyboard');
const $paramTabs = document.getElementById('paramTabs');
const $paramForm = document.getElementById('paramForm');
const $paramState = document.getElementById('paramState');
const $paramSelection = document.getElementById('paramSelection');
const $btnParamSelectAll = document.getElementById('btnParamSelectAll');
const $btnParamClear = document.getElementById('btnParamClear');
const $calibKeyboard = document.getElementById('calibKeyboard');
const $axisKeyboard = document.getElementById('axisKeyboard');
const $axisSupport = document.getElementById('axisSupport');
const $axisTabs = document.getElementById('axisTabs');
const $axisListPanel = document.getElementById('axisListPanel');
const $axisListText = document.getElementById('axisListText');
const $axisSetPanel = document.getElementById('axisSetPanel');
const $axisSelection = document.getElementById('axisSelection');
const $axisValue = document.getElementById('axisValue');
const $axisState = document.getElementById('axisState');
const $btnAxisRead = document.getElementById('btnAxisRead');
const $btnAxisSelectAll = document.getElementById('btnAxisSelectAll');
const $btnAxisClear = document.getElementById('btnAxisClear');
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
let sysBusy = false;
let ccPopupSupported = null;
let ccCalibSta = null;
let kcBusy = false;
const kcSelected = new Set();
let kcLayerIndex = 0;
let kcLayerCount = 4;
const kcValues = new Map();
const kcKeyEls = new Map();



function nowText() {
  return new Date().toLocaleTimeString('zh-CN', { hour12: false });
}

function scrollLogToBottom() {
  $log.scrollTop = $log.scrollHeight;
}

function appendLine(text, className = '') {
  const line = document.createElement('span');
  if (className) line.className = className;
  line.textContent = `${nowText()} ${text}\n`;
  $log.appendChild(line);
  if ($log.textContent.length > MAX_LOG_CHARS) {
    $log.textContent = $log.textContent.slice(-MAX_LOG_CHARS);
  }
  scrollLogToBottom();
  requestAnimationFrame(scrollLogToBottom);
}

/* 任何来源的日志变化都跟随到底部。 */
if (globalThis.MutationObserver) {
  new MutationObserver(scrollLogToBottom).observe($log, {
    childList: true,
    subtree: true,
    characterData: true
  });
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

