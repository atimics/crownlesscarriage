"""Reconcile precise welfare with raw towns and check observation preserves the world."""
import csv
import io
from pathlib import Path
import subprocess
import sys
import tempfile

binary = sys.argv[1]


def run(*args):
    result = subprocess.check_output([binary, "--seed", "1", "--years", "3", *args], text=True)
    rows = list(csv.DictReader(io.StringIO(result)))
    assert all(None not in r and all(v is not None for v in r.values()) for r in rows)
    return rows


with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "towns.csv"
    annual = run()
    observed = run("--settlements-csv", str(path), "--trace-every-days", "28")
    assert observed == annual  # Includes the full simulation state hash.
    assert run("--final-only") == annual[-1:]
    towns = list(csv.DictReader(path.open()))
    days = sorted({int(r["elapsed_days"]) for r in towns})
    assert days == [0, *range(28, 1096, 28), 1095], days
    assert all(int(r["day"]) == int(r["elapsed_days"]) + 1 for r in towns)
    final = [r for r in towns if r["elapsed_days"] == "1095"]
    assert len({r["settlement_id"] for r in final}) == len(final) == 6
    living = [r for r in final if int(r["population"]) > 0]
    row = annual[-1]
    assert row["metrics_version"] == "2"
    assert int(row["active_settlements"]) == len(living)
    assert int(row["abandoned_settlements"]) == len(final) - len(living)
    population = sum(int(r["population"]) for r in living)
    assert int(row["total_population"]) == population
    for field in ("hunger", "prosperity", "security"):
        average = sum(int(r[field]) for r in living) / len(living)
        weighted = sum(int(r[field]) * int(r["population"]) for r in living) / population
        assert abs(float(row["inhabited_" + field]) - average) < 0.000001
        assert abs(float(row["weighted_" + field]) - weighted) < 0.000001
    assert int(row["years_population_weighted_hunger_40_plus"]) == sum(
        float(r["weighted_hunger"]) >= 40 for r in annual)
    assert int(row["years_without_population"]) == sum(int(r["total_population"]) == 0 for r in annual)
    # The final observation is emitted once when it is also an interval boundary.
    run("--final-only", "--settlements-csv", str(path), "--trace-every-days", "365")
    towns = list(csv.DictReader(path.open()))
    assert len(towns) == 4 * 6
    for args in (("--trace-every-days", "0"),
                 ("--settlements-csv", str(Path(directory) / "missing" / "x.csv")),
                 ("--settlements-csv", str(path), "--nutrition-csv", str(path))):
        assert subprocess.run([binary, *args], capture_output=True).returncode != 0
print("Welfare reconciles with towns; traces preserve annual rows and state hashes.")
