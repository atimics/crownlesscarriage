"""Check semantic training output and native C parity before publication."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()
    test = json.loads((args.run / "test-evaluation.json").read_text())
    manifest = json.loads((args.run / "manifest.json").read_text())
    errors = []
    if manifest.get("status") != "complete":
        errors.append(f"training manifest status is {manifest.get('status')!r}")
    exported = hashlib.sha256((args.run / "last.ccv2").read_bytes()).hexdigest()
    if manifest.get("export_sha256") != exported:
        errors.append("export hash does not match manifest")
    records = test.get("records", [])
    if test.get("count") != len(records):
        errors.append("evaluation count does not match records")
    if manifest.get('datasets', {}).get('test', {}).get('rows') != len(records):
        errors.append('evaluation must cover the complete test dataset')
    if not records or test.get("exact") != test.get("count") or any(
        not (row.get("exact") and row.get("valid") and row.get("eos")) for row in records
    ):
        errors.append(f"exact policy gate failed: {test.get('exact')}/{test.get('count')}")
    if errors:
        raise SystemExit("; ".join(errors))
    checked = 0
    failures = []
    for number, row in enumerate(records):
        prefix = ",".join(str(value) for value in row["prefix_ids"])
        try:
            result = subprocess.run(
                [str(args.probe), str(args.run / "last.ccv2"), "--semantic-prefix", prefix, "--generate"],
                capture_output=True, text=True, timeout=args.timeout, check=False,
            )
        except subprocess.TimeoutExpired as error:
            failures.append({"row": number, "error": "probe timeout",
                             'stdout': repr(error.stdout), 'stderr': repr(error.stderr)})
            continue
        output = result.stdout.strip()
        try:
            native = [int(value) for value in output.split()] if output else []
        except ValueError:
            native = None
        if result.returncode != 0 or native != row["ids"]:
            failures.append({"row": number, "returncode": result.returncode,
                             "python": row["ids"], "native": native,
                             "stdout": result.stdout, "stderr": result.stderr})
        checked += 1
    parity = {"checked": checked, "failures": failures}
    (args.run / "native-parity.json").write_text(json.dumps(parity, indent=2) + "\n")
    if failures:
        raise SystemExit(f"native parity gate failed: {len(failures)} of {len(test['records'])}")
    print(json.dumps({"exact": test["exact"], "test": test["count"], "native_checked": checked}))


if __name__ == "__main__":
    main()
