#!/usr/bin/env python3
"""AegisOS live Linux process monitor and opt-in systemd/cgroup CPU-weight controller.

The monitor reads /proc and writes persistent CSV/JSONL logs. The run mode launches
explicitly selected foreground/background commands in separate systemd user scopes
and adjusts their cgroup v2 CPUWeight properties while they run.
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import shlex
import shutil
import signal
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Optional, Tuple

CLK_TCK = os.sysconf(os.sysconf_names["SC_CLK_TCK"])
PAGE_SIZE = os.sysconf("SC_PAGE_SIZE")
STOP = False


def now_iso() -> str:
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="milliseconds")


def on_signal(_signum, _frame):
    global STOP
    STOP = True


def read_process(pid: int) -> Optional[dict]:
    base = Path("/proc") / str(pid)
    try:
        raw = (base / "stat").read_text()
        # comm is parenthesized and may itself contain spaces or parentheses.
        left = raw.find("(")
        right = raw.rfind(")")
        if left < 0 or right < left:
            return None
        comm = raw[left + 1:right]
        fields = raw[right + 2:].split()  # begins with state (field 3)
        state = fields[0]
        ppid = int(fields[1])
        utime = int(fields[11])
        stime = int(fields[12])
        rss_pages = int(fields[21])
        cpu_ticks = utime + stime
        try:
            cmdline = (base / "cmdline").read_bytes().replace(b"\0", b" ").decode(errors="replace").strip()
        except (OSError, PermissionError):
            cmdline = ""
        if not cmdline:
            cmdline = f"[{comm}]"
        try:
            cgroup = (base / "cgroup").read_text().strip().replace("\n", ";")
        except (OSError, PermissionError):
            cgroup = ""
        return {
            "pid": pid,
            "ppid": ppid,
            "comm": comm,
            "cmdline": cmdline,
            "state": state,
            "cpu_ticks": cpu_ticks,
            "rss_kb": max(0, rss_pages * PAGE_SIZE // 1024),
            "cgroup": cgroup,
        }
    except (OSError, ValueError, IndexError, ProcessLookupError):
        return None


def snapshot_processes() -> Dict[int, dict]:
    result = {}
    try:
        entries = os.scandir("/proc")
    except OSError:
        return result
    with entries:
        for ent in entries:
            if ent.name.isdigit():
                pid = int(ent.name)
                proc = read_process(pid)
                if proc:
                    result[pid] = proc
    return result


class LiveLogger:
    def __init__(self, directory: Path):
        self.directory = directory
        directory.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
        self.process_path = directory / f"processes-{stamp}.csv"
        self.event_path = directory / f"events-{stamp}.jsonl"
        self.process_file = self.process_path.open("w", newline="", encoding="utf-8")
        self.events_file = self.event_path.open("w", encoding="utf-8")
        self.writer = csv.DictWriter(self.process_file, fieldnames=[
            "timestamp", "pid", "ppid", "comm", "cmdline", "state", "cpu_percent",
            "rss_kb", "cgroup",
        ])
        self.writer.writeheader()
        self.process_file.flush()
        self.event("session_start", mode="live_monitor", process_csv=str(self.process_path),
                   events_jsonl=str(self.event_path), platform=platform.platform(),
                   python=platform.python_version(), pid=os.getpid())

    def event(self, event_type: str, **data):
        item = {"timestamp": now_iso(), "event": event_type, **data}
        self.events_file.write(json.dumps(item, ensure_ascii=False) + "\n")
        self.events_file.flush()

    def process_rows(self, current: Dict[int, dict], previous: Dict[int, dict], elapsed: float):
        ts = now_iso()
        for pid, proc in current.items():
            old = previous.get(pid)
            delta = max(0, proc["cpu_ticks"] - old["cpu_ticks"]) if old else 0
            # 100% means one fully utilized logical CPU; multi-threaded processes can exceed 100%.
            cpu_percent = (delta / CLK_TCK / max(elapsed, 0.001)) * 100.0
            self.writer.writerow({
                "timestamp": ts, "pid": pid, "ppid": proc["ppid"], "comm": proc["comm"],
                "cmdline": proc["cmdline"], "state": proc["state"],
                "cpu_percent": f"{cpu_percent:.2f}", "rss_kb": proc["rss_kb"], "cgroup": proc["cgroup"],
            })
            if old is None:
                self.event("process_started_or_discovered", pid=pid, ppid=proc["ppid"], comm=proc["comm"], cmdline=proc["cmdline"])
        for pid, old in previous.items():
            if pid not in current:
                self.event("process_exited_or_disappeared", pid=pid, comm=old["comm"], cmdline=old["cmdline"])
        self.process_file.flush()

    def close(self):
        self.event("session_end")
        self.process_file.close()
        self.events_file.close()


def draw_dashboard(mode: str, current: Dict[int, dict], cpu: Dict[int, float], interval: float,
                   weights: Optional[Tuple[int, int]] = None, message: str = ""):
    print("\033[2J\033[H", end="")
    print("AegisOS — LIVE Linux Process Monitor / CPU Controller")
    print(f"Mode: {mode} | Refresh: {interval:g}s | Local time: {datetime.now().astimezone().isoformat(timespec='seconds')}")
    if weights:
        print(f"Managed cgroup CPUWeight: foreground={weights[0]} background={weights[1]}")
    if message:
        print(message)
    print("\nTop processes by CPU (Ctrl+C to stop):")
    print(f"{'PID':>7} {'PPID':>7} {'CPU%':>7} {'RSS MiB':>8} {'STAT':>5}  COMMAND")
    rows = sorted(current.values(), key=lambda p: cpu.get(p["pid"], 0), reverse=True)[:15]
    for p in rows:
        print(f"{p['pid']:>7} {p['ppid']:>7} {cpu.get(p['pid'], 0):>6.1f} {p['rss_kb']/1024:>8.1f} {p['state']:>5}  {p['cmdline'][:100]}")
    print("\nLogs are written continuously to disk; stopping this display does not erase them.")


def monitor_loop(logger: LiveLogger, interval: float, ui: bool, mode: str,
                 controller=None, stop_when=None):
    previous = snapshot_processes()
    logger.event("initial_process_snapshot", process_count=len(previous), pids=sorted(previous.keys()))
    last = time.monotonic()
    cpu = {pid: 0.0 for pid in previous}
    if ui:
        draw_dashboard(mode, previous, cpu, interval)
    while not STOP and not (stop_when and stop_when()):
        time.sleep(interval)
        current = snapshot_processes()
        now = time.monotonic()
        elapsed = now - last
        for pid, proc in current.items():
            old = previous.get(pid)
            delta = max(0, proc["cpu_ticks"] - old["cpu_ticks"]) if old else 0
            cpu[pid] = (delta / CLK_TCK / max(elapsed, 0.001)) * 100.0
        logger.process_rows(current, previous, elapsed)
        if controller:
            controller.update(current, cpu, logger)
        if ui:
            weights = controller.weights if controller else None
            draw_dashboard(mode, current, cpu, interval, weights, controller.message if controller else "")
        previous, last = current, now


def systemd_user_status() -> Tuple[bool, str]:
    if not shutil.which("systemd-run") or not shutil.which("systemctl"):
        return False, "systemd-run/systemctl not found"
    try:
        p = subprocess.run(["systemctl", "--user", "is-system-running"], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True, timeout=5)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return False, f"could not query user systemd manager ({exc})"
    state = p.stdout.strip() or p.stderr.strip()
    diagnostic = (p.stderr + " " + p.stdout).lower()
    if ("failed to connect" in diagnostic or "no such file or directory" in diagnostic
            or state.lower() in ("offline", "unknown") or p.returncode not in (0, 1)):
        return False, f"user systemd manager unavailable ({state})"
    return True, f"user systemd manager reachable ({state})"


def check_systemd_user() -> None:
    available, detail = systemd_user_status()
    if not available:
        raise RuntimeError(detail + ". Use monitor mode or run this in a login session with `systemctl --user` working.")
    if not Path("/sys/fs/cgroup/cgroup.controllers").exists():
        raise RuntimeError("This system does not expose cgroup v2 at /sys/fs/cgroup; live CPUWeight control is unavailable.")
    try:
        controllers = Path("/sys/fs/cgroup/cgroup.controllers").read_text().split()
    except OSError as exc:
        raise RuntimeError(f"Cannot read cgroup v2 controllers: {exc}") from exc
    if "cpu" not in controllers:
        raise RuntimeError("The cgroup v2 CPU controller is not available; CPUWeight control cannot be used.")


def print_doctor() -> int:
    print("AegisOS environment check")
    print(f"Platform: {platform.platform()}")
    print(f"Python:   {platform.python_version()}")
    print(f"/proc:    {'available' if Path('/proc/self/stat').exists() else 'NOT available'}")
    print(f"cgroup v2: {'available' if Path('/sys/fs/cgroup/cgroup.controllers').exists() else 'NOT available'}")
    if Path("/sys/fs/cgroup/cgroup.controllers").exists():
        try:
            print(f"controllers: {Path('/sys/fs/cgroup/cgroup.controllers').read_text().strip()}")
        except OSError as exc:
            print(f"controllers: unreadable ({exc})")
    available, detail = systemd_user_status()
    print(f"systemd user: {detail}")
    can_control = available and Path("/sys/fs/cgroup/cgroup.controllers").exists()
    if can_control:
        try:
            can_control = "cpu" in Path("/sys/fs/cgroup/cgroup.controllers").read_text().split()
        except OSError:
            can_control = False
    print(f"Live CPUWeight controller prerequisites: {'present (scope permissions still need runtime verification)' if can_control else 'not satisfied'}")
    if not can_control:
        print("Monitor-only mode can still collect /proc data. WSL/systemd setup may need configuration for live control.")
    return 0 if Path("/proc/self/stat").exists() else 1


def set_cpu_weight(unit: str, weight: int) -> None:
    p = subprocess.run(["systemctl", "--user", "set-property", unit, f"CPUWeight={weight}"],
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        detail = (p.stderr or p.stdout).strip()
        raise RuntimeError(f"Could not set {unit} CPUWeight={weight}: {detail}")


@dataclass
class AdaptiveController:
    foreground_unit: str
    background_unit: str
    foreground_weight: int = 500
    background_weight: int = 500
    message: str = "Controller waiting for live CPU measurements"
    last_update: float = 0.0

    @property
    def weights(self):
        return self.foreground_weight, self.background_weight

    def _scope_cpu(self, current: Dict[int, dict], cpu: Dict[int, float], unit: str) -> float:
        marker = "/" + unit
        total = 0.0
        for pid, proc in current.items():
            if marker in proc["cgroup"] or proc["cgroup"].endswith(unit):
                total += cpu.get(pid, 0.0)
        return total

    def update(self, current: Dict[int, dict], cpu: Dict[int, float], logger: LiveLogger):
        # A scope may finish before the other command. Never try to update an inactive unit.
        fg_state = subprocess.run(["systemctl", "--user", "show", self.foreground_unit,
                                   "--property=ActiveState", "--value"],
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        bg_state = subprocess.run(["systemctl", "--user", "show", self.background_unit,
                                   "--property=ActiveState", "--value"],
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        if fg_state.returncode != 0 or bg_state.returncode != 0 or fg_state.stdout.strip() != "active" or bg_state.stdout.strip() != "active":
            self.message = "One managed command has exited; no further CPU-weight changes will be applied."
            if not getattr(self, "inactive_noted", False):
                logger.event("managed_scope_inactive", foreground_state=fg_state.stdout.strip(),
                             background_state=bg_state.stdout.strip())
                self.inactive_noted = True
            return
        fg_cpu = self._scope_cpu(current, cpu, self.foreground_unit)
        bg_cpu = self._scope_cpu(current, cpu, self.background_unit)
        # Formula-based policy: allocate a total relative-weight budget of 1000.
        # Foreground share = clamp(0.55 + 0.35 * fg_demand/(fg_demand+bg_demand),
        #                          0.55, 0.90). If both scopes are idle, use the
        # minimum foreground preference (55%). Convert the share to integer weights.
        # CPUWeight is relative scheduling weight, not a hard CPU percentage cap.
        total_demand = fg_cpu + bg_cpu
        demand_ratio = fg_cpu / total_demand if total_demand > 0.01 else 0.0
        foreground_share = max(0.55, min(0.90, 0.55 + 0.35 * demand_ratio))
        fg_weight = int(round(foreground_share * 1000 / 10) * 10)
        desired = (max(100, min(900, fg_weight)), 1000 - max(100, min(900, fg_weight)))
        why = (f"formula policy: foreground demand ratio={demand_ratio:.3f}; "
               f"target foreground share={foreground_share:.1%}; total weight budget=1000")
        if desired != self.weights:
            old = self.weights
            try:
                set_cpu_weight(self.foreground_unit, desired[0])
                try:
                    set_cpu_weight(self.background_unit, desired[1])
                except RuntimeError:
                    # Best-effort rollback so a partial update does not silently
                    # leave a mismatched pair of weights.
                    set_cpu_weight(self.foreground_unit, old[0])
                    raise
            except RuntimeError as exc:
                self.message = f"CPUWeight update failed; no successful policy change recorded: {exc}"
                logger.event("cpu_weight_update_failed", foreground_unit=self.foreground_unit,
                             background_unit=self.background_unit, attempted_weights=desired,
                             retained_weights=old, reason=str(exc))
                return
            self.foreground_weight, self.background_weight = desired
            logger.event("cpu_weight_changed", foreground_unit=self.foreground_unit,
                         background_unit=self.background_unit, old_weights=old,
                         new_weights=desired, foreground_cpu_percent=round(fg_cpu, 2),
                         background_cpu_percent=round(bg_cpu, 2), reason=why)
            self.message = f"Last controller decision: {why}"
        else:
            self.message = f"Policy: {why}; weights unchanged"


def launch_scope(label: str, command: str, initial_weight: int) -> Tuple[subprocess.Popen, str]:
    argv = shlex.split(command)
    if not argv:
        raise ValueError(f"The {label} command is empty")
    unit_base = f"aegisos-{label}-{os.getpid()}"
    unit = unit_base + ".scope"
    cmd = ["systemd-run", "--user", "--scope", "--unit", unit_base,
           f"--property=CPUWeight={initial_weight}", "--", *argv]
    proc = subprocess.Popen(cmd)
    # Give systemd a moment to create the scope, then fail clearly if startup failed.
    time.sleep(0.35)
    check = subprocess.run(["systemctl", "--user", "show", unit, "--property=LoadState", "--value"],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if check.returncode != 0 or check.stdout.strip() != "loaded":
        # Very short commands can finish successfully and have their transient unit
        # unloaded before this check; treat that as a completed launch, not a failure.
        if proc.poll() is not None and proc.returncode == 0:
            return proc, unit
        raise RuntimeError(f"systemd did not create {unit}. Check systemd-run output and the command: {command}")
    return proc, unit


def run_managed(args) -> int:
    check_systemd_user()
    fg_proc = bg_proc = None
    logger = LiveLogger(Path(args.log_dir))
    try:
        fg_proc, fg_unit = launch_scope("foreground", args.foreground, 500)
        logger.event("managed_scope_started", role="foreground", unit=fg_unit, command=args.foreground, initial_cpu_weight=500)
        try:
            bg_proc, bg_unit = launch_scope("background", args.background, 500)
        except Exception:
            fg_proc.terminate()
            raise
        logger.event("managed_scope_started", role="background", unit=bg_unit, command=args.background, initial_cpu_weight=500)
        controller = AdaptiveController(fg_unit, bg_unit, 600, 100)
        logger.event("controller_started", policy="adaptive_cpu_weight", interval_seconds=args.interval,
                     explanation="Uses live /proc CPU measurements and changes systemd scope CPUWeight; weight is relative, not a percentage cap.")
        print(f"AegisOS is managing two explicitly launched commands.\nForeground scope: {fg_unit}\nBackground scope: {bg_unit}\nLogs: {args.log_dir}\nPress Ctrl+C to stop monitoring; child commands will be terminated.", flush=True)
        try:
            monitor_loop(logger, args.interval, not args.no_ui, "LIVE CONTROL", controller,
                         stop_when=lambda: fg_proc.poll() is not None and bg_proc.poll() is not None)
        finally:
            # Stop the systemd scopes themselves; terminating only systemd-run's
            # waiting client can otherwise leave the managed command running.
            for unit in (fg_unit, bg_unit):
                subprocess.run(["systemctl", "--user", "stop", unit], stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, check=False)
            for proc in (fg_proc, bg_proc):
                if proc and proc.poll() is None:
                    proc.terminate()
            for proc in (fg_proc, bg_proc):
                if proc:
                    try:
                        proc.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        proc.kill()
            logger.event("controller_stopped", foreground_unit=fg_unit, background_unit=bg_unit)
    finally:
        logger.close()
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="AegisOS live Ubuntu process monitor and opt-in cgroup CPU-weight controller")
    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("doctor", help="check whether this environment supports live CPU-weight control")
    mon = sub.add_parser("monitor", help="Monitor all visible /proc processes and persist live logs; does not change scheduling")
    mon.add_argument("--interval", type=float, default=1.0, help="sampling interval in seconds (default: 1)")
    mon.add_argument("--log-dir", default="live_data", help="directory for timestamped CSV/JSONL logs")
    mon.add_argument("--no-ui", action="store_true", help="disable the refreshing terminal dashboard")
    run = sub.add_parser("run", help="launch two commands in separate user systemd scopes and adapt their real CPU weights")
    run.add_argument("--foreground", required=True, help="foreground command to launch, quoted as one string")
    run.add_argument("--background", required=True, help="background command to launch, quoted as one string")
    run.add_argument("--interval", type=float, default=1.0, help="sampling interval in seconds (default: 1)")
    run.add_argument("--log-dir", default="live_data", help="directory for timestamped CSV/JSONL logs")
    run.add_argument("--no-ui", action="store_true", help="disable the refreshing terminal dashboard")
    args = parser.parse_args()
    if args.mode != "doctor" and args.interval < 0.2:
        parser.error("--interval must be at least 0.2 seconds")
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, on_signal)
    try:
        if args.mode == "doctor":
            return print_doctor()
        if args.mode == "monitor":
            logger = LiveLogger(Path(args.log_dir))
            try:
                logger.event("monitor_config", interval_seconds=args.interval, source="/proc")
                print(f"AegisOS live monitoring started. Logs: {args.log_dir}. Press Ctrl+C to stop.", flush=True)
                monitor_loop(logger, args.interval, not args.no_ui, "MONITOR ONLY")
            finally:
                logger.close()
            return 0
        return run_managed(args)
    except (RuntimeError, ValueError, OSError) as exc:
        print(f"AegisOS live error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
