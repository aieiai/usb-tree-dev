#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
flash.py - 用 J-Link 命令行烧录 GS_KB_V75（不需要打开 SES）

流程：
  1. 用 JLink.exe 把固件写进外部 XPI NOR（默认 flash_xip 的 .bin @ 0x80000400）；
     不会关闭你正在运行的 JLink 会话（需要独占探针时加 --kill-jlink）；
  2. 读回校验（.bin 用 verifybin）；
  3. 复位并运行；
  4. 默认自动重启 RTT / SecureCRT 会话（--no-rtt 可关闭）。

用法：
    python flash.py                # 烧录 build_cmake\output\GS_KB_V75.bin，并恢复 RTT
    python flash.py --no-rtt       # 只烧录，不动 RTT / JLink 会话
    python flash.py --erase        # 先整片擦除 NOR 再烧录（干净烧录）
    python flash.py --kill-jlink   # 探针被占用时：先关掉已有 JLink.exe 再烧录
    python flash.py D:\path\x.elf  # 烧录指定文件（.elf/.hex 用 loadfile，.bin 用 loadbin）
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
import erase as nor_erase  # noqa: E402  复用整片擦除命令与 0xFF 抽样校验

DEFAULT_IMAGE = PROJECT_DIR / "build_cmake" / "output" / "GS_KB_V75.bin"
FLASH_ADDR = "0x80000400"  # 链接脚本 __nor_cfg_option_load_addr__ = ORIGIN(FLASH) + 0x400


def kill_jlink():
    pids = rtt.tasklist_pids("JLink.exe")
    if not pids:
        return
    print("[i] 先关闭正在运行的 JLink.exe：%s" % ", ".join(str(p) for p in pids))
    for pid in pids:
        rtt.run_hidden(["taskkill", "/PID", str(pid), "/F"])
    time.sleep(1.0)


def make_erase_samples(image):
    """--erase 模式的读回抽样点：避开固件即将覆盖的区域（那里由 verifybin 负责校验）。"""
    prog_end = int(FLASH_ADDR, 16) + image.stat().st_size
    # flash 头部 0x80000000 ~ 0x800003FF：固件从 0x80000400 开始，永远烧不到这里
    pts = [(nor_erase.FLASH_BASE, 0x400)]
    tail = nor_erase.FLASH_BASE + nor_erase.FLASH_SIZE - 0x400
    if tail >= prog_end:  # 固件大到盖住尾部时才跳过尾部抽样
        pts.append((tail, 0x400))
    return pts


def build_commands(image, speed, erase=False, samples=(), sample_files=()):
    cmds = [
        "si JTAG",
        "speed %s" % speed,
        "device HPM5361xCBx",
        "JTAGConf -1, -1",
        "connect",
    ]
    if erase:
        # 先整片擦除外部 NOR（外部 flash bank 必须先 EnableEraseAllFlashBanks，否则不会真擦；
        # erase 格式是「起始地址, 结束地址」，不是长度！）
        cmds += nor_erase.erase_commands(nor_erase.FLASH_BASE, nor_erase.FLASH_SIZE)
        # 擦完立刻抽样读回（趁固件还没写进去），事后用 python 校验是否全 0xFF
        for (sa, slen), f in zip(samples, sample_files):
            cmds.append('savebin "%s", 0x%08X, 0x%X' % (f, sa, slen))
    if image.suffix.lower() == ".bin":
        cmds.append('loadbin "%s" %s' % (image, FLASH_ADDR))
        cmds.append('verifybin "%s" %s' % (image, FLASH_ADDR))
    else:
        cmds.append('loadfile "%s"' % image)
    cmds += ["r", "g", "exit"]
    return cmds


