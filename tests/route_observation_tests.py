import csv
import io
import pathlib
import subprocess
import sys
import tempfile
binary = sys.argv[1]
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory) / 'routes.csv'
    args = [binary, '--seed', '1', '--years', '2']
    ordinary = subprocess.check_output(args)
    measured = subprocess.check_output(args + ['--route-csv', str(path)])
    assert ordinary == measured
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    routes = {row['route_id'] for row in rows}
    assert len(rows) == 2 * len(routes) and routes
    previous = {}
    fields = ('closed_days', 'war_border_days', 'unavailable_inhabited_days', 'unavailable_ruin_days')
    for row in rows:
        assert None not in row and all(value is not None for value in row.values())
        assert int(row['interval_days']) == 365
        assert int(row['cumulative_sampled_days']) == int(row['year']) * 365
        for field in fields:
            value = int(row[field]); cumulative = int(row['cumulative_' + field])
            assert 0 <= value <= 365
            key = row['route_id'], field
            assert cumulative == previous.get(key, 0) + value
            previous[key] = cumulative
        assert int(row['unavailable_inhabited_days']) + int(row['unavailable_ruin_days']) <= 365
        assert int(row['current_outage_days']) <= int(row['longest_outage_days']) <= int(row['day']) - 1
    subprocess.check_output(args + ['--final-only', '--route-csv', str(path)])
    with path.open() as stream:
        assert list(csv.DictReader(stream)) == [row for row in rows if row['year'] == '2']
    path.write_text('preserve')
    failed = subprocess.run(args + ['--route-csv', str(path), '--nutrition-csv', str(path)], capture_output=True)
    assert failed.returncode != 0 and path.read_text() == 'preserve'
    failed = subprocess.run(args + ['--route-csv', str(path / 'missing')], capture_output=True)
    assert failed.returncode != 0
