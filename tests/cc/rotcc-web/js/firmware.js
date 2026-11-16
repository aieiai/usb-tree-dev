'use strict';

/* ------------------------------------------------------------ 升级流程 */

function isFirmwareActive() {
  return firmwareStarting || firmwareSession !== null;
}

function formatByteCount(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}

function monotonicNow() {
  return globalThis.performance?.now
    ? globalThis.performance.now()
    : Date.now();
}

function formatDuration(milliseconds) {
  const totalMs = Math.max(0, Math.round(milliseconds));
  if (totalMs < 60000) {
    return `${(totalMs / 1000).toFixed(2)} s`;
  }

  const totalSeconds = Math.floor(totalMs / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  const tenths = Math.floor((totalMs % 1000) / 100);
  if (hours > 0) {
    return `${hours}:${String(minutes).padStart(2, '0')}:` +
           `${String(seconds).padStart(2, '0')}.${tenths}`;
  }
  return `${minutes}:${String(seconds).padStart(2, '0')}.${tenths}`;
}

function renderFirmwareDuration(milliseconds) {
  $firmwareDuration.textContent =
    milliseconds > 0 ? `总用时 ${formatDuration(milliseconds)}` : '总用时 --';
}

function tickFirmwareDuration() {
  if (firmwareStartedAt === 0) return;
  firmwareLastDurationMs = monotonicNow() - firmwareStartedAt;
  renderFirmwareDuration(firmwareLastDurationMs);
}

function startFirmwareDuration() {
  if (firmwareDurationTimer) clearInterval(firmwareDurationTimer);
  firmwareLastDurationMs = 0;
  firmwareStartedAt = monotonicNow();
  $firmwareDuration.textContent = '总用时 0.00 s';
  firmwareDurationTimer = setInterval(tickFirmwareDuration, 100);
}

function stopFirmwareDuration() {
  if (firmwareDurationTimer) {
    clearInterval(firmwareDurationTimer);
    firmwareDurationTimer = 0;
  }
  if (firmwareStartedAt !== 0) {
    firmwareLastDurationMs = monotonicNow() - firmwareStartedAt;
    firmwareStartedAt = 0;
  }
  renderFirmwareDuration(firmwareLastDurationMs);
  return firmwareLastDurationMs;
}

function resetFirmwareDuration() {
  if (firmwareDurationTimer) {
    clearInterval(firmwareDurationTimer);
    firmwareDurationTimer = 0;
  }
  firmwareStartedAt = 0;
  firmwareLastDurationMs = 0;
  renderFirmwareDuration(0);
}

function setFirmwareState(text, className = '') {
  $firmwareState.textContent = text;
  $firmwareState.className = className;
}

function updateFirmwareProgress(done, total) {
  const now = Date.now();
  if (done > 0 && done < total && now - firmwareProgressUpdatedAt < 100) {
    return;
  }
  firmwareProgressUpdatedAt = now;
  $firmwareProgress.max = Math.max(1, total);
  $firmwareProgress.value = Math.min(done, total);
  const percent = total > 0 ? Math.floor(done * 100 / total) : 0;
  $firmwareMeta.textContent =
    `${formatByteCount(done)} / ${formatByteCount(total)} · ${percent}%`;
}

async function sendCcAbort(target) {
  if (!target || !device || !device.opened || !target.link.claimed) return;
  const frame = makeRotGenFrame(
    packCcMessage(CC.CMD_UPGRADE, CC.UPGRADE_ABORT)
  );
  await writeCcFrame(target, frame, 'ABORT').catch(error => {
    appendLine(`[CC] 发送取消帧失败：${error.message || error}`, 'warn');
  });
}

async function startFirmwareUpgrade() {
  if (isFirmwareActive()) return;
  const file = $firmwareFile.files[0];
  if (!file) {
    setFirmwareState('请选择升级文件。', 'err');
    return;
  }
  if (!device || !device.opened) {
    setFirmwareState('请先连接设备。', 'err');
    return;
  }

  firmwareStarting = true;
  startFirmwareDuration();
  let current = null;
  let target = null;
  let handlerKey = null;
  let succeeded = false;
  refreshControls();

  try {
    target = findCcTarget();
    if (!target) {
      throw new Error('没有找到 CC Interface 1 的 IN/OUT 端点');
    }
    appendLine(
      `[CC] 固件端点：IN=${target.epIn.packetSize} B，` +
      `OUT=${target.epOut.packetSize} B，USB=${device.speed || 'unknown'}，` +
      `单帧数据=${target.maxDataSize} B`
    );

    setFirmwareState(`读取文件 ${file.name} ...`);
    const bytes = new Uint8Array(await file.arrayBuffer());
    if (bytes.length < 16) {
      throw new Error('升级文件小于 16 字节，无法包含升级头');
    }
    if (readLe32(bytes, 0) !== UPGRADE_IMAGE_MAGIC) {
      throw new Error('升级文件头 magic 不匹配');
    }
    if (readLe32(bytes, 4) !== UPGRADE_DEVICE_ID) {
      throw new Error('升级文件头的设备 ID 不匹配');
    }
    if (readLe32(bytes, 8) === 0) {
      throw new Error('升级文件头的镜像长度为 0');
    }
    if (!device || !device.opened || !links.includes(target.link)) {
      throw new Error('设备已断开');
    }
    updateFirmwareProgress(0, bytes.length);

    current = await makeCcClient(target, {
      bytes,
      onProgress: updateFirmwareProgress
    });
    firmwareSession = current;
    refreshControls();

    handlerKey = endpointKey(target.epIn);
    target.link.rxHandlers.set(handlerKey, data => current.receive(data));
    if (!target.link.reads.has(handlerKey)) {
      startRead(target.link, target.epIn);
    }

    setFirmwareState(`正在通过 CC 升级 ${file.name}（${formatByteCount(bytes.length)}）...`);
    await current.run();
    succeeded = true;
    const durationMs = stopFirmwareDuration();
    setFirmwareState(
      `设备校验完成：${file.name}，总用时 ${formatDuration(durationMs)}`
    );
    appendLine(
      `[CC] ${file.name} 升级完成，共 ${bytes.length} 字节，` +
      `总用时 ${formatDuration(durationMs)}（${Math.round(durationMs)} ms）`,
      ''
    );
  } catch (error) {
    const durationMs = stopFirmwareDuration();
    if ((current && current.cancelled) || error instanceof CcAbortError) {
      setFirmwareState(
        `CC 传输已取消，总用时 ${formatDuration(durationMs)}`,
        'warn'
      );
      appendLine(
        `[CC] 传输已取消，总用时 ${formatDuration(durationMs)}` +
        `（${Math.round(durationMs)} ms）`,
        'warn'
      );
    } else {
      const message = error.message || String(error);
      setFirmwareState(
        `升级失败：${message}，总用时 ${formatDuration(durationMs)}`,
        'err'
      );
      appendLine(
        `[CC 失败] ${message}，总用时 ${formatDuration(durationMs)}` +
        `（${Math.round(durationMs)} ms）`,
        'err'
      );
    }
  } finally {
    stopFirmwareDuration();
    if (current && !succeeded) {
      await sendCcAbort(target);
    }
    if (target && handlerKey !== null) {
      target.link.rxHandlers.delete(handlerKey);
    }
    if (firmwareSession === current) firmwareSession = null;
    firmwareStarting = false;
    refreshControls();
  }
}

function cancelFirmwareUpgrade() {
  if (!firmwareSession) return;
  setFirmwareState('正在取消 CC 传输...', 'warn');
  firmwareSession.cancel();
}

