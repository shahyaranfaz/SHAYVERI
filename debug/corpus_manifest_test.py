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

    def shard(self, name: str = "shard_000001") -> Path:
        shard = self.ready / name
        shard.mkdir()
        (shard / "DONE").touch()
        (shard / "data.DONE").write_text("positions 2\ngames 1\n")
        (shard / "data.summary.txt").write_text("positions 2\n")
        (shard / "metadata.env").write_text("source=shayveri-v3\n")
        (shard / "data_0.bullet.bin").write_bytes(bytes(64))
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


if __name__ == "__main__":
    unittest.main()
