'use strict';

/* Rot-CC 设备协议 */
const CC = Object.freeze({
  HEADER_SIZE: 4,
  CMD_SYS: 0x00,
  CMD_KC: 0x01,
  CMD_UPGRADE: 0x02,
  SYS_VERSION: 0x00,
  SYS_HANDSHAKE: 0x01,
  SYS_INFO: 0x02,
  SYS_REBOOT: 0x03,
  SYS_RESET: 0x04,
  KC_GET: 0x00,
  KC_SET: 0x01,
  UPGRADE_QUERY: 0x00,
  UPGRADE_START: 0x01,
  UPGRADE_DATA: 0x02,
  UPGRADE_END: 0x03,
  UPGRADE_ABORT: 0x04,
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
  return packetSize - RG.HEADER_SIZE - CC.HEADER_SIZE - 2;
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

