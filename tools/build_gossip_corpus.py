#!/usr/bin/env python3
"""Build a checked core-language dataset from independent Crownless worlds."""
import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import tempfile

SPLITS = ("train", "validation", "test")


def encode(value):
    return json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(",", ":"))


def digest(data):
    return hashlib.sha256(data).hexdigest()


def file_hash(path):
    with Path(path).open("rb") as stream:
        checksum = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
        return checksum.hexdigest()


def world_split(seed):
    bucket = int(digest(f"crownless-core-v1:{seed}".encode())[:8], 16) % 100
    return "train" if bucket < 80 else "validation" if bucket < 90 else "test"


def prepare_row(row, seed):
    if row["version"] != 1 or row["provenance"]["world_seed"] != seed:
        raise ValueError("Exporter version or world seed mismatch")
    context = row["input"]
    if not context["account"] or not row["output"] or any(c.isdigit() for c in row["output"]):
        raise ValueError("Expected a held account and quantity-free speech")
    expected = ("I am unsure of this account: " if context["confidence"] < 40 else
                "This account has passed through several people: " if context["retellings"] >= 4 else
                "The account I heard says: " if context["variant"] == 0 else "This is what I was told: ")
    if not row["output"].startswith(expected):
        raise ValueError("Speech must preserve the account's uncertainty")
    row["id"] = digest(encode([context, row["output"]]).encode())
    row["prompt"] = ("Speak in plain Crownless English from this held account. "
                     "Express its uncertainty and keep quantities general.\nContext: " +
                     encode(context) + "\nSpeech:")
    return row


def collect(binary, seeds, days, database, logs):
    database.execute("CREATE TABLE examples (id TEXT PRIMARY KEY, split TEXT, kind TEXT, data TEXT)")
    database.execute("CREATE TABLE outputs (hash TEXT PRIMARY KEY, split TEXT)")
    counts = Counter()
    for seed in seeds:
        split = world_split(seed)
        # File-backed stderr keeps the pipe free while rows stream into SQLite.
        world_rows = 0
        with tempfile.TemporaryFile(mode="w+") as errors:
            process = subprocess.Popen([str(binary), "--seed", str(seed), "--days", str(days)],
                                       stdout=subprocess.PIPE, stderr=errors, text=True)
            try:
                for line in process.stdout:
                    row = prepare_row(json.loads(line), seed)
                    counts["raw_rows"] += 1
                    world_rows += 1
                    output_hash = digest(row["output"].encode())
                    owner = database.execute("SELECT split FROM outputs WHERE hash=?", (output_hash,)).fetchone()
                    if owner and owner[0] != split:
                        counts["cross_split_output_duplicates"] += 1
                        continue
                    database.execute("INSERT OR IGNORE INTO outputs VALUES (?, ?)", (output_hash, split))
                    cursor = database.execute("INSERT OR IGNORE INTO examples VALUES (?, ?, ?, ?)",
                                              (row["id"], split, row["input"]["kind"], encode(row)))
                    counts["unique_pairs" if cursor.rowcount else "duplicate_pairs"] += 1
                if process.wait() != 0:
                    errors.seek(0)
                    raise RuntimeError(f"World {seed} failed: {errors.read()}")
                errors.seek(0)
                report = json.loads(errors.read())
                if report["world_seed"] != seed or report["rows"] != world_rows:
                    raise ValueError("Exporter row count or report seed mismatch")
                logs.append(report)
            finally:
                process.stdout.close()
                if process.poll() is None:
                    process.kill()
                    process.wait()
        database.commit()
        print(f"World {seed}: {split}; {counts['unique_pairs']} unique pairs", flush=True)
    return counts


def select_rows(database, split, limit, per_kind):
    kinds = [row[0] for row in database.execute(
        "SELECT DISTINCT kind FROM examples WHERE split=? ORDER BY kind", (split,))]
    buckets = [deque(database.execute(
        "SELECT data FROM examples WHERE split=? AND kind=? ORDER BY id LIMIT ?",
        (split, kind, per_kind)).fetchall()) for kind in kinds]
    # Round-robin selection gives rare event families room in a bounded corpus.
    rows = []
    while len(rows) < limit and any(buckets):
        for bucket in buckets:
            if bucket and len(rows) < limit:
                rows.append(json.loads(bucket.popleft()[0]))
    return rows


