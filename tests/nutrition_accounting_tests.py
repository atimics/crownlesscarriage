import csv
import pathlib
import subprocess
import sys
import tempfile

binary = sys.argv[1]
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory) / 'nutrition.csv'
    args = [binary, '--seed', '1', '--years', '2']
    ordinary = subprocess.check_output(args)
    measured = subprocess.check_output(args + ['--nutrition-csv', str(path)])
    assert ordinary == measured, 'measurement changed the ordinary metrics'
    with path.open() as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 2 * 6 * 3
    previous = {}
    for row in rows:
        key = (row['settlement_id'], row['good'])
        assert row['good'] in ('Bread', 'Wheat', 'Meat')
        for field in ('aged_units', 'overflow_units', 'civilian_units', 'wasted_nutrition'):
            sampled = int(row[field])
            cumulative = int(row['cumulative_' + field])
            assert sampled >= 0
            assert cumulative == previous.get((key, field), 0) + sampled
            previous[(key, field)] = cumulative
        value = 1 if row['good'] == 'Wheat' else 2
        assert int(row['wasted_nutrition']) == value * (int(row['aged_units']) + int(row['overflow_units']))
    assert any(int(row['overflow_units']) > 0 for row in rows)
    assert any(int(row['aged_units']) > 0 for row in rows)
    subprocess.check_output(args + ['--final-only', '--nutrition-csv', str(path)])
    with path.open() as stream:
        assert list(csv.DictReader(stream)) == rows[18:]
    failed = subprocess.run(args + ['--nutrition-csv', str(path / 'missing')], capture_output=True)
    assert failed.returncode != 0
