#!/usr/bin/env python3
# Copyright (c) 2026 HPMicro
# SPDX-License-Identifier: BSD-3-Clause
"""
将 HPM SDK 本地化到当前工程，并内置编译、烧录所需的工具链和工具。

功能说明：
    1. 工程为空时，从 --sample 指定的 SDK sample 创建初始应用；
    2. 通过 CMake File API 分析依赖，只拷贝当前工程真正用到的 SDK 文件；
    3. 根据应用 CMakeLists 中的 CONFIG_* 裁剪未启用的中间件；
    4. 把原 SDK 的 tools/ 完整复制到工程 tools/，把 RISC-V 工具链复制到
       toolchain/<原工具链文件夹名>/；
    5. 生成 localized_sdk.cmake，所有路径都指向工程内部。

参数说明：
    --sdk-env         原 HPM SDK 环境根目录，默认 E:\\HPM\\hpm_sdk_env
    --sdk             直接指定 hpm_sdk 目录，优先级高于 --sdk-env
    --board           目标板卡，默认 hpm5300evk
    --sample          首次创建工程使用的 SDK sample，默认 hello_world
    --project-name    工程名称，也是输出文件名前缀，默认 user_app
    --hpm-build-type  依赖分析使用的构建类型，默认 flash_xip
    --force           重新生成本地化 SDK
    --refresh-toolchain 重新复制 tools/ 和 toolchain/
    --recreate        备份现有应用并清理生成内容，用当前参数重新创建工程

使用示例：
    # 1. 全部使用默认值（SDK 环境 E:\\HPM\\hpm_sdk_env、hello_world、hpm5300evk）
    python script/localize_project.py

    # 2. 指定 SDK 环境目录
    python script/localize_project.py --sdk-env E:\\HPM\\hpm_sdk_env

    # 3. 基于指定 sample 创建工程（只在首次创建时生效）
    python script/localize_project.py --sample filex
    python script/localize_project.py --sample netxduo/ping
    python script/localize_project.py --sample samples/hello_world
    python script/localize_project.py --sample samples/rtos/eclipse_threadx/hello_world

    # 4. 重新生成本地化 SDK（SDK 升级或新增组件后使用）
    python script/localize_project.py --force

    # 5. 只刷新工程内工具链和构建工具
    python script/localize_project.py --refresh-toolchain

    # 6. 指定其他板卡
    python script/localize_project.py --board hpm5300evk

    # 7. 指定工程名（也决定输出文件名 my_app.elf / my_app.bin）
    python script/localize_project.py --project-name my_app
    python script/localize_project.py --sample filex --project-name my_filex_app

    # 8. 参数写错后重新创建（会备份 CMakeLists.txt 和 src/）
    python script/localize_project.py --recreate --sample filex --project-name my_app

注意事项：
    - 首次运行需要原 SDK 环境；完成后编译和烧录不再依赖原 SDK。
    - --sample 只在工程还没有 CMakeLists.txt 和 src/ 时生效。
    - --project-name 会更新已有 CMakeLists.txt 中的 APP_NAME 和 project()。
    - --recreate 会备份 CMakeLists.txt 和 src/，保留 tools/ 和 toolchain/，再重新创建工程。
    - 重新本地化前，请确认 CMakeLists.txt 中的 CONFIG_* 和源文件列表是最新的。
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# 工程根目录（script 的上一级）
PROJECT_DIR = Path(__file__).resolve().parents[1]
# 默认的 HPM SDK 环境目录，可通过 --sdk-env 或环境变量 HPM_SDK_ENV 覆盖
DEFAULT_SDK_ENV = Path(r"E:\HPM\hpm_sdk_env")
# 默认板卡：HPM5300EVK（HPM5361）
DEFAULT_BOARD = "hpm5300evk"
# 默认 HPM 构建类型：Flash XIP
DEFAULT_HPM_BUILD_TYPE = "flash_xip"
# 默认用于创建工程的 sample（相对 hpm_sdk 根目录）
DEFAULT_SAMPLE = "samples/hello_world"
# 默认工程名称，也是输出文件名前缀（user_app.elf / user_app.bin）
DEFAULT_PROJECT_NAME = "user_app"


# 本地化 SDK 目录名前缀，完整目录名为 hpm_sdk_localized_for_<board>
LOCALIZED_PREFIX = "hpm_sdk_localized_for_"
# 工程内构建、烧录工具目录（完整镜像原 SDK 的 tools/）
TOOLS_REL_DIR = Path("tools")
# 工程内编译工具链目录（内部保留原 SDK 的工具链文件夹名）
TOOLCHAIN_REL_DIR = Path("toolchain")
# 生成器固定使用 Ninja，保证 Windows 下无需额外安装 make
GENERATOR = "Ninja"
# SDK 当前支持的 HPM 构建类型（与 cmake/application.cmake 保持一致）
SUPPORTED_BUILD_TYPES = (
    "ram",
    "flash_xip",
    "flash_xip_hybrid",
    "flash_sdram_xip",
    "flash_uf2",
    "flash_dfu",
    "flash_sdram_dfu",
    "sec_core_img",
)

# HPM 构建类型对应的 GCC 链接脚本，用于提前检查板卡/SoC 是否支持
BUILD_TYPE_LINKER = {
    "ram": "ram.ld",
    "flash_xip": "flash_xip.ld",
    "flash_xip_hybrid": "flash_xip.ld",
    "flash_sdram_xip": "flash_sdram_xip.ld",
    "flash_uf2": "flash_uf2.ld",
    "flash_dfu": "flash_dfu.ld",
    "flash_sdram_dfu": "flash_sdram_dfu.ld",
    "sec_core_img": "ram.ld",
}

# 少数无条件中间件会通过间接方式设置依赖，这里补充额外配置项。
# 其他无条件中间件的依赖会从其 CMakeLists.txt 中的 add_subdirectory_ifdef 自动推断。
EXTRA_UNCONDITIONAL_CONFIGS = {
    "wifi": ("CONFIG_MHD_4345X",),
}


def fail(message):
    """抛出带中文前缀的运行时错误。"""
    raise RuntimeError(message)


def detect_localized_board(project_dir):
    """从工程中的本地化 SDK 目录推断板卡名称。"""
    candidates = [
        d.name[len(LOCALIZED_PREFIX):]
        for d in project_dir.iterdir()
        if d.is_dir() and d.name.startswith(LOCALIZED_PREFIX)
    ]
    # 只存在一个本地化 SDK 时才自动推断，避免歧义
    if len(candidates) == 1:
        return candidates[0]
    return None


def resolve_paths(sdk_arg, sdk_env_arg):
    """解析原 SDK 路径，返回 (hpm_sdk 目录, SDK 环境根目录)。"""
    # 优先使用 --sdk-env，其次使用环境变量 HPM_SDK_ENV，最后使用默认路径
    env_root = (
        Path(sdk_env_arg).expanduser()
        if sdk_env_arg
        else Path(os.environ.get("HPM_SDK_ENV", str(DEFAULT_SDK_ENV)))
    )

    if sdk_arg:
        # 显式指定了 hpm_sdk 目录：若其父目录包含 tools，则视为完整 SDK 环境
        sdk_base = Path(sdk_arg).expanduser().resolve()
        parent = sdk_base.parent
        sdk_env = parent if (parent / "tools").is_dir() else env_root
    else:
        sdk_env = env_root.resolve()
        sdk_base = sdk_env / "hpm_sdk"

    # 通过 SDK 的 CMake 配置文件确认路径有效
    if not (sdk_base / "cmake" / "hpm-sdk-config.cmake").is_file():
        fail(
            "未找到 HPM SDK：%s\n"
            "请使用 --sdk <hpm_sdk 目录> 或 --sdk-env <SDK 环境根目录> 指定。" % sdk_base
        )
    return sdk_base, sdk_env


def validate_board_and_build_type(sdk_root, board, build_type):
    """校验板卡和 HPM 构建类型是否受当前 SDK/SoC 支持。"""
    boards_dir = sdk_root / "boards"
    board_yaml = boards_dir / board / ("%s.yaml" % board)
    if not board_yaml.is_file():
        available = []
        if boards_dir.is_dir():
            available = sorted(
                d.name
                for d in boards_dir.iterdir()
                if d.is_dir() and (d / ("%s.yaml" % d.name)).is_file()
            )
        fail(
            "板卡不存在：%s\n可用板卡：%s"
            % (board, "、".join(available) if available else "（未找到任何板卡）")
        )

    # 从板卡 yaml 中读取 SoC 名称
    match = re.search(
        r"^\s*soc\s*:\s*(\S+)",
        board_yaml.read_text(encoding="utf-8"),
        re.M,
    )
    if match is None:
        fail("板卡 yaml 中没有 soc 字段：%s" % board_yaml)
    soc = match.group(1)

    # 在 soc/<系列>/<SoC> 中定位 SoC 目录
    soc_dir = None
    soc_root = sdk_root / "soc"
    if soc_root.is_dir():
        for series in soc_root.iterdir():
            candidate = series / soc
            if candidate.is_dir():
                soc_dir = candidate
                break
    if soc_dir is None:
        fail("未找到 SoC 目录：%s（板卡：%s）" % (soc, board))

    # 检查构建类型对应的链接脚本是否存在
    linker_name = BUILD_TYPE_LINKER.get(build_type)
    if linker_name is None:
        fail("未知 HPM 构建类型：%s" % build_type)
    linker_file = soc_dir / "toolchains" / "gcc" / linker_name
    if linker_file.is_file():
        return

    # 根据实际存在的链接脚本列出可用构建类型
    available_types = []
    for name in SUPPORTED_BUILD_TYPES:
        candidate = soc_dir / "toolchains" / "gcc" / BUILD_TYPE_LINKER[name]
        if candidate.is_file() and name not in available_types:
            available_types.append(name)
    fail(
        "板卡 %s（SoC：%s）不支持构建类型 %s。\n"
        "缺少链接脚本：%s\n"
        "该板卡可用构建类型：%s"
        % (
            board,
            soc,
            build_type,
            linker_file,
            "、".join(available_types) if available_types else "无",
        )
    )


def recreate_project(project_dir):
    """备份应用文件并清理生成内容，便于用新参数重新创建工程。"""
    timestamp = time.strftime("%Y%m%d_%H%M%S")

    # 备份应用文件，避免重新创建时丢失用户代码
    cmake_file = project_dir / "CMakeLists.txt"
    if cmake_file.is_file():
        backup = project_dir / ("CMakeLists.txt.bak_%s" % timestamp)
        shutil.move(str(cmake_file), str(backup))
        print("已备份 %s -> %s" % (cmake_file, backup), flush=True)

    src_dir = project_dir / "src"
    if src_dir.is_dir():
        backup = project_dir / ("src_backup_%s" % timestamp)
        shutil.move(str(src_dir), str(backup))
        print("已备份 %s -> %s" % (src_dir, backup), flush=True)

    # 兼容旧版本的 sample_inc/ 和新版本的同名依赖目录（如 config/）
    for dirname in ("sample_inc", "config"):
        dependency_dir = project_dir / dirname
        if dependency_dir.is_dir():
            backup = project_dir / ("%s_backup_%s" % (dirname, timestamp))
            shutil.move(str(dependency_dir), str(backup))
            print("已备份 %s -> %s" % (dependency_dir, backup), flush=True)

    # 删除本地化 SDK、构建目录和生成的配置；保留 tools/ 和 toolchain/
    generated = [project_dir / "localized_sdk.cmake", project_dir / "build"]
    generated += sorted(project_dir.glob(LOCALIZED_PREFIX + "*"))
    for path in generated:
        if path.is_dir():
            shutil.rmtree(path)
            print("已删除目录：%s" % path, flush=True)
        elif path.is_file():
            path.unlink()
            print("已删除文件：%s" % path, flush=True)

    print("已清理生成内容，开始用当前参数重新创建工程。", flush=True)


def find_toolchain(sdk_env):
    """查找原 SDK 环境中的 RISC-V 工具链目录。"""
    # 优先使用外部环境变量指定的工具链，但不能把工程内工具当成复制源
    env_path = os.environ.get("GNURISCV_TOOLCHAIN_PATH")
    if env_path:
        candidate = Path(env_path).resolve()
        toolchain_root = (PROJECT_DIR / TOOLCHAIN_REL_DIR).resolve()
        if (
            (candidate / "bin").is_dir()
            and candidate != toolchain_root
            and toolchain_root not in candidate.parents
        ):
            return candidate

    root = sdk_env / "toolchains"
    if not root.is_dir():
        fail("未找到工具链目录：%s" % root)

    # 选择第一个包含 bin 目录的工具链，Windows 版优先
    candidates = sorted(
        d for d in root.iterdir() if d.is_dir() and (d / "bin").is_dir()
    )
    if not candidates:
        fail("在 %s 下没有找到 RISC-V 工具链" % root)

    for candidate in candidates:
        if "win" in candidate.name.lower():
            return candidate
    return candidates[0]


def find_tool(sdk_env, relative_paths, program_name):
    """在 SDK 环境中查找可执行文件，找不到时回退到系统 PATH。"""
    for relative in relative_paths:
        candidate = sdk_env / relative
        if candidate.is_file():
            return candidate

    found = shutil.which(program_name)
    if found:
        return Path(found)
    return None


def find_cmake(sdk_env):
    """查找 CMake，优先使用 SDK 自带的版本。"""
    cmake = find_tool(
        sdk_env,
        ("tools/cmake/bin/cmake.exe", "tools/cmake/bin/cmake"),
        "cmake",
    )
    if cmake is None:
        fail("未找到 CMake，请安装 CMake 或使用 HPM SDK 环境包。")
    return cmake


def find_ninja(sdk_env):
    """查找 Ninja，优先使用 SDK 自带的版本。"""
    ninja = find_tool(sdk_env, ("tools/ninja/ninja.exe", "tools/ninja/ninja"), "ninja")
    if ninja is None:
        fail("未找到 Ninja，请安装 Ninja 或使用 HPM SDK 环境包。")
    return ninja


def find_python(sdk_env):
    """查找 Python，优先使用 SDK 自带版本（其中包含 PyYAML）。"""
    for relative in (
        "tools/python3/python.exe",
        "tools/python3/bin/python3",
        "tools/python3/python",
    ):
        candidate = sdk_env / relative
        if candidate.is_file():
            return candidate
    # SDK 中没有自带 Python 时，使用当前运行本脚本的解释器
    return Path(sys.executable)


def make_sdk_env(sdk_base, sdk_env, toolchain):
    """构造执行 SDK 自带 localize_sdk.py 所需的环境变量。"""
    env = os.environ.copy()
    env["HPM_SDK_BASE"] = str(sdk_base)
    env["GNURISCV_TOOLCHAIN_PATH"] = str(toolchain)

    # OpenOCD 配置目录，供 SDK 生成 IDE 工程时使用
    openocd_scripts = sdk_base / "boards" / "openocd"
    if openocd_scripts.is_dir():
        env["OPENOCD_SCRIPTS"] = str(openocd_scripts)

    # 把 SDK 自带工具放到 PATH 最前面，确保调用的是同一套版本
    prepend = [
        sdk_env / "tools" / "cmake" / "bin",
        sdk_env / "tools" / "ninja",
        sdk_env / "tools" / "python3",
        toolchain / "bin",
        sdk_env / "tools" / "openocd",
        sdk_env / "tools" / "scripts",
    ]
    existing = [str(p) for p in prepend if p.is_dir()]
    env["PATH"] = os.pathsep.join(existing + [env.get("PATH", "")])
    return env


def project_toolchain_dir(project_dir):
    """查找工程内 toolchain/ 下的具体工具链目录（保留原 SDK 文件夹名）。"""
    root = project_dir / TOOLCHAIN_REL_DIR
    if root.is_dir():
        # 正常情况下结构为 toolchain/<原工具链名>/bin/...
        for candidate in sorted(d for d in root.iterdir() if d.is_dir()):
            if (candidate / "bin" / "riscv32-unknown-elf-gcc.exe").is_file():
                return candidate
        # 兼容旧布局 toolchain/bin/...
        if (root / "bin").is_dir():
            return root
    return root


def tool_paths(project_dir):
    """返回工程内构建工具（tools/）和编译工具链（toolchain/）的各个路径。"""
    tools_root = project_dir / TOOLS_REL_DIR
    return {
        "tools_root": tools_root,
        "toolchain_root": project_dir / TOOLCHAIN_REL_DIR,
        "toolchain": project_toolchain_dir(project_dir),
        "cmake": tools_root / "cmake",
        "ninja": tools_root / "ninja",
        "python": tools_root / "python3",
        "openocd": tools_root / "openocd",
        "scripts": tools_root / "scripts",
        "project_generator": tools_root / "project_generator",
    }


def tools_are_ready(project_dir):
    """检查工程内构建工具和编译工具链是否已经完整复制。"""
    paths = tool_paths(project_dir)
    required = (
        paths["toolchain"] / "bin" / "riscv32-unknown-elf-gcc.exe",
        paths["cmake"] / "bin" / "cmake.exe",
        paths["ninja"] / "ninja.exe",
        paths["python"] / "python.exe",
        paths["openocd"] / "openocd.exe",
    )
    return all(p.is_file() for p in required) and (
        paths["scripts"].is_dir() and paths["project_generator"].is_dir()
    )


def make_tool_env(project_dir, localized_name):
    """构造完全基于工程内工具的编译环境，不引用原 SDK。"""
    paths = tool_paths(project_dir)
    localized_dir = project_dir / localized_name

    env = os.environ.copy()
    # 强制指向工程内 SDK，避免外部环境变量把编译带回原 SDK
    env["HPM_SDK_BASE"] = str(localized_dir)
    # 强制指向工程内工具链（toolchain/<原工具链名>）
    env["GNURISCV_TOOLCHAIN_PATH"] = str(paths["toolchain"])
    # 强制指向工程内 OpenOCD 配置
    env["OPENOCD_SCRIPTS"] = str(localized_dir / "boards" / "openocd")

    # 把工程内工具放到 PATH 最前面
    prepend = [
        paths["cmake"] / "bin",
        paths["ninja"],
        paths["python"],
        paths["toolchain"] / "bin",
        paths["openocd"],
        paths["scripts"],
    ]
    existing = [str(p) for p in prepend if p.is_dir()]
    env["PATH"] = os.pathsep.join(existing + [env.get("PATH", "")])
    return env


def resolve_sample(sdk_base, sample):
    """解析 sample 路径，支持 sample 名、相对路径和绝对路径。"""
    sample_path = Path(sample)
    candidates = []
    if sample_path.is_absolute():
        candidates.append(sample_path)
    else:
        # 1. 相对 hpm_sdk 根目录
        candidates.append(sdk_base / sample_path)
        # 2. 相对 samples 目录，如 hello_world、eclipse_threadx/hello_world
        candidates.append(sdk_base / "samples" / sample_path)
        # 3. ThreadX sample 名，如 filex、netxduo/ping
        candidates.append(sdk_base / "samples" / "rtos" / "eclipse_threadx" / sample_path)

    for candidate in candidates:
        if candidate.is_dir() and (candidate / "CMakeLists.txt").is_file():
            return candidate.resolve()

    fail(
        "未找到示例：%s\n"
        "可传入示例名（如 hello_world、filex、netxduo/ping）、相对 hpm_sdk 的路径"
        "或绝对路径。" % sample
    )
    return None


def sample_relative_name(sample_dir, sdk_base):
    """返回示例相对 hpm_sdk 的路径，用于注释和日志。"""
    try:
        return sample_dir.relative_to(sdk_base).as_posix()
    except ValueError:
        return sample_dir.as_posix()


def copy_sample_root_files(sample_dir, project_dir):
    """复制示例根目录下的头文件/源文件（如 netxduo 的 nx_user.h）。"""
    for entry in sample_dir.iterdir():
        if entry.is_file() and entry.suffix.lower() in (".h", ".hpp", ".c", ".cpp"):
            shutil.copy2(entry, project_dir / entry.name)


def validate_project_name(project_name):
    """校验工程名称：只允许字母、数字、下划线和连字符，且不能以数字开头。"""
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_-]*$", project_name):
        fail(
            "工程名称不合法：%s\n"
            "仅支持字母、数字、下划线、连字符，且不能以数字开头。" % project_name
        )
    return project_name


def update_project_name(cmake_file, project_name):
    """更新已有 CMakeLists.txt 中的 APP_NAME 和 project() 名称。"""
    text = cmake_file.read_text(encoding="utf-8")

    # 更新 APP_NAME 行；如果不存在，则插入到 find_package 之前
    text, app_count = re.subn(
        r"(?m)^(\s*set\s*\(\s*APP_NAME\s+)[^\s\)]+",
        lambda match: match.group(1) + project_name,
        text,
        count=1,
    )
    if app_count == 0:
        text = re.sub(
            r"(?m)^(\s*find_package\(hpm-sdk)",
            lambda match: "set(APP_NAME %s)\n\n%s"
            % (project_name, match.group(1)),
            text,
            count=1,
        )

    # 更新 project() 行；示例中可能没有该行，没有时保持原样
    text = re.sub(
        r"(?m)^(\s*project\s*\(\s*)[^\)]+(\))",
        lambda match: match.group(1) + project_name + match.group(2),
        text,
        count=1,
    )

    original = cmake_file.read_text(encoding="utf-8")
    if text != original:
        cmake_file.write_text(text, encoding="utf-8")
        print("已将工程名称更新为 %s。" % project_name, flush=True)


def localize_sample_include_paths(sample_dir, sdk_base, project_dir, cmake_text):
    """把 sample 中指向自身目录外部的 sdk_inc 目录复制到工程内并改写路径。"""
    sample_root = sample_dir.resolve()
    sdk_root = sdk_base.resolve()

    # 工程根目录下已被占用的名称，避免覆盖应用目录或工具目录
    reserved_names = {
        "src",
        "script",
        "tools",
        "toolchain",
        "build",
        ".git",
        ".vscode",
        "CMakeLists.txt",
        "localized_sdk.cmake",
    }
    used_destinations = set()

    def replace_inc(match):
        tokens = match.group(1).split()
        new_tokens = []
        for token in tokens:
            # 跳过 CMake 变量和绝对路径
            if token.startswith("${") or Path(token).is_absolute():
                new_tokens.append(token)
                continue

            target = (sample_dir / token).resolve()
            # sample 目录内部的路径保持不变，如 sdk_inc(src)
            try:
                target.relative_to(sample_root)
                new_tokens.append(token)
                continue
            except ValueError:
                pass

            if not target.is_dir():
                new_tokens.append(token)
                continue

            # 默认使用目录原名（如 config），与 SDK 中的目录名保持一致
            destination_rel = Path(target.name)
            if (
                destination_rel.name in reserved_names
                or destination_rel.as_posix() in used_destinations
                or destination_rel.name.startswith(LOCALIZED_PREFIX)
            ):
                # 发生重名或与工程目录冲突时，退回带路径的唯一名称
                try:
                    relative = target.relative_to(sdk_root)
                except ValueError:
                    relative = Path(target.name)
                destination_rel = Path("sample_inc") / relative
            used_destinations.add(destination_rel.as_posix())

            destination = project_dir / destination_rel
            # 重新生成时覆盖旧目录，避免混入上一个 sample 的头文件
            if destination.exists():
                shutil.rmtree(destination)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(target, destination)
            print(
                "已复制 sample 依赖目录：%s -> %s" % (target, destination),
                flush=True,
            )
            new_tokens.append(destination_rel.as_posix())

        return "sdk_inc(%s)" % " ".join(new_tokens)

    return re.sub(r"sdk_inc\(([^\)]*)\)", replace_inc, cmake_text)


# 追加到工程 CMakeLists.txt 末尾的构建可复现性设置。
# assert() 展开后会把编译时传给 gcc 的源码路径（__FILE__）原样写进 .rodata，
# 本地化 SDK 的目录名比原 SDK 长，固件体积就会随工程所在路径变化；
# 用 -ffile-prefix-map 把它映射成固定的相对路径后，体积不再受路径影响。
COMPILE_REPRO_BLOCK = """
# ---- 以下由 script/localize_project.py 注入，重新本地化时保留 ----
# assert() 会把源码绝对路径（__FILE__）编译进 flash，本地化 SDK 目录名较长，
# 会让固件体积随工程路径变化。映射成相对路径后构建结果可复现。
if(DEFINED ENV{HPM_SDK_BASE})
    sdk_compile_options("-ffile-prefix-map=$ENV{HPM_SDK_BASE}=hpm_sdk")
