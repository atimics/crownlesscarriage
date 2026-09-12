"""Snapshot generated tests and target settings; source backtraces are excluded."""
import json
from pathlib import Path
import re
import sys

def clean(value):
    if isinstance(value,dict):
        return {k:(sorted((clean(item) for item in v), key=lambda item:item['id'])
                   if k=='dependencies' else clean(v))
                for k,v in value.items() if k not in ('backtrace','backtraces','backtraceGraph')}
    if isinstance(value,list):return [clean(v) for v in value]
    return value
build=Path(sys.argv[1]).resolve()
reply=build/'.cmake/api/v1/reply'
index=json.loads(sorted(reply.glob('index-*.json'))[-1].read_text())
model=json.loads((reply/index['reply']['codemodel-v2']['jsonFile']).read_text())
targets={}
for config in model['configurations']:
    for target in config['targets']:
        targets[target['name']]=clean(json.loads((reply/target['jsonFile']).read_text()))
tests=build/'CTestTestfile.cmake'
text=tests.read_text() if tests.exists() else ''
text='\n'.join(line for line in text.splitlines() if not line.startswith('#'))
text=re.sub(r'\s*_BACKTRACE_TRIPLES "[^"]*"','',text)
Path(sys.argv[2]).write_text(json.dumps({'targets':targets,'tests':text},sort_keys=True,indent=2)+'\n')
print(len(targets), 'targets;',len(re.findall(r'(?m)^add_test\(',text)), 'tests')
