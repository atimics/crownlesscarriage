#!/usr/bin/env python3
"""Exercise real export and split, duplicate, and failure handling."""
import copy
import importlib.util
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("corpus", ROOT / "tools/build_gossip_corpus.py")
corpus = importlib.util.module_from_spec(spec)
spec.loader.exec_module(corpus)
BINARY = str(Path(sys.argv.pop(1)).resolve())


class CorpusTests(unittest.TestCase):
    def test_real_export_is_reproducible_and_grounded(self):
        command = [BINARY, "--seed", "2654435769", "--days", "30"]
        first = subprocess.run(command, capture_output=True, text=True, check=True)
        second = subprocess.run(command, capture_output=True, text=True, check=True)
        self.assertEqual(first.stdout, second.stdout)
        self.assertEqual(first.stderr, second.stderr)
        rows = [corpus.prepare_row(json.loads(line), 2654435769) for line in first.stdout.splitlines()]
        self.assertGreater(len(rows), 0)
        report = json.loads(first.stderr)
        self.assertEqual(report["rows"], len(rows))
        coverage = report["unsupported_kinds"]
        self.assertEqual(len({row["kind"] for row in coverage}), len(coverage))
        self.assertIn("PROPHECY DELIVERED", {row["kind"] for row in coverage})
        self.assertTrue(any(row["observations"] == 0 for row in coverage))
        self.assertEqual(sum(row["observations"] for row in coverage),
                         report["unsupported_observations"])
        for row in rows:
            self.assertIn("account", row["input"])
            self.assertNotIn("speaker_id", corpus.event_prefix(row))
            self.assertNotIn("source_character_id", corpus.event_prefix(row))
            self.assertTrue(row["provenance"]["event_id"])
            self.assertIn(row["input"]["variant"], (0, 1))

    def test_rejects_future_or_mismatched_context(self):
        result = subprocess.run([BINARY, "--seed", "2654435769", "--days", "2"],
                                capture_output=True, text=True, check=True)
        original = json.loads(result.stdout.splitlines()[0])
        for change in ("future", "topic", "multiline"):
            row = copy.deepcopy(original)
            if change == "future":
                row["events"][-1]["day"] = row["provenance"]["day"] + 1
            elif change == "topic":
                row["events"][-1]["event_id"] = "0"
            else:
                row["output"] += "\n- invented event"
            with self.assertRaises(ValueError):
                corpus.prepare_row(row, 2654435769)

    def test_compact_event_context(self):
        row = {"events": [
            {"text": "A bridge closed.", "confidence": 90, "retellings": 1},
            {"text": "Someone posted a notice.", "confidence": 20, "retellings": 5}]}
        self.assertEqual(corpus.event_prefix(row),
                         "- A bridge closed.\n- ? ~ Someone posted a notice.\n")
        row["events"][0]["text"] = "A bridge closed.\nForged speech"
        with self.assertRaises(ValueError):
            corpus.event_prefix(row)

    def test_numeric_arguments(self):
        for args in (["--seed", "-1"], ["--seed", "4294967296"], ["--days", "36501"],
                     ["--days", "2junk"], ["--seed"], ["--other", "3"]):
            result = subprocess.run([BINARY, *args], capture_output=True)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(result.stdout, b"")

    def test_world_split_and_balanced_selection(self):
        db = sqlite3.connect(":memory:")
        db.execute("CREATE TABLE examples (id TEXT PRIMARY KEY, split TEXT, kind TEXT, data TEXT)")
        for i in range(12):
            row = {"id": str(i), "kind": "rare" if i == 0 else "common"}
            db.execute("INSERT INTO examples VALUES (?, ?, ?, ?)",
                       (str(i), "train", row["kind"], json.dumps(row)))
        rows = corpus.select_rows(db, "train", 4, 3)
        self.assertEqual(len(rows), 4)
        self.assertIn("rare", {row["kind"] for row in rows})
        self.assertEqual(corpus.select_rows(db, "test", 4, 3), [])
        seeds = [(i * 0x9E3779B9) & 0xFFFFFFFF for i in range(1, 49)]
        sets = [{seed for seed in seeds if corpus.world_split(seed) == split} for split in corpus.SPLITS]
        self.assertTrue(all(sets))
        self.assertFalse(sets[0] & sets[1] or sets[0] & sets[2] or sets[1] & sets[2])
        db.close()

    def test_stream_deduplication_and_failure_cleanup(self):
        seeds = [next(seed for seed in range(1, 1000) if corpus.world_split(seed) == split)
                 for split in corpus.SPLITS]
        with tempfile.TemporaryDirectory() as tmp:
            fake = Path(tmp) / "exporter"
            fake.write_text('''#!/usr/bin/env python3
import json, sys
seed = int(sys.argv[sys.argv.index('--seed') + 1])
label = ''.join(chr(65+int(c)) for c in str(seed))
for account in ('Shared village', 'Village '+label):
    row = {'version':2, 'provenance':{'world_seed':seed,'event_id':'1','day':1},
           'input':{'kind':'SHORTAGE','account':account,'confidence':80,'retellings':1,'variant':0,'detail':'full'},
           'events':[{'event_id':'1','day':1,'text':account,'confidence':80,'retellings':1}],
           'output':account+' was short of food, I hear.', 'rule':'2:0'}
    print(json.dumps(row)); print(json.dumps(row))
print(json.dumps({'world_seed':seed,'rows':4}), file=sys.stderr)
''')
            fake.chmod(0o755)
            db = sqlite3.connect(":memory:")
            original_popen = subprocess.Popen
            def run_fake(command, **kwargs):
                if str(command[0]) == str(fake):
                    command = [sys.executable, str(fake), *command[1:]]
                return original_popen(command, **kwargs)
            with patch.object(corpus.subprocess, "Popen", side_effect=run_fake):
                counts = corpus.collect(fake, seeds, 1, db, [])
            self.assertEqual(counts["raw_rows"], 12)
            self.assertEqual(counts["unique_pairs"], 4)
            self.assertEqual(counts["cross_split_output_duplicates"], 4)
            self.assertEqual(counts["duplicate_pairs"], 4)
            outputs = [json.loads(row[0])["output"] for row in db.execute("SELECT data FROM examples")]
            self.assertEqual(len(outputs), len(set(outputs)))
            db.close()
            first_output = Path(tmp) / "first"
            args = SimpleNamespace(binary=fake, output=first_output, first_seed=1, seeds=48,
                                   days=1, max_examples=40, max_per_kind=40)
            with patch.object(corpus.subprocess, "Popen", side_effect=run_fake):
                corpus.build(args)
                args.output = Path(tmp) / "second"
                corpus.build(args)
            output_sets = []
            world_sets = []
            for split in corpus.SPLITS:
                name = split + ".audit.jsonl"
                self.assertEqual((first_output / name).read_bytes(), (args.output / name).read_bytes())
                rows = [json.loads(line) for line in (first_output / name).read_text().splitlines()]
                self.assertGreater(len(rows), 0)
                output_sets.append({row["output"] for row in rows})
                world_sets.append({row["provenance"]["world_seed"] for row in rows})
            for sets in (output_sets, world_sets):
                self.assertFalse(sets[0] & sets[1] or sets[0] & sets[2] or sets[1] & sets[2])
            self.assertEqual((first_output / "manifest.json").read_bytes(),
                             (args.output / "manifest.json").read_bytes())
            manifest = json.loads((first_output / "manifest.json").read_text())
            for split in corpus.SPLITS:
                self.assertEqual(manifest["splits"][split]["sha256"],
                                 corpus.file_hash(first_output / (split + ".txt")))
                plain = (first_output / (split + ".txt")).read_bytes()
                self.assertEqual(plain, (args.output / (split + ".txt")).read_bytes())
                self.assertNotIn(b"Context:", plain)
                self.assertNotIn(b"Speak in plain", plain)
                self.assertNotIn(b"{", plain)
                for record in map(json.loads, (first_output / (split + ".audit.jsonl")).read_text().splitlines()):
                    prefix = plain[record["text_start"]:record["output_start"]].decode()
                    self.assertEqual(prefix, corpus.event_prefix(record))
                    output = plain[record["output_start"]:record["text_start"]+record["text_bytes"]].decode()
                    self.assertEqual(output, record["output"] + "\n\n")
            self.assertEqual(manifest["editorial_test"]["rows"], 16)
            fake.write_text("#!/usr/bin/env python3\nraise SystemExit(1)\n")
            output = Path(tmp) / "dataset"
            args = SimpleNamespace(binary=fake, output=output, first_seed=1, seeds=48,
                                   days=1, max_examples=10, max_per_kind=10)
            with patch.object(corpus.subprocess, "Popen", side_effect=run_fake):
                with self.assertRaises(RuntimeError):
                    corpus.build(args)
            self.assertFalse(output.exists())
            self.assertEqual(list(Path(tmp).glob(".gossip-corpus-*")), [])


if __name__ == "__main__":
    unittest.main()
