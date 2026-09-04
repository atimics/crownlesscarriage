"""Persistent shared-carriage worlds. Run with --help for hosting options."""
import argparse
import base64
from contextlib import contextmanager
import hashlib
import hmac
import json
import logging
import mimetypes
import os
from pathlib import Path
import re
import secrets
import sqlite3
import threading
import time
from urllib.parse import parse_qs, urlsplit
import zlib

from engine import Engine

PROTOCOL = 1
MAX_MEMBERS = 8
MAX_WORLDS = 16
MAX_BODY = 8192
HEX64 = re.compile(r"^[0-9a-f]{64}$")
WORLD_ID = re.compile(r"^[0-9a-f]{32}$")
NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9 '\-]{0,30}$")
ACTIONS = {'camp_road_site', 'pass_road_site', 'meet_pony', 'help_pony', 'swap_pony', 'leave_pony', 'skip_watch', 'negotiate', 'refuse', 'retrieve_map', 'return_treasure', 'goblin_intercept', 'provisions', 'pace', 'abandon', 'fight', 'withdraw', 'breed_horses', 'buy_map', 'dungeon_encounter', 'goblin_trade', 'enter_dungeon', 'press_on', 'trade', 'goblin_warn', 'goblin_tunnel', 'sell_map', 'intercept_tribute', 'search_dungeon', 'break', 'accept', 'open_shortcut', 'return_named_treasure', 'change_dungeon', 'camp', 'repair', 'archive_map', 'assign_horse', 'lodge', 'sell_treasure', 'talk', 'buy_treasure', 'leave_dungeon', 'move_dungeon', 'steal_named_treasure', 'travel', 'steal_hoard'}


class ApiError(Exception):
    def __init__(self, status, message):
        self.status, self.message = status, message


def require(condition, message, status=400):
    if not condition:
        raise ApiError(status, message)


def digest(value):
    return hashlib.sha256(value.encode("ascii")).hexdigest()


def number(value, minimum, maximum, label):
    require(type(value) is int and minimum <= value <= maximum, f"Choose a valid {label}.")
    return value


