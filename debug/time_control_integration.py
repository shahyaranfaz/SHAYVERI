#!/usr/bin/env python3
"""Integration checks for timed Lazy SMP and production ponder flow."""

import os
import queue
import subprocess
import sys
import threading
import time
import traceback


ENGINE_BINARY_NAME = "SHAYVERI.exe" if os.name == "nt" else "SHAYVERI"
DEFAULT_ENGINE_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", ENGINE_BINARY_NAME)
)
ENGINE_PATH = os.environ.get("SHAYVERI_ENGINE", DEFAULT_ENGINE_PATH)
RESPONSE_TIMEOUT_SEC = float(os.environ.get("SHAYVERI_TIME_RESPONSE_TIMEOUT_SEC", "3"))
PONDER_PROBE_SEC = float(os.environ.get("SHAYVERI_PONDER_PROBE_SEC", "0.15"))


class EngineSession:
    def __init__(self) -> None:
        self.proc = subprocess.Popen(
            [ENGINE_PATH],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        assert self.proc.stdin is not None
        assert self.proc.stdout is not None
        self.lines: queue.Queue[str] = queue.Queue()
        self.output: list[str] = []
        self.reader = threading.Thread(target=self._read_output, daemon=True)
        self.reader.start()

    def _read_output(self) -> None:
        assert self.proc.stdout is not None
        for line in self.proc.stdout:
            self.output.append(line)
            self.lines.put(line)

    def send(self, command: str) -> None:
        assert self.proc.stdin is not None
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()

    def wait_for_prefix(self, prefix: str, timeout: float = RESPONSE_TIMEOUT_SEC) -> str:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(
                    f"timed out waiting for {prefix!r}. Output tail: {self.tail()}"
                )
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty as exc:
                raise AssertionError(
                    f"timed out waiting for {prefix!r}. Output tail: {self.tail()}"
                ) from exc
            if line.startswith(prefix):
                return line

    def assert_no_prefix_for(self, prefix: str, duration: float) -> None:
        deadline = time.monotonic() + duration
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return
            try:
                line = self.lines.get(timeout=remaining)
            except queue.Empty:
                return
            if line.startswith(prefix):
                raise AssertionError(
                    f"unexpected {prefix!r} before ponderhit/stop: {line.strip()}"
                )

    def tail(self, count: int = 20) -> str:
        return "".join(self.output[-count:])

    def close(self) -> None:
        if self.proc.poll() is None:
            try:
                self.send("quit")
                self.proc.wait(timeout=RESPONSE_TIMEOUT_SEC)
            except (OSError, subprocess.TimeoutExpired):
                self.proc.kill()
                self.proc.wait()


def initialize(session: EngineSession) -> None:
    session.send("uci")
    session.wait_for_prefix("uciok")
    session.send("setoption name OwnBook value false")
    session.send("setoption name Ponder value true")
    session.send("isready")
    session.wait_for_prefix("readyok")


def test_lazy_smp_timed_stop(session: EngineSession) -> None:
    session.send("setoption name Threads value 4")
    session.send("ucinewgame")
    session.send("position startpos")
    started = time.monotonic()
    session.send("go wtime 500 btime 500 winc 0 binc 0")
    session.wait_for_prefix("bestmove ")
    elapsed = time.monotonic() - started
    if elapsed > RESPONSE_TIMEOUT_SEC:
        raise AssertionError(f"Lazy SMP timed search took {elapsed:.3f}s to stop")


def test_fixed_movetime_is_not_adaptively_shortened(session: EngineSession) -> None:
    session.send("setoption name Threads value 4")
    session.send("setoption name MoveOverhead value 10")
    session.send("ucinewgame")
    session.send("position startpos")
    started = time.monotonic()
    session.send("go movetime 250")
    session.wait_for_prefix("bestmove ")
    elapsed = time.monotonic() - started
    # The fixed allocation is 240 ms after overhead. Allow 25 ms for timer
    # granularity, but reject the ~190 ms adaptive early stops this guards.
    if elapsed < 0.215:
        raise AssertionError(f"fixed movetime ended early after {elapsed:.3f}s")


def test_concurrent_command_output(session: EngineSession) -> None:
    session.send("setoption name Threads value 4")
    session.send("ucinewgame")
    session.send("position startpos")
    output_start = len(session.output)
    session.send("go infinite")
    for _ in range(20):
        session.send("isready")
    for _ in range(20):
        session.wait_for_prefix("readyok")
    session.send("stop")
    session.wait_for_prefix("bestmove ")

    valid_prefixes = ("info ", "readyok", "bestmove ")
    malformed = [
        line.rstrip("\n")
        for line in session.output[output_start:]
        if line and not line.startswith(valid_prefixes)
    ]
    if malformed:
        raise AssertionError(f"interleaved UCI output: {malformed[:3]}")


def start_ponder(session: EngineSession) -> None:
    session.send("setoption name Threads value 2")
    session.send("ucinewgame")
    session.send("position startpos")
    session.send("go ponder wtime 1000 btime 1000 winc 0 binc 0")
    session.assert_no_prefix_for("bestmove ", PONDER_PROBE_SEC)


def test_ponderhit(session: EngineSession) -> None:
    start_ponder(session)
    session.send("ponderhit")
    session.wait_for_prefix("bestmove ")


def test_stop_during_ponder(session: EngineSession) -> None:
    start_ponder(session)
    session.send("stop")
    session.wait_for_prefix("bestmove ")


def start_terminal_ponder(session: EngineSession) -> None:
    session.send("setoption name Threads value 1")
    session.send("ucinewgame")
    session.send("position fen 7k/5Q2/7K/8/8/8/8/8 b - - 0 1")
    session.send("go ponder wtime 1000 btime 1000 winc 0 binc 0")
    session.assert_no_prefix_for("bestmove ", PONDER_PROBE_SEC)


def test_terminal_ponder_waits_for_stop(session: EngineSession) -> None:
    start_terminal_ponder(session)
    session.send("stop")
    session.wait_for_prefix("bestmove ")


def test_terminal_ponderhit(session: EngineSession) -> None:
    start_terminal_ponder(session)
    session.send("ponderhit")
    session.wait_for_prefix("bestmove ")


def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(
            f"engine binary not found: {ENGINE_PATH} "
            "(run `make` in repo root or set SHAYVERI_ENGINE)",
            file=sys.stderr,
        )
        return 1

    session = EngineSession()
    try:
        initialize(session)
        test_lazy_smp_timed_stop(session)
        test_fixed_movetime_is_not_adaptively_shortened(session)
        test_concurrent_command_output(session)
        test_ponderhit(session)
        test_stop_during_ponder(session)
        test_terminal_ponder_waits_for_stop(session)
        test_terminal_ponderhit(session)
        print(
            "Time-control integration checks passed "
            "(Lazy SMP + serialized output + ponderhit + ponder stop + terminal ponder)."
        )
        return 0
    except Exception as exc:
        print(f"Time-control integration checks failed: {exc}", file=sys.stderr)
        traceback.print_exc()
        return 2
    finally:
        session.close()


if __name__ == "__main__":
    sys.exit(main())
