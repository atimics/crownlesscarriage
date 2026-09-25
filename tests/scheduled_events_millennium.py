"""Check millennial replay against the parent simulation's saved state hashes."""

import csv
import io
import subprocess
import sys


EXPECTED_HASHES = {
    1: "1545048496086583334",
    2: "914304945445556228",
    3: "7099700105504748256",
}


def main() -> int:
    binary = sys.argv[1]
    for seed, expected in EXPECTED_HASHES.items():
        output = subprocess.check_output(
            [binary, "--seed", str(seed), "--years", "1000", "--final-only"],
            text=True,
        )
        row = next(csv.DictReader(io.StringIO(output)))
        actual = row["state_hash"]
        if actual != expected:
            print(f"seed {seed}: expected {expected}, got {actual}", file=sys.stderr)
            return 1
        print(f"seed {seed}: {actual}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
