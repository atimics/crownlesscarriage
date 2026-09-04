#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import sys

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from inspect_glb import collect_stats, parse_glb


ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = ROOT / "assets" / "world_kit_manifest.json"
CONNECTION_PATH = ROOT / "assets" / "world_kit_connections.json"
EXPECTED_VERSION = "0.5.1"
EXPECTED_BUCKS = ("lean", "standard", "heavy")
EXPECTED_MUSCLES = ("slight", "athletic", "power")
EXPECTED_TISSUES = ("low", "balanced", "central", "lower_body")
EXPECTED_BODY_BONES = (
    "root", "pelvis", "spine", "chest", "neck", "head",
    "upper_arm.L", "forearm.L", "hand.L",
    "upper_arm.R", "forearm.R", "hand.R",
    "thigh.L", "shin.L", "foot.L",
    "thigh.R", "shin.R", "foot.R",
)
EXPECTED_HEADS = ("square", "long", "broad", "veteran")
EXPECTED_HAIR = ("cropped", "swept", "bob", "crest", "braided", "rear_lock")
EXPECTED_CORE_SLOTS = (
    "torso", "pelvis", "upper_arm", "forearm",
    "thigh", "shin", "hand", "foot",
)
EXPECTED_SKELETON_PARTS = (
    "spine", "pelvis", "upper_arm", "forearm",
    "thigh", "shin", "hand", "foot",
)
EXPECTED_REVIEW_SIZE = {
    "world_scale_lineup": (1800, 720),
    "figure_parts_board": (1800, 1000),
    "figure_layer_stack": (1900, 850),
    "body_type_matrix": (1800, 800),
    "baked_body_recipe_matrix": (1900, 850),
    "procedural_cast": (1800, 800),
    "figure_turnaround": (1600, 800),
    "gameplay_silhouettes": (457, 285),
    "playset_combinations": (1800, 900),
}


def png_size(path: Path) -> tuple[int, int] | None:
    try:
        with path.open("rb") as handle:
            if handle.read(8) != b"\x89PNG\r\n\x1a\n":
                return None
            length = struct.unpack(">I", handle.read(4))[0]
            if handle.read(4) != b"IHDR" or length < 8:
                return None
            return struct.unpack(">II", handle.read(8))
    except OSError:
        return None


