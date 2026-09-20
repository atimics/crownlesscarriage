"""Focused checks for the typed v3 meaning to language boundary."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "dialogue"))
from meaning_language import LanguagePackError, render


def act(intent="offer_food", **extra):
    value = {
        "version": 3, "intent": intent, "actor": "a1", "recipient": "b1",
        "actor_name": "Mara 7", "recipient_name": "Kesh-2",
        "proposal": {"quantity": 2, "unit_price": 3, "total_cost": 6,
                     "condition": "daylight", "place_name": "Vault 9"},
    }
    value.update(extra)
    return value


class MeaningLanguageTests(unittest.TestCase):
    def test_names_and_numbers_survive_both_languages(self):
        english = render(act(), "human")
        goblin = render(act(), "hrakhor")
        for token in ("Vault 9", "2", "6"):
            self.assertIn(token, english)
            self.assertIn(token, goblin)
        self.assertIn("food", english)
        self.assertIn("portions of zhek", goblin)

    def test_quantity_changes_inflection(self):
        one = act(proposal={"quantity": 1, "unit_price": 3, "total_cost": 3,
                            "condition": "now", "place_name": "Vault 9"})
        many = act()
        self.assertIn("zhek", render(one, "hrakhor"))
        self.assertNotIn("portions of zhek", render(one, "hrakhor"))
        self.assertIn("portions of zhek", render(many, "hrakhor"))

    def test_shared_borrowed_words_and_vault_concept(self):
        proposal = {"quantity": 1, "unit_price": 1, "total_cost": 1,
                    "condition": "daylight", "place_name": "Vault 9"}
        line = render(act("condition", proposal=proposal), "hrakhor")
        self.assertIn("daylight", line)
        self.assertIn("1", line)

    def test_custom_pack_can_change_clause_order(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pack = json.loads((ROOT / "assets/worldpacks/languages/v3/human.json").read_text())
            pack["id"] = "test-order"
            pack["templates"]["offer_food"] = "{total_cost} total: {recipient_name} receives {quantity} {food} from {actor_name}."
            (root / "test-order.json").write_text(json.dumps(pack))
            self.assertEqual(render(act(), "test-order", root),
                             "6 total: Kesh-2 receives 2 portions of food from Mara 7.")

    def test_pack_errors_and_schema_errors_are_explicit(self):
        with self.assertRaises(LanguagePackError):
            render(act(), "missing")
        with self.assertRaises(LanguagePackError):
            render(act("offer_food", proposal={"quantity": 1}), "human")
        with self.assertRaises(LanguagePackError):
            render(act("report_shortage", claim={"kind": "food_store", "place_name": "P", "stock": 1,
                                                   "target": 3, "source": "inferred"}), "human")


if __name__ == "__main__":
    unittest.main()
