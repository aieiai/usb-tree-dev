#!/usr/bin/env python3
# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: BSD-3-Clause
"""
检查本工程的编译环境，缺失时从 HPM SDK 环境补齐。

功能说明：
    1. 逐项检查本地化 SDK、localized_sdk.cmake、构建工具（tools/）、
       编译工具链（toolchain/）；
    2. 全部就绪时只打印状态和后续命令，不写任何文件；
    3. 缺内容时定位原 HPM SDK 环境，按 localize_project.py 的流程补齐；
    4. --check 只检查不写盘，用退出码表示结果，适合新机器自检或 CI。

参数说明：
    --sdk-env         原 HPM SDK 环境根目录，默认 E:\\HPM\\hpm_sdk_env 或环境变量 HPM_SDK_ENV
    --sdk             直接指定 hpm_sdk 目录，优先级高于 --sdk-env
    --board           目标板卡，默认自动识别本地化目录，否则 hpm5300evk
    --hpm-build-type  HPM 构建类型，默认 flash_xip
    --force           重新生成本地化 SDK
    --check           只检查不写文件，环境不完整时返回 1

使用示例：
    # 1. 检查并补齐工程环境
    python script/setup.py

    # 2. 只检查，不修改任何文件
    python script/setup.py --check

    # 3. 指定 SDK 环境并重新生成本地化 SDK
    python script/setup.py --sdk-env E:\\HPM\\hpm_sdk_env --force

    # 4. 命令行没有 python 时，使用工程内置 Python
    tools\\python3\\python.exe script\\setup.py

注意事项：
    - tools/ 和 toolchain/ 不随仓库分发，克隆后需要补齐（或从旧工程复制）；
    - 本地化 SDK 随仓库提交，克隆后可直接编译；需要重新生成时加 --force；
    - 补齐需要完整的 HPM SDK 环境（含 hpm_sdk/、tools/、toolchains/），
      仅做 --check 时不需要。
"""

import argparse
import subprocess
import sys
from pathlib import Path

# 把 script 目录加入模块搜索路径，便于导入 localize_project
sys.path.insert(0, str(Path(__file__).resolve().parent))
import localize_project as lp


def pad_label(label, width=16):
    """按显示宽度补空格：中文按两列计算，让状态输出的冒号对齐。"""
    display = sum(2 if ord(ch) > 0x2E80 else 1 for ch in label)
    return label + " " * max(width - display, 0)


def check_items(project_dir, board):
    """返回 [(检查项名称, 路径, 是否就绪)]，用于逐项显示环境状态。"""
    paths = lp.tool_paths(project_dir)
    localized_dir = project_dir / (lp.LOCALIZED_PREFIX + board)
    config_file = project_dir / "localized_sdk.cmake"
    compiler = paths["toolchain"] / "bin" / "riscv32-unknown-elf-gcc.exe"
    return [
        ("本地化 SDK", localized_dir, localized_dir.is_dir()),
        ("SDK 路径配置", config_file, config_file.is_file()),
        ("构建工具", paths["tools_root"], lp.tools_are_ready(project_dir)),
        ("编译工具链", paths["toolchain"], compiler.is_file()),
    ]


def print_status(project_dir, board, items):
    """打印工程信息和逐项检查结果。"""
    print("=" * 68, flush=True)
    print("工程环境自检", flush=True)
    print("-" * 68, flush=True)
    print("%s: %s" % (pad_label("工程根目录"), project_dir), flush=True)
    print("%s: %s" % (pad_label("工程名称"), lp.get_app_name(project_dir)), flush=True)
    print("%s: %s" % (pad_label("目标板卡"), board), flush=True)
    for label, path, ok in items:
        print(
            "%s: %s（%s）" % (pad_label(label), path, "就绪" if ok else "缺失"),
            flush=True,
        )
    print("=" * 68, flush=True)


def print_next_steps():
    """打印环境就绪后的常用命令。"""
    print("编译：python script/build.py", flush=True)
    print("烧录：python script/flash.py", flush=True)