def validate(require_review_previews: bool = False) -> int:
    failures: list[str] = []
    for path in (MANIFEST_PATH, CONNECTION_PATH):
        if not path.exists():
            failures.append(f"missing {path}")
    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    connections = json.loads(CONNECTION_PATH.read_text(encoding="utf-8"))
    if manifest.get("library_version") != EXPECTED_VERSION:
        failures.append("manifest library version changed")
    if connections.get("library_version") != EXPECTED_VERSION:
        failures.append("connection library version changed")
    scale = manifest.get("scale_contract", {})
    if scale.get("adult_body_height") != 1.90:
        failures.append("adult body height is not 1.90 m")
    if scale.get("door_clear_opening") != [1.02, 2.10]:
        failures.append("door opening is not 1.02 x 2.10 m")
    if scale.get("runtime_whole_model_scale") != 1.0:
        failures.append("whole-model runtime scale must remain 1.0")

    standard = manifest.get("figure_standard", {})
    if tuple(standard.get("skeleton_families", ())) != EXPECTED_BUCKS:
        failures.append("skeleton family order changed")
    if tuple(standard.get("muscle_profiles", {})) != (
            "slight", "athletic", "power"):
        failures.append("muscle profile order changed")
    if tuple(standard.get("soft_tissue_profiles", {})) != (
            "low", "balanced", "central", "lower_body"):
        failures.append("soft-tissue profile order changed")
    mix_policy = standard.get("soft_tissue_mix_policy", {})
    if not mix_policy.get("regions_are_independent"):
        failures.append("soft-tissue regions are not independently mixable")
    if tuple(standard.get("head_families", ())) != EXPECTED_HEADS:
        failures.append("head family order changed")
    required_sockets = {
        "hand_left", "hand_right", "shoulder_left", "shoulder_right",
        "chest_front", "upper_back", "lower_back", "belt_left",
        "belt_right", "head_top", "face_front", "foot_left", "foot_right",
    }
    missing_sockets = required_sockets - set(standard.get("sockets", {}))
    if missing_sockets:
        failures.append(f"missing figure sockets {sorted(missing_sockets)}")

    modules = manifest.get("modules", [])
    if len(modules) != 93:
        failures.append(f"expected 93 modules, found {len(modules)}")
    module_ids = [entry.get("id") for entry in modules]
    if len(module_ids) != len(set(module_ids)):
        failures.append("duplicate module IDs")
    module_by_id = {str(entry.get("id")): entry for entry in modules}

    for buck in EXPECTED_BUCKS:
        for slot in EXPECTED_CORE_SLOTS:
            asset_id = f"wk_buck_{buck}_{slot}_v01"
            if asset_id not in module_by_id:
                failures.append(f"missing {asset_id}")
    for family in EXPECTED_HEADS:
        if f"wk_head_{family}_v01" not in module_by_id:
            failures.append(f"missing head family {family}")
    for style in EXPECTED_HAIR:
        if f"wk_hair_{style}_v01" not in module_by_id:
            failures.append(f"missing molded hair family {style}")
    expected_skeleton_parts = {
        f"wk_skeleton_{slot}_v01" for slot in EXPECTED_SKELETON_PARTS}
    missing_skeleton_parts = expected_skeleton_parts - set(module_by_id)
    if missing_skeleton_parts:
        failures.append(f"missing skeleton parts {sorted(missing_skeleton_parts)}")
    expected_muscles = {
        f"wk_muscle_{slot}_v01" for slot in (
            "chest", "back", "abdomen", "glute", "deltoid",
            "upper_arm", "forearm", "thigh", "calf", "neck")}
    missing_muscles = expected_muscles - set(module_by_id)
    if missing_muscles:
        failures.append(f"missing muscle modules {sorted(missing_muscles)}")
    expected_soft_tissue = {
        f"wk_soft_tissue_{slot}_v01" for slot in (
            "chest", "abdomen", "waist", "hip", "upper_arm",
            "forearm", "thigh", "calf")}
    missing_soft_tissue = expected_soft_tissue - set(module_by_id)
    if missing_soft_tissue:
        failures.append(
            f"missing soft-tissue modules {sorted(missing_soft_tissue)}")

    total_triangles = 0
    kinds: dict[str, int] = {}
    for entry in modules:
        asset_id = str(entry.get("id"))
        kind = str(entry.get("kind"))
        kinds[kind] = kinds.get(kind, 0) + 1
        if entry.get("model_kind") != "component":
            failures.append(f"{asset_id}: published module is not a component")
        if entry.get("layer") not in manifest.get("figure_layer_order", ()) and \
                kind in {"skeleton_part", "muscle_module", "soft_tissue_module",
                         "body_surface",
                         "head_family", "molded_hair", "garment_shell",
                         "armor_shell", "identity_shell", "prop"}:
            failures.append(f"{asset_id}: invalid figure layer {entry.get('layer')}")
        if not entry.get("category") or not entry.get("keywords"):
            failures.append(f"{asset_id}: missing category or keywords")
        allowed = entry.get("allowed_scale")
        if not isinstance(allowed, list) or len(allowed) != 2:
            failures.append(f"{asset_id}: invalid allowed scale")
        elif allowed[0] < 0.95 or allowed[1] > 1.05:
            failures.append(f"{asset_id}: allowed scale exceeds safe fit range")
        bounds = entry.get("bounds", {}).get("size", [])
        if len(bounds) != 3 or min(bounds) <= 0.0 or max(bounds) > 5.0:
            failures.append(f"{asset_id}: implausible bounds {bounds!r}")
        path = ROOT / str(entry.get("export", ""))
        if not path.exists():
            failures.append(f"{asset_id}: missing export {path}")
            continue
        gltf, _ = parse_glb(path)
        stats = collect_stats(path, gltf)
        total_triangles += stats.triangles
        failures.extend(f"{asset_id}: {failure}" for failure in stats.failures)
        if gltf.get("skins"):
            failures.append(f"{asset_id}: component contains a skin")
        if gltf.get("animations"):
            failures.append(f"{asset_id}: component contains animation")
        if kind in {"head_family", "molded_hair"}:
            primitives = [
                primitive
                for mesh in gltf.get("meshes", [])
                for primitive in mesh.get("primitives", [])
            ]
            if not primitives or any(
                    "COLOR_0" not in primitive.get("attributes", {})
                    for primitive in primitives):
                failures.append(
                    f"{asset_id}: runtime identity mesh lacks COLOR_0")
        if stats.triangles > 2500:
            failures.append(f"{asset_id}: {stats.triangles} triangles > 2500")
        if not any(node.get("extras") for node in gltf.get("nodes", [])):
            failures.append(f"{asset_id}: GLB nodes have no application metadata")

    for asset_id in expected_muscles:
        contract = module_by_id[asset_id].get("morph_contract", {})
        if contract.get("operation") != "inflate_cross_section":
            failures.append(f"{asset_id}: missing muscle inflation contract")
        if "bone_length" not in contract.get("preserves", []):
            failures.append(f"{asset_id}: muscle inflation may change bone length")
    for asset_id in expected_soft_tissue:
        contract = module_by_id[asset_id].get("volume_contract", {})
        if contract.get("operation") != "add_soft_volume":
            failures.append(f"{asset_id}: missing soft-volume contract")
        preserved = set(contract.get("preserves", []))
        if not {"bone_length", "joint_center", "socket_transform"} <= preserved:
            failures.append(f"{asset_id}: soft tissue may move the frame")
        if contract.get("range") != [0.0, 0.06]:
            failures.append(f"{asset_id}: soft-tissue range changed")
    for entry in modules:
        if entry.get("layer") == "skin_surface" and \
                set(entry.get("material_roles", ())) - {"skin"}:
            failures.append(
                f"{entry.get('id')}: skin surface contains clothing material roles")
        if entry.get("kind") == "body_surface":
            surface = entry.get("surface_contract", {})
            if surface.get("operation") != "derive_skin_wrap":
                failures.append(
                    f"{entry.get('id')}: missing derived-skin contract")
            inputs = surface.get("inputs", [])
            if not any(str(value).startswith("soft_tissue_fit_envelope.")
                       for value in inputs):
                failures.append(
                    f"{entry.get('id')}: skin ignores soft-tissue envelope")
    fitted = [entry for entry in modules
              if entry.get("layer") in {"garment", "armor"}]
    if sum(bool(entry.get("fit_contract")) for entry in fitted) < 13:
        failures.append("garment and armor fit-envelope contracts are incomplete")

    body_skins = manifest.get("baked_body_skins", [])
    if len(body_skins) != 36:
        failures.append(f"expected 36 baked body skins, found {len(body_skins)}")
    expected_body_recipes = {
        (frame, muscle, tissue)
        for frame in EXPECTED_BUCKS
        for muscle in EXPECTED_MUSCLES
        for tissue in EXPECTED_TISSUES
    }
    actual_body_recipes = {
        (entry.get("frame"), entry.get("muscle_profile"),
         entry.get("soft_tissue_profile"))
        for entry in body_skins
    }
    if actual_body_recipes != expected_body_recipes:
        failures.append("baked body recipe matrix is incomplete")
    for entry in body_skins:
        asset_id = str(entry.get("id"))
        path = ROOT / str(entry.get("export", ""))
        if not path.exists():
            failures.append(f"{asset_id}: missing baked body export {path}")
            continue
        gltf, _ = parse_glb(path)
        stats = collect_stats(path, gltf)
        failures.extend(f"{asset_id}: {failure}" for failure in stats.failures)
        if len(gltf.get("skins", [])) != 1:
            failures.append(f"{asset_id}: expected exactly one skin")
        if gltf.get("animations"):
            failures.append(f"{asset_id}: baked body contains animation")
        if tuple(entry.get("bones", ())) != EXPECTED_BODY_BONES:
            failures.append(f"{asset_id}: stable bone contract changed")
        if stats.triangles > 3000:
            failures.append(f"{asset_id}: {stats.triangles} triangles > 3000")
        for mesh in gltf.get("meshes", []):
            for primitive in mesh.get("primitives", []):
                attributes = set(primitive.get("attributes", {}))
                required = {"POSITION", "NORMAL", "COLOR_0", "JOINTS_0", "WEIGHTS_0"}
                missing = required - attributes
                if missing:
                    failures.append(
                        f"{asset_id}: primitive missing attributes {sorted(missing)}")

    connection_parts = connections.get("parts", [])
    connection_ids = [entry.get("asset_id") for entry in connection_parts]
    if set(connection_ids) != set(module_ids):
        failures.append("shadow connection part IDs do not match geometry modules")
    profiles = set(connections.get("connection_profiles", {}))
    for entry in connection_parts:
        for requirement in entry.get("requires", []):
            if requirement.get("profile") not in profiles:
                failures.append(
                    f"{entry.get('asset_id')}: unknown connection profile "
                    f"{requirement.get('profile')}")

    assemblies = connections.get("assemblies", [])
    if len(assemblies) != 8:
        failures.append(f"expected 8 reference assemblies, found {len(assemblies)}")
    for assembly in assemblies:
        if assembly.get("model_kind") != "assembly":
            failures.append(f"{assembly.get('id')}: is not marked assembly")
        if not assembly.get("variant_sets"):
            failures.append(f"{assembly.get('id')}: has no named variants")
        for reference in assembly.get("references", []):
            asset_id = reference.get("asset")
            if asset_id not in module_by_id:
                failures.append(f"{assembly.get('id')}: unknown part {asset_id}")
            transform = reference.get("local_transform", {})
            if transform.get("scale") != [1.0, 1.0, 1.0]:
                failures.append(
                    f"{assembly.get('id')}: reference {asset_id} uses free scaling")
        if assembly.get("category") == "figure_recipe":
            slots = {str(reference.get("slot"))
                     for reference in assembly.get("references", [])}
            for prefix in ("skeleton.", "muscle.", "soft_tissue.", "garment."):
                if not any(slot.startswith(prefix) for slot in slots):
                    failures.append(
                        f"{assembly.get('id')}: missing {prefix[:-1]} layer")

    if require_review_previews:
        for name, path_text in manifest.get("review_previews", {}).items():
            path = ROOT / path_text
            expected = EXPECTED_REVIEW_SIZE.get(name)
            actual = png_size(path)
            if actual is None:
                failures.append(f"{name}: missing or invalid PNG {path}")
            elif expected and actual != expected:
                failures.append(f"{name}: PNG size {actual} != {expected}")
        if set(manifest.get("review_previews", {})) != set(EXPECTED_REVIEW_SIZE):
            failures.append("review preview set is incomplete")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(f"validated {len(modules)} world-kit components, "
          f"{len(body_skins)} baked body skins, "
          f"{len(assemblies)} reference assemblies, {total_triangles} triangles")
    print("component kinds: " + ", ".join(
        f"{kind}={count}" for kind, count in sorted(kinds.items())))
    checks = "scale, shadow sockets, named variants, and metadata pass"
    if require_review_previews:
        checks = "scale, shadow sockets, named variants, metadata, and review sheets pass"
    print(checks)
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Validate the Crownless world kit.')
    parser.add_argument(
        "--review-previews",
        action="store_true",
        help="also require every generated review PNG",
    )
    args = parser.parse_args()
    raise SystemExit(validate(require_review_previews=args.review_previews))
