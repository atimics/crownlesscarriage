"""Data driven realization for the version 3 meaning acts.

The caller supplies a typed act.  A language pack supplies complete clause
templates and a small concept lexicon.  Names, IDs rendered as names, and
numbers are copied from the act.  This keeps the language layer separate from
the simulation and from the older policy renderer.
"""
from __future__ import annotations

import json
from pathlib import Path
import re
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
PACK_ROOT = ROOT / "assets" / "worldpacks" / "languages" / "v3"
INTENTS = {
    "report_shortage", "report_stock", "request_food", "offer_food",
    "counter_offer", "accept", "decline", "condition", "recall_success",
    "recall_failure", "thank", "end",
}
REASONS = {
    "severe_shortage", "shortage", "hunger", "help", "price",
    "insufficient_money", "empty_store", "no_need", "outcome",
}
CONDITIONS = {"now", "daylight"}
_NUMBER = re.compile(r"(?<![\w.])-?\d+(?:\.\d+)?(?![\w.])")


class LanguagePackError(ValueError):
    """A language pack or an act is malformed."""


def _read_pack(language: str, pack_root: str | Path | None) -> dict[str, Any]:
    root = Path(pack_root) if pack_root is not None else PACK_ROOT
    path = root / f"{language}.json"
    if not path.is_file():
        raise LanguagePackError(f"language pack not found: {language}")
    try:
        pack = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise LanguagePackError(f"invalid language pack: {path}") from exc
    if not isinstance(pack, dict) or pack.get("version") != 1 or pack.get("id") != language:
        raise LanguagePackError(f"language pack must have version 1 and id {language!r}")
    if not isinstance(pack.get("templates"), dict) or not isinstance(pack.get("lexicon"), dict):
        raise LanguagePackError("language pack needs templates and lexicon objects")
    return pack


def _required(act: dict[str, Any], name: str) -> Any:
    value = act.get(name)
    if value is None or value == "":
        raise LanguagePackError(f"act field {name!r} is required")
    return value


def _obj(act: dict[str, Any], name: str) -> dict[str, Any]:
    value = act.get(name)
    if value is None:
        return {}
    if not isinstance(value, dict):
        raise LanguagePackError(f"act field {name!r} must be an object")
    return value


def validate_act(act: dict[str, Any]) -> None:
    if not isinstance(act, dict) or act.get("version") != 3:
        raise LanguagePackError("meaning act version must be 3")
    intent = _required(act, "intent")
    if intent not in INTENTS:
        raise LanguagePackError(f"unsupported v3 intent: {intent}")
    for field in ("actor", "recipient", "actor_name", "recipient_name"):
        _required(act, field)
    claim, proposal, memory = _obj(act, "claim"), _obj(act, "proposal"), _obj(act, "memory")
    if claim:
        if claim.get("kind") != "food_store":
            raise LanguagePackError("claim.kind must be food_store")
        for field in ("place_name", "stock", "target"):
            _required(claim, field)
        if claim.get("source") != "observed":
            raise LanguagePackError("claim.source must be observed")
    if proposal:
        for field in ("quantity", "unit_price", "total_cost", "condition"):
            _required(proposal, field)
        if proposal["condition"] not in CONDITIONS:
            raise LanguagePackError("proposal.condition must be now or daylight")
    if memory:
        if memory.get("outcome") not in {"fulfilled", "failed"}:
            raise LanguagePackError("memory.outcome must be fulfilled or failed")
        for field in ("quantity", "total_cost", "event_id"):
            _required(memory, field)
    reason = act.get("reason") or claim.get("reason") or memory.get("reason")
    if reason is not None and reason not in REASONS:
        raise LanguagePackError(f"unsupported reason: {reason}")
    if intent in {"report_shortage", "report_stock"}:
        for field in ("claim",):
            if not _obj(act, field):
                raise LanguagePackError(f"{intent} requires {field}")
    if intent in {"request_food", "offer_food", "counter_offer", "condition"} and not proposal:
        raise LanguagePackError(f"{intent} requires proposal")
    if intent in {"recall_success", "recall_failure"} and not memory:
        raise LanguagePackError(f"{intent} requires memory")


def _number(value: Any) -> str:
    if isinstance(value, bool) or not isinstance(value, (int, float, str)):
        raise LanguagePackError("quantities and prices must be numeric")
    return str(value)


def _concept(pack: dict[str, Any], key: str, count: Any = None) -> str:
    value = pack["lexicon"].get(key)
    if value is None:
        raise LanguagePackError(f"language pack has no concept {key!r}")
    if isinstance(value, dict):
        form = value.get("plural" if count is not None and float(count) != 1 else "singular")
        if form is None:
            raise LanguagePackError(f"concept {key!r} has no required form")
        return str(form)
    return str(value)


def _fields(act: dict[str, Any], pack: dict[str, Any]) -> dict[str, Any]:
    claim, proposal, memory = _obj(act, "claim"), _obj(act, "proposal"), _obj(act, "memory")
    quantity = proposal.get("quantity", memory.get("quantity", 0))
    if quantity in (None, ""):
        quantity = 0
    fields: dict[str, Any] = {
        "actor_name": str(act["actor_name"]), "recipient_name": str(act["recipient_name"]),
        "place_name": str(proposal.get("place_name", claim.get("place_name", ""))),
        "quantity": _number(quantity), "stock": _number(claim.get("stock", 0)),
        "target": _number(claim.get("target", 0)),
        "unit_price": _number(proposal.get("unit_price", 0)),
        "total_cost": _number(proposal.get("total_cost", memory.get("total_cost", 0))),
        "reason": str(act.get("reason", claim.get("reason", memory.get("reason", "")))),
        "event_id": str(memory.get("event_id", "")),
        "food": _concept(pack, "food", quantity),
        "ash": _concept(pack, "ash"), "ashes": _concept(pack, "ashes"),
        "daylight": _concept(pack, "daylight"), "vault": _concept(pack, "vault"),
        "condition": _concept(pack, "daylight") if proposal.get("condition") == "daylight" else "now",
    }
    return fields


def render(act: dict[str, Any], language: str = "human", pack_root: str | Path | None = None) -> str:
    """Render a version 3 act using a language pack."""
    validate_act(act)
    pack = _read_pack(language, pack_root)
    template = pack["templates"].get(act["intent"])
    if not isinstance(template, str) or not template:
        raise LanguagePackError(f"language pack has no template for {act['intent']!r}")
    try:
        text = template.format(**_fields(act, pack))
    except KeyError as exc:
        raise LanguagePackError(f"template uses unknown slot: {exc.args[0]}") from exc
    if not text.strip() or _NUMBER.findall(text) is None:
        raise LanguagePackError("template produced empty speech")
    return text
