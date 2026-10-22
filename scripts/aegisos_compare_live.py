#!/usr/bin/env python3
"""Compare fixed-weight and adaptive AegisOS using repeated live CPU-contention tests."""
import argparse, csv, json, math, subprocess, sys
from pathlib import Path
from datetime import datetime


def percentile(values, p):
    values = sorted(values)
    if not values: return 0.0
    k = (len(values)-1)*p
    lo = math.floor(k); hi = math.ceil(k)
    return values[lo] if lo == hi else values[lo]*(hi-k)+values[hi]*(k-lo)


def summarize_csv(path):
    values=[]
    with path.open(newline='', encoding='utf-8') as f:
        for row in csv.DictReader(f):
            try: values.append(float(row['probe_latency_ms']))
            except (ValueError, KeyError): pass
    if not values: raise RuntimeError(f"No probe measurements found in {path}")
    return {'samples':len(values),'mean_ms':sum(values)/len(values),'p50_ms':percentile(values,.50),'p95_ms':percentile(values,.95),'max_ms':max(values)}


def background_units(path):
    if not path.is_file(): return 0
    vals=[]
    with path.open(newline='',encoding='utf-8') as f:
        for row in csv.DictReader(f):
            try: vals.append(int(row['completed_chunks']))
            except (ValueError,KeyError): pass
    return max(vals) if vals else 0


def aggregate(runs, mode):
    all_values=[]
    for path in runs:
        with path.open(newline='',encoding='utf-8') as f:
            for row in csv.DictReader(f):
                try: all_values.append(float(row['probe_latency_ms']))
                except (ValueError,KeyError): pass
    if not all_values: raise RuntimeError(f'No {mode} probe measurements were collected')
    return {'mode':mode,'samples':len(all_values),'mean_ms':sum(all_values)/len(all_values),
            'p50_ms':percentile(all_values,.50),'p95_ms':percentile(all_values,.95),'max_ms':max(all_values)}


def mean_ci(values):
    n=len(values); mean=sum(values)/n
    if n < 2: return {'mean':mean,'low':None,'high':None}
    sd=math.sqrt(sum((x-mean)**2 for x in values)/(n-1))
    tcrit={2:12.706,3:4.303,4:3.182,5:2.776,6:2.571,7:2.447,8:2.365,9:2.306,10:2.262}.get(n,1.96)
    half=tcrit*sd/math.sqrt(n)
    return {'mean':mean,'low':mean-half,'high':mean+half}


