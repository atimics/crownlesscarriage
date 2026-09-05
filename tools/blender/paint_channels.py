from __future__ import annotations

from collections.abc import Sequence

import bpy


# Opaque character shaders read COLOR_0.a as a surface label. One is legacy.
SURFACE_CLASSES = ("skin", "cloth", "leather", "hair", "metal", "eye")
SURFACE_CONTRACT = "COLOR_0:palette,value,fold,surface; surface=(id+0.5)/8"


def surface_class(name: str) -> str:
    lowered = name.lower()
    for label, tokens in (
        ("eye", ("eye", "mouth")),
        ("skin", ("skin",)),
        ("hair", ("hair", "brow")),
        ("metal", ("metal", "steel", "brass", "gold", "accent")),
        ("leather", ("leather",)),
    ):
        if any(token in lowered for token in tokens):
            return label
    return "cloth"


def semantic_value_base(name: str) -> float:
    lowered = name.lower()
    if any(token in lowered for token in ("eye", "hair", "dark", "shadow")):
        return 0.30
    if "skin" in lowered:
        return 0.56
    if any(token in lowered for token in ("metal", "steel", "brass", "gold",
                                           "accent")):
        return 0.62
    return 0.50


def semantic_fold_strength(name: str) -> float:
    lowered = name.lower()
    if any(token in lowered for token in ("shadow", "eye")):
        return 0.82
    if "hair" in lowered:
        return 0.68
    if any(token in lowered for token in ("outer", "cloth", "tunic", "cape",
                                           "trouser", "leather", "padding",
                                           "underlayer", "oxblood", "teal")):
        return 0.52
    if "skin" in lowered:
        return 0.18
    return 0.28


def face_value(normal: bpy.types.MeshPolygon, semantic_name: str) -> float:
    value = semantic_value_base(semantic_name)
    if normal.normal.z > 0.52:
        value += 0.20
    elif normal.normal.z < -0.52:
        value -= 0.18
    elif normal.normal.y < -0.42:
        value += 0.08
    elif normal.normal.x > 0.48:
        value -= 0.07
    if value < 0.40:
        return 0.25
    if value < 0.64:
        return 0.50
    return 0.75


def add_indexed_paint_channels(
    obj: bpy.types.Object,
    polygon_indices: Sequence[int],
    semantic_names: Sequence[str],
    *,
    surface_labels: bool = False,
    value_offset: float = 0.0,
) -> bpy.types.ByteColorAttribute:
    mesh = obj.data
    if len(polygon_indices) != len(mesh.polygons):
        raise RuntimeError(f"{obj.name} paint indices do not match polygons")
    for existing in tuple(mesh.color_attributes):
        if existing.name == "COLOR_0":
            mesh.color_attributes.remove(existing)
    attribute = mesh.color_attributes.new(
        name="COLOR_0", type="FLOAT_COLOR", domain="CORNER")
    palette_count = len(semantic_names)
    for polygon, semantic_index in zip(mesh.polygons, polygon_indices):
        if semantic_index < 0 or semantic_index >= palette_count:
            raise RuntimeError(f"{obj.name} has invalid paint semantic index")
        palette = (float(semantic_index) + 0.5) / float(palette_count)
        semantic = semantic_names[semantic_index]
        value = face_value(polygon, semantic)
        if value_offset:
            value = max(0.25, min(0.75, value + value_offset))
        fold = semantic_fold_strength(semantic)
        surface = surface_class(semantic)
        alpha = (SURFACE_CLASSES.index(surface) + 0.5) / 8.0 \
            if surface_labels else 1.0
        for loop_index in polygon.loop_indices:
            attribute.data[loop_index].color = (palette, value, fold, alpha)
    mesh.color_attributes.active_color = attribute
    obj["cc_paint_contract"] = SURFACE_CONTRACT if surface_labels else \
        "COLOR_0:palette,value,fold"
    if surface_labels:
        obj["cc_surface_classes"] = list(SURFACE_CLASSES)
    return attribute
