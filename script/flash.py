#!/usr/bin/env python3
# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: BSD-3-Clause
"""
使用工程内置 OpenOCD 烧录 HPM

功能说明：
    1. 从本地化 SDK 的板卡 yaml 读取 openocd-probe 和 openocd-soc；
    2. 使用工程内 tools/openocd/openocd.exe 和本地化 SDK 的 OpenOCD 配置；
    3. 默认烧录 build/output/<APP_NAME>.elf，烧录后执行 verify 和 reset；
    4. 不访问原 SDK 环境。

参数说明：
    --board     目标板卡，默认自动识别本地化目录
    --probe     调试器类型，默认从板卡 yaml 读取
    --soc       OpenOCD SoC 配置名，默认从板卡 yaml 读取
    --elf       待烧录 ELF 文件，默认 build/output/<APP_NAME>.elf
    --no-verify 烧录后不校验 Flash
    --dry-run   只打印 OpenOCD 命令，不实际烧录

使用示例：
    # 1. 使用默认配置烧录 user_app.elf
    python script/flash.py

    # 2. 只查看将要执行的 OpenOCD 命令
    python script/flash.py --dry-run

    # 3. 指定 ELF 文件
    python script/flash.py --elf build/output/user_app.elf

    # 4. 使用 J-Link 调试器
    python script/flash.py --probe jlink

    # 5. 烧录后不校验
    python script/flash.py --no-verify

    # 6. 命令行没有 python 时，使用工程内置 Python
    tools\\python3\\python.exe script\\flash.py

注意事项：
    - 实际烧录需要连接开发板和调试器；
    - 如果 FTDI 驱动未安装，先运行 tools\\FTDI_InstallDriver.exe；
    - 烧录前先执行 python script/build.py 生成 ELF 文件。
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

# 把 script 目录加入模块搜索路径，便于导入 localize_project
sys.path.insert(0, str(Path(__file__).resolve().parent))
import localize_project as lp


def detect_localized_board(project_dir):
    """从工程中的本地化 SDK 目录推断板卡名称。"""
    candidates = [
        d
        for d in project_dir.iterdir()
        if d.is_dir() and d.name.startswith(lp.LOCALIZED_PREFIX)
    ]
    if len(candidates) == 1:
        return candidates[0].name[len(lp.LOCALIZED_PREFIX):]
    return None


def read_board_field(board_yaml, field, default):
    """从板卡 yaml 中读取 openocd-probe / openocd-soc 等字段。"""
    if not board_yaml.is_file():
        return default
    text = board_yaml.read_text(encoding="utf-8")
    match = re.search(r"^\s*%s\s*:\s*(\S+)" % re.escape(field), text, re.M)
    return match.group(1) if match else default


def main(argv=None):
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description=(
            "使用工程内 OpenOCD 烧录 HPM 工程。\n"
            "烧录只使用 tools/openocd 和本地化 SDK 配置，不访问原 SDK。"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "使用示例：\n"
            "  python script/flash.py\n"
            "  python script/flash.py --dry-run\n"
            "  python script/flash.py --elf build/output/user_app.elf\n"
            "  python script/flash.py --probe jlink\n"
            "  python script/flash.py --no-verify\n"
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
        "默认：从板卡 yaml 读取（hpm5300evk 为 ft2232）",
    )
    parser.add_argument(
        "--soc",
        help="OpenOCD SoC 配置名，对应 boards/openocd/soc/<名称>.cfg。"
        "示例：--soc hpm5300；默认：从板卡 yaml 读取",
    )
    parser.add_argument(
        "--elf",
        help="待烧录的 ELF 文件。示例：--elf build/output/user_app.elf；"
        "默认：build/output/<APP_NAME>.elf",
    )
    parser.add_argument(
        "--no-verify",
        action="store_true",
        help="烧录后不校验 Flash 内容。示例：python script/flash.py --no-verify",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="只打印 OpenOCD 命令，不实际烧录。"
        "示例：python script/flash.py --dry-run",
    )
    args = parser.parse_args(argv)

    try:
        # 确定板卡和本地化 SDK 目录
        board = args.board or detect_localized_board(lp.PROJECT_DIR) or lp.DEFAULT_BOARD
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

        # 检查工程内 tools/openocd 是否已复制
        paths = lp.tool_paths(lp.PROJECT_DIR)
        openocd = paths["openocd"] / "openocd.exe"
        if not openocd.is_file():
            lp.fail("工程内未找到 OpenOCD：%s" % openocd)

        # OpenOCD 配置目录位于本地化 SDK 中
        openocd_cfg = localized_dir / "boards" / "openocd"

        # 从板卡 yaml 读取调试器和 SoC 配置名
        probe = args.probe or read_board_field(board_yaml, "openocd-probe", "ft2232")
        soc = args.soc or read_board_field(board_yaml, "openocd-soc", "hpm5300")

        # 校验三个 OpenOCD 配置文件是否存在
        cfg_files = (
            openocd_cfg / "probes" / ("%s.cfg" % probe),
            openocd_cfg / "soc" / ("%s.cfg" % soc),
            openocd_cfg / "boards" / ("%s.cfg" % board),
        )
        for cfg in cfg_files:
            if not cfg.is_file():
                lp.fail("缺少 OpenOCD 配置文件：%s" % cfg)

        # 确定待烧录的 ELF 文件；相对路径按工程根目录解析
        if args.elf:
            elf = Path(args.elf)
            if not elf.is_absolute():
                elf = lp.PROJECT_DIR / elf
        else:
            elf = lp.PROJECT_DIR / "build" / "output" / (
                "%s.elf" % lp.get_app_name(lp.PROJECT_DIR)
            )

        # 烧录开始前打印本次工程和烧录信息
        project_name = lp.get_app_name(lp.PROJECT_DIR)
        print("=" * 68, flush=True)
        print("本次烧录信息", flush=True)
        print("-" * 68, flush=True)
        print("工程根目录      : %s" % lp.PROJECT_DIR, flush=True)
        print("工程名称        : %s" % project_name, flush=True)
        print("目标板卡        : %s" % board, flush=True)
        print("调试器          : %s" % probe, flush=True)
        print("OpenOCD SoC     : %s" % soc, flush=True)
        print("待烧录 ELF      : %s" % elf, flush=True)
        print("OpenOCD 程序    : %s" % openocd, flush=True)
        print("OpenOCD 配置    : %s" % openocd_cfg, flush=True)
        print(
            "烧录后校验      : %s" % ("否" if args.no_verify else "是"),
            flush=True,
        )
        print("=" * 68, flush=True)

        if not elf.is_file():
            lp.fail(
                "未找到 ELF 文件：%s\n请先执行 script/build.py" % elf
            )

        # 组装 OpenOCD 命令：先加载调试器、SoC、板卡配置，再执行 program
        verify = "" if args.no_verify else "verify "
        program_cmd = "program {%s} %sreset exit" % (elf.as_posix(), verify)
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

        # 显示即将执行的命令，便于排查
        print("OpenOCD 命令：", flush=True)
        print("  " + subprocess.list2cmdline(cmd), flush=True)

        if args.dry_run:
            print("已跳过实际烧录（--dry-run）。", flush=True)
            return 0

        # 使用工程内环境变量，确保不引用原 SDK
        env = lp.make_tool_env(lp.PROJECT_DIR, localized_name)
        print("正在烧录 %s ..." % elf, flush=True)
        subprocess.run(cmd, env=env, check=True)
        print("烧录完成。", flush=True)
    except (RuntimeError, subprocess.CalledProcessError) as exc:
        print("\n错误：%s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