def difference_ci(baseline, adaptive):
    # Approximate Welch 95% interval for independent run-level mean latency samples.
    n1,n2=len(baseline),len(adaptive); m1=sum(baseline)/n1; m2=sum(adaptive)/n2
    v1=sum((x-m1)**2 for x in baseline)/(n1-1) if n1>1 else 0.0
    v2=sum((x-m2)**2 for x in adaptive)/(n2-1) if n2>1 else 0.0
    se=math.sqrt(v1/n1+v2/n2)
    denom=(v1/n1)**2/(n1-1)+(v2/n2)**2/(n2-1) if n1>1 and n2>1 else 0.0
    df=((v1/n1+v2/n2)**2/denom) if denom else n1+n2-2
    table=[(2,4.303),(3,3.182),(4,2.776),(5,2.571),(6,2.447),(7,2.365),(8,2.306),(9,2.262),(10,2.228),(11,2.201),(12,2.179),(15,2.131),(20,2.086),(30,2.042)]
    tcrit=next((t for d,t in table if df<=d),1.96)
    diff=m1-m2
    return {'mean_reduction_ms':diff,'low':diff-tcrit*se,'high':diff+tcrit*se,'confidence_level':0.95,'method':'Welch t interval across independent run-level means'}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--duration',type=int,default=30,help='seconds per test repetition')
    ap.add_argument('--repeats',type=int,default=3,help='repetitions per mode (default: 3)')
    ap.add_argument('--log-dir',default='live_data/comparison')
    ap.add_argument('--interval',type=float,default=1.0)
    ap.add_argument('--probe-interval',type=float,default=.2)
    ap.add_argument('--work',type=int,default=250000)
    ap.add_argument('--latency-threshold-ms',type=float,default=8.0)
    a=ap.parse_args()
    if a.duration < 10: ap.error('--duration must be at least 10 seconds')
    if not 2 <= a.repeats <= 10: ap.error('--repeats must be between 2 and 10')
    root=Path(a.log_dir); root.mkdir(parents=True,exist_ok=True)
    status_path=root/'comparison_status.json'; results_path=root/'comparison_results.json'; report=root/'LIVE_COMPARISON_REPORT.md'
    def status(state,message,**extra):
        status_path.write_text(json.dumps({'state':state,'message':message,'updated_at':datetime.now().astimezone().isoformat(timespec='seconds'),**extra},indent=2),encoding='utf-8')
    # Keep every comparison scoped to its own new measurements.
    for pattern in ('live-probes-baseline-*.csv','live-probes-adaptive-*.csv','live-background-baseline-*.csv','live-background-adaptive-*.csv','live-benchmark-events-baseline-*.jsonl','live-benchmark-events-adaptive-*.jsonl'):
        for old in root.glob(pattern): old.unlink(missing_ok=True)
    results_path.unlink(missing_ok=True); report.unlink(missing_ok=True)
    benchmark=Path(__file__).with_name('aegisos_live_benchmark.py')
    run_stats={'baseline':[],'adaptive':[]}; probe_files={'baseline':[],'adaptive':[]}; background_files={'baseline':[],'adaptive':[]}
    try:
        total=a.repeats*2
        index=0
        for mode in ('baseline','adaptive'):
            for repetition in range(1,a.repeats+1):
                index+=1
                status('running',f'Running {mode} test {repetition} of {a.repeats}',current_mode=mode,repetition=repetition,repeats=a.repeats,completed_tests=index,total_tests=total,duration_seconds=a.duration)
                cmd=[sys.executable,str(benchmark),'--mode',mode,'--duration',str(a.duration),'--log-dir',str(root),
                     '--interval',str(a.interval),'--probe-interval',str(a.probe_interval),'--work',str(a.work),
                     '--latency-threshold-ms',str(a.latency_threshold_ms)]
                subprocess.run(cmd,check=True)
                probes=sorted(root.glob(f'live-probes-{mode}-*.csv'),key=lambda p:p.stat().st_mtime)
                backgrounds=sorted(root.glob(f'live-background-{mode}-*.csv'),key=lambda p:p.stat().st_mtime)
                if len(probes)<repetition: raise RuntimeError(f'Missing {mode} probe log for repetition {repetition}')
                probe_path=probes[-1]; probe_files[mode].append(probe_path)
                background_path=backgrounds[-1] if backgrounds else None
                if background_path: background_files[mode].append(background_path)
                stats=summarize_csv(probe_path); stats['repetition']=repetition
                stats['background_chunks_completed']=background_units(background_path) if background_path else 0
                run_stats[mode].append(stats)
        base=aggregate(probe_files['baseline'],'baseline'); adap=aggregate(probe_files['adaptive'],'adaptive')
        changes={k:(base[k]-adap[k])/base[k]*100 if base[k] else 0.0 for k in ('mean_ms','p50_ms','p95_ms','max_ms')}
        base_run_means=[r['mean_ms'] for r in run_stats['baseline']]; adap_run_means=[r['mean_ms'] for r in run_stats['adaptive']]
        base_ci=mean_ci(base_run_means)
        adap_ci=mean_ci(adap_run_means)
        diff_ci=difference_ci(base_run_means,adap_run_means)
        bg_base=sum(r['background_chunks_completed'] for r in run_stats['baseline'])/a.repeats
        bg_adap=sum(r['background_chunks_completed'] for r in run_stats['adaptive'])/a.repeats
        bg_change=(bg_adap-bg_base)/bg_base*100 if bg_base else 0.0
        result={'generated_at':datetime.now().astimezone().isoformat(timespec='seconds'),'duration_seconds_per_run':a.duration,'repeats_per_mode':a.repeats,
                'baseline':base,'adaptive':adap,'improvement_percent':changes,'baseline_run_means_ms':base_run_means,'adaptive_run_means_ms':adap_run_means,
                'baseline_mean_95ci_ms':base_ci,'adaptive_mean_95ci_ms':adap_ci,'mean_latency_reduction_95ci_ms':diff_ci,
                'background_chunks_completed_per_run':{'baseline_mean':bg_base,'adaptive_mean':bg_adap,'change_percent':bg_change},
                'runs':run_stats,
                'measurement':'Wall-clock duration of a bounded foreground CPU probe under background CPU contention. Background progress is counted as completed fixed-size CPU-work chunks.',
                'interpretation':'Positive latency reduction means lower probe latency with adaptive mode. The 95% interval is based on run-level mean latency differences. Results apply to this controlled workload and do not directly measure desktop-application response time.'}
        results_path.write_text(json.dumps(result,indent=2),encoding='utf-8')
        lines=['# AegisOS Live Performance Comparison','',f"Generated: {result['generated_at']}",f"Runs per mode: {a.repeats} · {a.duration} seconds per run",'',
               '| Metric | Fixed-weight baseline | Adaptive AegisOS | Reduction / change |','|---|---:|---:|---:|']
        for label,key in [('Probe samples','samples'),('Mean latency (ms)','mean_ms'),('Median latency / p50 (ms)','p50_ms'),('P95 latency (ms)','p95_ms'),('Maximum latency (ms)','max_ms')]:
            fmt=lambda x: str(x[key]) if key=='samples' else f"{x[key]:.3f}"
            lines.append(f"| {label} | {fmt(base)} | {fmt(adap)} | {changes.get(key,0):.2f}% |" if key!='samples' else f"| {label} | {fmt(base)} | {fmt(adap)} | — |")
        lines += ['',f"Mean latency reduction 95% interval: {diff_ci['low']:.3f} to {diff_ci['high']:.3f} ms (positive means baseline latency exceeded adaptive latency).",
                  f"Background CPU-work chunks completed per run: baseline {bg_base:.1f}, adaptive {bg_adap:.1f} ({bg_change:+.2f}% change).",'',
                  'Measurements are from a controlled foreground CPU-probe workload under background CPU contention. They do not directly measure desktop-application response time.', '']
        report.write_text('\n'.join(lines),encoding='utf-8')
        status('complete','Comparison complete',current_mode=None,repeats=a.repeats,duration_seconds=a.duration,total_tests=total,results_file=results_path.name,report_file=report.name)
        print(f'Comparison complete. Report: {report}',flush=True)
    except Exception as exc:
        status('error',str(exc),current_mode=None,repeats=a.repeats,duration_seconds=a.duration)
        raise

if __name__=='__main__': main()
