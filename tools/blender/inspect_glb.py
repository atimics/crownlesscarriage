#!/usr/bin/env python3
"""Inspect and validate exported Crownless GLB assets without Blender.

The Blender validators reopen the generated .blend files. This tool is the
other half of the export contract: it checks the shipped GLB bytes themselves,
so structural regressions are caught in environments where Blender is not
installed (CI runners, review tooling, runtime packaging).

Checks performed:

- GLB container structure: magic, version, declared length, chunk layout,
  chunk padding, and trailing data.
- glTF document sanity: asset version, scene graph, accessor bounds.
- Geometry statistics: nodes, meshes, primitives, vertices, triangles, and a
  world-space bounding box composed from node transforms.
- Library profile contract: every mesh node carries ``cc_asset_id`` matching
  the file name, a non-empty ``cc_role``, and ``cc_library_version``; mesh
  nodes are named ``GEO_*``; materials use the shared ``MAT_*`` palette;
  geometry stays inside plausible world extents and triangle budgets.
- Optional manifest cross-check: every manifest asset has an export and every
  export on disk is referenced by the manifest (stale-export detection).

Usage:
    python3 tools/blender/inspect_glb.py assets/exports/glb/*.glb
    python3 tools/blender/inspect_glb.py --profile library \
        --manifest assets/asset_manifest.json --export-dir assets/exports/glb \
        assets/exports/glb/*.glb
    python3 tools/blender/inspect_glb.py --report /tmp/glb_report.json ...

Exit code is non-zero when any check fails.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

GLB_MAGIC = b"glTF"
GLB_VERSION = 2
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

MAX_WORLD_EXTENT_M = 100.0
DEFAULT_MAX_TRIANGLES = 40000


class GlbError(Exception):
    """Structural or contract failure for one GLB file."""


@dataclass
class GlbStats:
    path: Path
    nodes: int = 0
    meshes: int = 0
    primitives: int = 0
    vertices: int = 0
    triangles: int = 0
    materials: list[str] = field(default_factory=list)
    bounds_min: tuple[float, float, float] | None = None
    bounds_max: tuple[float, float, float] | None = None
    failures: list[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.failures


def parse_glb(path: Path) -> tuple[dict, bytes]:
    """Return (json document, binary chunk) or raise GlbError."""
    data = path.read_bytes()
    if len(data) < 12:
        raise GlbError("file too small for a GLB header")
    magic, version, declared = struct.unpack_from("<4sII", data, 0)
    if magic != GLB_MAGIC:
        raise GlbError(f"bad magic {magic!r}")
    if version != GLB_VERSION:
        raise GlbError(f"unsupported GLB version {version}")
    if declared != len(data):
        raise GlbError(f"declared length {declared} != file size {len(data)}")

    offset = 12
    document: dict | None = None
    binary = b""
    while offset < len(data):
        if offset + 8 > len(data):
            raise GlbError("truncated chunk header")
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        if chunk_length % 4:
            raise GlbError(f"chunk at {offset - 8} is not 4-byte aligned")
        if offset + chunk_length > len(data):
            raise GlbError("chunk overruns file")
        chunk = data[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == CHUNK_JSON:
            if document is not None:
                raise GlbError("duplicate JSON chunk")
            try:
                document = json.loads(chunk)
            except json.JSONDecodeError as exc:
                raise GlbError(f"JSON chunk does not parse: {exc}") from exc
        elif chunk_type == CHUNK_BIN:
            if binary:
                raise GlbError("duplicate BIN chunk")
            binary = chunk
        else:
            raise GlbError(f"unknown chunk type 0x{chunk_type:08X}")

    if document is None:
        raise GlbError("missing JSON chunk")
    asset = document.get("asset", {})
    if asset.get("version") != "2.0":
        raise GlbError(f"glTF asset version {asset.get('version')!r} != '2.0'")
    if not isinstance(document.get("nodes", []), list):
        raise GlbError("document nodes are not a list")
    return document, binary


def quat_to_matrix3(x: float, y: float, z: float, w: float) -> list[list[float]]:
    return [
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ]


def node_matrix(node: dict) -> list[list[float]]:
    """Column-vector 4x4 world transform for one node (row-major storage)."""
    if "matrix" in node:
        m = node["matrix"]  # glTF is column-major
        return [
            [m[0], m[4], m[8], m[12]],
            [m[1], m[5], m[9], m[13]],
            [m[2], m[6], m[10], m[14]],
            [m[3], m[7], m[11], m[15]],
        ]
    t = node.get("translation", [0.0, 0.0, 0.0])
    q = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = node.get("scale", [1.0, 1.0, 1.0])
    r = quat_to_matrix3(*q)
    return [
        [r[0][0] * s[0], r[0][1] * s[1], r[0][2] * s[2], t[0]],
        [r[1][0] * s[0], r[1][1] * s[1], r[1][2] * s[2], t[1]],
        [r[2][0] * s[0], r[2][1] * s[1], r[2][2] * s[2], t[2]],
        [0.0, 0.0, 0.0, 1.0],
    ]


def mat_mul(a: list[list[float]], b: list[list[float]]) -> list[list[float]]:
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def mat_point(m: list[list[float]], p: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = p
    return (
        m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
        m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
        m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3],
    )


def collect_stats(path: Path, document: dict) -> GlbStats:
    stats = GlbStats(path=path)
    nodes = document.get("nodes", [])
    meshes = document.get("meshes", [])
    accessors = document.get("accessors", [])
    stats.nodes = len(nodes)
    stats.meshes = len(meshes)
    stats.materials = [m.get("name", "<unnamed>") for m in document.get("materials", [])]

    identity = [[1.0 if i == j else 0.0 for j in range(4)] for i in range(4)]
    corners_min = [math.inf] * 3
    corners_max = [-math.inf] * 3

    def visit(index: int, parent: list[list[float]]) -> None:
        node = nodes[index]
        world = mat_mul(parent, node_matrix(node))
        if "mesh" in node:
            mesh = meshes[node["mesh"]]
            for primitive in mesh.get("primitives", []):
                stats.primitives += 1
                attributes = primitive.get("attributes", {})
                position_index = attributes.get("POSITION")
                if position_index is None:
                    stats.failures.append(f"mesh {mesh.get('name')!r} primitive has no POSITION")
                    continue
                position = accessors[position_index]
                stats.vertices += position.get("count", 0)
                indices = primitive.get("indices")
                if indices is not None:
                    stats.triangles += accessors[indices].get("count", 0) // 3
                else:
                    stats.triangles += position.get("count", 0) // 3
                lo, hi = position.get("min"), position.get("max")
                if lo is None or hi is None:
                    stats.failures.append(f"POSITION accessor {position_index} lacks min/max")
                    continue
                for corner in range(8):
                    point = (
                        lo[0] if corner & 1 else hi[0],
                        lo[1] if corner & 2 else hi[1],
                        lo[2] if corner & 4 else hi[2],
                    )
                    wx, wy, wz = mat_point(world, point)
                    corners_min[0] = min(corners_min[0], wx)
                    corners_min[1] = min(corners_min[1], wy)
                    corners_min[2] = min(corners_min[2], wz)
                    corners_max[0] = max(corners_max[0], wx)
                    corners_max[1] = max(corners_max[1], wy)
                    corners_max[2] = max(corners_max[2], wz)
        for child in node.get("children", []):
            visit(child, world)

    scene = document.get("scenes", [{}])[document.get("scene", 0)]
    roots = scene.get("nodes", [])
    if not roots and nodes:
        roots = list(range(len(nodes)))
    for root in roots:
        visit(root, identity)

    if math.isfinite(corners_min[0]):
        stats.bounds_min = tuple(round(v, 3) for v in corners_min)
        stats.bounds_max = tuple(round(v, 3) for v in corners_max)
    return stats


def accessor_first_values(document: dict, binary: bytes,
                          accessor_index: int) -> tuple[float, ...]:
    accessor = document["accessors"][accessor_index]
    view = document["bufferViews"][accessor["bufferView"]]
    component_type = accessor["componentType"]
    component_formats = {5121: "B", 5123: "H", 5126: "f"}
    component_counts = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    if component_type not in component_formats or accessor["type"] not in component_counts:
        return ()
    count = component_counts[accessor["type"]]
    fmt = "<" + component_formats[component_type] * count
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    values = struct.unpack_from(fmt, binary, offset)
    if accessor.get("normalized"):
        divisor = 255.0 if component_type == 5121 else 65535.0
        values = tuple(value / divisor for value in values)
    return tuple(float(value) for value in values)


def check_library_contract(stats: GlbStats, document: dict, binary: bytes,
                           max_triangles: int) -> None:
    """Enforce the crownless_asset_library export contract on one GLB."""
    stem = stats.path.stem
    nodes = document.get("nodes", [])
    meshes = document.get("meshes", [])
    mesh_nodes = [n for n in nodes if "mesh" in n]
    if not mesh_nodes:
        stats.failures.append("no mesh nodes")
    for node in mesh_nodes:
        name = node.get("name", "")
        if not name.startswith("GEO_"):
            stats.failures.append(f"mesh node {name!r} is not named GEO_*")
        extras = node.get("extras") or {}
        asset_id = extras.get("cc_asset_id")
        if asset_id != stem:
            stats.failures.append(f"node {name!r} cc_asset_id {asset_id!r} != file stem {stem!r}")
        if not extras.get("cc_role"):
            stats.failures.append(f"node {name!r} has no cc_role")
        if not extras.get("cc_library_version"):
            stats.failures.append(f"node {name!r} has no cc_library_version")
        if stem == "environment_market_granary_v01":
            if not extras.get("cc_paint_material"):
                stats.failures.append(
                    f"painted market node {name!r} has no material class")
            mesh = meshes[node["mesh"]]
            for primitive in mesh.get("primitives", []):
                attributes = primitive.get("attributes", {})
                if "COLOR_0" not in attributes:
                    stats.failures.append(
                        f"painted market node {name!r} has no COLOR_0")
                    continue
                paint_sample = accessor_first_values(
                    document, binary, attributes["COLOR_0"])
                if len(paint_sample) < 3 or all(
                    component > 0.98 for component in paint_sample[:3]
                ):
                    stats.failures.append(
                        f"painted market node {name!r} has a blank COLOR_0")
    for material in stats.materials:
        if not material.startswith("MAT_"):
            stats.failures.append(f"material {material!r} is not named MAT_*")
    if stats.bounds_min and stats.bounds_max:
        for axis in range(3):
            if abs(stats.bounds_min[axis]) > MAX_WORLD_EXTENT_M or abs(stats.bounds_max[axis]) > MAX_WORLD_EXTENT_M:
                stats.failures.append("geometry exceeds plausible world extents")
                break
    if stats.triangles > max_triangles:
        stats.failures.append(f"triangle budget exceeded: {stats.triangles} > {max_triangles}")


def check_manifest(manifest_path: Path, export_dir: Path) -> list[str]:
    """Cross-check manifest assets against exports on disk."""
    failures: list[str] = []
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    referenced = {asset["id"]: asset["export"] for asset in manifest.get("assets", [])}
    for asset_id, export in sorted(referenced.items()):
        candidate = manifest_path.parent / export
        if not candidate.exists():
            failures.append(f"manifest asset {asset_id} has no export at {export}")
        elif candidate.parent.resolve() != export_dir.resolve():
            failures.append(f"manifest asset {asset_id} exports outside {export_dir}")
    for glb in sorted(export_dir.glob("*.glb")):
        if glb.stem not in referenced:
            failures.append(f"stale export {glb.name} is not referenced by the manifest")
    return failures


def format_row(stats: GlbStats) -> str:
    if stats.bounds_min and stats.bounds_max:
        dims = tuple(round(stats.bounds_max[i] - stats.bounds_min[i], 2) for i in range(3))
        bounds = f"{dims[0]:.2f}x{dims[1]:.2f}x{dims[2]:.2f}m"
    else:
        bounds = "-"
    status = "ok" if stats.ok else "FAIL"
    return (
        f"{stats.path.name:<44} {stats.nodes:>5} {stats.meshes:>5} "
        f"{stats.triangles:>6} {stats.vertices:>7} {len(stats.materials):>4} {bounds:>16} {status}"
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("files", nargs="+", type=Path, help="GLB files to inspect")
    parser.add_argument("--profile", choices=("generic", "library"), default="generic",
                        help="contract profile; 'library' enforces the asset-library naming/metadata rules")
    parser.add_argument("--manifest", type=Path, help="asset manifest for cross-checks")
    parser.add_argument("--export-dir", type=Path, help="export directory scanned for stale GLBs")
    parser.add_argument("--max-tris", type=int, default=DEFAULT_MAX_TRIANGLES, help="per-file triangle budget")
    parser.add_argument("--report", type=Path, help="write a JSON statistics report")
    args = parser.parse_args(argv)

    if args.export_dir and not args.manifest:
        parser.error("--export-dir requires --manifest")

    results: list[GlbStats] = []
    for path in sorted(args.files):
        stats = GlbStats(path=path)
        try:
            document, binary = parse_glb(path)
            stats = collect_stats(path, document)
            if args.profile == "library":
                check_library_contract(stats, document, binary,
                                       args.max_tris)
        except GlbError as exc:
            stats.failures.append(str(exc))
        results.append(stats)

    print(f"{'file':<44} {'nodes':>5} {'mesh':>5} {'tris':>6} {'verts':>7} {'mat':>4} {'bounds':>16} status")
    for stats in results:
        print(format_row(stats))
        for failure in stats.failures:
            print(f"  FAIL {stats.path.name}: {failure}", file=sys.stderr)

    manifest_failures: list[str] = []
    if args.manifest:
        manifest_failures = check_manifest(args.manifest, args.export_dir)
        for failure in manifest_failures:
            print(f"  FAIL manifest: {failure}", file=sys.stderr)

    if args.report:
        report = {
            "files": [
                {
                    "file": str(stats.path),
                    "nodes": stats.nodes,
                    "meshes": stats.meshes,
                    "primitives": stats.primitives,
                    "vertices": stats.vertices,
                    "triangles": stats.triangles,
                    "materials": stats.materials,
                    "bounds_min": stats.bounds_min,
                    "bounds_max": stats.bounds_max,
                    "failures": stats.failures,
                }
                for stats in results
            ],
            "manifest_failures": manifest_failures,
        }
        args.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    failed = sum(not stats.ok for stats in results) + len(manifest_failures)
    total_tris = sum(stats.triangles for stats in results)
    print(f"Inspected {len(results)} GLB files ({total_tris} triangles total); {failed} failure(s).")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
