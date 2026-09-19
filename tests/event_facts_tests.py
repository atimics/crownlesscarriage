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
    return {"self": {"id": "17"}, "held_accounts": list(accounts),
            "knowledge": [{"event_text": "foreign state"}]}


def account(**changes):
    value = {"event_id": "900", "source_id": "22", "day": 8, "kind": 138,
             "confidence": 61, "retellings": 2, "private": False,
             "parser_supported": False, "account": "A purse was lifted."}
    value.update(changes)
    return value


class EventFactsTests(unittest.TestCase):
    def test_registry_has_every_sim_kind(self):
        receipt = validate_event_registry()
        self.assertEqual(receipt["event_count"], 139)
        self.assertEqual(sorted(v for v in EVENT_KIND_REGISTRY.values()), list(range(139)))

    def test_facts_use_held_accounts_and_keep_fields(self):
        fact = build_facts(participant(account(kind=0, confidence=73, private=True)))[0]
        self.assertEqual(fact.kind, 0)
        self.assertEqual(fact.text, "A purse was lifted.")
        self.assertEqual(fact.source_id, "22")
        self.assertIsNone(fact.certainty)
        self.assertEqual(fact.confidence, 73)
        self.assertEqual(fact.day, 8)
        self.assertTrue(fact.private)
        self.assertTrue(fact.account_ref.startswith("fact:"))
        self.assertEqual(len(fact.account_digest), 64)

    def test_reference_is_stable_and_owner_bound(self):
        first = build_facts(participant(account()))[0]
        second = build_facts(participant(account()))[0]
        self.assertEqual(first.account_ref, second.account_ref)
        altered = build_facts(participant(account(account="A different account.")))[0]
        self.assertNotEqual(first.account_ref, altered.account_ref)
        with self.assertRaisesRegex(ValueError, "owned"):
            validate_act({"kind": "report", "fact_ref": "fact:foreign"}, [first])

    def test_private_facts_cannot_be_disclosed(self):
        fact = build_facts(participant(account(private=True)))[0]
        with self.assertRaisesRegex(ValueError, "private"):
            render_fact_act({"kind": "report", "fact_ref": fact.account_ref}, [fact], "18")
        self.assertIn("unparsed account", render_fact_act(
            {"kind": "report", "fact_ref": fact.account_ref}, [fact], "17"))

    def test_act_kind_and_fields_are_checked(self):
        fact = build_facts(participant(account(parser_supported=True)))[0]
        with self.assertRaisesRegex(ValueError, "report, ask or warn"):
            validate_act({"kind": "say", "fact_ref": fact.account_ref}, [fact])
        self.assertIn("day 8", render_fact_act(
            {"kind": "warn", "fact_ref": fact.account_ref}, [fact]))
        self.assertIn("Do you know", render_fact_act(
            {"kind": "ask", "fact_ref": fact.account_ref}, [fact]))

    def test_missing_certainty_stays_unknown(self):
        value = account()
        del value["confidence"]
        fact = build_facts(participant(value))[0]
        self.assertIsNone(fact.certainty)
        self.assertIn("I have heard", render_fact_act(
            {"kind": "report", "fact_ref": fact.account_ref}, [fact]))

    def test_malformed_and_foreign_fields_are_rejected(self):
        for changes, message in (({"kind": 139}, "unknown event kind"),
                                 ({"day": -1}, "non-negative"),
                                 ({"confidence": 101}, "0 through 100"),
                                 ({"source_id": "0"}, "stable")):
            with self.subTest(changes=changes), self.assertRaisesRegex(ValueError, message):
                build_facts(participant(account(**changes)))


if __name__ == "__main__":
    unittest.main()
