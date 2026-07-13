#!/usr/bin/env python3
"""
UCI checks borrowed from the style of:
- official-stockfish/Stockfish/tests/instrumented.py
"""

import os
import re
import subprocess
import sys
import traceback

ENGINE_BINARY_NAME = "SHAYVERI.exe" if os.name == "nt" else "SHAYVERI"
DEFAULT_ENGINE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ENGINE_BINARY_NAME))
ENGINE_PATH = os.environ.get("SHAYVERI_ENGINE", DEFAULT_ENGINE_PATH)
TIMEOUT_SEC = int(os.environ.get("SHAYVERI_UCI_TIMEOUT_SEC", "60"))
TAIL_CHARS = 500
BENCH_SIGNATURE_NODES = 125213


def run_engine(commands: list[str], wait_for_bestmove: bool = False) -> str:
    proc = subprocess.Popen(
        [ENGINE_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert proc.stdin is not None
    assert proc.stdout is not None

    out: list[str] = []
    try:
        for command in commands:
            proc.stdin.write(command + "\n")
            proc.stdin.flush()
            if wait_for_bestmove and command.startswith("go "):
                while True:
                    line = proc.stdout.readline()
                    if line == "":
                        break
                    out.append(line)
                    if line.startswith("bestmove "):
                        break
        try:
            proc.stdin.write("quit\n")
            proc.stdin.flush()
        except OSError:
            pass
        try:
            rest, _ = proc.communicate(timeout=TIMEOUT_SEC)
            out.append(rest)
        except subprocess.TimeoutExpired:
            proc.kill()
            rest, _ = proc.communicate()
            out.append(rest)
            raise
    finally:
        if proc.poll() is None:
            proc.kill()

    return "".join(out)


def require(text: str, needle: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing '{needle}' in output: {text[-TAIL_CHARS:]}")


def require_bestmove(text: str) -> None:
    if not re.search(r"^bestmove\s+\S+", text, flags=re.MULTILINE):
        raise AssertionError(f"missing bestmove in output: {text[-TAIL_CHARS:]}")

def extract_bestmove(text: str) -> str:
    m = re.search(r"^bestmove\s+(\S+)", text, flags=re.MULTILINE)
    if not m:
        raise AssertionError(f"missing bestmove in output: {text[-TAIL_CHARS:]}")
    return m.group(1)

def extract_last_pv(text: str) -> str:
    pvs = re.findall(r"^info .*?\spv\s+(.+)$", text, flags=re.MULTILINE)
    return pvs[-1].strip() if pvs else ""

def extract_bench_nodes(text: str) -> int:
    m = re.search(r"^Nodes:\s+(\d+)", text, flags=re.MULTILINE)
    if not m:
        raise AssertionError(f"missing bench node summary in output: {text[-TAIL_CHARS:]}")
    return int(m.group(1))

def extract_max_info_nodes(text: str) -> int:
    nodes = [int(n) for n in re.findall(r"\bnodes\s+(\d+)", text)]
    if not nodes:
        raise AssertionError(f"missing info nodes in output: {text[-TAIL_CHARS:]}")
    return max(nodes)

def has_book_marker(text: str) -> bool:
    return "info string book" in text

def has_depth_info(text: str) -> bool:
    return bool(re.search(r"^info depth \d+ .*\bscore\s+(?:cp|-?\d+|mate)\s+", text, flags=re.MULTILINE))

def main() -> int:
    if not os.path.exists(ENGINE_PATH):
        print(
            f"engine binary not found: {ENGINE_PATH} "
            "(run `make` in repo root or set SHAYVERI_ENGINE)",
            file=sys.stderr,
        )
        return 1

    try:
        handshake = run_engine(["uci", "isready"])
        require(handshake, "uciok")
        require(handshake, "readyok")
        require(handshake, "option name Hash type spin default 64 min 1 max 32768")
        require(handshake, "option name Clear Hash type button")
        require(handshake, "option name Threads type spin default 1 min 1 max 512")
        require(handshake, "option name Ponder type check default false")
        require(handshake, "option name OwnBook type check default true")
        require(handshake, "option name Book_Info_Depth type spin default 8 min 0 max 32")
        require(handshake, "option name UseNNUE type check default true")
        require(handshake, "option name EvalFile type string default <embedded>")
        require(handshake, "option name Minimum Thinking Time type spin default 0 min 0 max 5000")
        require(handshake, "option name Move Overhead type spin default 10 min 0 max 5000")

        startpos = run_engine([
            "setoption name OwnBook value false",
            "ucinewgame",
            "position startpos",
            "go nodes 1000",
        ], wait_for_bestmove=True)
        require_bestmove(startpos)
        if extract_max_info_nodes(startpos) > 2000:
            raise AssertionError("go nodes 1000 did not stop near the requested node limit")

        depth1 = run_engine(["ucinewgame", "position startpos", "go depth 1"], wait_for_bestmove=True)
        require_bestmove(depth1)

        bench = run_engine(["bench 16 1 3 default depth"])
        bench_nodes = extract_bench_nodes(bench)
        if bench_nodes <= 0:
            raise AssertionError(f"bench returned invalid node count: {bench_nodes}")
        if bench_nodes != BENCH_SIGNATURE_NODES:
            raise AssertionError(
                f"bench signature changed, expected {BENCH_SIGNATURE_NODES}, got {bench_nodes}"
            )

        # Position borrowed from official Stockfish UCI checks.
        fen_case = run_engine([
            "ucinewgame",
            "position fen 5rk1/1K4p1/8/8/3B4/8/8/8 b - - 0 1",
            "go nodes 1000",
        ], wait_for_bestmove=True)
        require_bestmove(fen_case)

        book_info = run_engine([
            "setoption name OwnBook value true",
            "setoption name Book_Info_Depth value 2",
            "ucinewgame",
            "position startpos",
            "go depth 2",
        ], wait_for_bestmove=True)
        fast_book = run_engine([
            "setoption name OwnBook value true",
            "setoption name Book_Info_Depth value 0",
            "ucinewgame",
            "position startpos",
            "go depth 2",
        ], wait_for_bestmove=True)
        without_book = run_engine([
            "setoption name OwnBook value false",
            "ucinewgame",
            "position startpos",
            "go depth 2",
        ], wait_for_bestmove=True)
        require_bestmove(book_info)
        require_bestmove(fast_book)
        require_bestmove(without_book)
        if not has_book_marker(book_info) or not has_depth_info(book_info):
            raise AssertionError("book info search did not emit book marker and depth info")
        if not has_book_marker(fast_book) or has_depth_info(fast_book):
            raise AssertionError("Book_Info_Depth=0 did not preserve the fast book path")
        if has_book_marker(without_book):
            raise AssertionError("opening book probe still triggered with OwnBook=false")

        determinism_1 = run_engine([
            "setoption name Threads value 1",
            "setoption name OwnBook value false",
            "ucinewgame",
            "position fen 5K2/8/2qk4/2nPp3/3r4/6B1/B7/3R4 w - e6 0 1",
            "go depth 4",
        ], wait_for_bestmove=True)
        determinism_2 = run_engine([
            "setoption name Threads value 1",
            "setoption name OwnBook value false",
            "ucinewgame",
            "position fen 5K2/8/2qk4/2nPp3/3r4/6B1/B7/3R4 w - e6 0 1",
            "go depth 4",
        ], wait_for_bestmove=True)
        bm1, bm2 = extract_bestmove(determinism_1), extract_bestmove(determinism_2)
        pv1, pv2 = extract_last_pv(determinism_1), extract_last_pv(determinism_2)
        if bm1 != bm2 or pv1 != pv2:
            raise AssertionError(
                f"determinism failure: run1 bestmove={bm1} pv={pv1!r}, run2 bestmove={bm2} pv={pv2!r}"
            )

        print(
            "UCI checks passed "
            f"(flow + bench nodes={bench_nodes} + book probe + determinism)."
        )
        return 0
    except Exception as exc:
        print(f"UCI checks failed: {exc}", file=sys.stderr)
        traceback.print_exc()
        return 2


if __name__ == "__main__":
    sys.exit(main())
