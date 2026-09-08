"""Keep old hunger measurements and repeated annual rows out of endpoint charts."""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("plot_welfare", Path(__file__).resolve().parents[1] / "tools" / "plot_welfare.py")
plot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plot)


class WelfarePlotTests(unittest.TestCase):
    def test_old_metrics_and_duplicate_seeds_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "endpoints.csv"
            for rows in ([{"seed_number": "1", "year": "1000"}],
                         [{"seed_number": "1", "year": y, "metrics_version": "2"} for y in ("1", "2")],
                         [{"seed_number": "1", "year": "1000", "metrics_version": "2"}] * 2):
                with path.open("w", newline="") as stream:
                    writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                    writer.writeheader()
                    writer.writerows(rows)
                with self.assertRaises(ValueError):
                    plot.load_rows(path)

    def test_successor_remains_distinct_after_a_victory(self):
        self.assertEqual(plot.outcome({"dragon_stage": "1", "dragon_campaign_victories": "2"}), "Living successor")
        self.assertEqual(plot.outcome({"dragon_stage": "6", "dragon_campaign_victories": "2"}), "Slain at end")
        self.assertEqual(plot.outcome({"dragon_stage": "4", "dragon_campaign_victories": "0"}), "Original survives")


if __name__ == "__main__":
    unittest.main()
