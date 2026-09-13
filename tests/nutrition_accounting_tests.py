import csv
import io
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
    extended = subprocess.check_output(args + ['--campaign-metrics', '--nutrition-csv', str(path)])
    basic_rows = list(csv.DictReader(io.StringIO(ordinary.decode())))
    extended_rows = list(csv.DictReader(io.StringIO(extended.decode())))
    assert len(basic_rows) == len(extended_rows)
    for basic, extra in zip(basic_rows, extended_rows):
        assert {key: extra[key] for key in basic} == basic
        expected_campaign_fields = {
            'live_treasures', 'live_treasure_value', 'newest_treasure_day',
            'oldest_treasure_day', 'treasures_from_ruins', 'treasures_in_ruins',
            'treasure_identity_hash', 'next_entity_serial', 'character_coins',
            'hungry_travellers', 'unsheltered_travellers', 'named_bandits',
        }
        assert set(extra) - set(basic) == expected_campaign_fields
        assert None not in extra, 'CSV row has more values than header fields'
        assert all(value is not None for value in extra.values())
        for field in ('character_coins', 'hungry_travellers',
                      'unsheltered_travellers', 'named_bandits'):
            assert int(extra[field]) >= 0, (field, extra[field])
        assert 0 <= int(extra['live_treasures']) <= int(extra['treasure_count'])
        assert int(extra['live_treasure_value']) >= 0
        assert int(extra['oldest_treasure_day']) <= int(extra['newest_treasure_day'])

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
