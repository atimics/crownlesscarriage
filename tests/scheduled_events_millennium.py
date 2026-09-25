"""Check millennial replay against the parent simulation's saved state hashes."""

import csv
import io
import subprocess
import sys


EXPECTED_HASHES = {
    1: "1217683238109513077",
    2: "474392472725602783",
    3: "17551759968800637748",
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