endif()

# release 构建默认关闭 assert：断言字符串只占 flash，release 下没有调试价值。
# 需要在 release 中保留断言时，配置时加 -DAPP_KEEP_ASSERT_IN_RELEASE=ON。
option(APP_KEEP_ASSERT_IN_RELEASE "release 构建保留 assert" OFF)
string(TOLOWER "${CMAKE_BUILD_TYPE}" _APP_BUILD_TYPE)
if(_APP_BUILD_TYPE STREQUAL "release" AND NOT APP_KEEP_ASSERT_IN_RELEASE)
    sdk_compile_options("-DNDEBUG")
endif()
"""


def build_cmake_from_sample(sample_dir, sdk_base, project_dir, sample_name, project_name):
    """根据所选 sample 和工程名称生成 CMakeLists.txt。"""
    # 保留 sample 原有的 CONFIG_*、源文件列表和注释，只注入本地化配置和工程名。
    # 这样脚本不绑定任何 RTOS 或中间件，适用于 SDK 中的任意 sample。
    text = (sample_dir / "CMakeLists.txt").read_text(encoding="utf-8")

    # 把 sample 指向外部目录的 sdk_inc 路径复制到工程内并改写
    text = localize_sample_include_paths(
        sample_dir, sdk_base, project_dir, text
    )

    # 把示例的 project() 名称替换成用户指定的工程名
    text = re.sub(
        r"(?m)^(\s*project\s*\(\s*)[^\)]+(\))",
        lambda match: match.group(1) + project_name + match.group(2),
        text,
        count=1,
    )

    match = re.search(r"^\s*find_package\(hpm-sdk", text, re.M)
    if match is None:
        fail("示例 CMakeLists.txt 中没有 find_package(hpm-sdk ...)：%s" % sample_dir)

    header = (
        "# 本文件由 script/localize_project.py 基于 HPM SDK 示例生成。\n"
        "# 示例路径：%s\n"
        "# 下面保留示例原有的 CONFIG_*、源文件列表和注释，便于对照 SDK 升级。\n\n"
        % sample_name
    )
    inject = (
        "# 由本地化脚本注入：APP_NAME 必须在 find_package 之前定义\n"
        "set(APP_NAME %s)\n" % project_name
        + "# 引入工程内本地化 SDK、编译工具链和构建工具路径\n"
        "if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/localized_sdk.cmake)\n"
        "    include(${CMAKE_CURRENT_SOURCE_DIR}/localized_sdk.cmake)\n"
        "else()\n"
        "    message(FATAL_ERROR \"未找到 localized_sdk.cmake，请先运行 python script/setup.py 补齐工程环境\")\n"
        "endif()\n\n"
    )
    return (
        header
        + text[: match.start()]
        + inject
        + text[match.start():]
        + COMPILE_REPRO_BLOCK
    )


def bootstrap(project_dir, sdk_base, sample, project_name):
    """工程为空时，基于指定 sample 和工程名创建 CMakeLists.txt 和 src/。"""
    cmake_file = project_dir / "CMakeLists.txt"
    src_dir = project_dir / "src"

    # 分别判断 CMakeLists.txt 和 src/ 是否存在
    cmake_exists = cmake_file.is_file()
    src_exists = src_dir.is_dir() and any(src_dir.iterdir())

    # 工程完整时不再覆盖，--sample 只在首次创建时生效。
    # 显式指定 --project-name 时，允许更新已有 CMakeLists.txt 的工程名。
    if cmake_exists and src_exists:
        if project_name:
            update_project_name(cmake_file, project_name)
        print(
            "工程已存在，跳过示例模板创建（--sample 仅在首次创建时生效）。",
            flush=True,
        )
        return None, None

    # 只有其中一个存在，说明上次创建/删除不完整，直接给出明确提示
    if cmake_exists or src_exists:
        fail(
            "工程文件不完整：CMakeLists.txt %s，src/ %s。\n"
            "请使用 --recreate 备份现有文件并重新创建工程。"
            % (
                "存在" if cmake_exists else "缺失",
                "存在" if src_exists else "缺失",
            )
        )

    sample_dir = resolve_sample(sdk_base, sample)
    sample_name = sample_relative_name(sample_dir, sdk_base)
    effective_name = project_name or DEFAULT_PROJECT_NAME

    if not (sample_dir / "src").is_dir():
        fail("示例中没有 src 目录：%s" % sample_dir)

    # 复制示例源码和根目录头文件（如 nx_user.h）
    if src_dir.exists():
        src_dir.rmdir()
    shutil.copytree(sample_dir / "src", src_dir)
    copy_sample_root_files(sample_dir, project_dir)

    # 清理可能残留的 sample 外部依赖目录，避免复用上个 sample 的头文件
    sample_inc_dir = project_dir / "sample_inc"
    if sample_inc_dir.exists():
        shutil.rmtree(sample_inc_dir)

    # 生成工程 CMakeLists.txt
    cmake_file.write_text(
        build_cmake_from_sample(
            sample_dir, sdk_base, project_dir, sample_name, effective_name
        ),
        encoding="utf-8",
    )
    print(
        "已基于示例 %s 创建工程（工程名：%s）。" % (sample_name, effective_name),
        flush=True,
    )
    return sample_dir, sample_name


def _config_enabled(cmake_text, name):
    """判断应用 CMakeLists 中是否设置了某个 CONFIG_* 为 1。"""
    return re.search(
        r"^\s*set\s*\(\s*" + re.escape(name) + r"\s+1\s*\)", cmake_text, re.M
    ) is not None


def _prune_directory(path):
    """删除目录，目录存在时返回 True。"""
    if path.is_dir():
        shutil.rmtree(path)
        return True
    return False


def _parse_subdirectory_requirements(cmake_text):
    """解析 CMakeLists.txt，返回 {子目录: 依赖的配置集合或 None}。"""
    requirements = {}
    condition_stack = []

    for raw_line in cmake_text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        # 记录 if(CONFIG_xxx) 条件；其他 if 只入栈，不参与配置判断
        if re.match(r"^if\s*\(", line):
            match = re.match(
                r"^if\s*\(\s*(CONFIG_[A-Za-z0-9_]+)\s*\)", line
            )
            condition_stack.append({match.group(1)} if match else set())
            continue
        if re.match(r"^endif\s*\(", line):
            if condition_stack:
                condition_stack.pop()
            continue

        current_configs = set()
        for item in condition_stack:
            current_configs.update(item)

        # add_subdirectory_ifdef(CONFIG_X dir1 dir2 ...)
        match = re.match(
            r"^add_subdirectory_ifdef\s*\(\s*(CONFIG_[A-Za-z0-9_]+)\s+([^\)]+)\)",
            line,
        )
        if match:
            configs = set(current_configs)
            configs.add(match.group(1))
            for subdir in match.group(2).split():
                if subdir.startswith("${"):
                    continue
                requirements.setdefault(subdir, set()).update(configs)
            continue

        # add_subdirectory(dir1 dir2 ...)
        match = re.match(r"^add_subdirectory\s*\(\s*([^\)]+)\)", line)
        if match:
            for subdir in match.group(1).split():
                if subdir.startswith("${"):
                    continue
                if current_configs:
                    requirements.setdefault(subdir, set()).update(current_configs)
                else:
                    requirements[subdir] = None

    return requirements


def _comment_subdirectory_lines(cmake_file, subdir):
    """注释 CMakeLists.txt 中引用指定子目录的 add_subdirectory 语句。"""
    lines = cmake_file.read_text(encoding="utf-8").splitlines()
    pattern = re.compile(
        r"^\s*(add_subdirectory|add_subdirectory_ifdef)\s*\([^\)]*\b"
        + re.escape(subdir)
        + r"\b"
    )
    changed = False
    for index, line in enumerate(lines):
        if line.lstrip().startswith("#"):
            continue
        if pattern.search(line):
            indent = line[: len(line) - len(line.lstrip())]
            lines[index] = (
                indent + "# " + line.strip() + "  # 已裁剪：本工程未使用"
            )
            changed = True
    if changed:
        cmake_file.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _infer_dir_configs(target_dir):
    """从中间件 CMakeLists 的 add_subdirectory_ifdef 推断其依赖的配置项。"""
    cmake_file = target_dir / "CMakeLists.txt"
    if not cmake_file.is_file():
        return None

    requirements = _parse_subdirectory_requirements(
        cmake_file.read_text(encoding="utf-8")
    )
    configs = set()
    for sub_configs in requirements.values():
        if sub_configs:
            configs.update(sub_configs)
    return configs if configs else None


def _prune_nested_subdirectories(parent_dir, project_cmake_text):
    """裁剪某个中间件目录下未启用的子目录（按其 CMakeLists 中的配置判断）。"""
    cmake_file = parent_dir / "CMakeLists.txt"
    if not cmake_file.is_file():
        return

    requirements = _parse_subdirectory_requirements(
        cmake_file.read_text(encoding="utf-8")
    )
    for subdir, configs in requirements.items():
        # 无条件子目录（如公共源码目录）不能裁掉
        if configs is None:
            continue
        if any(_config_enabled(project_cmake_text, name) for name in configs):
            continue
        if _prune_directory(parent_dir / subdir):
            _comment_subdirectory_lines(cmake_file, subdir)


def prune_localized_sdk(sdk_dir, project_cmake_text):
    """根据应用 CMakeLists 中的 CONFIG_* 裁剪未使用的中间件。"""
    middleware_dir = sdk_dir / "middleware"
    middleware_cmake = middleware_dir / "CMakeLists.txt"
    if not middleware_cmake.is_file():
        return

    requirements = _parse_subdirectory_requirements(
        middleware_cmake.read_text(encoding="utf-8")
    )
    for subdir, configs in requirements.items():
        target_dir = middleware_dir / subdir
        if not target_dir.is_dir():
            continue

        # 无条件 add_subdirectory 的目录，从它自己的 CMakeLists 推断依赖
        if configs is None:
            configs = _infer_dir_configs(target_dir)
            extras = EXTRA_UNCONDITIONAL_CONFIGS.get(subdir, ())
            if configs is None and not extras:
                # 无法判断依赖时保留，避免误删
                continue
            configs = set(configs or ())
            configs.update(extras)

        # 任一依赖配置启用时保留该目录，并继续裁剪其内部未启用的子目录
        if any(_config_enabled(project_cmake_text, name) for name in configs):
            _prune_nested_subdirectories(target_dir, project_cmake_text)
            continue

        # 依赖配置全部未启用时，删除整个中间件目录
        if _prune_directory(target_dir):
            _comment_subdirectory_lines(middleware_cmake, subdir)


def _copy_tree(source, destination, description):
    """复制目录并打印中文进度信息。"""
    if not source.is_dir():
        fail("复制工具失败，源目录不存在：%s" % source)
    print("正在复制%s..." % description, flush=True)
    shutil.copytree(source, destination)
    size_mb = sum(
        f.stat().st_size for f in destination.rglob("*") if f.is_file()
    ) / (1024 * 1024)
    print("  %s -> %s（%.1f MB）" % (source, destination, size_mb), flush=True)


def bundle_tools(sdk_env, project_dir, refresh=False):
    """原样复制 SDK 的 tools/，并把工具链放入 toolchain/<原工具链名>/。"""
    paths = tool_paths(project_dir)

    # 已经完整复制过时直接跳过，除非显式要求刷新
    if tools_are_ready(project_dir) and not refresh:
        print(
            "工程内工具和工具链已存在，跳过复制（如需刷新请加 --refresh-toolchain）。",
            flush=True,
        )
        return paths

    # 刷新或上次复制不完整时，先清空两个生成目录
    if paths["tools_root"].exists():
        shutil.rmtree(paths["tools_root"])
    if paths["toolchain_root"].exists():
        shutil.rmtree(paths["toolchain_root"])

    # 1. tools/ 完整镜像原 SDK 的 tools/（cmake、ninja、python3、openocd、
    #    project_generator、scripts、FTDI 驱动安装程序等）
    tools_src = sdk_env / "tools"
    if not tools_src.is_dir():
        fail("原 SDK 中没有 tools 目录：%s" % tools_src)
    _copy_tree(tools_src, paths["tools_root"], "构建工具（完整 SDK tools 目录）")

    # 2. 工具链放到 toolchain/<原工具链文件夹名>/
    toolchain_src = find_toolchain(sdk_env)
    paths["toolchain_root"].mkdir(parents=True)
    _copy_tree(
        toolchain_src,
        paths["toolchain_root"] / toolchain_src.name,
        "RISC-V 编译工具链",
    )

    # 打印两个目录的大小
    tools_mb = sum(
        f.stat().st_size for f in paths["tools_root"].rglob("*") if f.is_file()
    ) / (1024 * 1024)
    toolchain_mb = sum(
        f.stat().st_size for f in paths["toolchain_root"].rglob("*") if f.is_file()
    ) / (1024 * 1024)
    print(
        "工程内工具复制完成：tools/ %.1f MB，toolchain/ %.1f MB"
        % (tools_mb, toolchain_mb),
        flush=True,
    )
    return paths


def write_localized_cmake(project_dir, localized_name, board, build_type):
    """生成 localized_sdk.cmake，全部使用工程内相对路径。"""
    localized_dir = project_dir / localized_name
    toolchain_dir = tool_paths(project_dir)["toolchain"]
    try:
        toolchain_rel = toolchain_dir.relative_to(project_dir).as_posix()
    except ValueError:
        toolchain_rel = TOOLCHAIN_REL_DIR.as_posix()

    lines = [
        "# 本文件由 script/localize_project.py 自动生成，请勿手工修改。",
        "# 本地化后的 HPM SDK 路径（相对工程根目录，整个工程可自由移动）",
        'set(ENV{HPM_SDK_BASE} "${CMAKE_CURRENT_LIST_DIR}/%s")' % localized_name,
        "# 工程内置的 RISC-V 编译工具链路径（toolchain/<原工具链文件夹名>）",
        'set(ENV{GNURISCV_TOOLCHAIN_PATH} "${CMAKE_CURRENT_LIST_DIR}/%s")'
        % toolchain_rel,
        "# 把工程内构建工具加入 CMake 进程的 PATH，保证 IDE 直接编译时也能找到 Python 等工具",
        'set(ENV{PATH} "${CMAKE_CURRENT_LIST_DIR}/tools/cmake/bin;${CMAKE_CURRENT_LIST_DIR}/tools/ninja;${CMAKE_CURRENT_LIST_DIR}/tools/python3;${CMAKE_CURRENT_LIST_DIR}/%s/bin;${CMAKE_CURRENT_LIST_DIR}/tools/openocd;${CMAKE_CURRENT_LIST_DIR}/tools/scripts;$ENV{PATH}")'
        % toolchain_rel,
    ]

    # 本地化 SDK 中包含 OpenOCD 配置时，一并设置脚本搜索路径
    if (localized_dir / "boards" / "openocd").is_dir():
        lines += [
            "# 工程内置的 OpenOCD 配置目录",
            'set(ENV{OPENOCD_SCRIPTS} "${CMAKE_CURRENT_LIST_DIR}/%s/boards/openocd")'
            % localized_name,
        ]

    lines += [
        "# 默认板卡（可通过 cmake -DBOARD=xxx 覆盖）",
        "if(NOT DEFINED BOARD)",
        "    set(BOARD %s)" % board,
        "endif()",
        "# 默认构建类型（可通过 cmake -DHPM_BUILD_TYPE=xxx 覆盖）",
        "if(NOT DEFINED HPM_BUILD_TYPE)",
        "    set(HPM_BUILD_TYPE %s)" % build_type,
        "endif()",
        "",
    ]
    (project_dir / "localized_sdk.cmake").write_text("\n".join(lines), encoding="utf-8")


def get_app_name(project_dir):
    """从 CMakeLists.txt 中读取 APP_NAME，读取失败时使用默认工程名。"""
    cmake_file = project_dir / "CMakeLists.txt"
    if cmake_file.is_file():
        match = re.search(
            r"^\s*set\s*\(\s*APP_NAME\s+([^\s\)]+)",
            cmake_file.read_text(encoding="utf-8"),
            re.M,
        )
        if match:
            return match.group(1)
    return DEFAULT_PROJECT_NAME


def print_project_info(
    project_dir,
    sdk_base,
    sdk_env,
    board,
    build_type,
    sample,
    project_name,
    force,
):
    """脚本开始时打印本次工程的全部关键信息。"""
    paths = tool_paths(project_dir)
    localized_dir = project_dir / (LOCALIZED_PREFIX + board)
    effective_name = project_name or get_app_name(project_dir)

    # 尝试解析 sample 的实际路径，解析失败时只显示参数值
    sample_display = sample
    try:
        sample_dir = resolve_sample(sdk_base, sample)
        relative_sample = sample_relative_name(sample_dir, sdk_base)
        if sample != relative_sample:
            sample_display = "%s（%s）" % (sample, relative_sample)
    except RuntimeError:
        pass

    # 判断本地化 SDK 和工程内工具的状态
    if force:
        sdk_state = "将重新生成"
    elif localized_dir.is_dir():
        sdk_state = "已本地化"
    else:
        sdk_state = "首次本地化"
    tools_state = "已就绪" if tools_are_ready(project_dir) else "未复制或不完整"

    # 优先显示工程内编译器，未复制时显示原 SDK 中的编译器
    compiler = paths["toolchain"] / "bin" / "riscv32-unknown-elf-gcc.exe"
    if not compiler.is_file():
        try:
            compiler = find_toolchain(sdk_env) / "bin" / "riscv32-unknown-elf-gcc.exe"
        except RuntimeError:
            pass

    print("=" * 68, flush=True)
    print("本次工程信息", flush=True)
    print("-" * 68, flush=True)
    print("工程根目录      : %s" % project_dir, flush=True)
    print("工程名称        : %s" % effective_name, flush=True)
    print("SDK 环境目录    : %s" % sdk_env, flush=True)
    print("hpm_sdk 目录     : %s" % sdk_base, flush=True)
    print("目标板卡        : %s" % board, flush=True)
    print("HPM 构建类型    : %s" % build_type, flush=True)
    print("使用的 sample   : %s" % sample_display, flush=True)
    print("本地化 SDK 目录 : %s（%s）" % (localized_dir, sdk_state), flush=True)
    print(
        "构建工具目录    : %s（%s）" % (paths["tools_root"], tools_state),
        flush=True,
    )
    print("编译工具链目录  : %s" % paths["toolchain"], flush=True)
    print("编译器          : %s" % compiler, flush=True)
    print("=" * 68, flush=True)


def _ignore_app_items(directory, names):
    """复制应用目录到临时分析目录时，忽略工程内的大目录和生成文件。"""
    ignored = set()
    for name in names:
        if name in {
            "build",
            "tools",
            "toolchain",
            "script",
            ".git",
            ".vscode",
            "__pycache__",
            "localized_sdk.cmake",
            "README.md",
            ".gitignore",
            ".clang-format",
        }:
            ignored.add(name)
        elif (
            name.startswith("build")
            or name.startswith(LOCALIZED_PREFIX)
            or name.startswith("src_backup_")
            or name.startswith("sample_inc_backup_")
            or name.startswith("config_backup_")
            or name.startswith("CMakeLists.txt.bak_")
        ):
            ignored.add(name)
    return ignored


def localize(
    project_dir,
    sdk_base,
    sdk_env,
    board,
    build_type,
    sample,
    project_name,
    force,
    refresh_toolchain=False,
):
    """执行完整的本地化流程。"""
    # 提前检查板卡和构建类型，避免拷贝几百 MB 后才在 CMake 阶段失败
    validate_board_and_build_type(sdk_base, board, build_type)

    # 本地化阶段使用原 SDK 环境中的工具
    cmake = find_cmake(sdk_env)
    find_ninja(sdk_env)  # 仅用于提前校验 Ninja 是否可用
    python = find_python(sdk_env)
    toolchain = find_toolchain(sdk_env)
    env = make_sdk_env(sdk_base, sdk_env, toolchain)

    # 工程为空时，基于指定示例和工程名生成应用
    sample_dir, sample_name = bootstrap(project_dir, sdk_base, sample, project_name)

    localized_name = LOCALIZED_PREFIX + board
    localized_dir = project_dir / localized_name

    # 已经本地化过且未要求强制刷新时，只补齐工程内工具和配置文件
    if localized_dir.is_dir() and not force:
        print("SDK 已本地化（板卡：%s）：%s" % (board, localized_dir), flush=True)
        if sample_name:
            print("当前工程示例：%s" % sample_name, flush=True)
        print("如需重新生成，请加 --force。", flush=True)
        bundle_tools(sdk_env, project_dir, refresh_toolchain)
        write_localized_cmake(project_dir, localized_name, board, build_type)
        return localized_dir

    # 在系统临时目录中完成分析、拷贝和裁剪，成功后再替换工程内旧目录
    tmp_root = Path(tempfile.mkdtemp(prefix="hpm_localize_"))
    staging = tmp_root / "app"
    build_dir = tmp_root / "build"
    dst_root = tmp_root / "dst"
    try:
        dst_root.mkdir()

        # 把应用工程复制到临时目录，保留示例根目录头文件等本地文件，
        # 但排除 tools、toolchain、build、本地化 SDK 等大目录和生成配置。
        shutil.copytree(
            project_dir,
            staging,
            ignore=_ignore_app_items,
            dirs_exist_ok=True,
        )

        # 工程 CMakeLists 要求 localized_sdk.cmake 存在，否则直接 FATAL_ERROR；
        # 依赖分析发生在本地化之前，这里先写一个占位文件满足 include。
        # SDK 路径、工具链和 PATH 仍由 make_sdk_env() 通过进程环境变量提供，
        # 分析结果与本地化后一致；该文件随临时目录一起删除。
        (staging / "localized_sdk.cmake").write_text(
            "# 依赖分析占位文件：SDK 路径由 CMake 进程环境变量提供。\n",
            encoding="utf-8",
        )

        # 创建 CMake File API 查询文件，让 localize_sdk.py 能拿到依赖信息
        query_dir = build_dir / ".cmake" / "api" / "v1" / "query"
        query_dir.mkdir(parents=True)
        (query_dir / "codemodel-v2").touch()

        sample_log = ("，示例：%s" % sample_name) if sample_name else ""
        print(
            "正在分析依赖（板卡：%s，构建类型：%s%s）..."
            % (board, build_type, sample_log),
            flush=True,
        )
        subprocess.run(
            [
                str(cmake),
                "-G",
                GENERATOR,
                "-S",
                str(staging),
                "-B",
                str(build_dir),
                "-DBOARD=%s" % board,
                "-DHPM_BUILD_TYPE=%s" % build_type,
                "-DCMAKE_BUILD_TYPE=debug",
            ],
            env=env,
            check=True,
        )

        print("正在拷贝 SDK 依赖文件...", flush=True)
        subprocess.run(
            [
                str(python),
                str(sdk_base / "scripts" / "localize_sdk.py"),
                "--src",
                str(staging),
                "--bld",
                str(build_dir),
                "--dst",
                str(dst_root),
                "--sdk",
                str(sdk_base),
                "--brd",
                board,
                "--gnt",
                GENERATOR,
                "-f",
            ],
            env=env,
            check=True,
        )

        new_sdk = dst_root / localized_name
        if not new_sdk.is_dir():
            fail("本地化结束但未生成目录：%s" % new_sdk)

        print("正在裁剪未使用的中间件...", flush=True)
        prune_localized_sdk(
            new_sdk, (project_dir / "CMakeLists.txt").read_text(encoding="utf-8")
        )

        # 用新目录替换工程内的旧目录
        if localized_dir.exists():
            shutil.rmtree(localized_dir)
        shutil.move(str(new_sdk), str(localized_dir))
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)

    # 原样复制 tools/，并把工具链放入 toolchain/<原工具链名>/
    bundle_tools(sdk_env, project_dir, refresh_toolchain)

    # 生成相对路径配置
    write_localized_cmake(project_dir, localized_name, board, build_type)

    size_mb = sum(
        f.stat().st_size for f in localized_dir.rglob("*") if f.is_file()
    ) / (1024 * 1024)
    print("本地化 SDK：%s（%.1f MB）" % (localized_dir, size_mb), flush=True)
    return localized_dir


def main(argv=None):
    """命令行入口。"""
    parser = argparse.ArgumentParser(
        description=(
            "将 HPM SDK 本地化到当前工程，并内置编译、烧录所需的工具链和工具。\n"
            "首次执行需要原 SDK 环境，完成后编译和烧录不再依赖原 SDK。"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "使用示例：\n"
            "  python script/localize_project.py\n"
            "  python script/localize_project.py --sdk-env E:\\HPM\\hpm_sdk_env\n"
            "  python script/localize_project.py --project-name my_app\n"
            "  python script/localize_project.py --sample filex --project-name my_filex_app\n"
            "  python script/localize_project.py --sample netxduo/ping --force\n"
            "  python script/localize_project.py --recreate --sample filex --project-name my_app\n"
            "  python script/localize_project.py --refresh-toolchain\n"
            "\n说明：\n"
            "  --sample 只在工程还没有 CMakeLists.txt 和 src/ 时生效；\n"
            "  --project-name 指定工程名，同时更新已有 CMakeLists 的 APP_NAME 和 project()；\n"
            "  --recreate 会备份 CMakeLists.txt 和 src/，清理本地化 SDK 和 build/ 后重建；\n"
            "  --force 重新生成 SDK，--refresh-toolchain 只重新复制 tools/ 和 toolchain/。"
        ),
    )
    parser.add_argument(
        "--sdk-env",
        help="原 HPM SDK 环境根目录。示例：--sdk-env E:\\HPM\\hpm_sdk_env；"
        "默认：%s 或环境变量 HPM_SDK_ENV" % DEFAULT_SDK_ENV,
    )
    parser.add_argument(
        "--sdk",
        help="直接指定 hpm_sdk 目录，优先级高于 --sdk-env。"
        "示例：--sdk E:\\HPM\\hpm_sdk_env\\hpm_sdk",
    )
    parser.add_argument(
        "--board",
        default=DEFAULT_BOARD,
        help="目标板卡。示例：--board hpm5300evk；默认：%s" % DEFAULT_BOARD,
    )
    parser.add_argument(
        "--sample",
        default=DEFAULT_SAMPLE,
        help="首次创建工程时使用的 SDK sample。支持名称（filex、netxduo/ping）、"
        "相对 hpm_sdk 路径（samples/hello_world、samples/rtos/eclipse_threadx/hello_world）"
        "或绝对路径。"
        "默认：%s；仅在工程首次创建时生效" % DEFAULT_SAMPLE,
    )
    parser.add_argument(
        "--project-name",
        help="工程名称，也是输出文件名前缀。示例：--project-name my_app；"
        "默认：首次创建时为 %s，已有工程保持 CMakeLists 中的 APP_NAME；"
        "仅支持字母、数字、下划线、连字符，且不能以数字开头" % DEFAULT_PROJECT_NAME,
    )
    parser.add_argument(
        "--hpm-build-type",
        default=DEFAULT_HPM_BUILD_TYPE,
        choices=SUPPORTED_BUILD_TYPES,
        help="依赖分析使用的 HPM 构建类型。示例：--hpm-build-type flash_xip、"
        "--hpm-build-type ram；默认：%s" % DEFAULT_HPM_BUILD_TYPE,
    )
    parser.add_argument(
        "-f",
        "--force",
        action="store_true",
        help="重新生成本地化 SDK。示例：python script/localize_project.py --force",
    )
    parser.add_argument(
        "--refresh-toolchain",
        action="store_true",
        help="重新复制 tools/ 和 toolchain/，不重新生成 SDK。"
        "示例：python script/localize_project.py --refresh-toolchain",
    )
    parser.add_argument(
        "--recreate",
        action="store_true",
        help="备份 CMakeLists.txt 和 src/，清理本地化 SDK、localized_sdk.cmake 和 build/，"
        "然后按当前参数重新创建工程。示例：python script/localize_project.py "
        "--recreate --sample filex --project-name my_app",
    )
    args = parser.parse_args(argv)

    try:
        sdk_base, sdk_env = resolve_paths(args.sdk, args.sdk_env)
        # 先校验参数，避免清理后才报错
        validate_board_and_build_type(sdk_base, args.board, args.hpm_build_type)
        if args.project_name:
            validate_project_name(args.project_name)
        if args.recreate:
            recreate_project(PROJECT_DIR)
        print_project_info(
            PROJECT_DIR,
            sdk_base,
            sdk_env,
            args.board,
            args.hpm_build_type,
            args.sample,
            args.project_name,
            args.force,
        )
        localize(
            PROJECT_DIR,
            sdk_base,
            sdk_env,
            args.board,
            args.hpm_build_type,
            args.sample,
            args.project_name,
            args.force,
            args.refresh_toolchain,
        )
    except subprocess.CalledProcessError as exc:
        command = (
            subprocess.list2cmdline(exc.cmd)
            if isinstance(exc.cmd, (list, tuple))
            else str(exc.cmd)
        )
        print(
            "\n错误：命令执行失败，退出码 %s。\n命令：%s\n"
            "请查看上方 CMake/SDK 输出。常见原因：\n"
            "  1. 板卡或构建类型不支持（例如 HPM5300 不支持 flash_sdram_xip）；\n"
            "  2. sample 名称或路径写错；\n"
            "  3. 文件被 IDE、终端或杀毒软件占用。\n"
            "可使用 --recreate 备份应用并清理后重试。"
            % (exc.returncode, command),
            file=sys.stderr,
        )
        return 1
    except RuntimeError as exc:
        print("\n错误：%s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
