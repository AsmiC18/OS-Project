#!/usr/bin/env python3
"""Small, bounded CPU workload for safely demonstrating AegisOS live control."""
import argparse
import time


def burn(until):
    x = 1
    while time.monotonic() < until:
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
    return x


p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--seconds", type=float, default=30, help="maximum run time in seconds")
p.add_argument("--workers", type=int, default=1, help="number of CPU-consuming worker processes")
a = p.parse_args()
if a.seconds <= 0 or not 1 <= a.workers <= 32:
    p.error("--seconds must be positive and --workers must be between 1 and 32")
end = time.monotonic() + a.seconds
if a.workers > 1:
    import multiprocessing as mp
    with mp.Pool(a.workers) as pool:
        pool.map(burn, [end] * a.workers)
else:
    burn(end)
print("CPU workload finished")
