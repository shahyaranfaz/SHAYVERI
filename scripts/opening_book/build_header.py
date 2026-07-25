#!/usr/bin/env python3
"""Build the deterministic majority-move SHAYVERI opening book."""

from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path

import chess


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[1]
OUTPUTS_DIR = SCRIPT_DIR / "outputs"
U64_MASK = 0xFFFFFFFFFFFFFFFF
MIN_TOTAL_PLAYS = 5


def load_zobrist_keys(path: Path):
    data = json.loads(path.read_text(encoding="utf-8"))
    pieces = [[value & U64_MASK for value in row] for row in data["pieces"]]
    sides = data["sides"] & U64_MASK
    castlings = [value & U64_MASK for value in data["castlings"]]
    en_passants = [value & U64_MASK for value in data["en_passants"]]
    return pieces, sides, castlings, en_passants


def book_key(board: chess.Board, keys) -> int:
    pieces, sides, castlings, en_passants = keys
    key = 0

    for square, piece in board.piece_map().items():
        piece_index = piece.piece_type + (0 if piece.color == chess.WHITE else 6)
        key ^= pieces[piece_index][square]

    if board.turn == chess.BLACK:
        key ^= sides

    castling = 0
    if board.has_kingside_castling_rights(chess.WHITE):
        castling |= 1
    if board.has_queenside_castling_rights(chess.WHITE):
        castling |= 2
    if board.has_kingside_castling_rights(chess.BLACK):
        castling |= 4
    if board.has_queenside_castling_rights(chess.BLACK):
        castling |= 8
    key ^= castlings[castling & 15]

    if board.ep_square is not None:
        key ^= en_passants[chess.square_file(board.ep_square)]

    return key & U64_MASK


def _is_legal_uci(board: chess.Board, move: str) -> bool:
    try:
        board.parse_uci(move)
        return True
    except ValueError:
        return False


def build_move_table(table_path: Path, keys, max_plies: int):
    positions = defaultdict(lambda: {"fen": "", "moves": defaultdict(int)})
    with table_path.open(newline="", encoding="utf-8-sig") as source:
        total_rows = sum(1 for _ in csv.DictReader(source))
    print(f"Processing {total_rows} table rows...")
    rows_processed = 0

    with table_path.open(newline="", encoding="utf-8-sig") as source:
        for row in csv.DictReader(source):
            rows_processed += 1
            if rows_processed % 10000 == 0 or rows_processed == total_rows:
                print(
                    f"Processed {rows_processed} / {total_rows} table rows...",
                    end="\r",
                    flush=True,
                )

            try:
                played = int(row["Played"])
            except (KeyError, TypeError, ValueError):
                continue

            board = chess.Board()
            moves = str(row.get("Moves", "")).split()[:max_plies]
            for move in moves:
                try:
                    parsed_move = board.parse_uci(move)
                except ValueError:
                    raise RuntimeError(
                        f"invalid move {move} in table row "
                        f"for position {board.fen()}"
                    ) from None

                key = book_key(board, keys)
                position = positions[key]
                current_fen = board.fen()
                if not position["fen"]:
                    position["fen"] = current_fen
                elif position["fen"].split()[:4] != current_fen.split()[:4]:
                    raise RuntimeError(
                        "Zobrist key maps to multiple board positions: "
                        f"key=0x{key:016x}, first={position['fen']}, "
                        f"next={current_fen}"
                    )

                position["moves"][move] += played
                board.push(parsed_move)

    print(f"Processed {rows_processed} / {total_rows} table rows.")

    for key, position in positions.items():
        board = chess.Board(position["fen"])
        invalid = [
            move
            for move in position["moves"]
            if not _is_legal_uci(board, move)
        ]
        if invalid:
            raise RuntimeError(
                "candidate moves are illegal for their FEN: "
                f"key=0x{key:016x}, fen={position['fen']}, "
                f"moves={','.join(invalid)}"
            )

    return positions


def select_majority_moves(positions, min_total_plays: int):
    entries = []
    total_positions = len(positions)
    for position_index, (key, position) in enumerate(positions.items(), 1):
        if position_index % 10000 == 0 or position_index == total_positions:
            print(
                f"Selected majority moves for {position_index} / "
                f"{total_positions} positions...",
                end="\r",
                flush=True,
            )

        move_counts = position["moves"]
        if (
            not position["fen"]
            or not move_counts
            or sum(move_counts.values()) < min_total_plays
        ):
            continue

        # The table is sorted lexicographically, so dict insertion order
        # preserves the historical tie-break behavior of max(..., get).
        move = max(move_counts, key=move_counts.get)
        entries.append((key, move))

    print(
        f"Selected majority moves for {total_positions} / "
        f"{total_positions} positions."
    )
    return sorted(entries, key=lambda entry: entry[0])


def write_source(path: Path, entries: list[tuple[int, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as output:
        output.write(
            "// Generated by scripts/opening_book/build_header.py. "
            "Do not edit by hand.\n\n"
        )
        output.write('#include "opening_book.h"\n\n#include <algorithm>\n\n')
        output.write("namespace SHAYVERI {\n\n")
        output.write("static constexpr BookEntry OPENING_BOOK[] = {\n")
        for key, move in entries:
            output.write(f'    {{ 0x{key:016x}ULL, "{move}" }},\n')
        output.write("};\n\n")
        output.write(f"static constexpr int OPENING_BOOK_SIZE = {len(entries)};\n")
        output.write(
            "\nconst BookEntry *probe_book(U64 key) {\n"
            "    const auto entry = std::lower_bound(OPENING_BOOK, OPENING_BOOK + OPENING_BOOK_SIZE, key,\n"
            "        [](const BookEntry &book, U64 target) { return book.key < target; });\n"
            "    return entry != OPENING_BOOK + OPENING_BOOK_SIZE && entry->key == key ? entry : nullptr;\n"
            "}\n"
        )
        output.write("\n} // namespace SHAYVERI\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--table", type=Path, default=OUTPUTS_DIR / "table.csv")
    parser.add_argument("--keys", type=Path, default=OUTPUTS_DIR / "zobrist_keys.json")
    parser.add_argument("--output", type=Path, default=REPO_ROOT / "src" / "opening_book.cpp")
    parser.add_argument("--max-plies", type=int, default=30)
    parser.add_argument("--min-total-plays", type=int, default=MIN_TOTAL_PLAYS)
    args = parser.parse_args()

    for path in (args.table, args.keys):
        if not path.exists():
            raise SystemExit(f"missing required file: {path}")
    if args.max_plies < 1 or args.min_total_plays < 1:
        raise SystemExit("--max-plies and --min-total-plays must be positive")

    keys = load_zobrist_keys(args.keys)
    positions = build_move_table(args.table, keys, args.max_plies)
    print(
        f"Selecting majority moves for {len(positions)} positions "
        f"with minimum cumulative plays {args.min_total_plays}"
    )
    entries = select_majority_moves(positions, args.min_total_plays)
    if not entries:
        raise SystemExit("no book positions survived the play-count filter")

    write_source(args.output, entries)
    print(f"Done - {len(entries)} entries written to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
