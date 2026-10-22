#!/usr/bin/env python3
"""Live AegisOS benchmark using measured foreground probe latency and real CPU contention.

Requires Linux/WSL with systemd user scopes and cgroup v2 CPU controller enabled.
Only the two explicitly launched benchmark commands are managed.
"""
from __future__ import annotations
import argparse, csv, json, os, signal, subprocess, sys, time
from datetime import datetime, timezone
from pathlib import Path

STOP = False
def stop_handler(_sig, _frame):
    global STOP
    STOP = True

def now():
    return datetime.now(timezone.utc).astimezone().isoformat(timespec="milliseconds")

def probe_worker(csv_path: str, interval: float, work: int):
    p = Path(csv_path); p.parent.mkdir(parents=True, exist_ok=True)
    with p.open("a", newline="", buffering=1) as f:
        w = csv.writer(f)
        if p.stat().st_size == 0: w.writerow(["timestamp", "probe_latency_ms", "iterations"])
        while True:
            start = time.perf_counter(); x = 0
            # Wall time includes both execution and any delay waiting to be scheduled.
            for i in range(work): x = ((x * 33) ^ i) & 0xffffffff
            w.writerow([now(), f"{(time.perf_counter()-start)*1000.0:.4f}", work])
            time.sleep(interval)

def background_worker(progress_csv: str | None = None):
    x = 1
    completed = 0
    path = Path(progress_csv) if progress_csv else None
    if path:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", newline="", buffering=1) as f:
            writer = csv.writer(f); writer.writerow(["timestamp", "completed_chunks"])
            last_write = time.monotonic()
            while True:
                for i in range(200000): x = (x * 1664525 + i + 1013904223) & 0xffffffff
                completed += 1
                if time.monotonic() - last_write >= 1.0:
                    writer.writerow([now(), completed]); last_write = time.monotonic()
    else:
        while True:
            for i in range(200000): x = (x * 1664525 + i + 1013904223) & 0xffffffff

