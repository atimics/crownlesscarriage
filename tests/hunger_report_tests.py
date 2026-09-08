"""Compare the two real report tools at the same world checkpoint."""
import csv
import io
import subprocess
import sys

runner, metrics = sys.argv[1:]
summary = subprocess.check_output(
    [runner, "--seed", "2654435769", "--years", "1"], text=True
)
rows = list(csv.DictReader(io.StringIO(subprocess.check_output(
    [metrics, "--seeds", "1", "--years", "1"], text=True
))))
assert len(rows) == 1, rows
row = rows[0]
assert None not in row and all(v is not None for v in row.values()), row
fields = dict(word.split("=", 1) for word in summary.split() if "=" in word)
assert fields["day"] == "366", fields
assert row["world_seed"] == "2654435769" and row["year"] == "1", row
for key in ("average_hunger", "maximum_hunger", "population_weighted_hunger"):
    assert fields[key] == row[key], (key, fields[key], row[key])
assert fields["inhabited_settlements"] == row["active_settlements"]
assert fields["ruins"] == row["abandoned_settlements"]
print("Runner and metrics agree on inhabited hunger and ruins.")
