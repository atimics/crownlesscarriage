"""Check physical graph connectivity independently of settlement storage order."""
import json
import subprocess
import sys
rows = [json.loads(line) for line in subprocess.check_output([sys.argv[1]], text=True).splitlines()]
assert len(rows) == 8
for row in rows:
    assert row['semantics'] == 'snapshot' and row['edge_rule'] == 'route_closed_false'
    assert row['direction'] == 'undirected' and row['ruins'] == 'included_as_nodes'
    assert row['inhabited_settlements'] == sum(town['inhabited'] for town in row['settlements'])
    for town in row['settlements']:
        group = [other for other in row['settlements'] if other['component_id'] == town['component_id']]
        assert town['component_id'] == str(min(int(other['id']) for other in group))
        assert town['component_inhabited_settlements'] == sum(other['inhabited'] for other in group)
assert rows[0] == rows[1]
assert all(town['component_inhabited_settlements'] == 4 for town in rows[0]['settlements'])
a, b, c, d = rows[2]['settlements']
assert a['component_id'] == d['component_id']
assert b['component_id'] == c['component_id'] != a['component_id']
assert all(town['component_inhabited_settlements'] == 2 for town in rows[2]['settlements'])
a, b, c, d = rows[3]['settlements']
assert a['component_id'] == b['component_id'] == c['component_id'] != d['component_id']
assert a['component_inhabited_settlements'] == 2 and not b['inhabited']
assert rows[4]['inhabited_settlements'] == 0 and rows[5]['inhabited_settlements'] == 1
assert sorted(rows[5]['settlements'], key=lambda x: x['id']) == sorted(rows[6]['settlements'], key=lambda x: x['id'])
assert rows[7]['settlements'] == [] and rows[7]['inhabited_settlements'] == 0
print('Verified alternate paths, disconnected groups, ruin transit, empty worlds, stable IDs, and state preservation')
