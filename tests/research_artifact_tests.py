#!/usr/bin/env python3
"""Exercise blob budgets and actual Git tree parsing."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from check_research_artifacts import Blob, audit, read_tree


class ArtifactTests(unittest.TestCase):
    def test_existing_large_blob_and_rename(self):
        old = Blob('assets/old.dat', 'old', 9000)
        new = Blob('docs/reviews/moved/old.dat', 'old', 9000)
        self.assertEqual(audit([old], [new], 100, 200)['new_bytes'], 0)

    def test_new_large_file(self):
        report = audit([], [Blob('docs/experiments/new/data.gz', 'a', 101)], 100, 200)
        self.assertEqual(len(report['errors']), 1)

    def test_unique_blobs_count_once(self):
        report = audit([], [Blob('docs/reviews/a/result', 'a', 100),
                            Blob('docs/reviews/b/result', 'a', 100)], 100, 100)
        self.assertEqual(report['new_bytes'], 100)
        self.assertEqual(len(report['files'][0]['paths']), 2)
        self.assertEqual(report['errors'], [])

    def test_total_budget_and_exact_boundary(self):
        files = [Blob('docs/reviews/a/one', 'a', 100), Blob('docs/reviews/a/two', 'b', 100)]
        self.assertEqual(audit([], files, 100, 200)['errors'], [])
        self.assertEqual(len(audit([], files, 100, 199)['errors']), 1)

    def test_replacing_large_file_counts_new_blob(self):
        before = [Blob('docs/reviews/a/data', 'a', 1000)]
        after = [Blob('docs/reviews/a/data', 'b', 900)]
        self.assertEqual(len(audit(before, after, 100, 200)['errors']), 2)

    def test_other_paths_and_deletions(self):
        before = [Blob('docs/experiments/old/data', 'a', 1000)]
        after = [Blob('docs/reviews-extra/data', 'b', 1000)]
        self.assertEqual(audit(before, after, 100, 200)['new_bytes'], 0)

    def test_git_tree_spaces_tabs_and_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', '-C', directory, *args], text=True).strip()
            git('init', '-q')
            git('config', 'user.email', 'test@example.invalid')
            git('config', 'user.name', 'Artifact test')
            git('config', 'commit.gpgsign', 'false')
            hooks = root / 'empty-hooks'; hooks.mkdir()
            git('config', 'core.hooksPath', str(hooks))
            (root / 'old').write_text('existing')
            git('add', '.'); git('commit', '-qm', 'base')
            base = git('rev-parse', 'HEAD')
            study = root / 'docs/reviews/test'; study.mkdir(parents=True)
            (root / 'old').rename(study / 'renamed file')
            (study / 'tab\tfile').write_bytes(b'x' * 11)
            git('add', '-A'); git('commit', '-qm', 'research')
            report = audit(read_tree(root, base), read_tree(root, 'HEAD'), 10, 20)
            self.assertEqual(report['new_bytes'], 11)
            self.assertIn('tab\tfile', report['files'][0]['paths'][0])
            command = [sys.executable, str(Path(__file__).resolve().parents[1] / 'tools/check_research_artifacts.py'),
                       '--repo', directory, '--base', base, '--max-file-bytes', '10', '--max-added-bytes', '20']
            self.assertEqual(subprocess.run(command, capture_output=True).returncode, 1)
            self.assertEqual(subprocess.run(command + ['--head', base], capture_output=True).returncode, 0)
            self.assertEqual(subprocess.run(command + ['--head', 'missing-revision'], capture_output=True).returncode, 2)
            self.assertEqual(subprocess.run(command + ['--max-added-bytes', '0'], capture_output=True).returncode, 2)


if __name__ == '__main__':
    unittest.main()