class Worlds:
    def __init__(self, path, engine):
        self.engine = engine
        self.lock = threading.RLock()
        self.path = Path(path).resolve()
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.guard = open(str(self.path) + ".lock", "a+b")
        try:
            if __import__("os").name == "nt":
                import msvcrt
                self.guard.seek(0)
                self.guard.write(b"0")
                self.guard.flush()
                self.guard.seek(0)
                msvcrt.locking(self.guard.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.guard.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            self.guard.close()
            raise RuntimeError("This world database already has a running host.") from None
        self.db = sqlite3.connect(self.path, check_same_thread=False, isolation_level=None)
        self.db.row_factory = sqlite3.Row
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("PRAGMA synchronous=FULL")
        self.db.execute("PRAGMA foreign_keys=ON")
        version = self.db.execute("PRAGMA user_version").fetchone()[0]
        if version not in (0, PROTOCOL):
            self.close()
            raise RuntimeError("This world database requires its matching server version.")
        self.db.executescript("""
        CREATE TABLE IF NOT EXISTS worlds (
          id TEXT PRIMARY KEY, name TEXT NOT NULL, owner TEXT NOT NULL,
          invite_nonce TEXT NOT NULL, invite_hash TEXT NOT NULL,
          state BLOB NOT NULL, view TEXT NOT NULL, revision INTEGER NOT NULL DEFAULT 0,
          action_revision INTEGER NOT NULL DEFAULT 0, paused INTEGER NOT NULL DEFAULT 0);
        CREATE TABLE IF NOT EXISTS members (
          world TEXT NOT NULL REFERENCES worlds(id), token_hash TEXT NOT NULL,
          id TEXT NOT NULL, name TEXT NOT NULL, sequence INTEGER NOT NULL DEFAULT 0,
          revoked INTEGER NOT NULL DEFAULT 0, PRIMARY KEY(world, token_hash), UNIQUE(world, id));
        CREATE TABLE IF NOT EXISTS receipts (
          world TEXT NOT NULL, member TEXT NOT NULL, sequence INTEGER NOT NULL,
          payload_hash TEXT NOT NULL, result TEXT NOT NULL,
          PRIMARY KEY(world,member,sequence),
          FOREIGN KEY(world,member) REFERENCES members(world,id));
        PRAGMA user_version=1;
        """)
        self.seen, self.last_tick, self.failed = {}, {}, set()

    def close(self):
        with self.lock:
            self.db.close()
            self.guard.close()

    @contextmanager
    def transaction(self):
        with self.lock:
            self.db.execute("BEGIN IMMEDIATE")
            try:
                yield
                self.db.execute("COMMIT")
            except BaseException:
                self.db.execute("ROLLBACK")
                raise

    def member(self, world, token):
        row = self.db.execute("SELECT * FROM members WHERE world=? AND token_hash=? AND revoked=0",
                              (world, digest(token))).fetchone()
        require(row is not None, "Join this carriage with an invitation.", 403)
        return row

    def view(self, world, token, campaign=False, after=None):
        with self.lock:
            member = self.member(world, token)
            row = self.db.execute("SELECT * FROM worlds WHERE id=?", (world,)).fetchone()
            self.seen[(world, member["id"])] = time.monotonic()
            if world not in self.last_tick:
                self.last_tick[world] = time.monotonic()
            result = dict(protocol=PROTOCOL, id=world, name=row["name"], revision=row["revision"],
                          action_revision=row["action_revision"], paused=bool(row["paused"]),
                          owner=row["owner"] == digest(token), member=member["id"],
                          next_sequence=member["sequence"] + 1, state=json.loads(row["view"]),
                          recovery_required=world in self.failed)
            result["crew"] = [dict(id=m["id"], name=m["name"],
                online=time.monotonic() - self.seen.get((world, m["id"]), -100) < 15)
                for m in self.db.execute("SELECT id,name FROM members WHERE world=? AND revoked=0 ORDER BY rowid", (world,))]
            if campaign and after != str(row["revision"]):
                result["campaign"] = base64.b64encode(row["state"]).decode("ascii")
            return result

    def create(self, token, body):
        world, name, player = body.get("id"), body.get("name"), body.get("player")
        require(isinstance(world, str) and WORLD_ID.fullmatch(world), "Choose a valid world identity.")
        require(isinstance(name, str) and NAME.fullmatch(name), "Use a world name of 1 to 31 letters.")
        require(isinstance(player, str) and NAME.fullmatch(player), "Use a crew name of 1 to 31 letters.")
        seed = number(body.get("seed", 3232176798), 0, 2**32 - 1, "world seed")
        with self.transaction():
            existing = self.db.execute("SELECT owner FROM worlds WHERE id=?", (world,)).fetchone()
            if existing:
                require(existing["owner"] == digest(token), "That world identity is already in use.", 409)
            else:
                require(self.db.execute("SELECT count(*) FROM worlds").fetchone()[0] < MAX_WORLDS,
                        "This host has reached its world limit.", 409)
                nonce = secrets.token_hex(16)
                invitation = hmac.new(token.encode(), (world + nonce).encode(), hashlib.sha256).hexdigest()
                with self.engine.open(seed) as sim:
                    saved, view = sim.save(), json.dumps(sim.snapshot())
                self.db.execute("INSERT INTO worlds(id,name,owner,invite_nonce,invite_hash,state,view) VALUES(?,?,?,?,?,?,?)",
                                (world, name, digest(token), nonce, digest(invitation), saved, view))
                self.db.execute("INSERT INTO members(world,token_hash,id,name) VALUES(?,?,?,?)",
                                (world, digest(token), secrets.token_hex(8), player))
        return self.view(world, token)

    def invite(self, world, token, rotate=False):
        with self.transaction():
            row = self.db.execute("SELECT * FROM worlds WHERE id=?", (world,)).fetchone()
            require(row is not None and row["owner"] == digest(token), "The world host manages invitations.", 403)
            nonce = secrets.token_hex(16) if rotate else row["invite_nonce"]
            invitation = hmac.new(token.encode(), (world + nonce).encode(), hashlib.sha256).hexdigest()
            if rotate:
                self.db.execute("UPDATE worlds SET invite_nonce=?,invite_hash=? WHERE id=?", (nonce, digest(invitation), world))
            return {"invite": invitation}

    def join(self, world, token, body):
        invitation, name = body.get("invite"), body.get("player")
        require(isinstance(invitation, str) and HEX64.fullmatch(invitation), "Use the complete invitation.")
        require(isinstance(name, str) and NAME.fullmatch(name), "Use a crew name of 1 to 31 letters.")
        with self.transaction():
            row = self.db.execute("SELECT invite_hash FROM worlds WHERE id=?", (world,)).fetchone()
            require(row is not None and hmac.compare_digest(row["invite_hash"], digest(invitation)), "This invitation has expired.", 403)
            member = self.db.execute("SELECT * FROM members WHERE world=? AND token_hash=?", (world, digest(token))).fetchone()
            if member:
                require(not member["revoked"], "Ask the world host for access.", 403)
            else:
                total = self.db.execute("SELECT count(*) FROM members WHERE world=?", (world,)).fetchone()[0]
                active = self.db.execute("SELECT count(*) FROM members WHERE world=? AND revoked=0", (world,)).fetchone()[0]
                require(active < MAX_MEMBERS and total < 64, "This carriage has reached its crew limit.", 409)
                self.db.execute("INSERT INTO members(world,token_hash,id,name) VALUES(?,?,?,?)",
                                (world, digest(token), secrets.token_hex(8), name))
        return self.view(world, token)

    def command(self, world, token, body):
        require(set(body) <= {"protocol", "sequence", "action_revision", "action", "target", "good", "amount", "campaign"},
                "Use the shared-carriage command fields.")
        require(type(body.get("protocol")) is int and body.get("protocol") == PROTOCOL, "Refresh the game to use this host's protocol.", 409)
        sequence = number(body.get("sequence"), 1, 2**53 - 1, "command sequence")
        revision = number(body.get("action_revision"), 0, 2**53 - 1, "company revision")
        action = body.get("action")
        require(isinstance(action, str) and action in ACTIONS, "Choose an available company action.")
        target = body.get("target", "0")
        require(isinstance(target, str) and re.fullmatch(r"[0-9]{1,20}", target) and int(target) < 2**64,
                "Choose a valid target.")
        good = number(body.get("good", 0), 0, 13, "good")
        amount = number(body.get("amount", 0), -1000000, 1000000, "quantity")
        payload = json.dumps({k: v for k, v in body.items() if k != "campaign"}, sort_keys=True)
        payload_hash = hashlib.sha256(payload.encode()).hexdigest()
        with self.transaction():
            member = self.member(world, token)
            receipt = self.db.execute("SELECT * FROM receipts WHERE world=? AND member=? AND sequence=?",
                                      (world, member["id"], sequence)).fetchone()
            if receipt:
                require(hmac.compare_digest(receipt["payload_hash"], payload_hash), "That command sequence already has a different action.", 409)
                result = json.loads(receipt["result"])
                result["duplicate"] = True
            else:
                require(sequence == member["sequence"] + 1, "Refresh the crew session before the next action.", 409)
                row = self.db.execute("SELECT * FROM worlds WHERE id=?", (world,)).fetchone()
                require(revision == row["action_revision"], "The company has changed. Review the latest state and choose again.", 409)
                require(world not in self.failed, "This world needs recovery before play resumes.", 503)
                require(not row["paused"], "Resume the world before taking a company action.", 409)
                with self.engine.open(saved=row["state"]) as sim:
                    accepted, message = sim.apply(action, target, good, amount)
                    if accepted:
                        self.db.execute("UPDATE worlds SET state=?,view=?,revision=revision+1,action_revision=action_revision+1 WHERE id=?",
                                        (sim.save(), json.dumps(sim.snapshot()), world))
                    result = {"accepted": accepted, "message": message or "Company action completed.",
                              "sequence": sequence, "duplicate": False}
                self.db.execute("INSERT INTO receipts VALUES(?,?,?,?,?)", (world, member["id"], sequence, payload_hash, json.dumps(result)))
                self.db.execute("UPDATE members SET sequence=? WHERE world=? AND id=?", (sequence, world, member["id"]))
                self.db.execute("DELETE FROM receipts WHERE world=? AND member=? AND sequence<=?", (world, member["id"], sequence - 128))
        result["world"] = self.view(world, token, body.get("campaign") is True)
        return result

    def tick(self, now=None):
        now = time.monotonic() if now is None else now
        with self.lock:
            for row in list(self.db.execute("SELECT id,paused FROM worlds")):
                world = row["id"]
                previous = self.last_tick.get(world, now)
                online = any(w == world and now - seen < 15 for (w, _), seen in self.seen.items())
                if row["paused"] or not online or world in self.failed:
                    self.last_tick[world] = now
                    continue
                ticks = min(60, int(max(0, now - previous) * 60))
                if ticks < 1:
                    continue
                try:
                    with self.transaction():
                        saved = self.db.execute("SELECT state,view FROM worlds WHERE id=?", (world,)).fetchone()
                        before = json.loads(saved["view"])
                        if before["journey"]["active"] and before["journey"]["phase"] == 1:
                            with self.engine.open(saved=saved["state"]) as sim:
                                sim.advance(ticks)
                                self.db.execute("UPDATE worlds SET state=?,view=?,revision=revision+1 WHERE id=?",
                                                (sim.save(), json.dumps(sim.snapshot()), world))
                    self.last_tick[world] = now
                except Exception:
                    self.failed.add(world)
                    logging.exception("World %s needs recovery", world)

    def owner_action(self, world, token, action, member=None):
        with self.transaction():
            row = self.db.execute("SELECT owner FROM worlds WHERE id=?", (world,)).fetchone()
            require(row is not None and row["owner"] == digest(token), "The world host manages the crew and pause control.", 403)
            if action in ("pause", "resume"):
                self.db.execute("UPDATE worlds SET paused=?,revision=revision+1 WHERE id=?", (int(action == "pause"), world))
                self.last_tick[world] = time.monotonic()
            elif action == "remove":
                require(member != self.member(world, token)["id"], "The host keeps their place in the carriage.")
                self.db.execute("UPDATE members SET revoked=1 WHERE world=? AND id=?", (world, member))
                self.seen.pop((world, member), None)
            else:
                raise ApiError(400, "Choose a host action.")
        return self.view(world, token)


class Application:
    def __init__(self, worlds, public_origin=None, game_dir=None):
        self.worlds = worlds
        self.public_origin = public_origin.rstrip("/") if public_origin else None
        self.static = Path(__file__).resolve().parents[2] / "web" / "coop"
        self.game_dir = Path(game_dir).resolve() if game_dir else None
        self.rates = {}
        self.rate_lock = threading.Lock()

    def route(self, env):
        method, path = env["REQUEST_METHOD"], env.get("PATH_INFO", "/")
        if path == "/healthz":
            return (503 if self.worlds.failed else 200), {"status": "recovery" if self.worlds.failed else "ready", "protocol": PROTOCOL,
                "revision": os.environ.get("CROWNLESS_REVISION", "development")}, None
        if not path.startswith("/api/"):
            require(method in ("GET", "HEAD"), "Use GET for game files.", 405)
            if path in ("/", "/coop.js", "/coop.css"):
                file = self.static / ("index.html" if path == "/" else path[1:])
            elif self.game_dir and path.startswith("/game/"):
                file = (self.game_dir / (path[6:] or "index.html")).resolve()
                require(file.is_relative_to(self.game_dir), "Choose a game file.", 404)
            else:
                raise ApiError(404, "This page is unavailable.")
            require(file.is_file(), "This game file is still being prepared.", 404)
            return 200, b"" if method == "HEAD" else file.read_bytes(), mimetypes.guess_type(str(file))[0] or "application/octet-stream"
        origin = env.get("HTTP_ORIGIN")
        expected = self.public_origin or env.get("wsgi.url_scheme", "http") + "://" + env.get("HTTP_HOST", "")
        require(origin is None or origin == expected, "Open the carriage on its host address.", 403)
        auth = env.get("HTTP_AUTHORIZATION", "")
        token = auth[7:] if auth.startswith("Bearer ") else ""
        require(HEX64.fullmatch(token), "Open or restore your crew session.", 401)
        identity = digest(token)
        with self.rate_lock:
            now = time.monotonic()
            if len(self.rates) > 4096:
                self.rates = {key: value for key, value in self.rates.items() if now - value[0] < 60}
            start, count = self.rates.get(identity, (now, 0))
            if now - start >= 1:
                start, count = now, 0
            require(count < 20, "The carriage is catching up. Try again shortly.", 429)
            self.rates[identity] = start, count + 1
        body = {}
        if method in ("POST", "DELETE"):
            require(env.get("CONTENT_TYPE", "").split(";")[0] == "application/json", "Send JSON for company actions.", 415)
            try:
                length = int(env.get("CONTENT_LENGTH", "0"))
            except ValueError:
                raise ApiError(400, "Use a valid request length.") from None
            require(0 < length <= MAX_BODY, "The company request exceeded its size limit.", 413)
            try:
                body = json.loads(env["wsgi.input"].read(length))
            except (ValueError, UnicodeError):
                raise ApiError(400, "Use a complete JSON request.") from None
            require(isinstance(body, dict), "Use a company request object.")
        if path == "/api/worlds" and method == "POST":
            return 200, self.worlds.create(token, body), None
        if path == "/api/worlds" and method == "GET":
            with self.worlds.lock:
                rows = self.worlds.db.execute("SELECT w.id,w.name FROM worlds w JOIN members m ON w.id=m.world WHERE m.token_hash=? AND m.revoked=0", (identity,))
                return 200, {"worlds": [dict(row) for row in rows], "game_available": self.game_dir is not None}, None
        match = re.fullmatch(r"/api/worlds/([0-9a-f]{32})(?:/(state|join|command|invite|host))?", path)
        require(match is not None, "Choose a world on this host.", 404)
        world, operation = match.group(1), match.group(2) or "state"
        if operation == "state" and method == "GET":
            query = parse_qs(env.get("QUERY_STRING", ""))
            campaign = query.get("campaign") == ["1"]
            return 200, self.worlds.view(world, token, campaign, query.get("after", [None])[0]), None
        if operation == "join" and method == "POST":
            return 200, self.worlds.join(world, token, body), None
        if operation == "command" and method == "POST":
            return 200, self.worlds.command(world, token, body), None
        if operation == "invite" and method in ("GET", "POST"):
            return 200, self.worlds.invite(world, token, method == "POST"), None
        if operation == "host" and method == "POST":
            return 200, self.worlds.owner_action(world, token, body.get("action"), body.get("member")), None
        raise ApiError(405, "Choose a supported world action.")

    def __call__(self, env, start_response):
        try:
            status, result, content_type = self.route(env)
        except ApiError as error:
            status, result, content_type = error.status, {"error": error.message}, None
        except Exception:
            logging.exception("World request failed")
            status, result, content_type = 503, {"error": "The host could not save this action. Retry the same request."}, None
        data = result if isinstance(result, bytes) else json.dumps(result, separators=(",", ":")).encode("utf-8")
        headers = [("Content-Type", content_type or "application/json"), ("Cache-Control", "no-store"),
                   ("X-Content-Type-Options", "nosniff"), ("Referrer-Policy", "no-referrer")]
        if "gzip" in env.get("HTTP_ACCEPT_ENCODING", "") and len(data) > 1024:
            compressor = zlib.compressobj(wbits=31)
            data = compressor.compress(data) + compressor.flush()
            headers.extend([("Content-Encoding", "gzip"), ("Vary", "Accept-Encoding")])
        headers.append(("Content-Length", str(len(data))))
        from http import HTTPStatus
        start_response(f"{status} {HTTPStatus(status).phrase}", headers)
        return [data]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", required=True, help="Built crownless_coop shared library")
    parser.add_argument("--database", default="out/worlds.sqlite3")
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    parser.add_argument("--public-origin", help="Public HTTPS origin when hosted behind a proxy")
    parser.add_argument("--game-dir", help="Built WebAssembly site to serve at /game/")
    parser.add_argument("--backup-to", help="Write a consistent host database backup, then exit")
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO)
    if args.backup_to:
        source = sqlite3.connect(f"file:{Path(args.database).resolve()}?mode=ro", uri=True)
        destination = sqlite3.connect(args.backup_to)
        source.backup(destination)
        destination.close()
        source.close()
        return
    from waitress import serve
    worlds = Worlds(args.database, Engine(args.library))
    stop = threading.Event()
    def clock():
        while not stop.wait(0.5):
            worlds.tick()
    thread = threading.Thread(target=clock, daemon=True)
    thread.start()
    try:
        serve(Application(worlds, args.public_origin, args.game_dir), host=args.bind, port=args.port,
              threads=8, connection_limit=128, channel_timeout=15, max_request_body_size=MAX_BODY,
              max_request_header_size=16384, clear_untrusted_proxy_headers=True)
    finally:
        stop.set()
        thread.join()
        worlds.close()


if __name__ == "__main__":
    main()
