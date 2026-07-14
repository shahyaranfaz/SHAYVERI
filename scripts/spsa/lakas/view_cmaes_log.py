#!/usr/bin/env python3
"""Summarize measured candidates from a Nevergrad ParametersLogger JSONL file."""

import argparse
import json
import math
from collections import OrderedDict
from pathlib import Path
from statistics import mean, median, stdev


def parse_args():
    parser = argparse.ArgumentParser(
        description="Inspect played CMA-ES candidates without treating match noise as proof."
    )
    parser.add_argument("path", type=Path, help="Nevergrad ParametersLogger JSONL file.")
    parser.add_argument(
        "--recent",
        type=int,
        default=100,
        help="Number of latest measured candidates used for the recent center (default: 100).",
    )
    parser.add_argument(
        "--include-penalties",
        action="store_true",
        help="Include loss=1 rows, which this Lakas driver uses for failed jobs.",
    )
    return parser.parse_args()


def is_param(key, value):
    return (
        isinstance(value, (int, float))
        and not isinstance(value, bool)
        and not key.startswith("#")
        and "#sigma" not in key
    )


def percentile(values, fraction):
    values = sorted(values)
    index = (len(values) - 1) * fraction
    lo = math.floor(index)
    hi = math.ceil(index)
    if lo == hi:
        return values[lo]
    return values[lo] * (hi - index) + values[hi] * (index - lo)


def fmt(value):
    if isinstance(value, float):
        return f"{value:.6g}"
    return str(value)


def load_rows(path):
    lines = path.read_text(encoding="utf-8").splitlines()
    rows = []
    for line_number, line in enumerate(lines, start=1):
        if not line.strip():
            continue
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError as exc:
            if line_number == len(lines):
                print(f"warning: ignored incomplete final line {line_number}: {exc.msg}")
                continue
            raise ValueError(f"invalid JSON on line {line_number}: {exc.msg}") from exc
    return rows


def dedupe_rows(rows):
    deduped = OrderedDict()
    for index, row in enumerate(rows):
        key = row.get("#uid")
        if key is None:
            key = (
                row.get("#session"),
                row.get("#num-tell"),
                row.get("#loss"),
                tuple((name, row[name]) for name in sorted(row) if is_param(name, row[name])),
                index,
            )
        deduped[key] = row
    return list(deduped.values())


def is_synthetic_seed(row):
    return (
        row.get("#num-ask") == 0
        and row.get("#num-tell") == 0
        and row.get("#num-tell-not-asked") == 0
    )


def parameter_names(rows):
    schemas = {
        frozenset(name for name, value in row.items() if is_param(name, value))
        for row in rows
    }
    if len(schemas) != 1:
        sizes = sorted({len(schema) for schema in schemas})
        raise ValueError(
            f"log contains {len(schemas)} parameter schemas with sizes {sizes}. "
            "Inspect each batch separately."
        )
    return [name for name, value in rows[0].items() if is_param(name, value)]


def sample_sd(values):
    return stdev(values) if len(values) > 1 else 0.0


def aggregate_value(rows, param):
    values = [row[param] for row in rows]
    if all(isinstance(value, int) and not isinstance(value, bool) for value in values):
        return round(median(values))
    return mean(values)


def normalized_medoid(group, all_rows, params):
    centroid = {param: mean(row[param] for row in group) for param in params}
    scales = {}
    for param in params:
        values = [row[param] for row in all_rows]
        scale = sample_sd(values)
        if scale == 0.0:
            scale = max(values) - min(values)
        scales[param] = scale if scale != 0.0 else 1.0

    return min(
        group,
        key=lambda row: sum(
            ((row[param] - centroid[param]) / scales[param]) ** 2
            for param in params
        ),
    )


def summarize_group(name, group, all_rows, params):
    representative = normalized_medoid(group, all_rows, params)
    print()
    print(name)
    print("-" * len(name))
    print(f"n: {len(group)}")
    print(f"lowest loss in group: {fmt(min(row['#loss'] for row in group))}")
    print("aggregate loss: not evaluated")
    print(f"representative measured loss: {fmt(representative['#loss'])}")
    print()
    print("parameter distribution:")
    for param in params:
        values = [row[param] for row in group]
        print(
            f"  {param:24s} mean={fmt(mean(values)):>10s}  "
            f"median={fmt(median(values)):>10s}  sd={fmt(sample_sd(values)):>10s}"
        )

    print()
    print("normalized representative measured candidate:")
    for param in params:
        print(f"  {param:24s} {fmt(representative[param])}")


def print_candidate(name, explanation, group, params):
    print()
    print(name)
    print("-" * len(name))
    print(explanation)
    for param in params:
        print(f"  {param:24s} {fmt(aggregate_value(group, param))}")


def row_order(row):
    tell = row.get("#num-tell")
    ask = row.get("#num-ask")
    return (
        tell if isinstance(tell, int) else -1,
        ask if isinstance(ask, int) else -1,
    )


