"""Build the browser camera capture fixture from an existing Emscripten build."""

import argparse
from pathlib import Path
import re
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("build", type=Path)
parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument("--output", type=Path)
args = parser.parse_args()
source = args.source.resolve()
build = args.build.resolve()
output = (args.output or build / "camera-capture").resolve()
output.mkdir(parents=True, exist_ok=True)
assets = output / "assets"
shutil.copytree(build / "web-runtime/assets", assets, dirs_exist_ok=True)
for shader in (source / "assets/shaders").iterdir():
    if shader.suffix in (".fs", ".vs"):
        (assets / "shaders" / shader.name).write_text(re.sub(
            r"^#version 330[\r\n]+",
            "#version 300 es\n\nprecision highp float;\nprecision highp int;\n",
            shader.read_text()))
raylib = build / "_deps/raylib-src"
for line in (build / "CMakeCache.txt").read_text().splitlines():
    if line.startswith("FETCHCONTENT_SOURCE_DIR_RAYLIB:PATH="):
        value = line.split("=", 1)[1]
        if value:
            raylib = Path(value)
libraries = ["local_renderer", "locomotion", "creature_catalog", "local_place",
             "client_policy", "road_book", "world", "sim"]
fixture = Path(__file__).resolve().parents[1] / "tests/camera_motion_capture.c"
command = ["emcc", "-O2", "-std=c17", "-DPLATFORM_WEB", "-DGRAPHICS_API_OPENGL_ES3",
           '-DCC_ASSET_SOURCE_ROOT="/"', "-I", str(source), "-I", str(source / "src"),
           "-I", str(raylib / "src"), str(fixture)]
command += [str(build / f"libcrownless_{name}.a") for name in libraries]
command += [str(build / "_deps/raylib-build/raylib/libraylib.a"),
            "-sUSE_GLFW=3", "-sMIN_WEBGL_VERSION=2", "-sMAX_WEBGL_VERSION=2",
            "-sASYNCIFY=1", "-sALLOW_MEMORY_GROWTH=1", "-sSTACK_SIZE=4194304",
            "-sMAXIMUM_MEMORY=536870912", "-sEXIT_RUNTIME=0",
            "-sEXPORTED_FUNCTIONS=_main,_CaptureTown,_CaptureTests",
            "-sEXPORTED_RUNTIME_METHODS=ccall", "--preload-file", str(assets) + "@/assets",
            "--preload-file", str(build / "web-runtime/lazy-assets") + "@/assets",
            "-o", str(output / "capture.html")]
subprocess.run(command, check=True)
