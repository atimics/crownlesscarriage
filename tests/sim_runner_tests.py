"""Check that an interrupted long run resumes with the same world state."""

from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import tempfile


def last_state(output):
    return re.findall(r"^day=(\d+) hash=([0-9a-f]+)", output, re.MULTILINE)[-1]


def main():
    runner = str(Path(sys.argv[1]).resolve())
    seed = "0x9e3779b9"
    with tempfile.TemporaryDirectory() as scratch:
        checkpoint = Path(scratch) / "world.ccsave"
        process = subprocess.Popen(
            [runner, "--seed", seed, "--years", "100000",
             "--checkpoint-every", "1", "--save", str(checkpoint)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        try:
            first_report = process.stdout.readline()
            assert first_report.startswith("day=366 "), first_report
            assert checkpoint.exists()
        finally:
            process.terminate()
            process.communicate(timeout=10)

        with sqlite3.connect(checkpoint) as database:
            day = database.execute(
                "SELECT current_day FROM meta WHERE id=1"
            ).fetchone()[0]
        assert day >= 366 and (day - 1) % 365 == 0, day
        resumed = subprocess.run(
            [runner, "--load", str(checkpoint), "--years", "2",
             "--checkpoint-every", "1", "--save", str(checkpoint)],
            capture_output=True, text=True, check=True,
        )
        direct = subprocess.run(
            [runner, "--seed", seed, "--years", str((day - 1) // 365 + 2)],
            capture_output=True, text=True, check=True,
        )
        assert last_state(resumed.stdout) == last_state(direct.stdout)

        missing = subprocess.run(
            [runner, "--load", str(Path(scratch) / "missing.ccsave")],
            capture_output=True, text=True,
        )
        assert missing.returncode != 0 and "load failed:" in missing.stderr
        invalid = subprocess.run(
            [runner, "--checkpoint-every", "1"], capture_output=True, text=True,
        )
        assert invalid.returncode != 0 and "requires --save" in invalid.stderr
    print("Interrupted simulation resumes with the same day and world hash.")


if __name__ == "__main__":
    main()
