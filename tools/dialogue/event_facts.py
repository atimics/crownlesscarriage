#!/usr/bin/env python3
"""Participant owned event facts for grounded dialogue acts.

The bridge accepts a participant snapshot and reads only ``held_accounts``.
It never turns global simulation state into a fact.  A fact keeps its source,
date, certainty and disclosure flag beside the account text.
"""

from __future__ import annotations

from dataclasses import asdict, dataclass
import hashlib
import json
from pathlib import Path
import re
from typing import Any, Iterable, Mapping


ROOT = Path(__file__).resolve().parents[2]
SIM_ENUM = ROOT / "src" / "sim" / "cc_sim.h"
ACTS = frozenset(("report", "ask", "warn"))
KNOWLEDGE_CERTAINTY = {1: "doubtful", 2: "told", 3: "witnessed"}


def _read_registry() -> dict[str, int]:
    text = SIM_ENUM.read_text(encoding="utf-8")
    match = re.search(r"typedef enum CcEventKind\s*\{(.*?)\n\s*\} CcEventKind", text, re.S)
    if match is None:
        raise ValueError("CcEventKind enum is missing")
    values = dict((name, int(value)) for name, value in re.findall(
        r"CC_EVENT_([A-Z0-9_]+)\s*=\s*(\d+)", match.group(1)))
    if not values or set(values.values()) != set(range(len(values))):
        raise ValueError("CcEventKind values must be contiguous from zero")
    return values


EVENT_KIND_REGISTRY = _read_registry()
EVENT_KIND_BY_VALUE = {value: name for name, value in EVENT_KIND_REGISTRY.items()}


def validate_event_registry(registry: Mapping[str, int] | None = None) -> dict[str, Any]:
    """Validate an event registry against the simulation enum.

    The returned receipt is suitable for a coverage guard and includes every
    enum value, including kinds with no account grammar rule.
    """
    current = dict(EVENT_KIND_REGISTRY if registry is None else registry)
    expected = _read_registry()
    if current != expected:
        missing = sorted(set(expected) - set(current))
        extra = sorted(set(current) - set(expected))
        raise ValueError(f"event registry differs from simulation enum; missing={missing}, extra={extra}")
    return {"event_count": len(current), "kinds": [
        {"name": name, "value": value} for name, value in sorted(current.items(), key=lambda item: item[1])
    ]}


def validate_registry(registry: Mapping[str, int] | None = None) -> dict[str, Any]:
    """Compatibility name for coverage guards."""
    return validate_event_registry(registry)


def event_kind_name(kind: int | str) -> str:
    if isinstance(kind, str):
        if kind.startswith("CC_EVENT_"):
            kind = kind[9:]
        if kind in EVENT_KIND_REGISTRY:
            return kind
        if kind.isdecimal():
            kind = int(kind)
    if isinstance(kind, int) and not isinstance(kind, bool) and kind in EVENT_KIND_BY_VALUE:
        return EVENT_KIND_BY_VALUE[kind]
    raise ValueError(f"unknown event kind: {kind!r}")


def event_kind_value(kind: int | str) -> int:
    name = event_kind_name(kind)
    return EVENT_KIND_REGISTRY[name]


def _id(value: Any, label: str) -> str:
    if isinstance(value, int) and not isinstance(value, bool) and value > 0:
        return str(value)
    if isinstance(value, str) and value.isdecimal() and int(value) > 0:
        return value
    raise ValueError(f"{label} needs a stable non-zero ID")


def _source_id(value: Any) -> str:
    if isinstance(value, bool):
        raise ValueError('source_id must be an ID')
    if value is None or value == 0 or value == "0":
        return "unknown"
    return _id(value, "source_id")


def _score(account: Mapping[str, Any], name: str) -> int | None:
    if name not in account:
        return None
    value = account[name]
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= 100:
        raise ValueError(f"{name} must be an integer from 0 through 100")
    return value