def main():
    args = parse_args()
    if not args.path.is_file():
        raise SystemExit(f"log file not found: {args.path}")
    if args.recent < 1:
        raise SystemExit("--recent must be a positive integer")

    raw_rows = load_rows(args.path)
    loss_rows = [row for row in dedupe_rows(raw_rows) if "#loss" in row]
    seed_rows = [row for row in loss_rows if is_synthetic_seed(row)]
    measured_rows = [row for row in loss_rows if not is_synthetic_seed(row)]
    penalty_rows = [row for row in measured_rows if row["#loss"] >= 1.0]
    if not args.include_penalties:
        measured_rows = [row for row in measured_rows if row["#loss"] < 1.0]

    if not measured_rows:
        raise SystemExit("no measured candidate rows found")

    params = parameter_names(measured_rows)
    rows_by_loss = sorted(measured_rows, key=lambda row: row["#loss"])
    rows_by_progress = sorted(measured_rows, key=row_order)
    losses = [row["#loss"] for row in measured_rows]
    sessions = {row.get("#session") for row in measured_rows if row.get("#session")}
    lineages = {row.get("#lineage") for row in measured_rows if row.get("#lineage")}
    if len(lineages) > 1:
        raise SystemExit(
            f"log contains {len(lineages)} optimizer lineages. "
            "Do not combine independent runs in one summary."
        )
    max_ask = max((row.get("#num-ask", 0) for row in measured_rows), default=0)
    max_tell = max((row.get("#num-tell", 0) for row in measured_rows), default=0)
    max_lineage_generation = max(
        (row.get("#generation", 0) for row in measured_rows),
        default=0,
    )

    print(f"measured rows: {len(measured_rows)}")
    print(f"synthetic seed rows excluded: {len(seed_rows)}")
    print(f"failed-job penalty rows: {len(penalty_rows)}")
    print(f"sessions: {len(sessions)}")
    print(f"optimizer lineages: {len(lineages)}")
    print(f"maximum logged counters: asks={max_ask} tells={max_tell}")
    print(
        "maximum candidate ancestry depth: "
        f"{max_lineage_generation} (not a CMA generation count)"
    )
    print(f"params: {len(params)}")

    print()
    print("MEASURED LOSS SUMMARY")
    print(f"lowest:  {fmt(min(losses))}  score={fmt(1.0 - min(losses))}")
    print(f"p01:     {fmt(percentile(losses, 0.01))}")
    print(f"p05:     {fmt(percentile(losses, 0.05))}")
    print(f"p10:     {fmt(percentile(losses, 0.10))}")
    print(f"median:  {fmt(median(losses))}  score={fmt(1.0 - median(losses))}")
    print(f"mean:    {fmt(mean(losses))}  score={fmt(1.0 - mean(losses))}")
    print(f"highest: {fmt(max(losses))}  score={fmt(1.0 - max(losses))}")
    print("note: every loss is one noisy match result, not an Elo estimate")

    for percent in (1, 5, 10):
        count = max(1, math.ceil(len(rows_by_loss) * percent / 100.0))
        summarize_group(
            f"LOWEST-LOSS {percent}% (SELECTION-BIASED)",
            rows_by_loss[:count],
            measured_rows,
            params,
        )

    best = rows_by_loss[0]
    print()
    print("LOWEST OBSERVED LOSS")
    print("-" * 20)
    print(f"loss: {fmt(best['#loss'])}")
    print("This is the most winner's-curse-prone candidate and should not be promoted directly.")
    for param in params:
        print(f"  {param:24s} {fmt(best[param])}")

    top_count = max(1, math.ceil(len(rows_by_loss) * 0.10))
    print_candidate(
        "TOP-10% AGGREGATE FOR VALIDATION",
        "Median for integer parameters and mean for floats. This aggregate was not evaluated.",
        rows_by_loss[:top_count],
        params,
    )

    recent = rows_by_progress[-min(args.recent, len(rows_by_progress)):]
    print_candidate(
        "RECENT SEARCH CENTER FOR VALIDATION",
        f"Center of the latest {len(recent)} measured candidates. This aggregate was not evaluated.",
        recent,
        params,
    )

    print()
    print("PARAMETER CONCENTRATION")
    print("-" * 23)
    print("Ratios below 1 show concentration, not causal sensitivity or proof of strength.")
    top10 = rows_by_loss[:top_count]
    for param in params:
        all_sd = sample_sd([row[param] for row in measured_rows])
        top_sd = sample_sd([row[param] for row in top10])
        recent_sd = sample_sd([row[param] for row in recent])
        top_ratio = top_sd / all_sd if all_sd else 0.0
        recent_ratio = recent_sd / all_sd if all_sd else 0.0
        print(
            f"  {param:24s} all_sd={fmt(all_sd):>10s}  "
            f"top10/all={fmt(top_ratio):>10s}  recent/all={fmt(recent_ratio):>10s}"
        )

    print()
    print("Use the decoded CMA distribution center as the optimizer-state candidate. Treat both")
    print("log aggregates as alternatives for direct A/B validation, never as freeze values.")


if __name__ == "__main__":
    main()
