#!/usr/bin/env python3
"""Tests for participant owned event facts."""

import copy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "dialogue"))
from event_facts import (EVENT_KIND_REGISTRY, build_facts, render_fact_act,
                         validate_act, validate_event_registry)


def participant(*accounts):
    return {"self": {"id": "17"}, "day": 8, "held_accounts": list(accounts),
            "knowledge": [{"event_text": "foreign state"}]}


def account(**changes):
    value = {"event_id": "900", "source_id": "22", "day": 8, "kind": 138,
             "confidence": 61, "retellings": 2, "private": False,
             "parser_supported": False, "account": "A purse was lifted."}
    value.update(changes)
    return value


def with_knowledge(value, **changes):
    result = copy.deepcopy(value)
    result["knowledge"] = [{"event_id": value["held_accounts"][0]["event_id"],
                            "event_text": value["held_accounts"][0]['account'], **changes}]
    return result


class EventFactsTests(unittest.TestCase):
    def test_registry_has_every_sim_kind(self):
        receipt = validate_event_registry()
        self.assertEqual(receipt["event_count"], 139)
        self.assertEqual(sorted(v for v in EVENT_KIND_REGISTRY.values()), list(range(139)))

    def test_facts_use_held_accounts_and_keep_fields(self):
        fact = build_facts(with_knowledge(participant(account(kind=0, confidence=73)), certainty=2, private=True))[0]
        self.assertEqual(fact.kind, 0)
        self.assertEqual(fact.text, "A purse was lifted.")
        self.assertEqual(fact.source_id, "22")
        self.assertEqual(fact.certainty, "told")
        self.assertEqual(fact.certainty_value, 2)
        self.assertEqual(fact.confidence, 73)
        self.assertEqual(fact.day, 8)
        self.assertTrue(fact.private)
        self.assertTrue(fact.account_ref.startswith("fact:"))
        self.assertEqual(len(fact.account_digest), 64)

    def test_reference_is_stable_and_owner_bound(self):
        first = build_facts(participant(account(source_id="0")))[0]
        second = build_facts(participant(account(source_id="0")))[0]
        self.assertEqual(first.account_ref, second.account_ref)
        altered = build_facts(participant(account(account="A different account.")))[0]
        self.assertNotEqual(first.account_ref, altered.account_ref)
        with self.assertRaisesRegex(ValueError, "owned"):
            validate_act({"kind": "report", "fact_ref": "fact:foreign"}, participant(account()))

    def test_private_facts_cannot_be_disclosed(self):
        current = with_knowledge(participant(account()), certainty=1, private=True)
        fact = build_facts(current)[0]
        with self.assertRaisesRegex(ValueError, "private"):
            render_fact_act({"kind": "report", "fact_ref": fact.account_ref}, current, "18")
        self.assertIn("A purse was lifted.", render_fact_act(
            {"kind": "report", "fact_ref": fact.account_ref}, current, "17"))

    def test_act_kind_and_fields_are_checked(self):
        current = participant(account(parser_supported=True))
        fact = build_facts(current)[0]
        with self.assertRaisesRegex(ValueError, "report, ask or warn"):
            validate_act({"kind": "say", "fact_ref": fact.account_ref}, current)
        self.assertIn("day 8", render_fact_act(
            {"kind": "warn", "fact_ref": fact.account_ref}, current))
        self.assertIn("Do you know", render_fact_act(
            {"kind": "ask", "fact_ref": fact.account_ref}, current))

    def test_missing_certainty_stays_unknown(self):
        value = account()
        del value["confidence"]
        current = participant(value)
        fact = build_facts(current)[0]
        self.assertIsNone(fact.certainty)
        self.assertIn("I have heard", render_fact_act(
            {"kind": "report", "fact_ref": fact.account_ref}, current))

    def test_snapshot_bounds_and_knowledge_certainty(self):
        current = with_knowledge(participant(account(source_id="0")),
                                 certainty=3, private=False)
        fact = build_facts(current)[0]
        self.assertEqual(fact.source_id, "unknown")
        self.assertEqual(fact.certainty, "witnessed")
        with self.assertRaisesRegex(ValueError, 'future day'):
            build_facts(participant(account(day=9)))
        with self.assertRaisesRegex(ValueError, "printable"):
            build_facts(participant(account(account="bad\ntext")))
        with self.assertRaisesRegex(ValueError, "under 144"):
            build_facts(participant(account(account="x" * 144)))

    def test_stale_fact_reference_is_rejected_from_current_snapshot(self):
        old = participant(account(event_id="901"))
        ref = build_facts(old)[0].account_ref
        current = participant(account(event_id="902"))
        with self.assertRaisesRegex(ValueError, "owned"):
            validate_act({"kind": "report", "fact_ref": ref}, current)

    def test_certainty_does_not_certify_changed_retelling(self):
        own = with_knowledge(participant(account()), certainty=3,
                             event_text='A different event account.')
        self.assertIsNone(build_facts(own)[0].certainty)

    def test_foreign_owner_and_private_account(self):
        own = participant(account(private=True))
        other = copy.deepcopy(own)
        other['self']['id'] = '18'
        ref = build_facts(other)[0].account_ref
        with self.assertRaisesRegex(ValueError, 'owned'):
            validate_act({'kind': 'report', 'fact_ref': ref}, own)
        own = with_knowledge(own, certainty=2, private=False)
        fact = build_facts(own)[0]
        self.assertTrue(fact.private)
        with self.assertRaisesRegex(ValueError, 'private'):
            validate_act({'kind': 'report', 'fact_ref': fact.account_ref}, own)

    def test_saved_raid_account(self):
        # Exported from world 1201 on day 181, participant Chenric.
        own = {'self': {'id': '1369094286720630865'}, 'day': 181, 'knowledge': [],
               'held_accounts': [{'event_id': '648518346341352652', 'source_id': '0',
                                  'day': 172, 'kind': 51, 'confidence': 93, 'retellings': 1,
                                  'parser_supported': True,
                                  'account': 'The Cinder Tithe raids Thornford: 20 Wheat, 16 crowns.'}]}
        fact = build_facts(own)[0]
        self.assertEqual(fact.source_id, 'unknown')
        self.assertIsNone(fact.certainty)
        self.assertIn('20 Wheat, 16 crowns', render_fact_act(
            {'kind': 'report', 'fact_ref': fact.account_ref}, own))

    def test_malformed_and_foreign_fields_are_rejected(self):
        for changes, message in (({"kind": 139}, "unknown event kind"),
                                 ({"day": -1}, "non-negative"),
                                 ({"confidence": 101}, "0 through 100")):
            with self.subTest(changes=changes), self.assertRaisesRegex(ValueError, message):
                build_facts(participant(account(**changes)))
        with self.assertRaisesRegex(ValueError, 'at most 32'):
            build_facts(participant(*(account(event_id=str(i+1)) for i in range(33))))


if __name__ == "__main__":
    unittest.main()
