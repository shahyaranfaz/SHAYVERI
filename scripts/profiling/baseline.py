#!/usr/bin/env python3
"""Record a reproducible SHAYVERI correctness and performance baseline."""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import queue
import re
import shutil
import statistics
import subprocess
import sys
import threading
import time

sys.dont_write_bytecode = True

from workloads import POSITION_CASES, TACTICAL_CASES, TIMED_CASES


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_ENGINE = REPO_ROOT / ("SHAYVERI.exe" if os.name == "nt" else "SHAYVERI")
EXPECTED_BENCH_NODES = 101863
INFO_RE = re.compile(
    r"^info depth (?P<depth>\d+) score (?P<score_type>cp|mate) "
    r"(?P<score>-?\d+).*?\bnodes (?P<nodes>\d+)\s+nps (?P<nps>\d+)"
)


def command_output(command: list[str]) -> str:
    try:
        return subprocess.run(
            command, check=False, text=True, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, timeout=10,
        ).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return "unavailable"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def git_value(*arguments: str) -> str:
    return command_output(["git", "-C", str(REPO_ROOT), *arguments])


def cpu_governors() -> str:
    governors = set()
    for path in Path("/sys/devices/system/cpu").glob("cpu*/cpufreq/scaling_governor"):
        try:
            governors.add(path.read_text(encoding="utf-8").strip())
        except OSError:
            pass
    return ",".join(sorted(governors)) or "unavailable"


def metadata(engine: Path, arguments: argparse.Namespace) -> dict[str, object]:
    return {
        "schema": "shayveri-profile-v1",
        "timestamp_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "engine": str(engine),
        "engine_sha256": sha256(engine),
        "git_commit": git_value("rev-parse", "HEAD"),
        "git_describe": git_value("describe", "--always", "--dirty"),
        "git_status": git_value("status", "--short"),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "cpu": platform.processor() or "unknown",
        "logical_cpus": os.cpu_count(),
        "uname": command_output(["uname", "-a"]),
        "lscpu": command_output(["lscpu"]),
        "compiler": command_output(["g++", "--version"]).splitlines()[0],
        "build_label": arguments.build_label,
        "build_flags": arguments.build_flags or "unknown",
        "make_dry_run": command_output(["make", "-Bn", "-C", str(REPO_ROOT), "all"]),
        "cpu_governor": cpu_governors(),
        "cpu_affinity": arguments.cpu or "unconstrained",
        "runs": arguments.runs,
        "depth": arguments.depth,
        "nodes": arguments.nodes,
        "movetime_ms": arguments.movetime,
        "threads": arguments.threads,
        "timed_scaling_metric": "median aggregate UCI NPS at the last completed iteration",
    }


