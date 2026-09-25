"""Run the frozen long-history measurements against a chosen built checkout."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", type=int, nargs="+", default=[5, 17, 42, 137, 731, 2026])
    parser.add_argument("--years", type=int, default=3000)
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    if not 1 <= args.years <= 10000 or args.jobs < 1 or any(s < 1 or s > 2147483647 for s in args.seeds):
        parser.error("Choose positive seeds, jobs, and 1 to 10000 years.")
    if len(set(args.seeds)) != len(args.seeds):
        parser.error("Choose each seed once.")
    source, build, output = args.source.resolve(), args.build.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("Choose an empty output directory to preserve earlier runs.")
    probe = Path(__file__).with_name("probe.c")
    binary = output / "probe"
    subprocess.run(["cc", "-O2", "-std=c17", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(source / "src"), "-I" + str(source / "tools"), str(probe),
                    str(build / "libcrownless_persistence.a"), str(build / "libcrownless_sim.a"),
                    "-lm", "-lsqlite3", "-o", str(binary)], check=True)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    changes = subprocess.check_output(["git", "diff", "HEAD"], cwd=source)
    metadata = {
        "source_commit": revision, "tracked_changes_sha256": hashlib.sha256(changes).hexdigest(),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "probe_sha256": hashlib.sha256(probe.read_bytes()).hexdigest(),
        "raw_world_seeds": args.seeds, "solar_year_days": 364,
        "years_per_world": args.years, "player_actions": 0,
        "validation": "weekly state checks and final save encode/decode hash equality",
        "event_examples": "first two per kind per century, all dragon succession and diplomacy changes",
    }
    (output / "run.json").write_text(json.dumps(metadata, indent=2) + "\n")

    def run(seed):
        start = time.monotonic()
        with (output / f"seed-{seed}-metrics.csv").open("w") as out, (output / f"seed-{seed}.log").open("w") as err:
            result = subprocess.run([str(binary), str(seed), str(args.years), str(output / f"seed-{seed}")],
                                    stdout=out, stderr=err)
        return {"seed": seed, "exit_code": result.returncode, "seconds": round(time.monotonic() - start, 2)}

    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        for future in as_completed([pool.submit(run, seed) for seed in args.seeds]):
            result = future.result()
            results.append(result)
            (output / "outcomes.json").write_text(json.dumps(sorted(results, key=lambda r: r["seed"]), indent=2) + "\n")
            print(json.dumps(result), flush=True)
    return int(any(r["exit_code"] for r in results))


if __name__ == "__main__":
    raise SystemExit(main())