def run_jlink(jlink, cmds):
    script = Path(tempfile.gettempdir()) / "gs_kb_flash.jlink"
    script.write_text("\n".join(cmds) + "\n", encoding="mbcs", errors="replace")
    proc = subprocess.run([str(jlink), "-CommanderScript", str(script)],
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = proc.stdout.decode("mbcs", errors="replace")
    print(out)
    return out


def flash_ok(out):
    bad = ("****** Error", "Failed to", "ERROR:", "Cannot connect",
           "mustn't", "must not", "Erasing failed", "No Flash bank within")
    return not any(b in out for b in bad)


def main():
    parser = argparse.ArgumentParser(description="用 J-Link 命令行烧录 GS_KB_V75（不需要打开 SES）")
    parser.add_argument("image", nargs="?", help="要烧录的固件（默认 build_cmake\\output\\GS_KB_V75.bin）")
    parser.add_argument("--speed", default="4000", help="JTAG 速度 kHz（默认 4000）")
    parser.add_argument("--no-rtt", action="store_true", help="烧录后不重启 RTT 会话")
    parser.add_argument("--erase", action="store_true", help="烧录前先整片擦除 NOR（干净烧录）")
    parser.add_argument("--jlink", help="JLink.exe 路径（默认自动查找）")
    parser.add_argument("--kill-jlink", action="store_true",
                        help="烧录前先关掉正在运行的 JLink.exe（旧行为，一般不需要）")
    args = parser.parse_args()

    image = Path(args.image).expanduser() if args.image else DEFAULT_IMAGE
    if not image.is_file():
        print("[x] 找不到要烧录的文件：%s" % image)
        print("    先运行 script\\build.py 编译（或 python script\\build_flash.py），或把文件路径作为参数传给本脚本。")
        return 2

    jlink = rtt.find_jlink(args.jlink)
    if jlink is None:
        print("[x] 找不到 JLink.exe，请用 --jlink 指定")
        return 2

    print("=== 烧录 J-Link RTT 固件 ===")
    print("[i] 固件   : %s" % image)
    if image.suffix.lower() == ".bin":
        print("[i] 地址   : %s" % FLASH_ADDR)
    if args.erase:
        print("[i] 模式   : 先整片擦除 NOR（0x%08X ~ 0x%08X）再烧录"
              % (nor_erase.FLASH_BASE, nor_erase.FLASH_BASE + nor_erase.FLASH_SIZE - 1))
    print("[i] JLink  : %s" % jlink)
    print("[i] 速度   : %s kHz" % args.speed)

    if args.kill_jlink:
        kill_jlink()
    else:
        pids = rtt.tasklist_pids("JLink.exe")
        if pids:
            print("[i] 检测到正在运行的 JLink.exe：%s（保留它，直接烧录）" % ", ".join(str(p) for p in pids))

    print("[+] 开始烧录（几秒钟）...")
    t0 = time.time()

    samples = ()
    sample_files = ()
    if args.erase:
        samples = make_erase_samples(image)
        sample_files = [str(Path(tempfile.gettempdir()) / ("gs_kb_flash_erase_check_%d.bin" % i))
                        for i in range(len(samples))]

    out = run_jlink(jlink, build_commands(image, args.speed,
                                          erase=args.erase, samples=samples,
                                          sample_files=sample_files))
    ok = flash_ok(out)

    if ok:
        print("[OK] 烧录完成（用时 %.1f 秒）" % (time.time() - t0))
    else:
        print("[x] 烧录失败：请看上面的 J-Link 输出（板子供电 / 探针连接 / 设备名）")
        print("    如果提示探针被占用（J-Link is already in use），可改用：")
        print("    python script\\flash.py --kill-jlink")
        return 1

    if args.erase:
        print("[i] 校验整片擦除结果（抽样读回）...")
        eok, err = nor_erase.verify_erased(samples, sample_files)
        for f in sample_files:
            Path(f).unlink(missing_ok=True)
        if not eok:
            print("[x] 整片擦除校验失败：%s" % err)
            print("    固件本身已烧录并校验通过，但 flash 其他区域可能残留旧数据。")
            return 1
        print("[OK] 整片擦除校验通过：固件区域之外全为 0xFF")

    if not args.no_rtt:
        print("[+] 自动重启 RTT / SecureCRT 会话...")
        return subprocess.call([sys.executable, str(SCRIPT_DIR / "open_jlink_rtt.py")])
    return 0


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(errors="replace")
    except Exception:
        pass
    sys.exit(main())
