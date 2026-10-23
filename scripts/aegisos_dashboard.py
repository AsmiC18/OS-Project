#!/usr/bin/env python3
"""Local browser dashboard for AegisOS live process logs (Python standard library only)."""
from __future__ import annotations
import argparse
import os
import csv
import json
import html
import subprocess
import sys
import signal
import threading
from collections import defaultdict
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

PAGE = r'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AegisOS Live Dashboard</title><style>
:root{color-scheme:dark;--bg:#0b1020;--panel:#121a2d;--panel2:#17223a;--line:#273550;--text:#e8eefb;--muted:#9aabc7;--accent:#70d6c4;--blue:#82aaff;--warn:#ffd166}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}header{padding:22px clamp(16px,4vw,48px);border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;gap:18px;flex-wrap:wrap}h1{font-size:22px;margin:0;letter-spacing:.2px}h1 span{color:var(--accent)}.sub{color:var(--muted);margin-top:4px}.status{display:flex;align-items:center;gap:9px;color:var(--muted)}.nav{display:flex;gap:8px;flex-wrap:wrap}.nav a{color:var(--text);text-decoration:none;border:1px solid var(--line);background:var(--panel);padding:8px 12px;border-radius:8px}.nav a:hover{border-color:var(--accent);color:var(--accent)}.dot{width:9px;height:9px;background:var(--accent);border-radius:50%;box-shadow:0 0 12px #70d6c455}.wrap{max-width:1500px;margin:auto;padding:22px clamp(16px,4vw,48px) 40px}.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:14px}.card{background:var(--panel);border:1px solid var(--line);border-radius:13px;padding:17px;min-width:0}.label{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.08em}.value{font-size:28px;font-weight:650;margin-top:7px;white-space:nowrap}.hint{color:var(--muted);font-size:12px;margin-top:3px}.columns{display:grid;grid-template-columns:minmax(0,1.45fr) minmax(300px,1fr);gap:14px;margin-top:14px}.title-row{display:flex;justify-content:space-between;gap:10px;align-items:center;margin-bottom:12px}h2{font-size:15px;margin:0}.pill{font-size:11px;color:var(--accent);background:#70d6c415;border:1px solid #70d6c43b;border-radius:99px;padding:3px 8px}canvas{width:100%;height:220px;display:block}.table-wrap{overflow:auto;max-height:510px}table{border-collapse:collapse;width:100%;font-size:12px;white-space:nowrap}th{text-align:left;color:var(--muted);font-weight:550;position:sticky;top:0;background:var(--panel);z-index:1}th,td{padding:9px 8px;border-bottom:1px solid var(--line)}td.cmd{max-width:380px;overflow:hidden;text-overflow:ellipsis}.cpu{color:var(--accent);font-variant-numeric:tabular-nums}.events{display:flex;flex-direction:column;gap:0;max-height:260px;overflow:auto}.event{padding:10px 0;border-bottom:1px solid var(--line)}.event:last-child{border:0}.event strong{font-weight:600}.event time{color:var(--muted);display:block;font-size:11px}.muted{color:var(--muted)}input{background:var(--bg);color:var(--text);border:1px solid var(--line);border-radius:8px;padding:8px 10px;min-width:180px;outline:none}footer{color:var(--muted);font-size:12px;margin-top:16px}.empty{color:var(--muted);padding:18px 0}@media(max-width:900px){.grid{grid-template-columns:repeat(2,minmax(0,1fr))}.columns{grid-template-columns:1fr}}@media(max-width:480px){.grid{grid-template-columns:1fr 1fr}.value{font-size:23px}.card{padding:13px}}
</style></head><body>
<header><div><h1><span>AEGIS</span>OS <span style="color:var(--text)">/ Live Dashboard</span></h1><div class="sub">Real process samples from your Ubuntu / WSL environment</div></div><nav class="nav"><a href="/">Live Dashboard</a><a href="/report">Live Report</a><a href="/download/report">Download Report</a></nav><div class="status"><i class="dot"></i><span id="status">Connecting to local monitor logs…</span></div></header>
<main class="wrap"><section class="grid">
<div class="card"><div class="label">Visible processes</div><div class="value" id="procCount">—</div><div class="hint">Unique PIDs in latest samples</div></div>
<div class="card"><div class="label">Sampled CPU total</div><div class="value" id="cpuTotal">—</div><div class="hint">Sum across processes; may exceed 100%</div></div>
<div class="card"><div class="label">Memory in latest samples</div><div class="value" id="memTotal">—</div><div class="hint">RSS summed across visible processes</div></div>
<div class="card"><div class="label">Recorded events</div><div class="value" id="eventCount">—</div><div class="hint" id="lastSample">Waiting for data</div></div>
<div class="card"><div class="label">Foreground probe latency</div><div class="value" id="probeLatency">—</div><div class="hint" id="probeSample">Start Live Benchmark to measure</div></div>
</section><section class="columns"><div class="card"><div class="title-row"><h2>Recent sampled CPU activity</h2><span class="pill">LIVE · refresh 2s</span></div><canvas id="chart" width="900" height="260" aria-label="Recent CPU activity chart"></canvas><div class="hint">Aggregate sampled process CPU over time. This is not a system-wide kernel CPU counter.</div></div>
<div class="card"><div class="title-row"><h2>Recent events</h2><span class="muted" id="eventFile">JSONL logs</span></div><div class="events" id="events"><div class="empty">Waiting for events…</div></div></div></section>
<section class="card" style="margin-top:14px"><div class="title-row"><h2>Processes (highest sampled CPU first)</h2><input id="filter" placeholder="Filter PID or command…" aria-label="Filter processes"></div><div class="table-wrap"><table><thead><tr><th>PID</th><th>PPID</th><th>CPU %</th><th>RSS MiB</th><th>State</th><th>Command</th></tr></thead><tbody id="processes"><tr><td colspan="6" class="muted">Waiting for samples…</td></tr></tbody></table></div></section>
<footer id="footer">This dashboard is read-only. Monitoring mode records process data but does not change CPU scheduling. Live CPU control is a separate opt-in mode and must be verified independently.</footer></main>
<script>
const $=id=>document.getElementById(id);let cached=[];const fmt=n=>Number(n||0).toLocaleString(undefined,{maximumFractionDigits:1});
function draw(points){const c=$('chart'),ctx=c.getContext('2d'),w=c.width,h=c.height;ctx.clearRect(0,0,w,h);const pad={l:48,r:12,t:14,b:28},pw=w-pad.l-pad.r,ph=h-pad.t-pad.b;ctx.strokeStyle='#273550';ctx.fillStyle='#9aabc7';ctx.lineWidth=1;ctx.font='12px system-ui';let max=Math.max(100,...points.map(p=>p.cpu));max=Math.ceil(max/50)*50;for(let i=0;i<=4;i++){let y=pad.t+ph*i/4;ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(w-pad.r,y);ctx.stroke();ctx.fillText(Math.round(max*(1-i/4))+'%',5,y+4)}if(points.length<2){ctx.fillText('Waiting for more samples…',pad.l+10,h/2);return}ctx.beginPath();points.forEach((p,i)=>{let x=pad.l+pw*i/(points.length-1),y=pad.t+ph*(1-Math.min(max,p.cpu)/max);i?ctx.lineTo(x,y):ctx.moveTo(x,y)});ctx.strokeStyle='#70d6c4';ctx.lineWidth=3;ctx.stroke();ctx.lineTo(w-pad.r,pad.t+ph);ctx.lineTo(pad.l,pad.t+ph);ctx.closePath();ctx.fillStyle='#70d6c41a';ctx.fill();ctx.fillStyle='#9aabc7';ctx.fillText(points[0].time||'',pad.l,h-7);let last=points[points.length-1];ctx.fillText(last.time||'',w-pad.r-100,h-7)}
function renderProcesses(){let q=$('filter').value.toLowerCase();let rows=cached.filter(p=>(String(p.pid)+' '+p.comm+' '+p.cmdline).toLowerCase().includes(q));$('processes').innerHTML=rows.slice(0,250).map(p=>`<tr><td>${p.pid}</td><td>${p.ppid}</td><td class="cpu">${fmt(p.cpu_percent)}%</td><td>${fmt(p.rss_mb)}</td><td>${esc(p.state||'')}</td><td class="cmd" title="${esc(p.cmdline)}">${esc(p.cmdline)}</td></tr>`).join('')||'<tr><td colspan="6" class="empty">No matching processes</td></tr>'}
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function renderEvents(events){$('events').innerHTML=events.length?events.slice().reverse().map(e=>`<div class="event"><strong>${esc((e.event||'event').replaceAll('_',' '))}</strong><div>${esc(e.comm||e.reason||e.explanation||e.command||('PID '+(e.pid??'')))}</div><time>${esc(e.timestamp||'')}</time></div>`).join(''):'<div class="empty">No events recorded yet.</div>'}
async function refresh(){try{const r=await fetch('/api/data',{cache:'no-store'});if(!r.ok)throw new Error('HTTP '+r.status);const d=await r.json();cached=d.processes||[];$('procCount').textContent=fmt(d.process_count);$('cpuTotal').textContent=fmt(d.cpu_total)+'%';$('memTotal').textContent=fmt(d.rss_total_mb)+' MiB';$('eventCount').textContent=fmt(d.event_count);$('probeLatency').textContent=d.probe_latency_ms==null?'—':fmt(d.probe_latency_ms)+' ms';$('probeSample').textContent=d.probe_timestamp?'Measured '+d.probe_timestamp:'Start Live Benchmark to measure';$('lastSample').textContent='Last sample: '+(d.last_sample||'not available');$('status').textContent=d.process_file?'Receiving saved log data':'Waiting for monitor to write logs';$('eventFile').textContent=(d.event_files||0)+' event file(s)';$('footer').textContent=d.note||'Read-only dashboard.';draw(d.history||[]);renderProcesses();renderEvents(d.events||[])}catch(e){$('status').textContent='Cannot read logs: '+e.message}}
$('filter').addEventListener('input',renderProcesses);refresh();setInterval(refresh,2000);
</script></body></html>'''


def load_data(root: Path) -> dict:
    process_files = sorted(root.glob('processes-*.csv'), key=lambda p: p.stat().st_mtime)
    event_files = sorted([*root.glob('events-*.jsonl'), *root.glob('live-benchmark-events-*.jsonl')], key=lambda p: p.stat().st_mtime)
    history_by_time = defaultdict(float)
    last_sample = None
    row_count = 0
    latest_session_rows = []
    latest_session_timestamp = ''

    for file_index, path in enumerate(process_files):
        file_latest_timestamp = ''
        file_latest_rows = []
        try:
            with path.open(newline='', encoding='utf-8', errors='replace') as f:
                for row in csv.DictReader(f):
                    row_count += 1
                    ts = row.get('timestamp', '')
                    try:
                        cpu = float(row.get('cpu_percent') or 0)
                    except (ValueError, TypeError):
                        cpu = 0.0
                    try:
                        rss = float(row.get('rss_kb') or 0) / 1024
                    except (ValueError, TypeError):
                        rss = 0.0
                    if ts:
                        history_by_time[ts] += cpu
                        if last_sample is None or ts > last_sample:
                            last_sample = ts
                    row['_cpu_numeric'] = cpu
                    row['_rss_mb_numeric'] = rss
                    # A process table represents the latest snapshot in the newest
                    # session, not every PID ever seen (PIDs can exit or be reused).
                    if ts and ts > file_latest_timestamp:
                        file_latest_timestamp = ts
                        file_latest_rows = [row]
                    elif ts and ts == file_latest_timestamp:
                        file_latest_rows.append(row)
            if file_index == len(process_files) - 1:
                latest_session_timestamp = file_latest_timestamp
                latest_session_rows = file_latest_rows
        except OSError:
            continue

    processes = []
    for row in latest_session_rows:
        clean = {key: value for key, value in row.items() if not key.startswith('_')}
        clean['cpu_percent'] = row['_cpu_numeric']
        clean['rss_mb'] = row['_rss_mb_numeric']
        processes.append(clean)
    processes.sort(key=lambda p: p['cpu_percent'], reverse=True)

    events = []
    event_count = 0
    for path in event_files:
        try:
            with path.open(encoding='utf-8', errors='replace') as f:
                for line in f:
                    try:
                        event = json.loads(line)
                    except json.JSONDecodeError:
                        continue
                    event_count += 1
                    events.append(event)
        except OSError:
            continue
    events.sort(key=lambda e: e.get('timestamp', ''))
    probe_latency_ms = None
    probe_timestamp = None
    probe_history = []
    probe_files = sorted(root.glob('live-probes-*.csv'), key=lambda p: p.stat().st_mtime)
    if probe_files:
        try:
            with probe_files[-1].open(newline='', encoding='utf-8', errors='replace') as f:
                probe_rows = list(csv.DictReader(f))
            for row in probe_rows[-120:]:
                try:
                    latency = float(row.get('probe_latency_ms') or 0)
                except (ValueError, TypeError):
                    continue
                ts = row.get('timestamp', '')
                try:
                    label = datetime.fromisoformat(ts).strftime('%H:%M:%S') if ts else ''
                except ValueError:
                    label = ts
                probe_history.append({'timestamp': ts, 'time': label, 'latency_ms': round(latency, 4)})
            if probe_rows:
                probe_latency_ms = round(float(probe_rows[-1]['probe_latency_ms']), 4)
                probe_timestamp = probe_rows[-1].get('timestamp')
        except (OSError, ValueError, KeyError, csv.Error):
            pass
    history = [{'time': datetime.fromisoformat(ts).strftime('%H:%M:%S') if ts else '',
                'timestamp': ts, 'cpu': round(cpu, 2)}
               for ts, cpu in sorted(history_by_time.items())[-60:]]
    return {
        'process_file': process_files[-1].name if process_files else None,
        'process_count': len(processes),
        'cpu_total': round(sum(p['cpu_percent'] for p in processes), 2),
        'rss_total_mb': round(sum(p['rss_mb'] for p in processes), 2),
        'last_sample': last_sample, 'latest_snapshot': latest_session_timestamp,
        'row_count': row_count, 'event_count': event_count,
        'event_files': len(event_files), 'processes': processes[:500],
        'probe_latency_ms': probe_latency_ms, 'probe_timestamp': probe_timestamp, 'probe_history': probe_history,
        'history': history, 'events': events[-30:],
        'note': 'Read-only dashboard. Process table shows the latest snapshot from the newest session. Monitor mode does not change scheduling; run mode controls only its two explicitly launched commands.'
    }




def load_comparison_data(log_dir: Path) -> dict:
    """Load the latest live comparison status and measured results."""
    root = log_dir / "comparison"
    status_path = root / "comparison_status.json"
    results_path = root / "comparison_results.json"
    report_path = root / "LIVE_COMPARISON_REPORT.md"
    status = {"state": "not_run", "message": "Run a live comparison to collect measurements."}
    results = None
    try:
        if status_path.is_file(): status = json.loads(status_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError): pass
    try:
        if results_path.is_file(): results = json.loads(results_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError): pass
    report = report_path.read_text(encoding="utf-8", errors="replace") if report_path.is_file() else ""
    def read_json(path):
        try: return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else None
        except (OSError, json.JSONDecodeError): return None
    evaluation = read_json(root / "policy_evaluation.json")
    selected_fixed_baseline = read_json(root / "selected_fixed_baseline.json")
    selected_policy = read_json(root / "selected_policy.json")
    if results and evaluation and not results.get("policy_evaluation"):
        results["policy_evaluation"] = evaluation
    return {"status": status, "results": results, "report": report, "report_available": report_path.is_file(),
            "evaluation": evaluation, "selected_fixed_baseline": selected_fixed_baseline, "selected_policy": selected_policy}


def load_project_data(project_root: Path) -> dict:
    """Load bundled simulator aggregates and report for the browser UI."""
    summary_path = project_root / "results" / "summary.csv"
    report_path = project_root / "results" / "REPORT.md"
    summary = []
    if summary_path.is_file():
        try:
            with summary_path.open(newline="", encoding="utf-8", errors="replace") as f:
                for row in csv.DictReader(f):
                    parsed = dict(row)
                    for key in ("runs", "mean_foreground_p95_ms", "median_foreground_p95_ms",
                                "mean_background_jobs_on_time", "mean_background_jobs_generated",
                                "mean_background_jobs_completed", "mean_background_jobs_deadline_missed",
                                "mean_background_deadline_miss_rate", "mean_background_cpu_ms",
                                "mean_deadline_miss_rate", "mean_cpu_utilization", "mean_controller_changes"):
                        try: parsed[key] = float(parsed[key])
                        except (KeyError, TypeError, ValueError): pass
                    summary.append(parsed)
        except OSError:
            pass
    total_runs = 0
    raw_path = project_root / "results" / "raw_results.csv"
    if raw_path.is_file():
        try:
            with raw_path.open(newline="", encoding="utf-8", errors="replace") as f:
                total_runs = sum(1 for _ in csv.DictReader(f))
        except OSError:
            pass
    report_html = markdown_to_html(report_path.read_text(encoding="utf-8", errors="replace")) if report_path.is_file() else ""
    return {"summary": summary, "total_runs": total_runs, "report_html": report_html,
            "summary_available": summary_path.is_file(), "report_available": report_path.is_file()}


REPORT_STYLE = r"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AegisOS Live Report</title><style>
:root{color-scheme:dark;--bg:#0b1020;--panel:#121a2d;--line:#273550;--text:#e8eefb;--muted:#9aabc7;--accent:#70d6c4}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.7 system-ui,-apple-system,Segoe UI,sans-serif}.top{padding:20px 5vw;border-bottom:1px solid var(--line);display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap}.top a,.actions a{color:var(--accent);text-decoration:none;border:1px solid var(--line);padding:7px 11px;border-radius:8px}.wrap{max-width:1100px;margin:30px auto;padding:0 20px 60px}.report{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:clamp(18px,4vw,40px);overflow:auto}h1,h2,h3{line-height:1.3}h1{font-size:30px}h2{font-size:22px;margin-top:30px;border-bottom:1px solid var(--line);padding-bottom:8px}h3{font-size:18px}p,li{color:#d8e2f5}code{background:#0b1020;padding:2px 5px;border-radius:4px;color:var(--accent)}table{width:100%;border-collapse:collapse;display:block;overflow:auto;margin:16px 0}th,td{text-align:left;padding:9px 11px;border-bottom:1px solid var(--line);white-space:nowrap}th{color:var(--accent)}.muted{color:var(--muted)}.actions{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:16px}.error{color:#ffd166}</style></head><body><header class="top"><div><strong>AEGISOS / Live Report</strong><div class="muted">Generated from saved live monitoring data</div></div><nav class="actions"><a href="/">Live Dashboard</a><a href="/download/report">Download Markdown</a><a href="/download/summary">Download CSV Summary</a></nav></header><main class="wrap"><div id="report" class="report"><p class="muted">Generating report from the latest saved logs…</p></div><p class="muted" id="reportUpdated"></p></main><script>
async function loadReport(){try{const r=await fetch('/api/report',{cache:'no-store'});const d=await r.json();if(!r.ok||d.error)throw Error(d.error||'Could not load report');document.getElementById('report').innerHTML=d.html;document.getElementById('reportUpdated').textContent='Refreshed from saved logs at '+new Date().toLocaleTimeString()+'. Auto-refreshes every 10 seconds while this page is open.';}catch(e){document.getElementById('report').innerHTML='<p class="error">'+String(e.message).replace(/[&<>\"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',"'":'&#39;'}[c]))+'</p><p>Start monitoring first, then reload this page.</p>'}}loadReport();setInterval(loadReport,10000);
</script></body></html>"""


def ensure_report(root: Path) -> Path:
    report_path = root / 'LIVE_REPORT.md'
    if not list(root.glob('processes-*.csv')):
        raise FileNotFoundError(f'No live process logs found in {root}. Start monitoring first.')
    script = Path(__file__).with_name('summarize_live.py')
    result = subprocess.run([sys.executable, str(script), '--log-dir', str(root)], capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError((result.stderr or result.stdout or 'Report generation failed').strip())
    return report_path


def inline_markdown(text: str) -> str:
    # Escape first, then apply a small safe subset of Markdown inline syntax.
    value = html.escape(text, quote=False)
    import re
    value = re.sub(r'`([^`]+)`', r'<code>\1</code>', value)
    value = re.sub(r'\*\*(.+?)\*\*', r'<strong>\1</strong>', value)
    value = re.sub(r'\*([^*]+)\*', r'<em>\1</em>', value)
    return value


def markdown_to_html(markdown: str) -> str:
    lines = markdown.splitlines()
    out = []
    in_list = False
    in_table = False
    table_header_done = False
    paragraph = []
    def flush_paragraph():
        if paragraph:
            out.append('<p>' + inline_markdown(' '.join(paragraph)) + '</p>')
            paragraph.clear()
    def close_list():
        nonlocal in_list
        if in_list:
            out.append('</ul>')
            in_list = False
    def close_table():
        nonlocal in_table, table_header_done
        if in_table:
            out.append('</tbody></table>')
            in_table = False
            table_header_done = False
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('|') and stripped.endswith('|'):
            flush_paragraph(); close_list()
            cells = [c.strip() for c in stripped.strip('|').split('|')]
            if all(set(c) <= set('-: ') for c in cells):
                continue
            if not in_table:
                out.append('<table><thead><tr>' + ''.join('<th>'+inline_markdown(c)+'</th>' for c in cells) + '</tr></thead><tbody>')
                in_table = True; table_header_done = True
            else:
                out.append('<tr>' + ''.join('<td>'+inline_markdown(c)+'</td>' for c in cells) + '</tr>')
            continue
        close_table()
        if not stripped:
            flush_paragraph(); close_list(); continue
        if stripped.startswith('#'):
            flush_paragraph(); close_list()
            level = min(len(stripped) - len(stripped.lstrip('#')), 3)
            title = stripped[level:].strip()
            out.append(f'<h{level}>{inline_markdown(title)}</h{level}>')
        elif stripped.startswith('- ') or stripped.startswith('* '):
            flush_paragraph()
            if not in_list:
                out.append('<ul>'); in_list = True
            out.append('<li>'+inline_markdown(stripped[2:])+'</li>')
        else:
            close_list(); paragraph.append(stripped)
    flush_paragraph(); close_list(); close_table()
    return '\n'.join(out)

# Child processes started through the localhost dashboard. Commands are fixed by the
# application; arbitrary shell commands are never accepted from browser requests.
_CONTROL_PROCESSES = {}
_CONTROL_LOCK = threading.Lock()

def _control_process(action: str, project_root: Path, log_dir: Path, options: dict | None = None) -> dict:
    options = options or {}
    monitor_script = project_root / "scripts" / "aegisos_live.py"
    burn_script = project_root / "scripts" / "cpu_burn.py"
    with _CONTROL_LOCK:
        if action in ("start_monitor", "start_controller", "start_comparison"):
            key = {"start_monitor":"monitor", "start_controller":"controller", "start_comparison":"comparison"}[action]
            existing = _CONTROL_PROCESSES.get(key)
            if existing is not None and existing.poll() is None:
                return {"message": f"{key.capitalize()} is already running", "running": True}
            if key == "monitor":
                command = [sys.executable, str(monitor_script), "monitor", "--interval", "1", "--log-dir", str(log_dir), "--no-ui"]
            elif key == "comparison":
                compare_script = project_root / "scripts" / "aegisos_compare_live.py"
                duration = int(options.get("duration", 30)); repeats = int(options.get("repeats", 3))
                if not 10 <= duration <= 300 or not 2 <= repeats <= 10:
                    raise ValueError("Evaluation duration must be 10–300 seconds and repetitions 2–10")
                command = [sys.executable, str(compare_script), "--duration", str(duration), "--repeats", str(repeats), "--log-dir", str(log_dir / "comparison")]
            else:
                foreground = f"{sys.executable} {burn_script} --seconds 120"
                background = f"{sys.executable} {burn_script} --seconds 120"
                command = [sys.executable, str(monitor_script), "run", "--foreground", foreground, "--background", background, "--interval", "1", "--log-dir", str(log_dir), "--no-ui"]
            if key == "comparison":
                output_path = log_dir / "comparison" / "dashboard_runner.log"
                output_path.parent.mkdir(parents=True, exist_ok=True)
                with output_path.open("a", encoding="utf-8") as output:
                    proc = subprocess.Popen(command, cwd=str(project_root), stdin=subprocess.DEVNULL,
                                            stdout=output, stderr=subprocess.STDOUT, start_new_session=True)
            else:
                proc = subprocess.Popen(command, cwd=str(project_root), stdin=subprocess.DEVNULL,
                                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
            _CONTROL_PROCESSES[key] = proc
            return {"message": f"Started {key}; refresh the live panels to see new data", "running": True, "pid": proc.pid}
        if action in ("stop_monitor", "stop_controller", "stop_comparison"):
            key = {"stop_monitor":"monitor", "stop_controller":"controller", "stop_comparison":"comparison"}[action]
            proc = _CONTROL_PROCESSES.get(key)
            if proc is None or proc.poll() is not None:
                return {"message": f"{key.capitalize()} is not running under this dashboard", "running": False}
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except (ProcessLookupError, PermissionError):
                proc.send_signal(signal.SIGTERM)
            try:
                proc.wait(timeout=6)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=2)
            return {"message": f"Stopped {key}", "running": False}
    raise ValueError("Unknown action")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log-dir', default='live_data', help='directory containing AegisOS process and event logs')
    ap.add_argument('--host', default='127.0.0.1', help='bind address (default: localhost only)')
    ap.add_argument('--port', type=int, default=8765)
    args = ap.parse_args()
    root = Path(args.log_dir).resolve()
    project_root = Path(__file__).resolve().parent.parent
    root.mkdir(parents=True, exist_ok=True)
    class Handler(BaseHTTPRequestHandler):
        def do_POST(self):
            route = urlparse(self.path).path
            if route != '/api/control':
                self.send_error(404); return
            try:
                length = min(int(self.headers.get('Content-Length', '0')), 4096)
                payload = json.loads(self.rfile.read(length) or b'{}')
                action = payload.get('action', '')
                result = _control_process(action, project_root, root, payload)
                body = json.dumps(result).encode()
                self.send_response(200); self.send_header('Content-Type', 'application/json; charset=utf-8')
            except (ValueError, json.JSONDecodeError, OSError, RuntimeError) as exc:
                body = json.dumps({'error': str(exc)}).encode()
                self.send_response(400); self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(body)))
            self.end_headers(); self.wfile.write(body)
        def do_GET(self):
            route = urlparse(self.path).path
            if route == '/api/data':
                body = json.dumps(load_data(root), ensure_ascii=False).encode()
                self.send_response(200); self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(body)))
                self.end_headers(); self.wfile.write(body)
            elif route == '/api/project':
                body = json.dumps(load_project_data(project_root), ensure_ascii=False).encode()
                self.send_response(200); self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(body)))
                self.end_headers(); self.wfile.write(body)
            elif route == '/api/comparison':
                body = json.dumps(load_comparison_data(root), ensure_ascii=False).encode()
                self.send_response(200); self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(body)))
                self.end_headers(); self.wfile.write(body)
            elif route == '/api/report':
                try:
                    report_path = ensure_report(root)
                    payload = {'html': markdown_to_html(report_path.read_text(encoding='utf-8')), 'generated_from': str(root)}
                    body = json.dumps(payload, ensure_ascii=False).encode()
                    status = 200
                except (OSError, RuntimeError, FileNotFoundError) as exc:
                    body = json.dumps({'error': str(exc)}).encode(); status = 400
                self.send_response(status); self.send_header('Content-Type', 'application/json; charset=utf-8')
                self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(body)))
                self.end_headers(); self.wfile.write(body)
            elif route == '/report':
                body = REPORT_STYLE.encode()
                self.send_response(200); self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Cache-Control', 'no-store'); self.send_header('Content-Length', str(len(body)))
                self.end_headers(); self.wfile.write(body)
            elif route == '/simulation-report':
                report_path = project_root / 'results' / 'REPORT.md'
                if report_path.is_file():
                    body = """<!doctype html><meta charset="utf-8"><title>AegisOS Simulation Report</title><style>body{max-width:1100px;margin:35px auto;padding:0 20px;background:#0b1020;color:#e8eefb;font:15px/1.7 system-ui}a{color:#70d6c4}</style><p><a href='/'>← Back to dashboard</a></p><div id='report'></div><script>fetch('/api/project').then(r=>r.json()).then(d=>document.getElementById('report').innerHTML=d.report_html)</script>""".encode()
                    self.send_response(200); self.send_header('Content-Type', 'text/html; charset=utf-8'); self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
                else: self.send_error(404, 'Simulation report not found')
            elif route in ('/download/simulation-report', '/download/raw-results', '/download/summary-results', '/download/comparison-report', '/download/comparison-results'):
                targets = {'/download/simulation-report': project_root / 'results' / 'REPORT.md', '/download/raw-results': project_root / 'results' / 'raw_results.csv', '/download/summary-results': project_root / 'results' / 'summary.csv', '/download/comparison-report': root / 'comparison' / 'LIVE_COMPARISON_REPORT.md', '/download/comparison-results': root / 'comparison' / 'comparison_results.json'}
                target = targets[route]
                if not target.is_file(): self.send_error(404, 'Requested result file not found'); return
                body = target.read_bytes(); content_type = 'text/markdown; charset=utf-8' if target.suffix == '.md' else ('application/json; charset=utf-8' if target.suffix == '.json' else 'text/csv; charset=utf-8')
                self.send_response(200); self.send_header('Content-Type', content_type); self.send_header('Content-Disposition', f'attachment; filename="{target.name}"'); self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
            elif route in ('/download/report', '/download/summary'):
                try:
                    report_path = ensure_report(root)
                    target = report_path if route.endswith('report') else root / 'live_process_summary.csv'
                    body = target.read_bytes()
                    content_type = 'text/markdown; charset=utf-8' if target.suffix == '.md' else ('application/json; charset=utf-8' if target.suffix == '.json' else 'text/csv; charset=utf-8')
                    self.send_response(200); self.send_header('Content-Type', content_type)
                    self.send_header('Content-Disposition', f'attachment; filename="{target.name}"')
                    self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
                except (OSError, RuntimeError, FileNotFoundError) as exc:
                    self.send_error(400, str(exc))
            elif route == '/' or route == '/index.html':
                page_file = Path(__file__).with_name('aegisos_dashboard.html')
                body = (page_file.read_bytes() if page_file.is_file() else PAGE.encode())
                self.send_response(200); self.send_header('Content-Type', 'text/html; charset=utf-8')
                self.send_header('Content-Length', str(len(body))); self.end_headers(); self.wfile.write(body)
            else:
                self.send_error(404)
        def handle_error(self, request, client_address):
            # Browsers may navigate away or cancel a polling request mid-response.
            # BrokenPipeError/ConnectionResetError here are normal client disconnects.
            import sys as _sys
            exc = _sys.exc_info()[1]
            if isinstance(exc, (BrokenPipeError, ConnectionResetError, ConnectionAbortedError)):
                return
            super().handle_error(request, client_address)
        def log_message(self, fmt, *a):
            print('[dashboard] ' + (fmt % a))
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f'AegisOS dashboard: http://127.0.0.1:{args.port}/')
    print(f'Reading logs from: {root}')
    print('Keep the monitor running in another terminal. Press Ctrl+C to stop the dashboard.')
    try: server.serve_forever()
    except KeyboardInterrupt: print('\nDashboard stopped.')
    finally: server.server_close()
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