def write_split(directory, split, rows):
    path = directory / f"{split}.jsonl"
    with path.open("w") as stream:
        for row in rows:
            stream.write(encode(row) + "\n")
    return {"rows": len(rows), "sha256": file_hash(path), "bytes": path.stat().st_size,
            "output_words": sum(len(row["output"].split()) for row in rows),
            "unique_outputs": len({row["output"] for row in rows}),
            "world_seeds": sorted({row["provenance"]["world_seed"] for row in rows}),
            "event_kinds": dict(sorted(Counter(row["input"]["kind"] for row in rows).items())),
            "rules": dict(sorted(Counter(row["rule"] for row in rows).items()))}


def source_record():
    root = Path(__file__).resolve().parents[1]
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    paths = sorted([*root.glob("src/**/*.c"), *root.glob("src/**/*.h"),
                    root / "tools/gossip_corpus.c", Path(__file__).resolve(), root / "CMakeLists.txt",
                    root / "tools/data/gossip_core_editorial.jsonl"])
    return {"commit": head, "files": {str(path.relative_to(root)): file_hash(path) for path in paths}}


def build(args):
    binary = Path(args.binary).resolve()
    output = Path(args.output).resolve()
    if output.exists():
        raise ValueError("Choose a fresh output directory")
    source = source_record()
    binary_hash = file_hash(binary)
    seeds = [((args.first_seed + i) * 0x9E3779B9) & 0xFFFFFFFF for i in range(args.seeds)]
    if set(map(world_split, seeds)) != set(SPLITS):
        raise ValueError("Choose more seeds to cover train, validation, and test worlds")
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=".gossip-corpus-", dir=output.parent))
    try:
        with sqlite3.connect(staging / "candidates.sqlite") as database:
            logs = []
            counts = collect(binary, seeds, args.days, database, logs)
            held_out = args.max_examples // 10
            limits = (args.max_examples - 2 * held_out, held_out, held_out)
            reports = {}
            for split, limit in zip(SPLITS, limits):
                rows = select_rows(database, split, limit, args.max_per_kind)
                if not rows:
                    raise ValueError(f"Empty {split} split; increase days or seeds")
                reports[split] = write_split(staging, split, rows)
        (staging / "candidates.sqlite").unlink()
        if source_record() != source or file_hash(binary) != binary_hash:
            raise ValueError("Source or exporter changed during collection; rerun with a stable build")
        editorial_source = Path(__file__).resolve().parent / "data/gossip_core_editorial.jsonl"
        shutil.copyfile(editorial_source, staging / "editorial_test.jsonl")
        manifest = {"version": 1, "generator": "crownless-personal-gossip",
                    "language": "plain English", "source": source,
                    "binary_sha256": binary_hash,
                    "settings": {key: getattr(args, key) for key in
                                 ("first_seed", "seeds", "days", "max_examples", "max_per_kind")},
                    "worlds": logs, "counts": dict(counts), "splits": reports,
                    "requested_examples": args.max_examples,
                    "written_examples": sum(report["rows"] for report in reports.values()),
                    "editorial_test": {"rows": len(editorial_source.read_text().splitlines()),
                                       "sha256": file_hash(editorial_source),
                                       "origin": "Separately authored challenge examples; human review pending"},
                    "evaluation_scope": "Simulation-world holdout using shared authored language rules",
                    "next_evaluation": "Add separately written test phrasing before assessing language generalisation"}
        (staging / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        staging.rename(output)
        print(json.dumps({name: report["rows"] for name, report in reports.items()}, sort_keys=True))
    finally:
        if staging.exists():
            shutil.rmtree(staging)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="out/build/core/crownless_gossip_corpus")
    parser.add_argument("--output", required=True)
    parser.add_argument("--first-seed", type=int, default=1)
    parser.add_argument("--seeds", type=int, default=48)
    parser.add_argument("--days", type=int, default=120)
    parser.add_argument("--max-examples", type=int, default=5000)
    parser.add_argument("--max-per-kind", type=int, default=1000)
    args = parser.parse_args()
    if not (1 <= args.first_seed <= 0xFFFFFFFF and 1 <= args.seeds <= 100000 and
            args.first_seed + args.seeds - 1 <= 0xFFFFFFFF and 1 <= args.days <= 36500 and
            args.max_examples >= 10 and args.max_per_kind >= 1):
        parser.error("Use valid positive counts, uint32 seed indices, and 1..36500 days")
    try:
        build(args)
    except (ValueError, KeyError, TypeError, RuntimeError, OSError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Corpus build failed: {error}\n")


if __name__ == "__main__":
    main()
