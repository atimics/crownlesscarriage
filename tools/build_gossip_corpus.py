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


def event_prefix(row):
    events = row.get("events")
    if events is None:
        context = row["input"]
        events = [{"text": context["account"], "confidence": context["confidence"],
                   "retellings": context["retellings"]}]
    if not 1 <= len(events) <= 3:
        raise ValueError("Expected one to three held events")
    lines = []
    for event in events:
        text = event["text"]
        if not text or "\n" in text or "\r" in text:
            raise ValueError("Each event must be one line")
        # Two compact evidence cues: uncertain and widely retold.
        cue = ("? " if event["confidence"] < 40 else "") + ("~ " if event["retellings"] >= 4 else "")
        lines.append("- " + cue + text)
    return "\n".join(lines) + "\n"


def prepare_row(row, seed):
    if row["version"] != 2 or row["provenance"]["world_seed"] != seed:
        raise ValueError("Exporter version or world seed mismatch")
    context = row["input"]
    if not context["account"] or not row["output"] or any(c.isdigit() for c in row["output"]):
        raise ValueError("Expected a held account and quantity-free speech")
    if context["detail"] not in ("full", "actor", "subject"):
        raise ValueError("Expected the chosen level of detail")
    if context["detail"] != "full" and (context["confidence"] >= 40 or
            context["kind"] not in ("NOTICE", "WAR DECLARED", "PEACE")):
        raise ValueError("Generalised details require a supported low-confidence account")
    if any(marker in row["output"] for marker in (
            "I am unsure of this account:", "This account has passed through several people:",
            "The account I heard says:", "This is what I was told:")):
        raise ValueError("Use conversational wording for uncertainty and circulation")
    prefix = event_prefix(row)
    if "\n" in row["output"] or "\r" in row["output"]:
        raise ValueError("Speech must fit on one line")
    events = row["events"]
    if events[-1]["event_id"] != row["provenance"]["event_id"]:
        raise ValueError("The final event must match the spoken account")
    if any(event["day"] > events[-1]["day"] for event in events) or events[-1]["day"] > row["provenance"]["day"]:
        raise ValueError("Events must have occurred by the observation day")
    if any(int(a["event_id"]) >= int(b["event_id"]) for a, b in zip(events, events[1:])):
        raise ValueError("Held events must follow event order")
    row["id"] = digest(encode([prefix, row["output"]]).encode())
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
    path = directory / f"{split}.txt"
    audit = directory / f"{split}.audit.jsonl"
    with path.open("wb") as text_stream, audit.open("w") as audit_stream:
        for row in rows:
            prefix = event_prefix(row).encode("utf-8")
            sample = prefix + row["output"].encode("utf-8") + b"\n\n"
            record = dict(row)
            record["text_start"] = text_stream.tell()
            record["output_start"] = text_stream.tell() + len(prefix)
            record["text_bytes"] = len(sample)
            text_stream.write(sample)
            audit_stream.write(encode(record) + "\n")
    return {"rows": len(rows), "sha256": file_hash(path), "audit_sha256": file_hash(audit),
            "bytes": path.stat().st_size,
            "output_words": sum(len(row["output"].split()) for row in rows),
            "unique_outputs": len({row["output"] for row in rows}),
            "world_seeds": sorted({row["provenance"]["world_seed"] for row in rows if "provenance" in row}),
            "event_kinds": dict(sorted(Counter(row["input"]["kind"] for row in rows).items())),
            "rules": dict(sorted(Counter(row.get("rule", "editorial") for row in rows).items()))}


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
        editorial_rows = [json.loads(line) for line in editorial_source.read_text().splitlines()]
        editorial_report = write_split(staging, "editorial_test", editorial_rows)
        manifest = {"version": 2, "generator": "crownless-personal-gossip",
                    "language": "plain English", "training_format": "event lines followed by speech", "source": source,
                    "binary_sha256": binary_hash,
                    "settings": {key: getattr(args, key) for key in
                                 ("first_seed", "seeds", "days", "max_examples", "max_per_kind")},
                    "worlds": logs, "counts": dict(counts), "splits": reports,
                    "requested_examples": args.max_examples,
                    "written_examples": sum(report["rows"] for report in reports.values()),
                    "editorial_test": {**editorial_report,
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
