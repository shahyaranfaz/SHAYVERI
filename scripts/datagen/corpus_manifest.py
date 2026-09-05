#!/usr/bin/env python3
"""Build or verify immutable manifests for ready SHAYVERI datagen shards."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


RECORD_SIZE = 32
REQUIRED_FILES = ("DONE", "data.DONE", "data.summary.txt", "metadata.env", "SHA256SUMS")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_shard(path: Path) -> dict[str, object]:
    if not path.is_dir():
        raise ValueError(f"shard is not a directory: {path}")
    for name in REQUIRED_FILES:
        if not (path / name).is_file():
            raise ValueError(f"missing {name}: {path}")

    declared: dict[str, str] = {}
    for line in (path / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
        parts = line.split(maxsplit=1)
        if len(parts) != 2:
            raise ValueError(f"invalid SHA256SUMS line: {line!r}")
        relative_path = Path(parts[1].lstrip(" *"))
        if relative_path.is_absolute() or ".." in relative_path.parts:
            raise ValueError(f"unsafe SHA256SUMS path: {relative_path}")
        relative = relative_path.as_posix()
        declared[relative] = parts[0]
    for relative, expected in declared.items():
        artifact = path / relative
        if not artifact.is_file() or sha256(artifact) != expected:
            raise ValueError(f"recorded SHA-256 mismatch: {artifact}")

    bullet_files = sorted(path.glob("*.bullet.bin"), key=lambda item: item.name)
    if not bullet_files:
        raise ValueError(f"no Bullet files: {path}")
    files: list[dict[str, object]] = []
    positions = 0
    for bullet in bullet_files:
        size = bullet.stat().st_size
        if size % RECORD_SIZE:
            raise ValueError(f"size is not divisible by {RECORD_SIZE}: {bullet}")
        if bullet.name not in declared:
            raise ValueError(f"Bullet file missing from SHA256SUMS: {bullet}")
        positions += size // RECORD_SIZE
        files.append({"path": str(bullet.resolve()), "size": size, "sha256": sha256(bullet)})

    summary_positions = first_integer(path / "data.summary.txt", "positions")
    done_positions = first_integer(path / "data.DONE", "positions")
    if positions != summary_positions or positions != done_positions:
        raise ValueError(f"position counts disagree for {path}")

    return {
        "shard": str(path.resolve()),
        "positions": positions,
        "identity": {
            name: sha256(path / name)
            for name in ("data.DONE", "data.summary.txt", "metadata.env", "SHA256SUMS")
        },
        "files": files,
    }


def first_integer(path: Path, key: str) -> int:
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == key:
            return int(parts[1])
    raise ValueError(f"missing {key} in {path}")


def canonical_bytes(payload: dict[str, object]) -> bytes:
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode()


def validate_unique_payloads(records: list[dict[str, object]]) -> None:
    seen_shards: dict[tuple[tuple[int, str], ...], str] = {}
    seen_files: dict[tuple[int, str], str] = {}
    for record in records:
        shard = str(record["shard"])
        files = record["files"]
        fingerprint = tuple(sorted((int(item["size"]), str(item["sha256"])) for item in files))
        previous_shard = seen_shards.get(fingerprint)
        if previous_shard is not None:
            raise ValueError(f"duplicate shard payload: {shard} matches {previous_shard}")
        seen_shards[fingerprint] = shard

        for item in files:
            identity = (int(item["size"]), str(item["sha256"]))
            path = str(item["path"])
            previous_file = seen_files.get(identity)
            if previous_file is not None:
                raise ValueError(f"duplicate Bullet file: {path} matches {previous_file}")
            seen_files[identity] = path


def build(output: Path, shards: list[Path]) -> None:
    if output.exists():
        raise ValueError(f"refusing to overwrite manifest: {output}")
    records = [inspect_shard(path) for path in sorted(shards, key=lambda item: str(item.resolve()))]
    if not records:
        raise ValueError("at least one ready shard is required")
    validate_unique_payloads(records)
    body: dict[str, object] = {
        "format": "shayveri-corpus-manifest-v1",
        "record_size": RECORD_SIZE,
        "positions": sum(int(record["positions"]) for record in records),
        "shards": records,
    }
    manifest_hash = hashlib.sha256(canonical_bytes(body)).hexdigest()
    document = {**body, "manifest_sha256": manifest_hash}
    output.write_bytes(canonical_bytes(document))
    print(f"manifest_sha256={manifest_hash}")


def verify(manifest: Path) -> None:
    document = json.loads(manifest.read_text(encoding="utf-8"))
    expected_hash = document.pop("manifest_sha256", None)
    actual_hash = hashlib.sha256(canonical_bytes(document)).hexdigest()
    if expected_hash != actual_hash:
        raise ValueError("manifest SHA-256 mismatch")
    if document.get("format") != "shayveri-corpus-manifest-v1" or document.get("record_size") != RECORD_SIZE:
        raise ValueError("unsupported manifest format")
    total = 0
    current_records: list[dict[str, object]] = []
    for shard in document.get("shards", []):
        shard_path = Path(shard["shard"])
        current = inspect_shard(shard_path)
        if current != shard:
            raise ValueError(f"shard identity changed: {shard_path}")
        current_records.append(current)
        total += int(current["positions"])
    validate_unique_payloads(current_records)
    if total != document.get("positions"):
        raise ValueError("manifest position total mismatch")
    print(f"verified manifest_sha256={actual_hash} positions={total}")


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    build_parser = subparsers.add_parser("build")
    build_parser.add_argument("--output", type=Path, required=True)
    build_parser.add_argument("shards", nargs="+", type=Path)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("manifest", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "build":
            build(args.output, args.shards)
        else:
            verify(args.manifest)
        return 0
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