def main(argv=None):
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description=(
            "检查本工程的编译环境，缺失时从 HPM SDK 环境补齐。\n"
            "全部就绪时不做任何修改，只打印状态。"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "使用示例：\n"
            "  python script/setup.py\n"
            "  python script/setup.py --check\n"
            "  python script/setup.py --sdk-env E:\\HPM\\hpm_sdk_env --force\n"
            "\n说明：\n"
            "  --check 只检查不写文件，环境不完整时退出码为 1；\n"
            "  补齐需要完整的 HPM SDK 环境，本地化 SDK 已入库时可跳过补齐。"
        ),
    )
    parser.add_argument(
        "--sdk-env",
        help="仅在需要补齐时使用，原 HPM SDK 环境根目录。"
        "示例：--sdk-env E:\\HPM\\hpm_sdk_env；默认：%s" % lp.DEFAULT_SDK_ENV,
    )
    parser.add_argument(
        "--sdk",
        help="仅在需要补齐时使用，直接指定 hpm_sdk 目录，优先级高于 --sdk-env。"
        "示例：--sdk E:\\HPM\\hpm_sdk_env\\hpm_sdk",
    )
    parser.add_argument(
        "--board",
        help="目标板卡。示例：--board hpm5300evk；"
        "默认：自动识别本地化目录，否则使用 %s" % lp.DEFAULT_BOARD,
    )
    parser.add_argument(
        "--hpm-build-type",
        default=lp.DEFAULT_HPM_BUILD_TYPE,
        choices=lp.SUPPORTED_BUILD_TYPES,
        help="HPM 构建类型。示例：--hpm-build-type flash_xip；"
        "默认：%s" % lp.DEFAULT_HPM_BUILD_TYPE,
    )
    parser.add_argument(
        "-f",
        "--force",
        action="store_true",
        help="重新生成本地化 SDK。示例：python script/setup.py --force",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="只检查不写文件，环境不完整时返回 1。示例：python script/setup.py --check",
    )
    args = parser.parse_args(argv)

    try:
        # 确定板卡：命令行参数 > 本地化目录推断 > 默认板卡
        board = args.board or lp.detect_localized_board(lp.PROJECT_DIR) or lp.DEFAULT_BOARD
        items = check_items(lp.PROJECT_DIR, board)
        print_status(lp.PROJECT_DIR, board, items)
        missing = [name for name, _path, ok in items if not ok]

        if not missing and not args.force:
            print("工程环境完整，无需补齐。", flush=True)
            print_next_steps()
            return 0

        if args.check:
            if missing:
                print("缺少：%s" % "、".join(missing), flush=True)
                print(
                    "运行 python script/setup.py 补齐（需要 HPM SDK 环境）。",
                    flush=True,
                )
                return 1
            print("已指定 --force，但 --check 不写文件，跳过重新生成。", flush=True)
            return 0

        # 补齐需要原 SDK 环境：路径无效时给出明确的准备步骤
        try:
            sdk_base, sdk_env = lp.resolve_paths(args.sdk, args.sdk_env)
        except RuntimeError as exc:
            print("\n错误：%s" % exc, file=sys.stderr)
            print(
                "补齐工程环境需要完整的 HPM SDK 环境（含 hpm_sdk/、tools/、toolchains/）：\n"
                "  1. 解压 HPM SDK 环境包到本地目录；\n"
                "  2. 用 --sdk-env <SDK 环境目录> 指定，或设置环境变量 HPM_SDK_ENV；\n"
                "  3. 重新执行 python script/setup.py。",
                file=sys.stderr,
            )
            return 1

        print("开始补齐工程环境（SDK 环境：%s）..." % sdk_env, flush=True)
        lp.localize(
            lp.PROJECT_DIR,
            sdk_base,
            sdk_env,
            board,
            args.hpm_build_type,
            lp.DEFAULT_SAMPLE,
            None,
            args.force,
            False,
        )
    except subprocess.CalledProcessError as exc:
        print(
            "\n错误：补齐过程中命令执行失败，退出码 %s。\n"
            "请查看上方 CMake/SDK 输出。" % exc.returncode,
            file=sys.stderr,
        )
        return 1
    except RuntimeError as exc:
        print("\n错误：%s" % exc, file=sys.stderr)
        return 1

    print("工程环境已就绪。", flush=True)
    print_next_steps()
    return 0


if __name__ == "__main__":
    sys.exit(main())
