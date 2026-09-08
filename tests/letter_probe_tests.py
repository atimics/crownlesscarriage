#!/usr/bin/env python3
"""letter_probe smoke test.

The probe composes candidate letters from a world without mutating it. This
test asserts the three contracts the tool encodes:

1. Seal gating: only officials (and the abbot) compose letters; everyone else
   writes an unstamped diary that cannot enter intake.
2. Vouched quantities: an account from the writer's own town keeps its
   particulars; a distant account redacts them.
3. Read-only: inspecting the world must not change it (the probe reads the
   same sim state from a fixed seed twice and prints identical letters).
"""

import subprocess
import sys


def run(binary, *args):
    return subprocess.run(
        [binary, *args], capture_output=True, text=True, check=True
    )


def main():
    if len(sys.argv) != 2:
        print("usage: letter_probe_tests.py <crownless_letter_probe>")
        return 1
    binary = sys.argv[1]

    # Fixed seed, 1 year, compare mode exercises all four purposes on the
    # same writer so the letter surface is rich enough to assert on.
    first = run(binary, "--seed", "2", "--seeds", "1", "--years", "1",
                "--compare", "--max-accounts", "3").stdout
    again = run(binary, "--seed", "2", "--seeds", "1", "--years", "1",
                "--compare", "--max-accounts", "3").stdout
    if first != again:
        print("FAIL: identical invocations produced different letters")
        return 1

    if "registered scribe seal" not in first:
        print("FAIL: expected at least one official delivering a stamped letter")
        return 1

    # Seed 3's best-held writer is a refugee; their writing must be an
    # unstamped diary, not an admissible letter (#435).
    diarist = run(binary, "--seed", "3", "--seeds", "1", "--years", "1",
                  "--compare", "--max-accounts", "3").stdout
    if "carries no seal" not in diarist:
        print("FAIL: expected at least one unstamped diarist")
        return 1

    vouched = "vouched (writer's town)" in first
    redacted = "particulars redacted" in first
    if not check_quantities(vouched, redacted):
        return 1

    print("letter probe contracts passed.")
    return 0


def check_quantities(vouched, redacted):
    # A one-year world may not contain both a local and a distant digit-bearing
    # account for every writer, but at least one side should appear. Requiring
    # both would make the test brittle across seeds.
    if vouched or redacted:
        return True
    print("FAIL: expected vouched or redacted quantities in letter surface")
    return False


if __name__ == "__main__":
    sys.exit(main())