class Engine:
    def __init__(self, executable: Path, cpu: str, timeout: float) -> None:
        command = [str(executable)]
        if cpu:
            if shutil.which("taskset") is None:
                raise RuntimeError("taskset is required when --cpu is set")
            command = ["taskset", "-c", cpu, *command]
        self.process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, bufsize=1,
        )
        self.timeout = timeout
        self.raw: list[str] = []
        self.lines: queue.Queue[str | None] = queue.Queue()
        self.reader = threading.Thread(target=self._read_output, daemon=True)
        self.reader.start()
        self.send("uci")
        self.wait_for("uciok")
        self.send("setoption name OwnBook value false")
        self.send("setoption name BookInfoDepth value 0")
        self.send("isready")
        self.wait_for("readyok")

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()

    def _read_output(self) -> None:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            self.raw.append(line)
            self.lines.put(line.rstrip("\n"))
        self.lines.put(None)

    def readline(self, remaining: float) -> str:
        try:
            line = self.lines.get(timeout=max(0.001, remaining))
        except queue.Empty as error:
            raise TimeoutError("timed out waiting for engine output") from error
        if line is None:
            raise RuntimeError(f"engine exited early with {self.process.poll()}")
        return line

    def wait_for(self, prefix: str) -> list[str]:
        lines: list[str] = []
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            line = self.readline(deadline - time.monotonic())
            lines.append(line)
            if line.startswith(prefix):
                return lines
        raise TimeoutError(f"timed out waiting for {prefix!r}")

    def search(self, position: str, go: str, threads: int) -> dict[str, object]:
        self.send(f"setoption name Threads value {threads}")
        self.send("ucinewgame")
        self.send(f"position {position}")
        started = time.perf_counter_ns()
        self.send(go)
        lines = self.wait_for("bestmove ")
        wall_ms = (time.perf_counter_ns() - started) / 1_000_000.0
        info = None
        for line in lines:
            match = INFO_RE.match(line)
            if match:
                info = match.groupdict()
        bestmove_line = next(line for line in reversed(lines) if line.startswith("bestmove "))
        bestmove = bestmove_line.split()[1]
        return {
            "bestmove": bestmove,
            "depth": int(info["depth"]) if info else 0,
            "score_type": info["score_type"] if info else "unknown",
            "score": int(info["score"]) if info else 0,
            "nodes": int(info["nodes"]) if info else 0,
            "nps": int(info["nps"]) if info else 0,
            "wall_ms": round(wall_ms, 3),
        }

    def bench(self) -> dict[str, int | float]:
        self.send("bench 16 1 3 default depth")
        lines = self.wait_for("===========================")
        # The first separator opens the summary; consume through its close.
        if not any(line.startswith("Nodes:") for line in lines):
            lines.extend(self.wait_for("==========================="))
        nodes = time_ms = nps = None
        for line in lines:
            if line.startswith("Nodes:"):
                nodes = int(line.split()[1])
            elif line.startswith("Time"):
                time_ms = float(line.split()[2])
            elif line.startswith("NPS"):
                nps = int(line.split()[2])
        if nodes is None or time_ms is None or nps is None:
            raise RuntimeError("malformed bench summary")
        if nodes != EXPECTED_BENCH_NODES:
            raise RuntimeError(
                f"bench signature mismatch: {nodes} != {EXPECTED_BENCH_NODES}"
            )
        return {"nodes": nodes, "time_ms": time_ms, "nps": nps}

    def close(self) -> None:
        if self.process.poll() is None:
            try:
                self.send("quit")
                self.process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.process.kill()
                self.process.wait()


def add_result(results: list[dict[str, object]], category: str, case: str,
               run: int, threads: int, data: dict[str, object]) -> None:
    results.append({
        "category": category, "case": case, "run": run,
        "threads": threads, **data,
    })


def run_search(engine: Engine, position: str, go: str, threads: int) -> dict[str, object]:
    result = engine.search(position, go, threads)
    result["position"] = position
    result["go"] = go
    return result


def summarize(results: list[dict[str, object]]) -> list[dict[str, object]]:
    groups: dict[tuple[str, str, int], list[dict[str, object]]] = {}
    for result in results:
        key = (str(result["category"]), str(result["case"]), int(result["threads"]))
        groups.setdefault(key, []).append(result)
    summary = []
    for (category, case, threads), rows in sorted(groups.items()):
        entry: dict[str, object] = {
            "category": category, "case": case, "threads": threads,
            "runs": len(rows),
        }
        for field in ("wall_ms", "nodes", "nps"):
            values = [float(row[field]) for row in rows if field in row]
            if values:
                entry[f"median_{field}"] = statistics.median(values)
        entry["bestmoves"] = sorted({str(row["bestmove"]) for row in rows if "bestmove" in row})
        summary.append(entry)
    return summary


def scaling(summary: list[dict[str, object]]) -> list[dict[str, object]]:
    timed = {
        (str(row["case"]), int(row["threads"])): row
        for row in summary if row["category"] == "timed"
    }
    rows = []
    for (case, threads), row in sorted(timed.items()):
        base = timed.get((case, 1))
        if base is None:
            continue
        base_nodes = float(base.get("median_nodes", 0))
        nodes = float(row.get("median_nodes", 0))
        base_nps = float(base.get("median_nps", 0))
        nps = float(row.get("median_nps", 0))
        speedup = nps / base_nps if base_nps else 0.0
        rows.append({
            "case": case,
            "threads": threads,
            "median_nodes": nodes,
            "median_nps": nps,
            "node_ratio_vs_1t": nodes / base_nodes if base_nodes else 0.0,
            "speedup_vs_1t": speedup,
            "scaling_efficiency": speedup / threads,
        })
    return rows