@dataclass(frozen=True)
class EventFact:
    owner_id: str
    event_id: str
    account_ref: str
    account_digest: str
    kind: int
    kind_name: str
    text: str
    source_id: str
    certainty: str | None
    certainty_value: int | None
    confidence: int | None
    day: int
    private: bool
    parser_supported: bool

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def _fact_from_account(owner_id: str, account: Mapping[str, Any], knowledge: Mapping[str, Any] | None) -> EventFact:
    event_id = _id(account.get("event_id"), "event_id")
    source_id = _source_id(account.get("source_id", account.get("source_character_id")))
    kind = event_kind_value(account.get("kind"))
    text = account.get("account", account.get("text", ""))
    if (not isinstance(text, str) or not text.strip() or
            any(ord(char) < 32 or ord(char) == 127 for char in text) or len(text.encode("utf-8")) >= 144):
        raise ValueError("account text must be printable UTF-8 text under 144 bytes")
    day = account.get("day")
    if isinstance(day, bool) or not isinstance(day, int) or day < 0:
        raise ValueError("day must be a non-negative integer")
    private = account.get("private", account.get("private_knowledge", False))
    known_private = knowledge.get('private', False) if knowledge else False
    if not isinstance(private, bool) or not isinstance(known_private, bool):
        raise ValueError("private must be boolean")
    private = private or known_private
    # A caller supplied parser flag is not evidence. This bridge quotes held
    # text until the native account grammar proves a parse.
    parser_supported = False
    certainty_value = knowledge.get("certainty") if knowledge else None
    if certainty_value is not None and (type(certainty_value) is not int or certainty_value not in KNOWLEDGE_CERTAINTY):
        raise ValueError("certainty must be doubtful, told or witnessed")
    certainty = KNOWLEDGE_CERTAINTY.get(certainty_value)
    confidence = _score(account, "confidence")
    digest_input = {"owner_id": owner_id, "event_id": event_id, "kind": kind,
                    "text": text, "source_id": source_id, "certainty": certainty_value,
                    "confidence": confidence,
                    "day": day, "private": private}
    digest = hashlib.sha256(json.dumps(digest_input, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return EventFact(owner_id, event_id, "fact:" + digest[:24], digest, kind,
                     event_kind_name(kind), text, source_id, certainty, certainty_value, confidence, day,
                     private, parser_supported)


def build_facts(participant: Mapping[str, Any]) -> list[EventFact]:
    """Build facts from the participant's held accounts only."""
    if not isinstance(participant, Mapping):
        raise ValueError("participant must be an object")
    self_data = participant.get("self", {})
    owner = self_data.get("id") if isinstance(self_data, Mapping) else None
    owner_id = _id(owner, "participant owner_id")
    accounts = participant.get("held_accounts", [])
    if not isinstance(accounts, list):
        raise ValueError("held_accounts must be a list")
    today = participant.get('day')
    if type(today) is not int or today < 0:
        raise ValueError('participant day must be a non-negative integer')
    knowledge_by_event = {}
    for item in participant.get("knowledge", []):
        if isinstance(item, Mapping) and item.get("event_id") is not None:
            key = str(item['event_id'])
            if key in knowledge_by_event:
                raise ValueError('duplicate event knowledge needs resolution')
            knowledge_by_event[key] = item
    facts = [_fact_from_account(owner_id, item, knowledge_by_event.get(str(item.get("event_id")))) for item in accounts
             if isinstance(item, Mapping)]
    if len(facts) != len(accounts):
        raise ValueError("each held account must be an object")
    if any(fact.day > today for fact in facts):
        raise ValueError('held account is from a future day')
    if len({fact.account_ref for fact in facts}) != len(facts):
        raise ValueError("held accounts need distinct stable references")
    return facts


def fact_for_ref(facts: Iterable[EventFact], account_ref: str) -> EventFact:
    for fact in facts:
        if fact.account_ref == account_ref:
            return fact
    raise ValueError("fact reference is not owned by the participant")


def validate_act(act: Mapping[str, Any], participant: Mapping[str, Any], listener_id: Any = None) -> EventFact:
    """Validate report, ask and warn acts against the participant's own facts."""
    if not isinstance(act, Mapping) or act.get("kind") not in ACTS:
        raise ValueError("act kind must be report, ask or warn")
    if set(act) - {"kind", "fact_ref"}:
        raise ValueError("act has unknown fields")
    if not isinstance(act.get("fact_ref"), str):
        raise ValueError("fact_ref is required")
    if not isinstance(participant, Mapping):
        raise ValueError("act validation needs the current participant snapshot")
    current = build_facts(participant)
    fact = fact_for_ref(current, act["fact_ref"])
    if fact.private and (listener_id is None or _id(listener_id, "listener_id") != fact.owner_id):
        raise ValueError("private fact cannot be disclosed to this listener")
    return fact


def render_fact_act(act: Mapping[str, Any], participant: Mapping[str, Any], listener_id: Any = None) -> str:
    """Render a bounded, attributed claim after act validation."""
    fact = validate_act(act, participant, listener_id)
    source = "an unknown source" if fact.source_id == "unknown" else ("my own account" if fact.source_id == fact.owner_id else "person " + fact.source_id)
    certainty = "I have heard this" if fact.certainty is None else "I was told this" if fact.certainty == "told" else "I witnessed this" if fact.certainty == "witnessed" else "I have a doubtful account"
    quoted = '"' + fact.text.replace('"', "'") + '"'
    if act["kind"] == "ask":
        return f"Do you know this account from {source}: {quoted}?"
    prefix = "Be warned" if act["kind"] == "warn" else certainty
    return f"{prefix}: {quoted} (an unparsed account from {source}, on day {fact.day})"
