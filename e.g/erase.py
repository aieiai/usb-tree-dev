#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
erase.py - 用 J-Link 命令行整片擦除 GS_KB_V75 的外部 NOR Flash（不需要打开 SES）

擦除范围：板载 1MB QSPI NOR @ 0x80000000（含 nor_cfg_option / boot header / 应用）。
擦完之后芯片里什么都没有，无法直接运行，需要重新烧录固件（script\flash.py）。
不会关闭你正在运行的 JLink 会话（需要独占探针时加 --kill-jlink）。

用法：
    python erase.py                          # 整片擦除 0x80000000 ~ 0x800FFFFF（1MB）
    python erase.py --addr 0x80003000        # 只擦从该地址到 flash 末尾
    python erase.py --addr 0x80000000 --size 0x1000   # 擦指定区间（addr + size）
    python erase.py --kill-jlink             # 探针被占用时：先关掉已有 JLink.exe 再擦除

注意：J-Link 的 erase 命令格式是「erase 起始地址, 结束地址」（不是长度），
擦完会自动读回抽样校验是否全为 0xFF。
"""

import argparse
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_DIR = SCRIPT_DIR.parent
sys.path.insert(0, str(SCRIPT_DIR))
sys.dont_write_bytecode = True  # 避免在 script\ 下生成 __pycache__

import open_jlink_rtt as rtt  # noqa: E402  复用其中的 find_jlink / tasklist_pids 等

FLASH_BASE = 0x80000000       # 板载 NOR Flash 起始地址（BOARD_FLASH_BASE_ADDRESS）
FLASH_SIZE = 0x100000         # 1MB（BOARD_FLASH_SIZE）
DEVICE = "HPM5361xCBx"


def kill_jlink():
    pids = rtt.tasklist_pids("JLink.exe")
    if not pids:
        return
    print("[i] 先关闭正在运行的 JLink.exe：%s" % ", ".join(str(p) for p in pids))
    for pid in pids:
        rtt.run_hidden(["taskkill", "/PID", str(pid), "/F"])
    time.sleep(1.0)


def make_samples(addr, size):
    """在擦除范围内取 3 个抽样点（头 / 中 / 尾），用于擦后读回校验。"""
    s = min(0x400, size)
    offsets = sorted({0, (size - s) // 2, size - s})
    return [(addr + off, s) for off in offsets]


def erase_commands(addr, size):
    """生成 J-Link 擦除命令序列（外部 QSPI bank 需先 EnableEraseAllFlashBanks）。"""
    return [
        "exec EnableEraseAllFlashBanks",
        # J-Link erase 格式是「起始地址, 结束地址」（不是长度！）
        "erase 0x%08X, 0x%08X" % (addr, addr + size - 1),
    ]


def build_commands(addr, size, speed, samples, sample_files):
    cmds = [
        "si JTAG",
        "speed %s" % speed,
        "device %s" % DEVICE,
        "JTAGConf -1, -1",
        "connect",
    ]
    cmds += erase_commands(addr, size)
    for (sa, slen), f in zip(samples, sample_files):
        cmds.append('savebin "%s", 0x%08X, 0x%X' % (f, sa, slen))
    cmds += ["r", "g", "exit"]
    return cmds


def run_jlink(jlink, cmds):
    script = Path(tempfile.gettempdir()) / "gs_kb_erase.jlink"
    script.write_text("\n".join(cmds) + "\n", encoding="mbcs", errors="replace")
    proc = subprocess.run([str(jlink), "-CommanderScript", str(script)],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = proc.stdout.decode("mbcs", errors="replace")
    print(out)
    return out


def erase_ok(out):
    bad = ("****** Error", "Failed to", "ERROR:", "Cannot connect",
           "mustn't", "must not", "Erasing failed")
    if any(b in out for b in bad):
        return False
    return "Erasing done." in out  # J-Link 只有真的执行了擦除才会打印这句


def verify_erased(samples, sample_files):
    """读回抽样文件，检查是否全为 0xFF。返回 (是否通过, 出错信息)。"""
    for (sa, _slen), f in zip(samples, sample_files):
        p = Path(f)
        if not p.is_file():
            return False, "0x%08X：读回文件未生成" % sa
        data = p.read_bytes()
        bad = [i for i, b in enumerate(data) if b != 0xFF]
        if bad:
            return False, ("0x%08X：还有 %d 个字节不是 0xFF（例如 0x%02X @ 偏移 0x%X）"
                           % (sa, len(bad), data[bad[0]], bad[0]))
    return True, None


def parse_int(text):
    return int(text, 0)  # 支持 0x 前缀 / 十进制


def main():
    parser = argparse.ArgumentParser(description="用 J-Link 命令行整片擦除 GS_KB_V75 的 NOR Flash")
    parser.add_argument("--addr", type=parse_int, default=FLASH_BASE,
                        help="擦除起始地址（默认 0x%08X）" % FLASH_BASE)
    parser.add_argument("--size", type=parse_int, default=None,
                        help="擦除字节数（默认到 flash 末尾；1MB 整片 = 0x100000）")
    parser.add_argument("--speed", default="4000", help="JTAG 速度 kHz（默认 4000）")
    parser.add_argument("--jlink", help="JLink.exe 路径（默认自动查找）")
    parser.add_argument("--kill-jlink", action="store_true",
                        help="擦除前先关掉正在运行的 JLink.exe（一般不需要）")
    args = parser.parse_args()

    # 校验擦除范围落在板载 flash 内
    if args.addr < FLASH_BASE or args.addr >= FLASH_BASE + FLASH_SIZE:
        print("[x] 地址 0x%08X 不在板载 NOR Flash 范围内（0x%08X ~ 0x%08X）"
              % (args.addr, FLASH_BASE, FLASH_BASE + FLASH_SIZE - 1))
        return 2
    size = args.size if args.size is not None else FLASH_BASE + FLASH_SIZE - args.addr
    if args.addr + size > FLASH_BASE + FLASH_SIZE:
        print("[x] 擦除范围超出板载 NOR Flash（最多到 0x%08X）" % (FLASH_BASE + FLASH_SIZE - 1))
        return 2
    if size <= 0:
        print("[x] 擦除大小必须大于 0")
        return 2

    jlink = rtt.find_jlink(args.jlink)
    if jlink is None:
        print("[x] 找不到 JLink.exe，请用 --jlink 指定")
        return 2

    print("=== 擦除 J-Link NOR Flash ===")
    print("[i] 范围   : 0x%08X ~ 0x%08X（%d KB）" % (args.addr, args.addr + size - 1, size // 1024))
    print("[i] JLink  : %s" % jlink)
    print("[i] 速度   : %s kHz" % args.speed)
    if args.addr == FLASH_BASE and size == FLASH_SIZE:
        print("[!] 整片擦除：nor_cfg_option / boot header / 应用都会被清掉，擦完需重新烧录固件。")

    if args.kill_jlink:
        kill_jlink()
    else:
        pids = rtt.tasklist_pids("JLink.exe")
        if pids:
            print("[i] 检测到正在运行的 JLink.exe：%s（保留它，直接擦除）" % ", ".join(str(p) for p in pids))

    print("[+] 开始擦除（擦完自动读回校验）...")
    t0 = time.time()

    samples = make_samples(args.addr, size)
    sample_files = [str(Path(tempfile.gettempdir()) / ("gs_kb_erase_check_%d.bin" % i))
                    for i in range(len(samples))]
    out = run_jlink(jlink, build_commands(args.addr, size, args.speed, samples, sample_files))

    if not erase_ok(out):
        print("[x] 擦除失败：请看上面的 J-Link 输出（板子供电 / 探针连接 / 设备名）")
        print("    如果提示探针被占用（J-Link is already in use），可改用：")
        print("    python script\\erase.py --kill-jlink")
        return 1

    print("[i] J-Link 报告擦除完成，开始读回校验...")
    ok, err = verify_erased(samples, sample_files)
    for f in sample_files:
        Path(f).unlink(missing_ok=True)

    if not ok:
        print("[x] 校验失败：%s" % err)
        print("    flash 可能没被真正擦除，请检查 J-Link 输出或换个速度重试（--speed 2000）。")
        return 1

    print("[OK] 擦除完成并校验通过：抽样区域全为 0xFF（用时 %.1f 秒）" % (time.time() - t0))
    print("[i] 重新烧录：python script\\flash.py")
    return 0


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(errors="replace")
    except Exception:
        pass
    sys.exit(main())
