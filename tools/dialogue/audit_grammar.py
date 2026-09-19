"""Inventory simulation enums against the two current dialogue grammars."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from syntax import PLANS,TERMS
from semantic_ids import MOVES


def check_coverage(result):
    """Require an explicit account rule for every declared simulation event."""
    problems = []
    if not result['event_kinds']:
        problems.append('simulation event inventory is empty')
    for key, label in (('account_missing_events', 'events need account rules'),
                       ('unknown_rule_events', 'rules refer to unknown events')):
        if result[key]:
            problems.append(label + ': ' + ', '.join(result[key]))
    if problems:
        raise ValueError('; '.join(problems))


def inventory(root):
    header=root/'src/sim/cc_sim.h'
    text=header.read_text()
    def values(tag,prefix):
        body=re.search(r'typedef enum '+tag+r'\s*\{(.*?)\}\s*'+tag,text,re.S).group(1)
        body=re.sub(r'/\*.*?\*/|//[^\n]*','',body,flags=re.S)
        return [v for v in re.findall(r'\b('+prefix+r'[A-Z_][A-Z_0-9]*)\s*(?:=|,|$)',body)
                if not v.endswith('_COUNT') and not v.endswith('_NONE')]
    events=values('CcEventKind','CC_EVENT_')
    rules_path=root/'tools/data/core_account_rules.json'
    rules=json.loads(rules_path.read_text())['rules']
    covered=sorted({'CC_EVENT_'+r['kind'] for r in rules})
    return {'sources':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in (header,rules_path)},
            'event_kinds':events,'account_rules':len(rules),
            'account_covered_events':covered,'account_missing_events':sorted(set(events)-set(covered)),
            'unknown_rule_events':sorted(set(covered)-set(events)),
            'commands':values('CcCommandKind','CC_COMMAND_'),
            'personal_knowledge_kinds':values('CcKnowledgeKind','CC_KNOWLEDGE_'),
            'certainty':values('CcKnowledgeCertainty','CC_KNOWLEDGE_'),
            'memory_kinds':values('CcCharacterMemoryKind','CC_CHARACTER_MEMORY_'),
            'relationship_histories':values('CcRelationshipHistory','CC_RELATIONSHIP_HISTORY_'),
            'goals':values('CcCharacterGoal','CC_CHARACTER_GOAL_'),
            'activities':values('CcCharacterActivity','CC_CHARACTER_ACTIVITY_'),
            'dialogue_moves':list(MOVES),'topics':list(PLANS),'plans':list(TERMS),
            'conditions':list(TERMS.values()),
            'policy_input_fields':['goal','hungry_days > 0','stress >= 60','courage','coins','public acts'],
            'policy_concrete_event_claims':0,
            'scope':'Enum/template coverage is an inventory, not a measure of semantic understanding.'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--check',action='store_true',
                        help='Fail when any simulation event lacks an account rule')
    args=parser.parse_args()
    result=inventory(Path(__file__).resolve().parents[2])
    if args.output:
        args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'events':len(result['event_kinds']),'account_covered':len(result['account_covered_events']),
                      'account_missing':len(result['account_missing_events']),'commands':len(result['commands'])}))
    if args.check:
        try:
            check_coverage(result)
        except ValueError as error:
            parser.exit(1,str(error)+'\n')


if __name__=='__main__':main()
