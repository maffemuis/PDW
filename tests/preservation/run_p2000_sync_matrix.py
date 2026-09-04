#!/usr/bin/env python3
"""Run end-to-end normal/inverted POCSAG sync error regressions."""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path


def run_case(root: Path, pdw: Path, case_name: str, expect_message: bool) -> None:
    out_dir = root / "preservation-local"
    out_dir.mkdir(parents=True, exist_ok=True)
    wav = out_dir / f"p2000-{case_name}.wav"
    full = out_dir / f"p2000-{case_name}.full.jsonl"
    actual = out_dir / f"p2000-{case_name}.actual.jsonl"

    subprocess.run(
        [sys.executable, str(root / "tests/preservation/generate_p2000_fixture.py"), str(wav), case_name],
        check=True,
        cwd=root,
    )
    full.write_bytes(b"")
    actual.write_bytes(b"")
    env = os.environ.copy()
    env["PDW_PRESERVATION_REPLAY_WAV"] = str(wav)
    env["PDW_PRESERVATION_CAPTURE"] = str(full)
    env["PDW_PRESERVATION_GOLDEN_CAPTURE"] = str(actual)
    env["PDW_PRESERVATION_REPLAY_EXIT"] = "1"
    completed = subprocess.run([str(pdw)], cwd=root, env=env, timeout=30)
    if completed.returncode != 0:
        raise RuntimeError(f"{case_name}: replay exit {completed.returncode}")

    rows = [json.loads(line) for line in actual.read_text(encoding="utf-8").splitlines() if line.strip()]
    if expect_message:
        if len(rows) != 1:
            raise AssertionError(f"{case_name}: expected one decoded message, got {len(rows)}")
        row = rows[0]
        expected = {
            "capcode": "0123456",
            "mode": "POCSAG-4",
            "type": " ALPHA ",
            "bitrate": "1200",
            "message": "BRANDWEER TEST",
        }
        for key, value in expected.items():
            if row.get(key) != value:
                raise AssertionError(f"{case_name}: {key}={row.get(key)!r}, expected {value!r}")
    elif rows:
        raise AssertionError(f"{case_name}: >=5-bit bad sync must fail closed, decoded {len(rows)} message(s)")


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} PDW.exe", file=sys.stderr)
        return 2
    root = Path(__file__).resolve().parents[2]
    pdw = Path(sys.argv[1]).resolve()
    matrix = []
    for inverted in (False, True):
        prefix = "sync-inverted-" if inverted else "sync-"
        for errors in (0, 1, 2, 4, 5):
            matrix.append((f"{prefix}{errors}", errors < 5))
    for case_name, expect_message in matrix:
        print(f"sync-matrix: {case_name} expect_message={expect_message}")
        run_case(root, pdw, case_name, expect_message)
    print("P2000 sync matrix passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
