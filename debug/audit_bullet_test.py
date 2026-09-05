#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "datagen" / "audit_bullet.py"
SPEC = importlib.util.spec_from_file_location("audit_bullet", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


def record(score: int) -> bytes:
    occupancy = (1 << 4) | (1 << 8) | (1 << 60)
    packed = bytearray(16)
    for index, code in enumerate((5, 0, 13)):
        packed[index // 2] |= code << (4 * (index % 2))
    return struct.pack("<Q16shBBB3s", occupancy, packed, score, 1, 4, 4, bytes(3))


class AuditBulletTest(unittest.TestCase):
    def test_bounds_sample_per_file(self) -> None:
        with tempfile.TemporaryDirectory(prefix="shayveri_audit_test_") as directory:
            root = Path(directory)
            first = root / "first.bullet.bin"
            second = root / "second.bullet.bin"
            first.write_bytes(record(1) + record(2) + record(3))
            second.write_bytes(record(4) + record(5) + record(6))

            report = AUDIT.audit([first, second], max_records_per_file=2)

            self.assertEqual(report["files"], 2)
            self.assertEqual(report["records"], 4)
            self.assertEqual(report["max_records_per_file"], 2)


if __name__ == "__main__":
    unittest.main()
