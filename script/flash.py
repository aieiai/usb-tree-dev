#!/usr/bin/env python3
# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: BSD-3-Clause
"""
使用 J-Link Commander 或工程内置 OpenOCD 烧录 HPM

功能说明：
    1. 从本地化 SDK 的板卡 yaml 读取 openocd-probe 和 openocd-soc；
    2. J-Link 使用 SEGGER Commander，其他探针使用工程内 OpenOCD；
    3. 默认烧录 build/output/<APP_NAME>.bin，烧录后执行 verify 和 reset；
    4. 不访问原 SDK 环境。

参数说明：
    --board     目标板卡，默认自动识别本地化目录
    --probe     调试器类型，默认从板卡 yaml 读取
    --soc       OpenOCD SoC 配置名，默认从板卡 yaml 读取
    --elf       显式选择 ELF 文件；默认使用 BIN
    --bin       显式选择 BIN 文件；不带路径时使用 build/output/<APP_NAME>.bin
    --address   BIN 的 Flash 写入地址，默认从板卡 OpenOCD 配置读取
    --no-verify 烧录后不校验 Flash
    --erase-all 擦除目标芯片的全部 Flash（当前仅支持 J-Link）
    --dry-run   只打印 OpenOCD 命令，不实际烧录

使用示例：
    # 1. 默认烧录 user_app.bin
    python script/flash.py

    # 2. 只查看将要执行的 OpenOCD 命令
    python script/flash.py --dry-run

    # 3. 指定 ELF 文件
    python script/flash.py --elf build/output/user_app.elf

    # 4. 烧录 BIN 文件（默认使用板卡 Flash 起始地址）
    python script/flash.py --bin
    python script/flash.py --bin build/output/user_app.bin
    python script/flash.py --bin build/output/user_app.bin --address 0x80000000

    # 5. 使用 J-Link 调试器
    python script/flash.py --probe jlink

    # 6. 烧录后不校验
    python script/flash.py --no-verify

    # 7. 擦除 J-Link 目标芯片全部 Flash
    python script/flash.py --erase-all

    # 8. 命令行没有 python 时，使用工程内置 Python
    tools\\python3\\python.exe script\\flash.py

注意事项：
    - 实际烧录需要连接开发板和调试器；
    - 如果 FTDI 驱动未安装，先运行 tools\\FTDI_InstallDriver.exe；
    - 烧录前先执行 python script/build.py 生成 ELF 文件。
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# 把 script 目录加入模块搜索路径，便于导入 localize_project
sys.path.insert(0, str(Path(__file__).resolve().parent))
import localize_project as lp

try:
    import winreg
except ImportError:
    winreg = None


def read_board_field(board_yaml, field, default):
    """从板卡 yaml 中读取 openocd-probe / openocd-soc 等字段。"""
    if not board_yaml.is_file():
        return default
    text = board_yaml.read_text(encoding="utf-8")
    match = re.search(r"^\s*%s\s*:\s*(\S+)" % re.escape(field), text, re.M)
    return match.group(1) if match else default


def read_flash_base(board_cfg):
    """Read the flash-bank base address from an OpenOCD board config."""
    if not board_cfg.is_file():
        return None
    text = board_cfg.read_text(encoding="utf-8")
    for line in text.splitlines():
        line = line.split("#", 1)[0]
        match = re.match(
            r"^\s*flash\s+bank\s+\S+\s+\S+\s+(0x[0-9a-fA-F]+|[0-9]+)",
            line,
        )
        if match:
            return int(match.group(1), 0)
    return None


def find_jlink_exe():
    """Find SEGGER J-Link Commander from PATH, environment, or Windows registry."""
    candidates = []
    configured = os.environ.get("JLINK_EXE")
    if configured:
        candidates.append(Path(configured))

    on_path = shutil.which("JLink.exe")
    if on_path:
        candidates.append(Path(on_path))

    if winreg is not None:
        uninstall_keys = (
            r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
            r"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall",
        )
        for key_path in uninstall_keys:
            try:
                root = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key_path)
            except OSError:
                continue
            with root:
                for index in range(winreg.QueryInfoKey(root)[0]):
                    try:
                        subkey_name = winreg.EnumKey(root, index)
                        with winreg.OpenKey(root, subkey_name) as subkey:
                            display_name = winreg.QueryValueEx(subkey, "DisplayName")[0]
                            install_dir = winreg.QueryValueEx(subkey, "InstallLocation")[0]
                    except OSError:
                        continue
                    if "j-link" in display_name.lower() and install_dir:
                        candidates.append(Path(install_dir) / "JLink.exe")

    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def run_jlink_command(jlink, device, command, env, success_text, failure_message):
    """Run a non-interactive SEGGER J-Link Commander command and verify success."""
    if not device:
        lp.fail("板卡 YAML 未提供 device 字段，无法选择 J-Link 目标芯片")

    script_path = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="ascii", suffix=".jlink", delete=False
        ) as command_file:
            command_file.write(command)
            script_path = Path(command_file.name)

        cmd = [
            str(jlink),
            "-device",
            device,
            "-if",
            "JTAG",
            "-speed",
            "4000",
            "-JTAGConf",
            "-1,-1",
            "-autoconnect",
            "1",
            "-CommanderScript",
            str(script_path),
        ]
        print("J-Link 命令：", flush=True)
        print("  " + subprocess.list2cmdline(cmd), flush=True)
        result = subprocess.run(
            cmd,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        print(result.stdout, end="", flush=True)
        if result.returncode != 0:
            raise subprocess.CalledProcessError(result.returncode, cmd)
        if (
            success_text not in result.stdout
            or not re.search(r"(?m)^O\.K\.$", result.stdout)
        ):
            lp.fail(failure_message)
    finally:
        if script_path is not None:
            script_path.unlink(missing_ok=True)


def flash_with_jlink(jlink, device, elf, env):
    """Program and verify an ELF through SEGGER J-Link Commander."""
    command = 'loadfile "%s"\nr\ng\nexit\n' % elf.as_posix()
    run_jlink_command(
        jlink,
        device,
        command,
        env,
        "Flash download:",
        "J-Link 未确认 Flash 烧录和校验成功，请检查上方 Commander 输出",
    )


def erase_all_with_jlink(jlink, device, env):
    """Erase all target flash through SEGGER J-Link Commander."""
    run_jlink_command(
        jlink,
        device,
        "erase\nr\ng\nexit\n",
        env,
        "Erasing device",
        "J-Link 未确认全片擦除成功，请检查上方 Commander 输出",
    )


def main(argv=None):
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description=(
            "使用 J-Link Commander 或工程内 OpenOCD 烧录 HPM 工程。\n"
            "工具与配置均从工程或本机 SEGGER 安装读取，不访问原 SDK。"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "使用示例：\n"
            "  python script/flash.py\n"
            "  python script/flash.py --dry-run\n"
            "  python script/flash.py --elf build/output/user_app.elf\n"
            "  python script/flash.py --probe jlink\n"
            "  python script/flash.py --no-verify\n"
            "  python script/flash.py --erase-all\n"
            "  tools\\python3\\python.exe script\\flash.py\n"
            "\n说明：\n"
            "  烧录前先执行 python script/build.py 生成 ELF；\n"
            "  实际烧录需要连接开发板和调试器。"
        ),
    )
    parser.add_argument(
        "--board",
        help="目标板卡。示例：--board hpm5300evk；"
        "默认：自动识别本地化目录，否则使用 %s" % lp.DEFAULT_BOARD,
    )
    parser.add_argument(
        "--probe",
        help="调试器类型，对应 boards/openocd/probes/<名称>.cfg。"
        "示例：--probe ft2232、--probe jlink、--probe cmsis_dap；"
        "默认：hpm5300evk 使用 jlink，其他板卡从 yaml 读取",
    )
    parser.add_argument(
        "--soc",
        help="OpenOCD SoC 配置名，对应 boards/openocd/soc/<名称>.cfg。"
        "示例：--soc hpm5300；默认：从板卡 yaml 读取",
    )
    parser.add_argument(
        "--elf",
        help="显式烧录 ELF 文件；默认使用 build/output/<APP_NAME>.bin",
    )
    parser.add_argument(
        "--bin",
        nargs="?",
        const="",
        help="烧录 BIN 文件；不带路径时使用 build/output/<APP_NAME>.bin",
    )
    parser.add_argument(
        "--address",
        type=lambda value: int(value, 0),
        help="BIN 的 Flash 写入地址，接受十进制或 0x 十六进制；默认从板卡配置读取",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="烧录后不校验 Flash 内容。示例：python script/flash.py --no-verify",
    )
    parser.add_argument(
        "--erase-all",
        action="store_true",
        help="擦除 J-Link 目标芯片全部 Flash；不会烧录 ELF。当前仅支持 --probe jlink。",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只打印烧录或擦除命令，不实际操作目标芯片。"
        "示例：python script/flash.py --dry-run",
    )
    args = parser.parse_args(argv)

    try:
        if args.erase_all and (args.elf or args.bin is not None or args.address is not None):
            lp.fail("--erase-all 不能与 --elf、--bin 或 --address 同时使用")

        # 确定板卡和本地化 SDK 目录
        board = args.board or lp.detect_localized_board(lp.PROJECT_DIR) or lp.DEFAULT_BOARD
        localized_name = lp.LOCALIZED_PREFIX + board
        localized_dir = lp.PROJECT_DIR / localized_name
        if not localized_dir.is_dir():
            lp.fail(
                "未找到本地化 SDK：%s\n请先执行 script/localize_project.py" % localized_dir
            )

        # 检查本地化 SDK 中是否存在该板卡
        board_yaml = localized_dir / "boards" / board / ("%s.yaml" % board)
        if not board_yaml.is_file():
            available = sorted(
                d.name
                for d in (localized_dir / "boards").iterdir()
                if d.is_dir() and (d / ("%s.yaml" % d.name)).is_file()
            )
            lp.fail(
                "本地化 SDK 中不存在板卡：%s\n可用板卡：%s"
                % (board, "、".join(available) if available else "无")
            )

        # OpenOCD 配置目录位于本地化 SDK 中
        openocd_cfg = localized_dir / "boards" / "openocd"

        # 从板卡 yaml 读取调试器和 SoC 配置名
        board_probe = read_board_field(board_yaml, "openocd-probe", "ft2232")
        probe = args.probe or ("jlink" if board == "hpm5300evk" else board_probe)
        soc = args.soc or read_board_field(board_yaml, "openocd-soc", "hpm5300")

        # J-Link's Windows driver is not accessible to the bundled OpenOCD libusb backend.
        # Use SEGGER Commander for J-Link, retaining OpenOCD for other probes.
        paths = lp.tool_paths(lp.PROJECT_DIR)
        openocd = paths["openocd"] / "openocd.exe"
        jlink = None
        device = None
        cfg_files = ()
        if probe == "jlink":
            jlink = find_jlink_exe()
            if jlink is None:
                lp.fail(
                    "未找到 SEGGER J-Link Commander。安装 J-Link Software Pack，"
                    "或设置环境变量 JLINK_EXE 指向 JLink.exe。"
                )
            device = read_board_field(board_yaml, "device", None)
        else:
            if args.erase_all:
                lp.fail("全片擦除当前仅支持 J-Link；请使用 --probe jlink")
            if not openocd.is_file():
                lp.fail("工程内未找到 OpenOCD：%s" % openocd)
            cfg_files = (
                openocd_cfg / "probes" / ("%s.cfg" % probe),
                openocd_cfg / "soc" / ("%s.cfg" % soc),
                openocd_cfg / "boards" / ("%s.cfg" % board),
            )
            for cfg in cfg_files:
                if not cfg.is_file():
                    lp.fail("缺少 OpenOCD 配置文件：%s" % cfg)

        if args.elf and args.bin is not None:
            lp.fail("--elf 和 --bin 不能同时使用")
        if args.address is not None and args.elf:
            lp.fail("--address 用于 BIN，不能与 --elf 一起使用")

        # 确定待烧录文件；裸 BIN 必须有写入地址
        if args.erase_all:
            firmware = None
            is_bin = False
            flash_address = None
        elif args.bin is not None or not args.elf:
            is_bin = True
            bin_path = args.bin or (
                "build/output/%s.bin" % lp.get_app_name(lp.PROJECT_DIR)
            )
            firmware = Path(bin_path)
            if not firmware.is_absolute():
                firmware = lp.PROJECT_DIR / firmware
            board_cfg = openocd_cfg / "boards" / ("%s.cfg" % board)
            flash_address = args.address
            if flash_address is None:
                flash_address = read_flash_base(board_cfg)
            if flash_address is None:
                lp.fail(
                    "无法从板卡 OpenOCD 配置读取 Flash 起始地址，"
                    "请用 --address 0x... 指定 BIN 写入地址"
                )
        elif args.elf:
            is_bin = False
            firmware = Path(args.elf)
            if not firmware.is_absolute():
                firmware = lp.PROJECT_DIR / firmware
            flash_address = None
        else:
            is_bin = False
            firmware = Path(args.elf)
            if not firmware.is_absolute():
                firmware = lp.PROJECT_DIR / firmware
            flash_address = None

        # 烧录开始前打印本次工程和烧录信息
        project_name = lp.get_app_name(lp.PROJECT_DIR)
        print("=" * 68, flush=True)
        print("本次烧录信息", flush=True)
        print("-" * 68, flush=True)
        print("工程根目录      : %s" % lp.PROJECT_DIR, flush=True)
        print("工程名称        : %s" % project_name, flush=True)
        print("目标板卡        : %s" % board, flush=True)
        print("调试器          : %s" % probe, flush=True)
        if probe == "jlink":
            print("J-Link 目标芯片 : %s" % device, flush=True)
        else:
            print("OpenOCD SoC     : %s" % soc, flush=True)
        print(
            "操作            : %s"
            % (
                "全片擦除"
                if args.erase_all
                else ("烧录 BIN" if is_bin else "烧录 ELF")
            ),
            flush=True,
        )
        if firmware is not None:
            print("待烧录文件      : %s" % firmware, flush=True)
        if is_bin:
            print("BIN 写入地址    : 0x%08X" % flash_address, flush=True)
        if probe == "jlink":
            print("J-Link 程序     : %s" % jlink, flush=True)
        else:
            print("OpenOCD 程序    : %s" % openocd, flush=True)
            print("OpenOCD 配置    : %s" % openocd_cfg, flush=True)
        if args.erase_all:
            print("擦除后校验      : J-Link 擦除命令", flush=True)
        else:
            print(
                "烧录后校验      : %s" % ("否" if args.no_verify else "是"),
                flush=True,
            )
        print("=" * 68, flush=True)

        if firmware is not None and not firmware.is_file():
            lp.fail(
                "未找到待烧录文件：%s\n请先执行 script/build.py 或检查路径"
                % firmware
            )

        if args.dry_run:
            if probe == "jlink":
                if args.erase_all:
                    operation = "erase"
                elif is_bin:
                    operation = "loadbin <BIN> 0x%08X; reset; go" % flash_address
                else:
                    operation = "loadfile <ELF>; reset; go"
                print(
                    "J-Link 命令：JLink.exe -device %s -if JTAG -speed 4000 "
                    "-JTAGConf -1,-1 -autoconnect 1 "
                    "-CommanderScript <临时命令文件>（%s）"
                    % (device, operation),
                    flush=True,
                )
            else:
                verify = "" if args.no_verify else "verify "
                address_arg = " 0x%08X" % flash_address if is_bin else ""
                program_cmd = "program {%s}%s %sreset exit" % (
                    firmware.as_posix(),
                    address_arg,
                    verify,
                )
                cmd = [
                    str(openocd),
                    "-f",
                    str(cfg_files[0]),
                    "-f",
                    str(cfg_files[1]),
                    "-f",
                    str(cfg_files[2]),
                    "-c",
                    program_cmd,
                ]
                print("OpenOCD 命令：", flush=True)
                print("  " + subprocess.list2cmdline(cmd), flush=True)
            print("已跳过实际烧录（--dry-run）。", flush=True)
            return 0

        # 使用工程内环境变量，确保不引用原 SDK
        env = lp.make_tool_env(lp.PROJECT_DIR, localized_name)
        if args.erase_all:
            print("正在擦除目标芯片全部 Flash ...", flush=True)
            erase_all_with_jlink(jlink, device, env)
        elif probe == "jlink":
            print("正在烧录 %s ..." % firmware, flush=True)
            if is_bin:
                command = 'loadbin "%s" 0x%08X\nr\ng\nexit\n' % (
                    firmware.as_posix(),
                    flash_address,
                )
                run_jlink_command(
                    jlink,
                    device,
                    command,
                    env,
                    "Flash download:",
                    "J-Link 未确认 BIN 烧录和校验成功，请检查上方 Commander 输出",
                )
            else:
                flash_with_jlink(jlink, device, firmware, env)
        else:
            verify = "" if args.no_verify else "verify "
            address_arg = " 0x%08X" % flash_address if is_bin else ""
            program_cmd = "program {%s}%s %sreset exit" % (
                firmware.as_posix(),
                address_arg,
                verify,
            )
            cmd = [
                str(openocd),
                "-f",
                str(cfg_files[0]),
                "-f",
                str(cfg_files[1]),
                "-f",
                str(cfg_files[2]),
                "-c",
                program_cmd,
            ]
            print("正在烧录 %s ..." % firmware, flush=True)
            subprocess.run(cmd, env=env, check=True)
        print("全片擦除完成。" if args.erase_all else "烧录完成。", flush=True)
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print("\n错误：%s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
