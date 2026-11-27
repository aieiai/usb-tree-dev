'use strict';

/* Rot-CC 设备协议 */
const CC = Object.freeze({
  HEADER_SIZE: 4,
  CMD_SYS: 0x00,
  CMD_UPGRADE: 0x01,
  CMD_KC: 0x02,
  CMD_AXIS: 0x03,
  CMD_CALIB: 0x04,
  CMD_NOTIFY: 0x05,
  SYS_HANDSHAKE: 0x01,
  SYS_INFO: 0x02,
  SYS_REBOOT: 0x03,
  SYS_RESET: 0x04,
  SYS_KEY_COUNT: 0x05,
  SYS_POPUP_SUPPORT: 0x06,
  SYS_POPUP_GET: 0x07,
  SYS_POPUP_SET: 0x08,
  KC_GET: 0x00,
  KC_SET: 0x01,
  KC_LAYER_COUNT: 0x02,
  KC_ACTIVE_LAYER: 0x03,
  KC_SET_ACTIVE_LAYER: 0x04,
  KC_TICK_GET: 0x05,
  KC_TICK_SET: 0x06,
  KC_TRG_GET: 0x07,
  KC_TRG_SET: 0x08,
  KC_DZ_GET: 0x09,
  KC_DZ_SET: 0x0a,
  KC_NOM_GET: 0x0b,
  KC_NOM_SET: 0x0c,
  KC_RT_GET: 0x0d,
  KC_RT_SET: 0x0e,
  AXIS_LIST: 0x00,
  AXIS_GET: 0x01,
  AXIS_SET: 0x02,
  CALIB_STATUS_GET: 0x00,
  CALIB_STATUS_SET: 0x01,
  CALIB_AUTO_GET: 0x02,
  CALIB_AUTO_SET: 0x03,
  CALIB_SENSOR_RPT_SET: 0x04,
  CALIB_SENSOR_FREQ_SET: 0x05,
  CALIB_STA_STOP: 0x80,
  CALIB_STA_START: 0x81,
  CALIB_STA_DONE: 0x82,
  NOTIFY_EXCEPTION: 0x00,
  NOTIFY_SENSOR: 0x01,
  SENSOR_DATA_LEN: 6,
  UPGRADE_INFO: 0x00,
  UPGRADE_INFO_LEN: 48,
  UPGRADE_REQUEST: 0x01,
  UPGRADE_DATA: 0x02,
  UPGRADE_REQ_START: 1,
  UPGRADE_REQ_STOP: 2,
  UPGRADE_REQ_FINISH: 3,
  MIN_FRAME_SIZE: 64,
  MAX_FRAME_SIZE: 1024
});


function packCcMessage(cmd, subcmd, reserve = 0, payload = new Uint8Array()) {
  const msg = new Uint8Array(CC.HEADER_SIZE + payload.length);
  msg[0] = cmd;
  msg[1] = subcmd;
  writeLe16(msg, 2, reserve);
  msg.set(payload, CC.HEADER_SIZE);
  return msg;
}

function parseCcMessage(payload) {
  const msg = asUint8Array(payload);
  if (msg.length < CC.HEADER_SIZE) return null;
  return {
    cmd: msg[0],
    subcmd: msg[1],
    reserve: readLe16(msg, 2),
    payload: msg.subarray(CC.HEADER_SIZE)
  };
}


function ccMaxDataSize(packetSize) {
  return packetSize - RG.HEADER_SIZE - CC.HEADER_SIZE;
}

/* 单帧最多能返回多少个定长条目（用于按键参数等批量读取的分片）。 */
function ccMaxEntries(packetSize, entrySize) {
  return Math.max(1, Math.floor(ccMaxDataSize(packetSize) / entrySize));
}

/* 标准 CRC-32（poly 0xEDB88320，初值/结果取反），与设备 upgrade 模块一致。 */
function ccCrc32(data) {
  let crc = 0xffffffff;
  for (const byte of data) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return (~crc) >>> 0;
}

function ccStatusText(status) {
  if (status === 0) return '成功';
  const group = Math.floor(status / 512);
  const code = status % 512;
  if (group === 0) {
    const common = ['成功', '失败', '超时', '参数错误', '不支持', '未找到'];
    return common[code] || `通用错误 0x${status.toString(16)}`;
  }
  if (group === 3) {
    const hal = ['', '参数错误', '启动失败', '端点忙', '设备未就绪'];
    return hal[code] || `HAL 错误 0x${status.toString(16)}`;
  }
  if (group === 4) {
    const rotcc = ['', '初始化失败', '未初始化', '发送超时', '发送锁失败'];
    return rotcc[code] || `CC 错误 0x${status.toString(16)}`;
  }
  if (group === 6) {
    const upgrade = [
      '', '升级初始化失败', '升级未初始化', '升级忙', '升级槽无效',
      '文件头无效', '镜像过大', '数据不完整', '校验失败',
      '镜像无效', '升级记录无效', '镜像验证失败',
      'BootROM 双槽布局不匹配', '需要 EXIP 镜像'
    ];
    return upgrade[code] || `升级错误 0x${status.toString(16)}`;
  }
  return `设备错误 0x${status.toString(16).padStart(8, '0')}`;
}

