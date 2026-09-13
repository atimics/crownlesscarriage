import csv
import io
import pathlib
import subprocess
import sys
import tempfile

runner, metrics = sys.argv[1:]

def run(binary, args):
    return subprocess.run([binary, *args], capture_output=True, text=True, timeout=15)

with tempfile.TemporaryDirectory() as directory:
    output = pathlib.Path(directory) / 'preserved-output'
    for args in [
        ['--years', '4294967296'], ['--years', '18446744073709551616'],
        ['--years', '1tail'], ['--years', '-1'], ['--seed', '4294967296'],
        ['--seed', '-1'], ['--seed', '0x100000000'], ['--seed', '12x'],
        ['--interval', '999999999999999999999'], ['--report-every', '1x'],
        ['--checkpoint-every', '4294967296'], ['--unknown'], ['--seed'],
    ]:
        output.write_text('keep me')
        result = run(runner, ['--save', str(output), *args])
        assert result.returncode != 0, (args, result.stdout)
        assert result.stderr, args
        assert output.read_text() == 'keep me', args
    for args in [
        ['--seed', '2147483647', '--seeds', '2', '--years', '1'],
        ['--seed', '1', '--years', '2147483647'],
    ]:
        output.write_text('keep me')
        result = run(metrics, ['--nutrition-csv', str(output), *args])
        assert result.returncode != 0, args
        assert result.stdout == '', args
        assert output.read_text() == 'keep me', args

    # The largest supported seed index still produces one real, validated year.
    result = run(metrics, ['--seed', '2147483647', '--years', '1', '--final-only'])
    assert result.returncode == 0, result.stderr
    rows = list(csv.DictReader(io.StringIO(result.stdout)))
    assert len(rows) == 1
    assert int(rows[0]['seed_number']) == 2147483647
    assert int(rows[0]['world_seed']) == (2147483647 * 0x9e3779b9) & 0xffffffff
    assert int(rows[0]['year']) == 1

    # Zero years and the largest raw world seed remain valid runner inputs.
    saved = pathlib.Path(directory) / 'world.sqlite'
    result = run(runner, ['--seed', '0xffffffff', '--years', '0', '--save', str(saved)])
    assert result.returncode == 0, result.stderr
    # The loaded world's identity belongs to the save, even with a different CLI seed.
    result = run(runner, ['--load', str(saved), '--seed', '123', '--years', '0', '--chronicle'])
    assert result.returncode == 0, result.stderr
    assert result.stdout.splitlines()[0] == '== world seed=4294967295 =='
