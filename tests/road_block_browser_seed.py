"""Load one generated road fixture into a private browser test world."""

import json
import sqlite3
import sys
from pathlib import Path


repository = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repository / "tools" / "coop"))
from engine import Engine  # noqa: E402


def main():
    library, database, world_id, fixture = sys.argv[1:]
    state = Path(fixture).read_bytes()
    with Engine(library).open(saved=state) as sim:
        view = sim.snapshot()
    with sqlite3.connect(database) as db:
        changed = db.execute(
            "UPDATE worlds SET state=?, view=?, revision=revision+1, "
            "action_revision=action_revision+1 WHERE id=?",
            (state, json.dumps(view, separators=(",", ":")), world_id),
        )
        if changed.rowcount != 1:
            raise RuntimeError("The browser fixture world is missing.")
    print(json.dumps({"hash": view["hash"], "phase": view["journey"]["phase"]}))


if __name__ == "__main__":
    main()
