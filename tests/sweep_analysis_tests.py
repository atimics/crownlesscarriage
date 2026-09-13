import csv
import pathlib
import re
import subprocess
import sys
import tempfile

script = pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'analyze_sweep.py'
fields = ['active_settlements', 'average_hunger', 'maximum_hunger',
          'population_weighted_hunger', 'dragon_slain', 'average_prosperity',
          'average_legitimacy', 'total_population']
rows = [
    [2, 20, 30, 17, 1, 10, 10, 1000],
    [1, 81, 90, 81, 1, 10, 10, 500],
    [0, -1, -1, -1, 1, 0, 0, 0],
    [0, 100, 100, 100, 1, 0, 0, 0],
]
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory) / 'input.csv'
    def write(data):
        with path.open('w', newline='') as stream:
            writer = csv.writer(stream)
            writer.writerow(fields)
            writer.writerows(data)
    def run():
        return subprocess.run([sys.executable, str(script), str(path)],
                              capture_output=True, text=True, check=True).stdout
    write(rows)
    output = run()
    all_worlds = output.split('== all (4 worlds) ==')[1].split('\n\n')[0]
    average = next(line for line in all_worlds.splitlines() if 'average_hunger' in line)
    assert re.search(r'mean=\s*50\.50', average), average
    assert re.search(r'median=\s*50\.50', average), average
    assert 'observed=2 unavailable=2' in average
    weighted = next(line for line in all_worlds.splitlines() if 'population_weighted_hunger' in line)
    assert re.search(r'mean=\s*49\.00', weighted), weighted
    assert '== poor-no-dragon (1 worlds) ==' in output
    abandoned = output.split('== all-abandoned (2 worlds) ==')[1].split('\n\n')[0]
    for key in ('average_hunger', 'maximum_hunger', 'population_weighted_hunger'):
        line = next(line for line in abandoned.splitlines() if key in line)
        assert 'unavailable observed=0 unavailable=2' in line and 'mean=' not in line
    write(rows[2:])
    output = run()
    assert 'poor-no-dragon: no worlds' in output
    assert 'average_hunger' in output and 'unavailable observed=0 unavailable=2' in output
    write([])
    assert 'all: no worlds' in run()
    bad = rows[0].copy(); bad[1] = 101
    write([bad])
    failed = subprocess.run([sys.executable, str(script), str(path)], capture_output=True, text=True)
    assert failed.returncode != 0 and 'Invalid average_hunger' in failed.stderr
