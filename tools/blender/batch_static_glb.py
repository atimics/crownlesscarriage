#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct
import sys

from inspect_glb import CHUNK_BIN, CHUNK_JSON, GLB_MAGIC, parse_glb


COMPONENT_FORMATS = {
    5120: "b",
    5121: "B",
    5122: "h",
    5123: "H",
    5125: "I",
    5126: "f",
}
COMPONENT_COUNTS = {
    "SCALAR": 1,
    "VEC2": 2,
    "VEC3": 3,
    "VEC4": 4,
}
SUPPORTED_ATTRIBUTES = {
    "POSITION",
    "NORMAL",
    "COLOR_0",
    "COLOR_1",
    "TEXCOORD_0",
}


def identity_matrix() -> list[list[float]]:
    return [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def multiply_matrix(left: list[list[float]],
                    right: list[list[float]]) -> list[list[float]]:
    return [
        [sum(left[row][axis] * right[axis][column]
             for axis in range(4)) for column in range(4)]
        for row in range(4)
    ]


def node_matrix(node: dict) -> list[list[float]]:
    if "matrix" in node:
        values = node["matrix"]
        return [[float(values[column * 4 + row]) for column in range(4)]
                for row in range(4)]

    translation = node.get("translation", [0.0, 0.0, 0.0])
    scale = node.get("scale", [1.0, 1.0, 1.0])
    x, y, z, w = (float(value) for value in
                  node.get("rotation", [0.0, 0.0, 0.0, 1.0]))
    rotation = [
        [1.0 - 2.0 * (y * y + z * z),
         2.0 * (x * y - z * w),
         2.0 * (x * z + y * w), 0.0],
        [2.0 * (x * y + z * w),
         1.0 - 2.0 * (x * x + z * z),
         2.0 * (y * z - x * w), 0.0],
        [2.0 * (x * z - y * w),
         2.0 * (y * z + x * w),
         1.0 - 2.0 * (x * x + y * y), 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
    for row in range(3):
        for column in range(3):
            rotation[row][column] *= float(scale[column])
        rotation[row][3] = float(translation[row])
    return rotation


def determinant3(matrix: list[list[float]]) -> float:
    return (
        matrix[0][0] * (matrix[1][1] * matrix[2][2] -
                        matrix[1][2] * matrix[2][1]) -
        matrix[0][1] * (matrix[1][0] * matrix[2][2] -
                        matrix[1][2] * matrix[2][0]) +
        matrix[0][2] * (matrix[1][0] * matrix[2][1] -
                        matrix[1][1] * matrix[2][0])
    )


def normal_matrix(matrix: list[list[float]]) -> list[list[float]]:
    upper = [row[:3] for row in matrix[:3]]
    determinant = determinant3(upper)
    if abs(determinant) < 1.0e-12:
        raise ValueError("rigid GLB node has a singular transform")
    inverse = [
        [
            (upper[1][1] * upper[2][2] - upper[1][2] * upper[2][1]),
            (upper[0][2] * upper[2][1] - upper[0][1] * upper[2][2]),
            (upper[0][1] * upper[1][2] - upper[0][2] * upper[1][1]),
        ],
        [
            (upper[1][2] * upper[2][0] - upper[1][0] * upper[2][2]),
            (upper[0][0] * upper[2][2] - upper[0][2] * upper[2][0]),
            (upper[0][2] * upper[1][0] - upper[0][0] * upper[1][2]),
        ],
        [
            (upper[1][0] * upper[2][1] - upper[1][1] * upper[2][0]),
            (upper[0][1] * upper[2][0] - upper[0][0] * upper[2][1]),
            (upper[0][0] * upper[1][1] - upper[0][1] * upper[1][0]),
        ],
    ]
    inverse = [[value / determinant for value in row] for row in inverse]
    return [[inverse[column][row] for column in range(3)] for row in range(3)]


def transform_position(matrix: list[list[float]],
                       value: tuple[float, ...]) -> tuple[float, ...]:
    return tuple(sum(matrix[row][column] * value[column]
                     for column in range(3)) + matrix[row][3]
                 for row in range(3))


def transform_normal(matrix: list[list[float]],
                     value: tuple[float, ...]) -> tuple[float, ...]:
    transformed = tuple(sum(matrix[row][column] * value[column]
                            for column in range(3)) for row in range(3))
    length = math.sqrt(sum(component * component for component in transformed))
    if length <= 1.0e-12:
        return (0.0, 1.0, 0.0)
    return tuple(component / length for component in transformed)


def normalized_component(value: int, component_type: int) -> float:
    if component_type == 5120:
        return max(float(value) / 127.0, -1.0)
    if component_type == 5121:
        return float(value) / 255.0
    if component_type == 5122:
        return max(float(value) / 32767.0, -1.0)
    if component_type == 5123:
        return float(value) / 65535.0
    raise ValueError(f"unsupported normalized component type {component_type}")


def read_accessor(document: dict, binary: bytes,
                  accessor_index: int) -> list[tuple[float, ...]]:
    accessor = document["accessors"][accessor_index]
    if "sparse" in accessor or "bufferView" not in accessor:
        raise ValueError("sparse or implicit accessors are not supported")
    component_type = accessor["componentType"]
    value_type = accessor["type"]
    if component_type not in COMPONENT_FORMATS or value_type not in COMPONENT_COUNTS:
        raise ValueError("unsupported GLB accessor format")
    count = COMPONENT_COUNTS[value_type]
    fmt = "<" + COMPONENT_FORMATS[component_type] * count
    element_size = struct.calcsize(fmt)
    view = document["bufferViews"][accessor["bufferView"]]
    stride = view.get("byteStride", element_size)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    values: list[tuple[float, ...]] = []
    for index in range(accessor["count"]):
        value = struct.unpack_from(fmt, binary, offset + index * stride)
        if accessor.get("normalized"):
            value = tuple(normalized_component(component, component_type)
                          for component in value)
        values.append(tuple(float(component) for component in value))
    return values


def read_indices(document: dict, binary: bytes,
                 primitive: dict, vertex_count: int) -> list[int]:
    if "indices" not in primitive:
        return list(range(vertex_count))
    return [int(value[0]) for value in
            read_accessor(document, binary, primitive["indices"])]


def collect_groups(document: dict, binary: bytes) -> tuple[dict, dict]:
    if document.get("skins") or document.get("animations"):
        raise ValueError("batch_static_glb only accepts rigid models")
    if any("bufferView" in image for image in document.get("images", [])):
        raise ValueError("embedded GLB images cannot be batched")

    nodes = document.get("nodes", [])
    meshes = document.get("meshes", [])
    groups: dict[tuple[int, tuple[str, ...]], dict[str, object]] = {}
    metadata: dict[str, object] = {}

    def visit(node_index: int, parent: list[list[float]]) -> None:
        node = nodes[node_index]
        world = multiply_matrix(parent, node_matrix(node))
        if "mesh" in node:
            if "skin" in node:
                raise ValueError("batch_static_glb does not accept skinned nodes")
            extras = node.get("extras") or {}
            for key in ("cc_asset_id", "cc_library_version"):
                if key in extras and key not in metadata:
                    metadata[key] = extras[key]
            mesh = meshes[node["mesh"]]
            normal_transform = normal_matrix(world)
            reverse_winding = determinant3([row[:3] for row in world[:3]]) < 0.0
            for primitive in mesh.get("primitives", []):
                if primitive.get("mode", 4) != 4:
                    raise ValueError("only triangle primitives can be batched")
                if primitive.get("targets"):
                    raise ValueError("morph targets cannot be batched")
                attributes = primitive.get("attributes", {})
                unsupported = set(attributes) - SUPPORTED_ATTRIBUTES
                if unsupported:
                    raise ValueError(
                        f"unsupported attributes: {sorted(unsupported)}")
                layout = tuple(sorted(attributes))
                material = int(primitive.get("material", -1))
                key = (material, layout)
                group = groups.setdefault(key, {
                    "attributes": {semantic: [] for semantic in layout},
                    "indices": [],
                })
                decoded = {semantic: read_accessor(document, binary, accessor)
                           for semantic, accessor in attributes.items()}
                vertex_count = len(decoded.get("POSITION", []))
                if vertex_count <= 0 or any(len(values) != vertex_count
                                            for values in decoded.values()):
                    raise ValueError("primitive attributes have inconsistent counts")
                destination = group["attributes"]
                base = len(destination["POSITION"])
                for semantic, values in decoded.items():
                    if semantic == "POSITION":
                        values = [transform_position(world, value)
                                  for value in values]
                    elif semantic == "NORMAL":
                        values = [transform_normal(normal_transform, value)
                                  for value in values]
                    destination[semantic].extend(values)
                indices = read_indices(document, binary, primitive, vertex_count)
                if len(indices) % 3 != 0:
                    raise ValueError("triangle index count is not divisible by three")
                if reverse_winding:
                    for index in range(0, len(indices), 3):
                        indices[index + 1], indices[index + 2] = (
                            indices[index + 2], indices[index + 1])
                group["indices"].extend(base + index for index in indices)
        for child in node.get("children", []):
            visit(child, world)

    scene_index = document.get("scene", 0)
    scenes = document.get("scenes", [{}])
    roots = scenes[scene_index].get("nodes", []) if scenes else []
    if not roots:
        children = {child for node in nodes for child in node.get("children", [])}
        roots = [index for index in range(len(nodes)) if index not in children]
    for root in roots:
        visit(root, identity_matrix())
    if not groups:
        raise ValueError("GLB contains no rigid mesh primitives")
    return groups, metadata


def append_buffer_view(binary: bytearray, views: list[dict], data: bytes,
                       target: int) -> int:
    binary.extend(b"\0" * (-len(binary) % 4))
    offset = len(binary)
    binary.extend(data)
    views.append({
        "buffer": 0,
        "byteOffset": offset,
        "byteLength": len(data),
        "target": target,
    })
    return len(views) - 1


def encode_attribute(semantic: str, values: list[tuple[float, ...]]) -> tuple:
    if semantic in {"POSITION", "NORMAL"}:
        value_type = "VEC3"
        flattened = [float(component) for value in values
                     for component in value[:3]]
        return struct.pack(f"<{len(flattened)}f", *flattened), 5126, value_type, False
    if semantic == "TEXCOORD_0":
        flattened = [float(component) for value in values
                     for component in value[:2]]
        return struct.pack(f"<{len(flattened)}f", *flattened), 5126, "VEC2", False
    if semantic.startswith("COLOR_"):
        colors: list[int] = []
        for value in values:
            rgba = tuple(value[:4]) if len(value) >= 4 else tuple(value[:3]) + (1.0,)
            colors.extend(round(max(0.0, min(1.0, component)) * 65535.0)
                          for component in rgba)
        return struct.pack(f"<{len(colors)}H", *colors), 5123, "VEC4", True
    raise ValueError(f"cannot encode attribute {semantic}")


def build_document(source: dict, groups: dict, metadata: dict,
                   source_name: str) -> tuple[dict, bytes]:
    binary = bytearray()
    views: list[dict] = []
    accessors: list[dict] = []
    primitives: list[dict] = []

    for (material, layout), group in sorted(groups.items()):
        primitive_attributes: dict[str, int] = {}
        attributes = group["attributes"]
        for semantic in layout:
            values = attributes[semantic]
            payload, component_type, value_type, normalized = encode_attribute(
                semantic, values)
            view = append_buffer_view(binary, views, payload, 34962)
            accessor = {
                "bufferView": view,
                "componentType": component_type,
                "count": len(values),
                "type": value_type,
            }
            if normalized:
                accessor["normalized"] = True
            if semantic == "POSITION":
                accessor["min"] = [min(value[axis] for value in values)
                                   for axis in range(3)]
                accessor["max"] = [max(value[axis] for value in values)
                                   for axis in range(3)]
            accessors.append(accessor)
            primitive_attributes[semantic] = len(accessors) - 1

        indices = group["indices"]
        component_type = 5123 if max(indices) <= 65535 else 5125
        fmt = "H" if component_type == 5123 else "I"
        payload = struct.pack(f"<{len(indices)}{fmt}", *indices)
        view = append_buffer_view(binary, views, payload, 34963)
        accessors.append({
            "bufferView": view,
            "componentType": component_type,
            "count": len(indices),
            "type": "SCALAR",
            "min": [min(indices)],
            "max": [max(indices)],
        })
        primitive = {
            "attributes": primitive_attributes,
            "indices": len(accessors) - 1,
            "mode": 4,
        }
        if material >= 0:
            primitive["material"] = material
        primitives.append(primitive)

    generator = source.get("asset", {}).get("generator", "")
    asset = dict(source.get("asset", {"version": "2.0"}))
    asset["version"] = "2.0"
    asset["generator"] = (generator + " + Crownless static batcher").strip(" +")
    extras = {
        "cc_asset_id": metadata.get("cc_asset_id", Path(source_name).stem),
        "cc_role": "batched_static",
        "cc_library_version": metadata.get("cc_library_version", "web"),
        "cc_paint_material": "mixed",
    }
    document = {
        "asset": asset,
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": views,
        "accessors": accessors,
        "materials": source.get("materials", []),
        "meshes": [{"name": "GEO_BATCHED", "primitives": primitives}],
        "nodes": [{"name": "GEO_BATCHED", "mesh": 0, "extras": extras}],
        "scenes": [{"name": "Scene", "nodes": [0]}],
        "scene": 0,
    }
    for key in ("extensionsUsed", "extensionsRequired", "extensions",
                "textures", "images", "samplers"):
        if key in source:
            document[key] = source[key]
    return document, bytes(binary)


def write_glb(path: Path, document: dict, binary: bytes) -> None:
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    binary += b"\0" * (-len(binary) % 4)
    chunks = (
        struct.pack("<II", len(encoded), CHUNK_JSON) + encoded +
        struct.pack("<II", len(binary), CHUNK_BIN) + binary
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<4sII", GLB_MAGIC, 2, 12 + len(chunks)) +
                     chunks)


def batch_static_glb(source_path: Path, destination_path: Path) -> tuple[int, int]:
    source, binary = parse_glb(source_path)
    before = sum(len(mesh.get("primitives", []))
                 for mesh in source.get("meshes", []))
    groups, metadata = collect_groups(source, binary)
    document, output = build_document(source, groups, metadata,
                                      source_path.name)
    write_glb(destination_path, document, output)
    return before, len(groups)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description='Batch rigid GLB primitives by material for the browser build.')
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", nargs="?", type=Path)
    args = parser.parse_args(argv)
    destination = args.destination or args.source
    before, after = batch_static_glb(args.source, destination)
    print(f"Batched {args.source.name}: {before} -> {after} primitives")
    return 0


if __name__ == "__main__":
    sys.exit(main())
