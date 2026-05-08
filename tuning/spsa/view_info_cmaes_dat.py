#!/usr/bin/env python3
import json, sys, math
from statistics import mean, median, stdev

DROP_KEYS = (
    "#",              # metadata
    "#sigma",         # handled separately
)

def is_param(k, v):
    return (
            isinstance(v, (int, float))
            and not k.startswith("#")
            and "#sigma" not in k
    )

def percentile(xs, p):
    xs = sorted(xs)
    if not xs:
        return float("nan")
    i = (len(xs) - 1) * p
    lo, hi = math.floor(i), math.ceil(i)
    if lo == hi:
        return xs[lo]
    return xs[lo] * (hi - i) + xs[hi] * (i - lo)

def fmt(x):
    if isinstance(x, float):
        return f"{x:.6g}"
    return str(x)

path = sys.argv[1] if len(sys.argv) > 1 else "cma.jsonl"

rows = []
with open(path, "r", encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        rows.append(json.loads(line))

rows = [r for r in rows if "#loss" in r]
rows.sort(key=lambda r: r["#loss"])

params = [k for k, v in rows[0].items() if is_param(k, v)]
losses = [r["#loss"] for r in rows]

print(f"rows: {len(rows)}")
print(f"params: {len(params)}")
print()
print("LOSS SUMMARY")
print(f"best:     {fmt(min(losses))}")
print(f"p01:      {fmt(percentile(losses, 0.01))}")
print(f"p05:      {fmt(percentile(losses, 0.05))}")
print(f"p10:      {fmt(percentile(losses, 0.10))}")
print(f"median:   {fmt(median(losses))}")
print(f"mean:     {fmt(mean(losses))}")
print(f"worst:    {fmt(max(losses))}")

def summarize_group(name, group):
    print()
    print(name)
    print("-" * len(name))

    n = len(group)
    centroid = {p: mean([r[p] for r in group]) for p in params}
    medoid = min(group, key=lambda r: sum(
        ((r[p] - centroid[p]) ** 2) for p in params
    ))

    print(f"n: {n}")
    print(f"best loss in group: {fmt(group[0]['#loss'])}")
    print(f"centroid loss proxy: not evaluated")
    print(f"medoid loss: {fmt(medoid['#loss'])}")
    print()
    print("centroid params:")
    for p in params:
        vals = [r[p] for r in group]
        c = mean(vals)
        m = median(vals)
        sd = stdev(vals) if len(vals) > 1 else 0.0
        print(f"  {p:24s} mean={fmt(c):>10s}  median={fmt(m):>10s}  sd={fmt(sd):>10s}")

    print()
    print("medoid params:")
    for p in params:
        print(f"  {p:24s} {fmt(medoid[p])}")

top1 = rows[:max(1, len(rows) // 100)]
top5 = rows[:max(1, len(rows) * 5 // 100)]
top10 = rows[:max(1, len(rows) * 10 // 100)]

summarize_group("TOP 1%", top1)
summarize_group("TOP 5%", top5)
summarize_group("TOP 10%", top10)

print()
print("BEST SINGLE")
print("-" * 11)
print(f"loss: {fmt(rows[0]['#loss'])}")
for p in params:
    print(f"  {p:24s} {fmt(rows[0][p])}")

print()
print("ROBUST FREEZE CANDIDATE")
print("-" * 23)
print("Use TOP 10% median for ints, TOP 10% mean for floats.")
for p in params:
    vals = [r[p] for r in top10]
    sample = rows[0][p]
    if isinstance(sample, int):
        val = round(median(vals))
    else:
        val = mean(vals)
    print(f"  {p:24s} {fmt(val)}")

print()
print("STABILITY / SENSITIVITY")
print("-" * 23)
print("Small top10 sd = stable. Large top10 sd = noisy or underdetermined.")
for p in params:
    vals_all = [r[p] for r in rows]
    vals_top = [r[p] for r in top10]
    sd_all = stdev(vals_all) if len(vals_all) > 1 else 0.0
    sd_top = stdev(vals_top) if len(vals_top) > 1 else 0.0
    shrink = 1.0 - (sd_top / sd_all) if sd_all else 0.0
    print(f"  {p:24s} top10_sd={fmt(sd_top):>10s}  all_sd={fmt(sd_all):>10s}  shrink={fmt(shrink):>10s}")