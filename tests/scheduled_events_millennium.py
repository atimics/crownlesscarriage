"""Check millennial replay against the saved baseline for each schema."""

import csv
import io
import subprocess
import sys


EXPECTED_HASHES = {
    118: {1: "1545048496086583334", 2: "914304945445556228", 3: "7099700105504748256"},
    # Schema 119 adds raw stone inventory and mine production.
    119: {1: "7437858379406480744", 2: "12539320987051683624", 3: "13067450569132321010"},
    # Schema 120 adds a temporary cargo overflow allowance for legacy mine
    # save recovery.
    120: {
        1: "6606000563570832781",
        2: "1799973476309109639",
        3: "16512264284917628599",
    },
    # Schema 121 adds procedural personal item requests and belongings.
    121: {
        1: "11390053274053792359",
        2: "1918738345993467953",
        3: "9398023883865324237",
    },
}


def main() -> int:
    binary = sys.argv[1]
    for seed in (1, 2, 3):
        output = subprocess.check_output(
            [binary, "--seed", str(seed), "--years", "1000", "--final-only"],
            text=True,
        )
        row = next(csv.DictReader(io.StringIO(output)))
        schema = int(row["schema_version"])
        expected = EXPECTED_HASHES[schema][seed]
        actual = row["state_hash"]
        if actual != expected:
            print(f"seed {seed}: expected {expected}, got {actual}", file=sys.stderr)
            return 1
        print(f"seed {seed}: {actual}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
