"""Protect the sampling boundary between observed knowledge and staged speech."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_speech_review import balanced, pairs, prefix


def held(person, day=3, place='7', event='8', confidence=80):
    return dict(type='held', person_id=str(person), day=day, place_id=place,
                event_id=event, kind=4, confidence=confidence, account='A held account.')


class SpeechReviewTests(unittest.TestCase):
    def test_pairs_require_two_people_here_with_this_event_today(self):
        first = held(1)
        for other in (held(1), held(2, day=4), held(2, place='9'), held(2, event='9')):
            self.assertEqual(pairs([first, other], 901), [])
        self.assertEqual(pairs([held(1, place='0'), held(2, place='0')], 901), [])
        result = pairs([first, held(2, confidence=20)], 901)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]['seed'], 901)
        self.assertEqual({p['person_id'] for p in result[0]['people']}, {'1', '2'})

    def test_balance_keeps_rare_cases_and_ignores_stream_order(self):
        rows = [dict(kind='trade', id=i) for i in range(100)] + [dict(kind='raid', id=100)]
        selected = balanced(rows, 4, lambda r: r['kind'])
        self.assertEqual(selected, balanced(reversed(rows), 4, lambda r: r['kind']))
        self.assertEqual(len(selected), 4)
        self.assertIn('raid', [r['kind'] for r in selected])

    def test_plain_pairs_keep_both_evidence_cues(self):
        account = dict(account='Someone posted a notice.', confidence=20, retellings=4)
        self.assertEqual(prefix(account), '- ? ~ Someone posted a notice.\n')
        self.assertEqual(prefix(dict(account, confidence=80, retellings=0)),
                         '- Someone posted a notice.\n')


if __name__ == '__main__':
    unittest.main()
