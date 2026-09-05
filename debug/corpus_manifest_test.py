#!/usr/bin/env python3

import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "datagen" / "corpus_manifest.py"


class CorpusManifestTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="shayveri_manifest_test_")
        self.root = Path(self.temp.name)
        self.ready = self.root / "ready"
        self.ready.mkdir()

    def tearDown(self) -> None:
        self.temp.cleanup()

    def shard(
        self, name: str = "shard_000001", parent: Path | None = None, fill: int = 0
    ) -> Path:
        shard = (parent or self.ready) / name
        shard.mkdir()
        (shard / "DONE").touch()
        (shard / "data.DONE").write_text("positions 2\ngames 1\n")
        (shard / "data.summary.txt").write_text("positions 2\n")
        (shard / "metadata.env").write_text("source=shayveri-v3\n")
        (shard / "data_0.bullet.bin").write_bytes(bytes([fill]) * 64)
        checks = []
        for filename in ("metadata.env", "data.DONE", "data.summary.txt", "data_0.bullet.bin"):
            digest = hashlib.sha256((shard / filename).read_bytes()).hexdigest()
            checksum_name = f"./{filename}" if filename.endswith(".bullet.bin") else filename
            checks.append(f"{digest}  {checksum_name}\n")
        (shard / "SHA256SUMS").write_text("".join(checks))
        return shard

    def run_script(self, *args: object) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SCRIPT), *(str(arg) for arg in args)],
            text=True, capture_output=True, check=False,
        )

    def test_build_and_verify_complete_ready_shard(self) -> None:
        shard = self.shard()
        manifest = self.root / "manifest.json"
        self.assertEqual(self.run_script("build", "--output", manifest, shard).returncode, 0)
        self.assertEqual(self.run_script("verify", manifest).returncode, 0)

    def test_accepts_completed_transferred_shard(self) -> None:
        transferred = self.root / "bullet"
        transferred.mkdir()
        shard = self.shard(parent=transferred)
        manifest = self.root / "transferred.json"
        self.assertEqual(self.run_script("build", "--output", manifest, shard).returncode, 0)
        self.assertEqual(self.run_script("verify", manifest).returncode, 0)

    def test_rejects_incomplete_shard(self) -> None:
        shard = self.shard()
        (shard / "data.DONE").unlink()
        result = self.run_script("build", "--output", self.root / "manifest.json", shard)
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_non_record_aligned_bullet_file(self) -> None:
        shard = self.shard()
        (shard / "data_0.bullet.bin").write_bytes(bytes(63))
        result = self.run_script("build", "--output", self.root / "manifest.json", shard)
        self.assertNotEqual(result.returncode, 0)

    def test_detects_mutation_after_manifest_freeze(self) -> None:
        shard = self.shard()
        manifest = self.root / "manifest.json"
        self.assertEqual(self.run_script("build", "--output", manifest, shard).returncode, 0)
        (shard / "data_0.bullet.bin").write_bytes(bytes(32) + bytes([1]) * 32)
        result = self.run_script("verify", manifest)
        self.assertNotEqual(result.returncode, 0)

    def test_rejects_duplicate_shard_payload(self) -> None:
        first = self.shard("shard_000001")
        second = self.shard("shard_000002")
        result = self.run_script("build", "--output", self.root / "manifest.json", first, second)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate shard payload", result.stderr)

    def test_rejects_duplicate_bullet_file_across_distinct_shards(self) -> None:
        first = self.shard("shard_000001", fill=1)
        second = self.shard("shard_000002", fill=2)
        duplicate = second / "data_1.bullet.bin"
        duplicate.write_bytes((first / "data_0.bullet.bin").read_bytes())
        (second / "data.DONE").write_text("positions 4\ngames 1\n")
        (second / "data.summary.txt").write_text("positions 4\n")
        checks = []
        for filename in (
            "metadata.env", "data.DONE", "data.summary.txt",
            "data_0.bullet.bin", "data_1.bullet.bin",
        ):
            digest = hashlib.sha256((second / filename).read_bytes()).hexdigest()
            checksum_name = f"./{filename}" if filename.endswith(".bullet.bin") else filename
            checks.append(f"{digest}  {checksum_name}\n")
        (second / "SHA256SUMS").write_text("".join(checks))

        result = self.run_script("build", "--output", self.root / "manifest.json", first, second)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("duplicate Bullet file", result.stderr)


if __name__ == "__main__":
    unittest.main()
