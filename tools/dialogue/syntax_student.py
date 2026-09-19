"""Run two independent views through a native syntax checkpoint."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess

from paired_participants import validate_snapshot
from syntax import FORMAT, render, validate
from syntax_training import prefix
from semantic_ids import encode_input, decode_act, pack_act


def run(snapshot, model, probe, output, surface_probe=None, turns=8, semantic=False):
    validate_snapshot(snapshot)
    if type(turns) is not int or not 1 <= turns <= 32:
        raise ValueError('turns must be 1..32')
    output.mkdir(parents=True, exist_ok=False)
    history=[]; attempts=[]
    record={'format':'crownless-semantic-ids-v1' if semantic else FORMAT,'snapshot':snapshot,'attempts':attempts,'status':'running',
            'model_sha256':hashlib.sha256(model.read_bytes()).hexdigest(),
            'probe_sha256':hashlib.sha256(probe.read_bytes()).hexdigest(),
            'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    try:
        for turn in range(turns):
            person=copy.deepcopy(snapshot['participants'][turn%2])
            heard=copy.deepcopy(history)
            ids=encode_input(person,heard) if semantic else None
            text=','.join(map(str,ids)) if semantic else prefix(person,heard)
            attempt={'turn':turn,'speaker_id':person['self']['id'],
                     'input':{'participant':person,'observed_acts':heard},'prefix':text}
            attempts.append(attempt)
            if semantic:
                attempt['prefix_ids']=ids
            try:
                result=subprocess.run([str(probe),str(model),'--semantic-prefix' if semantic else '--participant-prefix',text,'--generate'],
                                      capture_output=True,timeout=60)
            except subprocess.TimeoutExpired as error:
                attempt.update(stdout_hex=(error.stdout or b'').hex(),stderr_hex=(error.stderr or b'').hex())
                raise
            attempt.update(returncode=result.returncode,stdout_hex=result.stdout.hex(),stderr_hex=result.stderr.hex())
            if result.returncode:
                raise ValueError('native generation failed')
            value=decode_act(list(map(int,result.stdout.split()))) if semantic else json.loads(result.stdout)
            act=validate(value,person,heard)
            attempt['act']=act
            if semantic:
                attempt['binary_hex']=pack_act(act).hex()
            attempt['human']=render(act,variant=turn%2)
            if surface_probe:
                attempt['goblin']=render(act,'goblin',turn%2,surface_probe)
            history.append({'speaker_id':person['self']['id'],'act':copy.deepcopy(act)})
            if act['move']=='end':
                break
        record.update(status='complete',ended=history[-1]['act']['move']=='end')
    except BaseException as error:
        record.update(status='failed',error=repr(error))
        raise
    finally:
        (output/'result.json').write_text(json.dumps(record,indent=2)+'\n')
    return record


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('snapshot','model','probe','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--surface-probe',type=Path)
    parser.add_argument('--semantic-ids',action='store_true')
    args=parser.parse_args()
    result=run(json.loads(args.snapshot.read_text()),args.model.resolve(),args.probe.resolve(),
               args.output,args.surface_probe.resolve() if args.surface_probe else None,semantic=args.semantic_ids)
    print(json.dumps({'status':result['status'],'turns':len(result['attempts']),'ended':result['ended']}))


if __name__=='__main__':
    main()
