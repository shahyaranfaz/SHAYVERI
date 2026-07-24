# Performance profiling

## Purpose

Measure SHAYVERI performance with repeatable workloads, catch correctness or
determinism regressions, and investigate confirmed speed changes with Linux
hardware counters and sampled profiles.

## Motivation

Search speed is sensitive to the engine build, CPU placement, thread count,
and workload. A single NPS result can therefore be misleading. These tools
record the engine and host metadata, repeat representative searches, verify a
trusted bench signature, and preserve raw results so two builds can be
compared on the same machine.

## Method

`baseline.py` runs four kinds of checks:

1. The trusted single-threaded bench, whose expected node count is `94602`.
2. Fixed-depth and fixed-node searches for deterministic best-move checks.
3. Timed searches at several thread counts for Lazy SMP scaling.
4. A small tactical best-move signature for correctness.

Each run records build and machine metadata, raw UCI output, individual
measurements, medians, and thread-scaling statistics. `compare.py` reports
percentage changes between two baselines. The shell tools use the same trusted
bench workload for quick NPS checks, hardware-counter measurements, and
instruction-level hotspot profiles.

## Usage

Build the engine, then record a baseline from the repository root:

```bash
make
python3 scripts/profiling/baseline.py \
  --run-name before \
  --cpu 0 \
  --threads 1,2,4,8
```

After rebuilding with a change, repeat the measurement under the same
conditions:

```bash
python3 scripts/profiling/baseline.py \
  --run-name after \
  --cpu 0 \
  --threads 1,2,4,8

python3 scripts/profiling/compare.py \
  scripts/profiling/outputs/before/baseline.json \
  scripts/profiling/outputs/after/baseline.json
```

For a quick repeated single-threaded bench:

```bash
RUNS=11 CPU=0 bash scripts/profiling/bench.sh
```

Set `CPU=` or omit `--cpu` to leave scheduling unconstrained. Comparisons are
most useful when both builds use the same machine, CPU affinity, governor,
compiler flags, thread counts, and otherwise idle system.

## Linux perf

Once the repeated bench shows a credible change, collect hardware counters:

```bash
RUNS=7 REPEATS=25 CPU=0 bash scripts/profiling/perf_stat.sh
```

Capture a sampled call-graph profile and inspect it:

```bash
REPEATS=25 CPU=0 bash scripts/profiling/perf_record.sh
perf report -i scripts/profiling/outputs/shayveri-bench.perf.data
```

The perf scripts require `perf`; CPU pinning also requires `taskset`. Access to
hardware events may depend on the host's kernel perf settings. On hybrid Intel
CPUs, `perf_record.sh` selects the P-core cycles event when available.

## Layout and outputs

- `baseline.py`: full correctness, determinism, performance, and scaling run
- `compare.py`: CSV comparison of two `baseline.json` summaries
- `bench.sh`: repeated fixed-workload NPS benchmark
- `perf_stat.sh`: repeated hardware-counter measurement
- `perf_record.sh`: sampled call-graph capture with sidecar metadata
- `workloads.py`: shared representative positions and tactical signatures
- `outputs/<run>/`: `baseline.json`, `results.csv`, and raw `engine.log`

The baseline defaults to the repository's Linux `SHAYVERI` executable.
Override it with `--engine`; the shell tools use `ENGINE`. Useful baseline
controls include `--runs`, `--depth`, `--nodes`, `--movetime`, `--threads`,
`--all-timed-cases`, `--pgo-cases`, `--timeout`, `--build-label`, and
`--build-flags`.

The fixed-depth and fixed-node checks intentionally require one thread because
multi-threaded Lazy SMP search is not deterministic. Use the timed workload to
measure multi-thread scaling.