def correctness_errors(results: list[dict[str, object]]) -> list[str]:
    errors = []
    if not all(bool(row.get("solved", True)) for row in results):
        errors.append("tactical correctness signature failed")
    fixed: dict[tuple[str, str], set[str]] = {}
    for row in results:
        if row["category"] in ("fixed_depth", "fixed_nodes"):
            key = (str(row["category"]), str(row["case"]))
            fixed.setdefault(key, set()).add(str(row["bestmove"]))
    for key, moves in sorted(fixed.items()):
        if len(moves) != 1:
            errors.append(f"non-deterministic {key[0]} {key[1]} bestmoves: {sorted(moves)}")
    return errors


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    fields = [
        "category", "case", "run", "threads", "position", "go", "bestmove", "depth",
        "score_type", "score", "nodes", "nps", "wall_ms", "expected", "solved",
    ]
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def parse_threads(value: str) -> list[int]:
    threads = [int(item) for item in value.split(",") if item]
    if not threads or any(item < 1 for item in threads):
        raise argparse.ArgumentTypeError("threads must be comma-separated positive integers")
    return threads


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=DEFAULT_ENGINE)
    parser.add_argument("--output-dir", type=Path, default=SCRIPT_DIR / "outputs")
    parser.add_argument("--run-name", default="")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--depth", type=int, default=10)
    parser.add_argument("--nodes", type=int, default=100_000)
    parser.add_argument("--movetime", type=int, default=250)
    parser.add_argument("--threads", type=parse_threads, default=parse_threads("1,2,4,8"))
    parser.add_argument("--cpu", default="", help="taskset CPU list; empty disables pinning")
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--build-label", default="release")
    parser.add_argument(
        "--build-flags",
        default=" ".join(filter(None, (os.environ.get("CXXFLAGS"), os.environ.get("LDFLAGS")))),
        help="exact compiler/linker flags used for the measured engine",
    )
    args = parser.parse_args()
    if args.runs < 1 or args.depth < 1 or args.nodes < 1 or args.movetime < 1:
        parser.error("runs, depth, nodes, and movetime must be positive")
    engine_path = args.engine.resolve()
    if not engine_path.is_file():
        parser.error(f"engine not found: {engine_path}")

    commit = git_value("rev-parse", "--short", "HEAD") or "unknown"
    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_name = args.run_name or f"{timestamp}-{commit}"
    run_dir = args.output_dir.resolve() / run_name
    if run_dir.exists():
        parser.error(f"output directory already exists: {run_dir}")
    run_dir.mkdir(parents=True)

    results: list[dict[str, object]] = []
    engine = Engine(engine_path, args.cpu, args.timeout)
    try:
        for run in range(1, args.runs + 1):
            add_result(results, "bench", "trusted", run, 1, engine.bench())
        for case, position in POSITION_CASES:
            for run in range(1, args.runs + 1):
                add_result(results, "fixed_depth", case, run, 1,
                           run_search(engine, position, f"go depth {args.depth}", 1))
                add_result(results, "fixed_nodes", case, run, 1,
                           run_search(engine, position, f"go nodes {args.nodes}", 1))
        for threads in args.threads:
            for case, position in TIMED_CASES:
                for run in range(1, args.runs + 1):
                    add_result(results, "timed", case, run, threads,
                               run_search(engine, position, f"go movetime {args.movetime}", threads))
        for case, position, expected in TACTICAL_CASES:
            data = run_search(engine, position, "go depth 4", 1)
            data["expected"] = expected
            data["solved"] = data["bestmove"] == expected
            add_result(results, "tactical", case, 1, 1, data)
    finally:
        (run_dir / "engine.log").write_text("".join(engine.raw), encoding="utf-8")
        engine.close()

    summary = summarize(results)
    errors = correctness_errors(results)
    document = {
        "metadata": metadata(engine_path, args),
        "results": results,
        "summary": summary,
        "scaling": scaling(summary),
        "correctness_errors": errors,
    }
    (run_dir / "baseline.json").write_text(
        json.dumps(document, indent=2) + "\n", encoding="utf-8"
    )
    write_csv(run_dir / "results.csv", results)
    if errors:
        raise RuntimeError(
            f"correctness signature failed; results preserved in {run_dir}: "
            + "; ".join(errors)
        )
    print(f"baseline written: {run_dir}")
    print(f"results={len(results)} bench_nodes={EXPECTED_BENCH_NODES}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"baseline failed: {error}", file=sys.stderr)
        sys.exit(2)
