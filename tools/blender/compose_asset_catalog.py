#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[2]
CATALOG_DIR = ROOT / "assets" / "previews" / "catalog"
THUMB_DIR = CATALOG_DIR / "thumbs"
FONT_PATH = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

GROUPS = {
    "carriage": {
        "title": "Carriage Core + Modular Layers",
        "columns": 5,
        "assets": [
            ("carriage_base_v01", "Carriage Base"),
            ("module_cargo_rack_v01", "Cargo Rack"),
            ("module_armoured_body_v01", "Armoured Body"),
            ("module_medical_bunk_v01", "Medical Bunk"),
            ("module_passenger_bench_v01", "Passenger Bench"),
            ("module_hidden_compartment_v01", "Hidden Compartment"),
            ("module_scout_perch_v01", "Scout Perch"),
            ("module_monster_cage_v01", "Monster Cage"),
            ("module_relic_containment_v01", "Relic Containment"),
            ("module_document_safe_v01", "Document Safe"),
        ],
    },
    "environments": {
        "title": "Environment Kits",
        "columns": 2,
        "assets": [
            ("environment_road_straight_v01", "Straight Road"),
            ("environment_bridge_checkpoint_v01", "Bridge Checkpoint"),
            ("environment_mine_entrance_v01", "Mine Entrance"),
            ("environment_market_granary_v01", "Market + Granary"),
        ],
    },
    "states": {
        "title": "State-Dressing Layers in Market Context",
        "columns": 3,
        "assets": [
            ("state_food_shortage_v01", "Food Shortage"),
            ("state_harsh_enforcement_v01", "Harsh Enforcement"),
            ("state_market_recovery_v01", "Market Recovery"),
        ],
    },
}


def load_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    try:
        return ImageFont.truetype(FONT_PATH, size)
    except OSError:
        return ImageFont.load_default()


def compose(group_name: str, spec: dict[str, object]) -> None:
    assets = spec["assets"]
    columns = int(spec["columns"])
    rows = (len(assets) + columns - 1) // columns
    cell_width = 512
    cell_height = 558
    title_height = 78
    canvas = Image.new("RGB", (columns * cell_width, title_height + rows * cell_height), "#12171b")
    draw = ImageDraw.Draw(canvas)
    title_font = load_font(32)
    label_font = load_font(22)
    draw.text((24, 22), str(spec["title"]), fill="#f2eadc", font=title_font)

    for index, (asset_id, label) in enumerate(assets):
        column = index % columns
        row = index // columns
        x = column * cell_width
        y = title_height + row * cell_height
        thumbnail = Image.open(THUMB_DIR / f"{asset_id}.png").convert("RGB")
        canvas.paste(thumbnail, (x, y))
        draw.rectangle((x, y + 512, x + cell_width, y + cell_height), fill="#1b2328")
        draw.text((x + 16, y + 526), label, fill="#f4eee2", font=label_font)

    canvas.save(CATALOG_DIR / f"asset_catalog_{group_name}.png", optimize=True)


def main() -> None:
    CATALOG_DIR.mkdir(parents=True, exist_ok=True)
    for group_name, spec in GROUPS.items():
        compose(group_name, spec)
    print(f"Composed {len(GROUPS)} asset catalog sheets in {CATALOG_DIR}")


if __name__ == "__main__":
    main()
