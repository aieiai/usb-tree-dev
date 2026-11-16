'use strict';

/* Rot-Gen 通用帧层 */
const RG = Object.freeze({
  SOF: 0x52,
  VERSION: 1,
  HEADER_SIZE: 12,
  SECRET_KEY: 0,
  ADDR_CC: 0,
  ADDR_KEYBOARD: 1,
  OPT_REQUEST: 0x08,
  CRC8_OFFSET: 9,
  CRC16_OFFSET: 10
});


/* ------------------------------------------------------------ Rot-Gen 帧 */

function readLe16(data, offset) {
  return data[offset] | (data[offset + 1] << 8);
}

function readLe32(data, offset) {
  return (data[offset] |
          (data[offset + 1] << 8) |
          (data[offset + 2] << 16) |
          (data[offset + 3] << 24)) >>> 0;
}

function writeLe16(data, offset, value) {
  data[offset] = value & 0xff;
  data[offset + 1] = (value >>> 8) & 0xff;
}

function writeLe32(data, offset, value) {
  data[offset] = value & 0xff;
  data[offset + 1] = (value >>> 8) & 0xff;
  data[offset + 2] = (value >>> 16) & 0xff;
  data[offset + 3] = (value >>> 24) & 0xff;
}

function rgCrc8(data) {
  let crc = 0;
  for (const byte of data) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
      crc &= 0xff;
    }
  }
  return crc;
}

function rgCrc16Update(crc, data) {
  for (const byte of data) {
    crc ^= byte << 8;
    for (let bit = 0; bit < 8; bit++) {
      crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
      crc &= 0xffff;
    }
  }
  return crc;
}


function makeRotGenFrame(payload, options = RG.OPT_REQUEST) {
  const frame = new Uint8Array(RG.HEADER_SIZE + payload.length);
  frame[0] = RG.SOF;
  frame[1] = RG.VERSION;
  frame[2] = options;
  frame[3] = RG.ADDR_CC;
  frame[4] = RG.ADDR_KEYBOARD;
  writeLe16(frame, 5, payload.length);
  writeLe16(frame, 7, RG.SECRET_KEY);
  frame[RG.CRC8_OFFSET] = rgCrc8(frame.subarray(0, RG.CRC8_OFFSET));
  frame.set(payload, RG.HEADER_SIZE);

  let crc = rgCrc16Update(0, frame.subarray(0, RG.CRC16_OFFSET));
  crc = rgCrc16Update(crc, frame.subarray(RG.HEADER_SIZE));
  writeLe16(frame, RG.CRC16_OFFSET, crc);
  return frame;
}

function parseRotGenFrame(data) {
  const frame = asUint8Array(data);
  if (frame.length < RG.HEADER_SIZE ||
      frame[0] !== RG.SOF ||
      frame[1] !== RG.VERSION) {
    return null;
  }
  const payloadLength = readLe16(frame, 5);
  if (RG.HEADER_SIZE + payloadLength !== frame.length) return null;
  if (rgCrc8(frame.subarray(0, RG.CRC8_OFFSET)) !== frame[RG.CRC8_OFFSET]) {
    return null;
  }
  let crc = rgCrc16Update(0, frame.subarray(0, RG.CRC16_OFFSET));
  crc = rgCrc16Update(crc, frame.subarray(RG.HEADER_SIZE));
  if (crc !== readLe16(frame, RG.CRC16_OFFSET)) return null;

  return {
    options: frame[2],
    src: frame[3],
    dst: frame[4],
    payload: frame.subarray(RG.HEADER_SIZE)
  };
}

