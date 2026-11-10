#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
open_jlink_rtt.py - 一键启动 J-Link RTT 调试环境（JLink.exe + SecureCRT）

流程：
  1. 结束已经在运行的 JLink.exe（默认先询问，可用 -y 跳过询问）；
  2. 用同目录下的 jlink_rtt.jlink 启动 JLink.exe（新开一个控制台窗口，保持运行）；
  3. 等待 RTT Telnet 端口（默认 19021）就绪；
  4. 打开 SecureCRT 的 127.0.0.1 会话（可用 --session / --crt 修改）。

常用用法：
    python open_jlink_rtt.py             # 结束旧 JLink 并重启（不再询问），并打开 SecureCRT
    python open_jlink_rtt.py --ask       # 重启前先询问一次 [Y/n]
    python open_jlink_rtt.py --no-kill   # 不动已经跑着的 JLink
    python open_jlink_rtt.py --no-crt    # 只启动 JLink，不打开 SecureCRT
    python open_jlink_rtt.py --keep-old-crt  # 保留上次的 RTT 窗口（默认会自动关掉旧的）

直接 python open_jlink_rtt.py 和双击 open_jlink_rtt.cmd 效果一样；
.cmd 只是为了在没有 .py 文件关联的系统上也能双击运行（并帮你找到 Python）。
默认会先关掉上次留下的 RTT 窗口（标题形如 "<会话名> - ..." 的 SecureCRT），
避免每运行一次就多出一个窗口。
"""

import argparse
import glob
import os
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_JLINK_SCRIPT = SCRIPT_DIR / "jlink_rtt.jlink"
DEFAULT_RTT_PORT = 19021
DEFAULT_CRT_SESSION = "127.0.0.1"
DEFAULT_TIMEOUT = 60.0
WEB_PANEL_PORT = 19080

CREATE_NEW_CONSOLE = 0x00000010
CREATE_NO_WINDOW = 0x08000000

JLINK_PATTERNS = [
    r"C:\Program Files\SEGGER\JLink*\JLink.exe",
    r"C:\Program Files (x86)\SEGGER\JLink*\JLink.exe",
    r"D:\Program Files\SEGGER\JLink*\JLink.exe",
    r"D:\SEGGER\JLink*\JLink.exe",
]

CRT_PATTERNS = [
    r"D:\Program Files\SecureCRT*_Portable\*\App\SecureCRT+FX\SecureCRT.exe",
    r"C:\Program Files\SecureCRT*_Portable\*\App\SecureCRT+FX\SecureCRT.exe",
    r"C:\Program Files\VanDyke Software\Clients\SecureCRT.exe",
    r"C:\Program Files (x86)\VanDyke Software\Clients\SecureCRT.exe",
]


def run_hidden(cmd):
    """执行命令并返回输出（不弹黑框）。"""
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           creationflags=CREATE_NO_WINDOW)
    except OSError:
        return ""
    return p.stdout.decode("mbcs", errors="replace")


def tasklist_pids(image_name):
    """返回指定映像名的进程 PID 列表。"""
    out = run_hidden(["tasklist", "/FI", "IMAGENAME eq " + image_name,
                      "/FO", "CSV", "/NH"])
    pids = []
    for line in out.splitlines():
        line = line.strip()
        if not line.startswith('"'):
            continue
        cols = [c.strip().strip('"') for c in line.split('","')]
        if len(cols) >= 2:
            try:
                pids.append(int(cols[1]))
            except ValueError:
                pass
    return pids


def list_securecrt_windows():
    """返回 [(pid, 窗口标题), ...]（tasklist /V 的最后一列是窗口标题）。"""
    out = run_hidden(["tasklist", "/V", "/FI", "IMAGENAME eq SecureCRT.exe",
                      "/FO", "CSV", "/NH"])
    result = []
    for line in out.splitlines():
        line = line.strip()
        if not line.startswith('"'):
            continue
        cols = [c.strip().strip('"') for c in line.split('","')]
        if len(cols) < 9:
            continue
        try:
            result.append((int(cols[1]), cols[-1]))
        except ValueError:
            pass
    return result


def close_old_rtt_windows(session):
    """关掉上次留下的 RTT 窗口（标题以 "<会话名> - " 开头），避免窗口越堆越多。

    流程：先"温柔关闭"（taskkill，不带 /F，发 WM_CLOSE） -> 等 1 秒 -> 没退出再强制关闭。
    """
    victims = [(pid, title) for pid, title in list_securecrt_windows()
               if title.startswith(session + " - ")]
    if not victims:
        return
    for pid, title in victims:
        print("[i] 关闭旧的 RTT 窗口：PID %d（%s）" % (pid, title))
        run_hidden(["taskkill", "/PID", str(pid)])
    victim_pids = {pid for pid, _ in victims}
    deadline = time.time() + 1.0
    while time.time() < deadline:
        if not (victim_pids & set(tasklist_pids("SecureCRT.exe"))):
            return
        time.sleep(0.2)
    # 1 秒内没退出 => 强制关闭
    left = victim_pids & set(tasklist_pids("SecureCRT.exe"))
    if left:
        print("[i] 温柔关闭没成功，强制关闭：PID %s" % ", ".join(str(p) for p in sorted(left)))
        for pid in left:
            run_hidden(["taskkill", "/PID", str(pid), "/F"])
        deadline = time.time() + 1.0
        while time.time() < deadline:
            if not (victim_pids & set(tasklist_pids("SecureCRT.exe"))):
                return
            time.sleep(0.2)
        print("[!] 有旧的 SecureCRT 窗口没能自动关闭（可以手动把它关掉）")


def netstat_lines():
    return run_hidden(["netstat", "-ano", "-p", "TCP"]).splitlines()


def port_listening(port):
    """端口是否处于 LISTENING 状态。"""
    needle = ":" + str(port)
    for line in netstat_lines():
        cols = line.split()
        if len(cols) >= 4 and cols[0].upper() == "TCP" \
                and cols[1].endswith(needle) and cols[3].upper().startswith("LISTEN"):
            return True
    return False


def port_owner(port):
    """返回监听该 TCP 端口的 (pid, name)；找不到返回 None。"""
    out = run_hidden(["netstat", "-ano", "-p", "TCP"])
    pid = None
    for line in out.splitlines():
        cols = line.split()
        if len(cols) >= 5 and cols[0].upper() == "TCP" \
                and cols[3].upper().startswith("LISTEN") and cols[1].endswith(":" + str(port)):
            try:
                pid = int(cols[4])
                break
            except ValueError:
                pass
    if pid is None:
        return None
    name = ""
    out = run_hidden(["tasklist", "/FI", "PID eq %d" % pid, "/FO", "CSV", "/NH"])
    for line in out.splitlines():
        line = line.strip()
        if line.startswith('"'):
            name = line.split('","')[0].strip('"')
            break
    return pid, name


def port_has_client(port):
    """是否已经有客户端连上该端口（ESTABLISHED）。"""
    needle = ":" + str(port)
    for line in netstat_lines():
        cols = line.split()
        if len(cols) >= 4 and cols[0].upper() == "TCP" \
                and cols[1].endswith(needle) and cols[3].upper().startswith("ESTABLISHED"):
            return True
    return False


def find_jlink(explicit=None):
    """查找 JLink.exe（优先命令行参数，其次注册表，再按常见安装路径）。"""
    if explicit:
        p = Path(explicit)
        return p if p.is_file() else None
    env = os.environ.get("JLINK_EXE")
    if env and Path(env).is_file():
        return Path(env)
    try:
        import winreg
        for sub in (r"SOFTWARE\SEGGER\J-Link", r"SOFTWARE\WOW6432Node\SEGGER\J-Link"):
            try:
                with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, sub) as key:
                    install_path, _ = winreg.QueryValueEx(key, "InstallPath")
                    exe = Path(install_path) / "JLink.exe"
                    if exe.is_file():
                        return exe
            except OSError:
                continue
    except ImportError:
        pass
    found = []
    for pattern in JLINK_PATTERNS:
        found.extend(Path(p) for p in glob.glob(pattern))
    found = sorted({p for p in found if p.is_file()})
    return found[-1] if found else None


def find_securecrt(explicit=None):
    """查找 SecureCRT.exe（优先命令行参数，其次环境变量，再按常见安装路径）。"""
    if explicit:
        p = Path(explicit)
        return p if p.is_file() else None
    env = os.environ.get("SECURECRT_EXE")
    if env and Path(env).is_file():
        return Path(env)
    found = []
    for pattern in CRT_PATTERNS:
        found.extend(Path(p) for p in glob.glob(pattern))
    found = sorted({p for p in found if p.is_file()})
    return found[-1] if found else None


def find_crt_launcher(crt_exe):
    """便携版 SecureCRT：找包根目录下的启动器（它负责把 Config 路径指向便携 Data 目录）。"""
    try:
        root = crt_exe.parents[2]
    except IndexError:
        return None
    for name in ("SecureCRT_Portable(Admin).exe", "SecureCRT_Portable.exe"):
        cand = root / name
        if cand.is_file():
            return cand
    return None


def confirm(prompt, default_yes=True):
    try:
        reply = input(prompt).strip().lower()
    except EOFError:
        return default_yes
    if reply == "":
        return default_yes
    return reply in ("y", "yes")


def wait_port(port, timeout):
    print("[i] 等待 RTT Telnet 端口 %d 就绪" % port, end="", flush=True)
    deadline = time.time() + timeout
    while time.time() < deadline:
        if port_listening(port):
            print(" OK")
            return True
        print(".", end="", flush=True)
        time.sleep(0.5)
    print()
    return False


def fetch_rtt_page(web_port):
    """读取 J-Link Web 控制面板的 RTT 页面（失败返回 None）。"""
    try:
        with urllib.request.urlopen("http://127.0.0.1:%d/rtt.htm" % web_port, timeout=2) as resp:
            return resp.read().decode("utf-8", "replace")
    except Exception:
        return None


def wait_rtt_ready(timeout, web_port=WEB_PANEL_PORT):
    """等待 RTT 控制块真正就绪（面板里出现 RTT CB verified / Found RTT control block）。"""
    print("[i] 等待 RTT 控制块就绪", end="", flush=True)
    deadline = time.time() + timeout
    start = time.time()
    saw_page = False
    while time.time() < deadline:
        html = fetch_rtt_page(web_port)
        if html is not None:
            saw_page = True
            if ("RTT CB verified" in html) or ("RTT_RTTAddr$@0x" in html) \
                    or ("Found RTT control block" in html):
                print(" OK")
                return True
        elif not saw_page and time.time() - start > 5.0:
            print(" (Web 面板不可用，跳过等待)")
            return False
        print(".", end="", flush=True)
        time.sleep(0.5)
    print()
    return False


def open_securecrt(crt_exe, launcher, session, port, crt_was_running):
    if crt_was_running:
        # SecureCRT 已在运行：它自己的 Auto Reconnect 可能已经连上新端口，先等几秒
        deadline = time.time() + 5.0
        while time.time() < deadline:
            if port_has_client(port):
                print("[i] RTT 端口已有客户端连接（原会话已自动重连），不再新开会话")
                return
            time.sleep(0.5)
        print("[+] 在 SecureCRT 中打开会话：%s" % session)
        subprocess.Popen([str(crt_exe), "/S", session], cwd=str(crt_exe.parent))
    else:
        # SecureCRT 没在运行：用便携版启动器启动（它会把 Config 路径指到便携 Data 目录）
        target = launcher if launcher is not None else crt_exe
        print("[+] 启动 SecureCRT：%s" % target.name)
        subprocess.Popen([str(target), "/S", session], cwd=str(target.parent))


def main():
    parser = argparse.ArgumentParser(
        description="一键启动 J-Link RTT：JLink.exe（按 jlink_rtt.jlink 连接 MCU）+ SecureCRT 会话")
    parser.add_argument("--jlink", help="JLink.exe 路径（默认自动查找）")
    parser.add_argument("--script", help="J-Link Commander 命令脚本（默认：同目录 jlink_rtt.jlink）")
    parser.add_argument("--crt", help="SecureCRT.exe 路径（默认自动查找）")
    parser.add_argument("--session", default=DEFAULT_CRT_SESSION,
                        help="SecureCRT 会话名（默认：127.0.0.1）")
    parser.add_argument("--port", type=int, default=DEFAULT_RTT_PORT,
                        help="RTT Telnet 端口（默认：19021）")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT,
                        help="等待 RTT 就绪的超时秒数（默认：60）")
    parser.add_argument("-y", "--yes", action="store_true",
                        help="不询问，直接结束已运行的 JLink.exe（默认就是这个行为）")
    parser.add_argument("--ask", action="store_true",
                        help="重启前先询问一次 [Y/n]")
    parser.add_argument("--no-kill", action="store_true",
                        help="不结束已运行的 JLink.exe")
    parser.add_argument("--no-crt", action="store_true",
                        help="只启动 JLink，不打开 SecureCRT")
    parser.add_argument("--keep-old-crt", action="store_true",
                        help="不自动关闭上次留下的 RTT 窗口（默认会先关掉）")
    args = parser.parse_args()

    print("=== 启动 J-Link RTT ===")

    script = Path(args.script).expanduser() if args.script else DEFAULT_JLINK_SCRIPT
    if not script.is_file():
        print("[x] 找不到命令脚本：%s" % script)
        return 2

    jlink = find_jlink(args.jlink)
    if jlink is None:
        print("[x] 找不到 JLink.exe，请用 --jlink 指定")
        return 2

    crt = None if args.no_crt else find_securecrt(args.crt)
    if not args.no_crt and crt is None:
        print("[!] 找不到 SecureCRT.exe（可用 --crt 指定），本次只启动 JLink")
    if crt is not None and not args.keep_old_crt:
        # 先关掉上次留下的 RTT 窗口，避免每运行一次就多一个窗口
        close_old_rtt_windows(args.session)
    crt_was_running = bool(tasklist_pids("SecureCRT.exe")) if crt is not None else False

    print("[i] JLink.exe   : %s" % jlink)
    print("[i] 命令脚本    : %s" % script)
    print("[i] RTT 端口    : %d" % args.port)
    if crt is not None:
        print("[i] SecureCRT   : %s（会话：%s）" % (crt, args.session))

    start_new = True
    pids = tasklist_pids("JLink.exe")
    if pids:
        print("[i] 检测到正在运行的 JLink.exe：%s" % ", ".join(str(p) for p in pids))
        if args.no_kill:
            print("[i] --no-kill：沿用现有 JLink.exe，不重新启动")
            start_new = False
        else:
            if args.ask and not args.yes:
                do_kill = confirm("    结束它们并用 %s 重新启动？[Y/n] " % script.name)
            else:
                do_kill = True
            if do_kill:
                for pid in pids:
                    run_hidden(["taskkill", "/PID", str(pid), "/F"])
                time.sleep(1.0)
                if tasklist_pids("JLink.exe"):
                    print("[!] 没能结束 JLink.exe（可能需要以管理员身份运行）")
                else:
                    print("[+] 已结束旧的 JLink.exe")
            else:
                print("[i] 保留现有 JLink.exe")
                start_new = False

    if start_new:
        owner = port_owner(args.port)
        if owner is not None:
            owner_pid, owner_name = owner
            print("[!] RTT 端口 %d 已经被 %s (PID %d) 占用" % (args.port, owner_name, owner_pid))
            low = owner_name.lower()
            if low.startswith("emstudio"):
                print("    SEGGER Embedded Studio（SES）的调试会话会占用 J-Link 的 RTT 端口。")
                print("    本脚本启动的 JLink.exe 绑不上它，终端会连到 SES 那边，导致看不到日志。")
                print("    >>> 请先关闭 SES 的调试会话（或直接退出 SES），再重新运行本脚本。")
                return 1
            print("    本脚本启动的 JLink.exe 将无法绑定该端口，终端可能连到别的服务端。")
            print("    请先关闭占用它的程序（例如 J-Link RTT Viewer / 另一个 JLink 会话），再重新运行。")
            return 1
        cmd = [str(jlink), "-CommanderScript", str(script)]
        if args.port != DEFAULT_RTT_PORT:
            # 修改了 RTT 端口时同步给 JLink.exe
            cmd += ["-RTTTelnetPort", str(args.port)]
        print("[+] 启动 JLink.exe ...")
        subprocess.Popen(cmd, cwd=str(script.parent), creationflags=CREATE_NEW_CONSOLE)

    if not wait_port(args.port, args.timeout):
        print("[!] 等待超时：RTT 端口还没起来，请看 JLink 控制台里 connect 是否成功")

    if crt is not None:
        # 先把 SecureCRT 连上（RTT Telnet 端口已就绪），再等 J-Link 找到 RTT 控制块
        open_securecrt(crt, find_crt_launcher(crt), args.session, args.port, crt_was_running)
        if wait_rtt_ready(args.timeout):
            print("[+] RTT 控制块已就绪：SecureCRT 里现在应该能看到日志了")
        else:
            print("[!] 超时：还没等到 RTT 控制块")
            print("    如果 JLink 控制台停在提示上（如 JTAGConf> / Device>），在里面按一下回车；")
            print("    正常情况下 J-Link 会自己扫到控制块，SecureCRT 里稍后会出日志")

    print("=== 完成 ===")
    return 0


if __name__ == "__main__":
    try:
        sys.stdout.reconfigure(errors="replace")
    except Exception:
        pass
    sys.exit(main())
