#!/usr/bin/env python3

import json
import re
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
WEBGL2_MIN_VERTEX_UNIFORM_VECTORS = 256


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read_glb_json(path: Path) -> dict:
    data = path.read_bytes()
    require(len(data) >= 20, f"{path}: GLB is too short")
    magic, version, length = struct.unpack_from("<4sII", data)
    require(magic == b"glTF", f"{path}: GLB magic is invalid")
    require(version == 2, f"{path}: GLB version {version} is unsupported")
    require(length == len(data), f"{path}: GLB length is invalid")
    offset = 12
    while offset + 8 <= len(data):
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset:offset + chunk_length]
        offset += chunk_length
        if chunk_type == 0x4E4F534A:
            return json.loads(chunk.rstrip(b" \x00").decode("utf-8"))
    raise AssertionError(f"{path}: GLB JSON chunk is missing")


def main() -> None:
    shader_path = ROOT / "assets/shaders/world_lit_skinned.vs"
    shader = shader_path.read_text(encoding="utf-8")
    matrix_uniforms = re.findall(
        r"\buniform\s+mat4\s+\w+(?:\s*\[\s*(\d+)\s*\])?\s*;",
        shader,
    )
    require(matrix_uniforms, "skinned shader has no mat4 uniforms")
    matrix_count = sum(int(size) if size else 1 for size in matrix_uniforms)
    vector_count = matrix_count * 4
    require(
        vector_count <= WEBGL2_MIN_VERTEX_UNIFORM_VECTORS,
        f"skinned shader needs {vector_count} vertex uniform vectors; "
        f"WebGL2 guarantees {WEBGL2_MIN_VERTEX_UNIFORM_VECTORS}",
    )

    palette_match = re.search(r"boneMatrices\s*\[\s*(\d+)\s*\]", shader)
    require(palette_match is not None, "skinned shader bone palette is missing")
    palette_size = int(palette_match.group(1))

    client_path = ROOT / "src/client/local3d/asset_loading.inc"
    client = client_path.read_text(encoding="utf-8")
    runtime_match = re.search(
        r"#define\s+CC_HERO_SKIN_MAX_BONES\s+(\d+)", client
    )
    require(runtime_match is not None, "client skin palette limit is missing")
    runtime_limit = int(runtime_match.group(1))
    require(
        runtime_limit == palette_size,
        f"client accepts {runtime_limit} bones but shader accepts {palette_size}",
    )
    require(
        client.count("bone_count > CC_HERO_SKIN_MAX_BONES") >= 2,
        "hero and NPC loaders must reject rigs larger than the shader palette",
    )

    skinned_assets = 0
    largest_skin = 0
    for asset_path in sorted((ROOT / "assets/exports").rglob("*.glb")):
        document = read_glb_json(asset_path)
        for skin in document.get("skins", []):
            joint_count = len(skin.get("joints", []))
            skinned_assets += 1
            largest_skin = max(largest_skin, joint_count)
            require(
                0 < joint_count <= palette_size,
                f"{asset_path}: skin has {joint_count} joints; "
                f"shader palette accepts {palette_size}",
            )
    require(skinned_assets > 0, "no shipped skinned assets were checked")
    print(
        f"WebGL2 skinning budget passed: {vector_count}/"
        f"{WEBGL2_MIN_VERTEX_UNIFORM_VECTORS} vectors, "
        f"{skinned_assets} skins, largest skin {largest_skin} bones"
    )


if __name__ == "__main__":
    main()
