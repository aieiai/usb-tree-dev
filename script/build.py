#!/usr/bin/env python3
# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: BSD-3-Clause
"""
编译当前 HPM 工程。

功能说明：
    1. 工程已完成本地化时，只使用工程内 hpm_sdk_localized_for_<board>、
       tools/（构建工具）和 toolchain/<工具链名>/（编译器），完全不访问原 SDK；
    2. 本地化 SDK 或工具缺失时，才访问原 SDK 并自动调用 localize_project.py 补全；
    3. 支持 clean、release、ram/flash_xip 等编译方式。

参数说明：
    --sdk-env         补全本地化内容时使用的原 SDK 环境目录
    --sdk             直接指定 hpm_sdk 目录，优先级高于 --sdk-env
    --board           目标板卡，默认自动识别本地化目录
    --hpm-build-type  HPM 构建类型，默认 flash_xip
    --config          编译配置，可选 debug / release，默认 debug
    --release         等同于 --config release
    --clean           编译前删除 build/ 目录
    --refresh-toolchain 重新复制 tools/ 和 toolchain/（需要原 SDK）

使用示例：
    # 1. 默认编译（flash_xip + debug）
    python script/build.py

    # 2. 清理后重新编译
    python script/build.py --clean

    # 3. 生成 RAM 调试版本
    python script/build.py --hpm-build-type ram

    # 4. Release 编译
    python script/build.py --config release

    # 5. 指定板卡
    python script/build.py --board hpm5300evk

    # 6. 命令行没有 python 时，使用工程内置 Python
    tools\\python3\\python.exe script\\build.py

注意事项：
    - 首次编译若工程尚未本地化，会自动执行本地化，此时需要原 SDK；
    - 本地化完成后可删除或移走原 SDK，不影响编译。
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

# 把 script 目录加入模块搜索路径，便于导入 localize_project
sys.path.insert(0, str(Path(__file__).resolve().parent))
import localize_project as lp


def find_cmake_in_project(project_dir):
    """从工程内 tools/cmake 查找 CMake 可执行文件。"""
    paths = lp.tool_paths(project_dir)
    for candidate in (
        paths["cmake"] / "bin" / "cmake.exe",
        paths["cmake"] / "bin" / "cmake",
    ):
        if candidate.is_file():
            return candidate
    lp.fail("工程内未找到 CMake，请重新执行 script/localize_project.py")
    return None


def main(argv=None):
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description=(
            "编译已本地化的 HPM 工程。\n"
            "已本地化时只使用工程内 tools/ 和 toolchain/，不访问原 SDK。"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "使用示例：\n"
            "  python script/build.py\n"
            "  python script/build.py --clean\n"
            "  python script/build.py --hpm-build-type ram\n"
            "  python script/build.py --config release\n"
            "  tools\\python3\\python.exe script\\build.py\n"
            "\n说明：\n"
            "  首次编译且工程未本地化时，会自动调用 localize_project.py；\n"
            "  本地化完成后编译不再访问原 SDK。"
        ),
    )
    parser.add_argument(
        "--sdk-env",
        help="仅在本地化 SDK 或工具缺失、需要补全时使用。"
        "示例：--sdk-env E:\\HPM\\hpm_sdk_env；默认：%s" % lp.DEFAULT_SDK_ENV,
    )
    parser.add_argument(
        "--sdk",
        help="仅在需要补全时使用，直接指定 hpm_sdk 目录。"
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
        help="HPM 构建类型。示例：--hpm-build-type flash_xip、--hpm-build-type ram；"
        "默认：%s" % lp.DEFAULT_HPM_BUILD_TYPE,
    )
    parser.add_argument(
        "--config",
        default="debug",
        choices=("debug", "release"),
        help="编译配置。示例：--config debug、--config release；默认：debug",
    )
    parser.add_argument(
        "--release",
        action="store_true",
        help="等同于 --config release。示例：python script/build.py --release",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="编译前删除 build/ 目录。示例：python script/build.py --clean",
    )
    parser.add_argument(
        "--refresh-toolchain",
        action="store_true",
        help="重新复制 tools/ 和 toolchain/（需要原 SDK）。"
        "示例：python script/build.py --refresh-toolchain",
    )
    args = parser.parse_args(argv)

    try:
        # 确定板卡：命令行参数 > 本地化目录推断 > 默认板卡
        board = args.board or lp.detect_localized_board(lp.PROJECT_DIR) or lp.DEFAULT_BOARD
        localized_name = lp.LOCALIZED_PREFIX + board
        localized_dir = lp.PROJECT_DIR / localized_name
        project_name = lp.get_app_name(lp.PROJECT_DIR)
        config = "release" if args.release else args.config
        paths = lp.tool_paths(lp.PROJECT_DIR)
        tools_ready = lp.tools_are_ready(lp.PROJECT_DIR)

        # 编译开始前打印本次工程和编译信息
        print("=" * 68, flush=True)
        print("本次编译信息", flush=True)
        print("-" * 68, flush=True)
        print("工程根目录      : %s" % lp.PROJECT_DIR, flush=True)
        print("工程名称        : %s" % project_name, flush=True)
        print("目标板卡        : %s" % board, flush=True)
        print("HPM 构建类型    : %s" % args.hpm_build_type, flush=True)
        print("编译配置        : %s" % config, flush=True)
        print(
            "本地化 SDK 目录 : %s（%s）"
            % (
                localized_dir,
                "已本地化" if localized_dir.is_dir() else "缺失，将自动本地化",
            ),
            flush=True,
        )
        print(
            "构建工具目录    : %s（%s）"
            % (paths["tools_root"], "已就绪" if tools_ready else "未复制或不完整"),
            flush=True,
        )
        print("编译工具链目录  : %s" % paths["toolchain"], flush=True)
        print("输出目录        : %s" % (lp.PROJECT_DIR / "build" / "output"), flush=True)
        print(
            "输出文件        : %s.elf / %s.bin" % (project_name, project_name),
            flush=True,
        )
        print("=" * 68, flush=True)

        # 判断是否需要访问原 SDK：本地化 SDK 缺失、工具缺失或要求刷新工具
        need_sdk = (
            not localized_dir.is_dir()
            or not lp.tools_are_ready(lp.PROJECT_DIR)
            or args.refresh_toolchain
        )

        if need_sdk:
            sdk_base, sdk_env = lp.resolve_paths(args.sdk, args.sdk_env)

            if not localized_dir.is_dir():
                # 首次编译：先完成 SDK 本地化和工具复制
                print("检测到本地化 SDK 不存在，先执行本地化...", flush=True)
                lp.localize(
                    lp.PROJECT_DIR,
                    sdk_base,
                    sdk_env,
                    board,
                    args.hpm_build_type,
                    lp.DEFAULT_SAMPLE,
                    None,
                    False,
                    args.refresh_toolchain,
                )
            else:
                # SDK 已本地化，仅补全工程内工具
                lp.bundle_tools(sdk_env, lp.PROJECT_DIR, args.refresh_toolchain)

        # 校验板卡和构建类型；例如 HPM5300 不支持 flash_sdram_xip
        lp.validate_board_and_build_type(
            localized_dir, board, args.hpm_build_type
        )

        # 每次编译都刷新相对路径配置，确保 localized_sdk.cmake 不会丢失
        lp.write_localized_cmake(
            lp.PROJECT_DIR, localized_name, board, args.hpm_build_type
        )

        # 到这里所有工具都应来自工程内部，不再访问原 SDK
        if not lp.tools_are_ready(lp.PROJECT_DIR):
            lp.fail("工程内工具或工具链不完整，请重新执行 script/localize_project.py --force")

        cmake = find_cmake_in_project(lp.PROJECT_DIR)
        env = lp.make_tool_env(lp.PROJECT_DIR, localized_name)
        build_dir = lp.PROJECT_DIR / "build"

        # 需要清理时删除整个 build 目录
        if args.clean and build_dir.exists():
            shutil.rmtree(build_dir)

        print(
            "开始编译：工程=%s，板卡=%s，构建类型=%s，配置=%s"
            % (project_name, board, args.hpm_build_type, config),
            flush=True,
        )

        # 配置工程：所有路径均由 localized_sdk.cmake 指向工程内部
        subprocess.run(
            [
                str(cmake),
                "-G",
                lp.GENERATOR,
                "-S",
                str(lp.PROJECT_DIR),
                "-B",
                str(build_dir),
                "-DBOARD=%s" % board,
                "-DHPM_BUILD_TYPE=%s" % args.hpm_build_type,
                "-DCMAKE_BUILD_TYPE=%s" % config,
            ],
            env=env,
            check=True,
        )

        # 执行编译
        subprocess.run([str(cmake), "--build", str(build_dir)], env=env, check=True)

        # 打印输出文件位置和大小
        app_name = lp.get_app_name(lp.PROJECT_DIR)
        elf = build_dir / "output" / ("%s.elf" % app_name)
        binary = build_dir / "output" / ("%s.bin" % app_name)
        if elf.is_file():
            print("ELF：%s" % elf, flush=True)
            if binary.is_file():
                print(
                    "BIN：%s（%d 字节）" % (binary, binary.stat().st_size),
                    flush=True,
                )
    except subprocess.CalledProcessError as exc:
        print(
            "\n错误：编译命令执行失败，退出码 %s。\n"
            "请查看上方 CMake/Ninja 输出。\n"
            "可尝试：python script/build.py --clean；\n"
            "如果板卡、sample 或构建类型选错，请先执行 "
            "python script/localize_project.py --recreate ... 重新创建工程。"
            % exc.returncode,
            file=sys.stderr,
        )
        return 1
    except RuntimeError as exc:
        print("\n错误：%s" % exc, file=sys.stderr)
        print(
            "提示：可运行 python script/setup.py 检查工程环境并补齐缺失内容。",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