def run(args):
    root = Path(args.log_dir); root.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    probe_csv = root / f"live-probes-{args.mode}-{stamp}.csv"
    events_path = root / f"live-benchmark-events-{args.mode}-{stamp}.jsonl"
    background_progress_csv = root / f"live-background-{args.mode}-{stamp}.csv"
    def event(kind, **fields):
        with events_path.open("a", encoding="utf-8") as f:
            f.write(json.dumps({"timestamp": now(), "event": kind, **fields}) + "\n")
    if not Path("/sys/fs/cgroup/cgroup.controllers").exists():
        raise RuntimeError("cgroup v2 is unavailable; live CPUWeight benchmark cannot run here.")
    try: controllers = Path("/sys/fs/cgroup/cgroup.controllers").read_text().split()
    except OSError as e: raise RuntimeError(f"Cannot read cgroup v2 controllers: {e}")
    if "cpu" not in controllers: raise RuntimeError("The cgroup v2 CPU controller is unavailable.")
    fg_unit, bg_unit = f"aegisos-probe-{os.getpid()}.scope", f"aegisos-load-{os.getpid()}.scope"
    py = sys.executable; script = str(Path(__file__).resolve()); children = []
    fg_cmd = ["systemd-run", "--user", "--scope", "--unit", fg_unit, "--property=CPUWeight=600", py, script,
              "--probe-worker", str(probe_csv), str(args.probe_interval), str(args.work)]
    bg_cmd = ["systemd-run", "--user", "--scope", "--unit", bg_unit, "--property=CPUWeight=100", py, script, "--background-worker", "--background-progress-csv", str(background_progress_csv)]
    try:
        fg = subprocess.Popen(fg_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True); children.append(fg)
        bg = subprocess.Popen(bg_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True); children.append(bg)
        event("benchmark_started", foreground_unit=fg_unit, background_unit=bg_unit, initial_weights=[600,100],
              probe_interval_seconds=args.probe_interval, probe_work_iterations=args.work,
              measurement="wall-clock duration of bounded foreground CPU probe; includes scheduling delay")
        print(f"Live benchmark running. Probe CSV: {probe_csv}\nStop with Ctrl+C. Only AegisOS benchmark processes are managed.", flush=True)
        signal.signal(signal.SIGINT, stop_handler); signal.signal(signal.SIGTERM, stop_handler)
        last_line = 0; fg_weight, bg_weight = 600, 100; high_count = low_count = 0
        deadline = time.monotonic() + args.duration if args.duration else None
        while not STOP and (deadline is None or time.monotonic() < deadline):
            time.sleep(args.interval)
            if fg.poll() is not None or bg.poll() is not None:
                event("benchmark_child_exited", foreground_returncode=fg.poll(), background_returncode=bg.poll()); break
            try:
                with probe_csv.open(newline="", encoding="utf-8") as f: rows = list(csv.DictReader(f))
            except (OSError, csv.Error): continue
            if len(rows) <= last_line: continue
            new_rows = rows[last_line:]; last_line = len(rows)
            recent = [float(r["probe_latency_ms"]) for r in new_rows if r.get("probe_latency_ms")]
            if not recent: continue
            latest = recent[-1]
            if args.mode == "baseline":
                continue
            if latest > args.latency_threshold_ms: high_count += 1; low_count = 0
            else: low_count += 1; high_count = 0
            desired = (800,100) if high_count >= args.consecutive_samples else ((400,150) if low_count >= args.consecutive_samples else (fg_weight,bg_weight))
            if desired != (fg_weight,bg_weight):
                old = [fg_weight,bg_weight]
                try:
                    subprocess.run(["systemctl","--user","set-property",fg_unit,f"CPUWeight={desired[0]}"],check=True,capture_output=True,text=True)
                    subprocess.run(["systemctl","--user","set-property",bg_unit,f"CPUWeight={desired[1]}"],check=True,capture_output=True,text=True)
                except subprocess.CalledProcessError as e:
                    event("cpu_weight_update_failed", error=(e.stderr or str(e)).strip(), attempted_weights=desired)
                else:
                    fg_weight,bg_weight = desired
                    reason = "measured foreground probe latency above threshold" if desired[0] > old[0] else "probe latency below threshold; restore balanced weights"
                    event("cpu_weight_changed", old_weights=old, new_weights=desired, latest_probe_latency_ms=round(latest,4),
                          recent_mean_probe_latency_ms=round(sum(recent)/len(recent),4), threshold_ms=args.latency_threshold_ms, reason=reason)
                    print(f"{now()} probe={latest:.2f} ms, CPUWeight {old} -> {desired}", flush=True)
        event("benchmark_stopping", final_weights=[fg_weight,bg_weight])
    finally:
        for p in children:
            if p.poll() is None: p.terminate()
        for p in children:
            try: p.wait(timeout=3)
            except subprocess.TimeoutExpired: p.kill()
        for unit in (fg_unit,bg_unit): subprocess.run(["systemctl","--user","stop",unit],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    print(f"Benchmark logs saved in {root}")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log-dir",default="live_data"); ap.add_argument("--interval",type=float,default=1.0,help="controller sampling interval")
    ap.add_argument("--probe-interval",type=float,default=.2,help="seconds between foreground probes")
    ap.add_argument("--work",type=int,default=250000,help="bounded CPU iterations per probe")
    ap.add_argument("--latency-threshold-ms",type=float,default=8.0,help="measured foreground probe latency threshold")
    ap.add_argument("--mode", choices=("adaptive", "baseline"), default="adaptive", help="baseline keeps CPU weights fixed; adaptive changes weights based on measured latency")
    ap.add_argument("--consecutive-samples",type=int,default=3); ap.add_argument("--duration",type=float,default=0,help="seconds; 0 runs until Ctrl+C")
    ap.add_argument("--probe-worker",nargs=3,metavar=("CSV","INTERVAL","WORK")); ap.add_argument("--background-worker",action="store_true"); ap.add_argument("--background-progress-csv")
    a=ap.parse_args()
    if a.probe_worker: probe_worker(a.probe_worker[0],float(a.probe_worker[1]),int(a.probe_worker[2])); return 0
    if a.background_worker: background_worker(a.background_progress_csv); return 0
    if a.interval<.2 or a.probe_interval<.05 or a.work<1000 or a.consecutive_samples<1: ap.error("intervals/work/sample counts are too small")
    try: run(a)
    except (RuntimeError,OSError) as e: print(f"AegisOS live benchmark error: {e}",file=sys.stderr); return 2
    return 0
if __name__=="__main__": raise SystemExit(main())
