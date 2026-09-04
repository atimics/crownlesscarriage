#!/usr/bin/env python3
"""Build the C soundtrack catalog from its theme and take metadata."""
import argparse
import json
from pathlib import Path


def render(data):
    cues, takes = data["cues"], data["takes"]
    assert len(cues) == 64 and len(takes) == 149
    assert [cue["number"] for cue in cues] == list(range(1, 65))
    lines = ["/* Theme tags for the Crownless soundtrack. */",
             "const CcMusicCue cc_music_cues[CC_MUSIC_CUE_COUNT] = {"]
    for cue in cues:
        assert -1 <= cue["region"] < 6
        for key in ("theme", "secondary"):
            assert cue[key].isidentifier() and cue[key].isascii()
        lines.append("    {%s, %s, CC_MUSIC_%s, CC_MUSIC_%s, %d, %s}," % (
            json.dumps(cue["title"]), json.dumps(cue["place"]),
            cue["theme"].upper(), cue["secondary"].upper(), cue["region"],
            "true" if cue["combat"] else "false"))
    lines += ["};", "", "const CcMusicTake cc_music_takes[CC_MUSIC_TAKE_COUNT] = {"]
    seen = set()
    for take in takes:
        key = take["track"], take["variant"]
        assert key not in seen and 1 <= key[0] <= 64 and 1 <= key[1] <= 7
        seen.add(key)
        assert take["stem"] == f"{key[0]:02d}-{key[1]:02d}"
        assert 10 <= take["duration"] <= 1800 and 0 < take["weight"] <= 1
        lines.append(f'    {{{key[0]-1}, {key[1]}, {take["duration"]}.0f, {take["weight"]}f}},')
    return "\n".join(lines + ["};", ""])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = root / "assets/audio/music/catalog.json"
    target = root / "src/client/cc_music_catalog.inc"
    expected = render(json.loads(source.read_text()))
    if args.check:
        if target.read_text() != expected:
            raise SystemExit("Run python3 tools/music_catalog.py to refresh the C catalog.")
    else:
        target.write_text(expected)


if __name__ == "__main__":
    main()
