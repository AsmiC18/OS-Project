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

def calculate_weights(latency_ms: float, target_ms: float, total_weight: int = 1000,
                      base_share: float = 0.50, coefficient: float = 0.20,
                      min_share: float = 0.55, max_share: float = 0.90, round_to: int = 10):
    """Return weights calculated from measured latency, not a lookup table.

    ratio = measured latency / target latency
    foreground share = clamp(0.50 + 0.20 * ratio, 0.55, 0.90)
    Integer CPUWeight values sum to total_weight; each group is kept >= 100.
    """
    if target_ms <= 0 or total_weight < 200:
        raise ValueError("target_ms must be positive and total_weight at least 200")
    ratio = max(0.0, latency_ms / target_ms)
    if not 0.0 <= min_share <= max_share <= 1.0 or round_to < 1:
        raise ValueError("invalid policy share bounds or rounding step")
    share = max(min_share, min(max_share, base_share + coefficient * ratio))
    fg = int(round((share * total_weight) / round_to) * round_to)
    fg = max(round_to, min(total_weight - round_to, fg))
    return fg, total_weight - fg, ratio, share


def run(args):
    root = Path(args.log_dir); root.mkdir(parents=True, exist_ok=True)
    policy = {}
    policy_path = Path(args.policy_config) if args.policy_config else root / "selected_policy.json"
    if args.mode == "adaptive" and policy_path.is_file():
        try:
            loaded = json.loads(policy_path.read_text(encoding="utf-8"))
            if loaded.get("status") == "accepted":
                policy = loaded.get("formula", {})
        except (OSError, json.JSONDecodeError):
            policy = {}
    target_ms = float(policy.get("target_latency_ms", args.latency_threshold_ms))
    total_budget = int(policy.get("total_weight_budget", 1000))
    base_share = float(policy.get("base_share", 0.50))
    coefficient = float(policy.get("latency_coefficient", 0.20))
    min_share = float(policy.get("minimum_foreground_share", 0.55))
    max_share = float(policy.get("maximum_foreground_share", 0.90))
    round_to = int(policy.get("round_to_nearest", 10))
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    run_label = args.label or args.mode
    if not run_label.replace("_", "").replace("-", "").isalnum():
        raise RuntimeError("--label may contain only letters, numbers, hyphens, and underscores")
    probe_csv = root / f"live-probes-{run_label}-{stamp}.csv"
    events_path = root / f"live-benchmark-events-{run_label}-{stamp}.jsonl"
    background_progress_csv = root / f"live-background-{run_label}-{stamp}.csv"
    def event(kind, **fields):
        with events_path.open("a", encoding="utf-8") as f:
            f.write(json.dumps({"timestamp": now(), "event": kind, **fields}) + "\n")
    if not Path("/sys/fs/cgroup/cgroup.controllers").exists():
        raise RuntimeError("cgroup v2 is unavailable; live CPUWeight benchmark cannot run here.")
    try: controllers = Path("/sys/fs/cgroup/cgroup.controllers").read_text().split()
    except OSError as e: raise RuntimeError(f"Cannot read cgroup v2 controllers: {e}")
    if "cpu" not in controllers: raise RuntimeError("The cgroup v2 CPU controller is unavailable.")
    fg_unit, bg_unit = f"aegisos-probe-{os.getpid()}.scope", f"aegisos-load-{os.getpid()}.scope"
    if args.mode == "baseline":
        initial_fg, initial_bg = 600, 100  # legacy original-project reference only
    elif args.mode == "fixed":
        initial_fg, initial_bg = args.fixed_foreground_weight, args.fixed_background_weight
        if initial_fg < 1 or initial_bg < 1:
            raise RuntimeError("fixed weights must both be positive")
    else:
        # Start from the measured fixed baseline selected by the comparison runner.
        initial_fg = args.initial_foreground_weight
        initial_bg = args.initial_background_weight
        if initial_fg < 1 or initial_bg < 1:
            raise RuntimeError("initial adaptive weights must both be positive")
        if initial_fg + initial_bg != total_budget:
            raise RuntimeError("initial adaptive weights must sum to the total weight budget")
    py = sys.executable; script = str(Path(__file__).resolve()); children = []
    fg_cmd = ["systemd-run", "--user", "--scope", "--unit", fg_unit, f"--property=CPUWeight={initial_fg}", py, script,
              "--probe-worker", str(probe_csv), str(args.probe_interval), str(args.work)]
    bg_cmd = ["systemd-run", "--user", "--scope", "--unit", bg_unit, f"--property=CPUWeight={initial_bg}", py, script, "--background-worker", "--background-progress-csv", str(background_progress_csv)]
    try:
        fg = subprocess.Popen(fg_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True); children.append(fg)
        bg = subprocess.Popen(bg_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True); children.append(bg)
        event("benchmark_started", foreground_unit=fg_unit, background_unit=bg_unit, initial_weights=[initial_fg,initial_bg], weight_formula="fg_share=clamp(base_share+latency_coefficient*(latency/target),min_share,max_share); fg_weight=round_to_nearest(total_weight*share); bg_weight=total_weight-fg_weight",
              selected_policy_path=str(policy_path), selected_policy_status=("accepted" if policy else "default_parameters"),
              probe_interval_seconds=args.probe_interval, probe_work_iterations=args.work,
              measurement="wall-clock duration of bounded foreground CPU probe; includes scheduling delay")
        print(f"Live benchmark running. Probe CSV: {probe_csv}\nStop with Ctrl+C. Only AegisOS benchmark processes are managed.", flush=True)
        signal.signal(signal.SIGINT, stop_handler); signal.signal(signal.SIGTERM, stop_handler)
        last_line = 0; fg_weight, bg_weight = initial_fg, initial_bg; high_count = low_count = 0
        smoothed_latency = None
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
            if args.mode != "adaptive":
                continue
            # EWMA dampens one-off probe spikes. Process each newly collected sample
            # so the consecutive-sample guard reflects actual measurements.
            alpha = args.ewma_alpha
            for sample in recent:
                smoothed_latency = sample if smoothed_latency is None else (
                    alpha * sample + (1.0 - alpha) * smoothed_latency
                )
                if smoothed_latency > target_ms:
                    high_count += 1
                    low_count = 0
                else:
                    low_count += 1
                    high_count = 0
            if max(high_count, low_count) < args.consecutive_samples:
                continue
            if max(high_count, low_count) < args.consecutive_samples:
                continue
            desired_fg, desired_bg, ratio, share = calculate_weights(
                smoothed_latency, target_ms, total_budget, base_share, coefficient,
                min_share, max_share, round_to
            )
            # Limit each update to avoid large oscillations. The formula still
            # determines the target; the slew limit determines how quickly we move.
            max_step = args.max_weight_step
            desired_fg = max(fg_weight - max_step, min(fg_weight + max_step, desired_fg))
            desired_fg = max(round_to, min(total_budget - round_to,
                                           int(round(desired_fg / round_to) * round_to)))
            desired = (desired_fg, total_budget - desired_fg)
            if desired != (fg_weight,bg_weight):
                old = [fg_weight,bg_weight]
                try:
                    subprocess.run(["systemctl","--user","set-property",fg_unit,f"CPUWeight={desired[0]}"],check=True,capture_output=True,text=True)
                    subprocess.run(["systemctl","--user","set-property",bg_unit,f"CPUWeight={desired[1]}"],check=True,capture_output=True,text=True)
                except subprocess.CalledProcessError as e:
                    event("cpu_weight_update_failed", error=(e.stderr or str(e)).strip(), attempted_weights=desired,
                          latency_ratio=round(ratio,4), target_latency_ms=args.latency_threshold_ms)
                else:
                    fg_weight,bg_weight = desired
                    event("cpu_weight_changed", old_weights=old, new_weights=desired, latest_probe_latency_ms=round(latest,4),
                          recent_mean_probe_latency_ms=round(sum(recent)/len(recent),4),
                          ewma_probe_latency_ms=round(smoothed_latency,4), target_latency_ms=target_ms,
                          latency_ratio=round(ratio,4), calculated_foreground_share=round(share,4), total_weight_budget=1000,
                          formula=f"fg_share=clamp({base_share}+{coefficient}*(latency/target),{min_share},{max_share}); fg_weight=round_to_{round_to}(total_weight*share); bg_weight=total_weight-fg_weight",
                          reason="weights recalculated from measured foreground probe latency")
                    print(f"{now()} probe={latest:.2f} ms (ratio={ratio:.2f}), CPUWeight {old} -> {desired}", flush=True)
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
    ap.add_argument("--latency-threshold-ms",type=float,default=8.0,help="target foreground probe latency in ms (unless accepted policy overrides it)")
    ap.add_argument("--policy-config",default=None,help="accepted selected_policy.json to use for adaptive formula parameters")
    ap.add_argument("--mode", choices=("adaptive", "baseline", "fixed"), default="adaptive", help="adaptive calculates weights from measured latency; baseline is the legacy 600/100 reference; fixed applies explicit fixed weights")
    ap.add_argument("--fixed-foreground-weight", type=int, default=500, help="foreground weight for --mode fixed")
    ap.add_argument("--fixed-background-weight", type=int, default=500, help="background weight for --mode fixed")
    ap.add_argument("--initial-foreground-weight", type=int, default=600, help="initial foreground weight for adaptive mode")
    ap.add_argument("--initial-background-weight", type=int, default=400, help="initial background weight for adaptive mode")
    ap.add_argument("--ewma-alpha", type=float, default=0.25, help="EWMA responsiveness, from 0 to 1")
    ap.add_argument("--max-weight-step", type=int, default=30, help="maximum foreground-weight change per update")
    ap.add_argument("--label", default=None, help="optional filename label, used to distinguish candidate-policy runs")
    ap.add_argument("--consecutive-samples",type=int,default=3); ap.add_argument("--duration",type=float,default=0,help="seconds; 0 runs until Ctrl+C")
    ap.add_argument("--probe-worker",nargs=3,metavar=("CSV","INTERVAL","WORK")); ap.add_argument("--background-worker",action="store_true"); ap.add_argument("--background-progress-csv")
    a=ap.parse_args()
    if a.probe_worker: probe_worker(a.probe_worker[0],float(a.probe_worker[1]),int(a.probe_worker[2])); return 0
    if a.background_worker: background_worker(a.background_progress_csv); return 0
    if a.interval<.2 or a.probe_interval<.05 or a.work<1000 or a.consecutive_samples<1: ap.error("intervals/work/sample counts are too small")
    if not 0.0 < a.ewma_alpha <= 1.0: ap.error("--ewma-alpha must be in (0, 1]")
    if a.max_weight_step < 10 or a.max_weight_step % 10: ap.error("--max-weight-step must be a positive multiple of 10")
    try: run(a)
    except (RuntimeError,OSError) as e: print(f"AegisOS live benchmark error: {e}",file=sys.stderr); return 2
    return 0
if __name__=="__main__": raise SystemExit(main())
