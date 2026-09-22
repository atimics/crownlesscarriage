from pathlib import Path
import hashlib


def replace(path, old, new):
    p = Path(path)
    text = p.read_text()
    assert text.count(old) == 1, (path, old[:80], text.count(old))
    p.write_text(text.replace(old, new))

replace('src/client/local3d/town_sky.inc',
        'float v=(float)(i+1)*(2.0f*PI/(float)TOWN_RIDGE_SECTORS;',
        'float v=(float)(i+1)*(2.0f*PI/(float)TOWN_RIDGE_SECTORS);')
replace('src/client/cc_local3d.c',
        '#include "client/local3d/road_book.inc"',
        '#include "client/local3d/town_sky.inc"\n#include "client/local3d/road_book.inc"')
p = Path('src/client/local3d/authored_places.inc')
s = p.read_text()
a = s.index('static void DrawBackdropPeak(')
b = s.index('static void DrawTownTerrainStage(', a)
s = s[:a] + s[b:]
assert s.count('    DrawTownBackdrop(place);\n') == 1
p.write_text(s.replace('    DrawTownBackdrop(place);\n', ''))
replace('src/client/local3d/road_book.inc',
        '    BeginMode3D(camera);\n    BeginWorldLighting(camera, &street_art);',
        '    BeginMode3D(camera);\n    DrawTownSky(sim, place, camera, clock);\n    DrawTownHorizon(sim, place);\n    BeginWorldLighting(camera, &street_art);\n    SetTownSkyFog(sim, place);')
replace('tests/renderer_regression_tests.c',
        '#include "ground_town_camera_tests.inc"',
        '#include "ground_town_camera_tests.inc"\n#include "town_sky_tests.inc"')
replace('tests/renderer_regression_tests.c',
        '    TestCarriageWorldTargets();',
        '    TestTownSkyContract();\n    if (argc == 2 && strcmp(argv[1], "--sky-data") == 0) return 0;\n    TestCarriageWorldTargets();')
replace('tests/renderer_regression_tests.c',
        '    if (argc == 3 && (strcmp(argv[1], "--graphics") == 0 ||',
        '    if (argc == 3 && (strcmp(argv[1], "--sky-captures") == 0 ||\n                      strcmp(argv[1], "--sky-graphics") == 0 ||\n                      strcmp(argv[1], "--graphics") == 0 ||')
replace('tests/renderer_regression_tests.c',
        '        if (strcmp(argv[1], "--carriage-graphics") == 0) {',
        '        if (strcmp(argv[1], "--sky-captures") == 0) {\n            TestTownSkyGraphics();\n            CaptureTownSkies(argv[2]);\n        } else if (strcmp(argv[1], "--sky-graphics") == 0) {\n            TestTownSkyGraphics();\n        } else if (strcmp(argv[1], "--carriage-graphics") == 0) {')
replace('cmake/tests/80-client.cmake',
        '    add_test(NAME renderer_skin_rotation COMMAND renderer_regression_tests)',
        '    add_test(NAME town_sky_data COMMAND renderer_regression_tests --sky-data)\n    add_test(NAME renderer_skin_rotation COMMAND renderer_regression_tests)')
expected = {
 'src/client/local3d/town_sky.inc': '4433553e036a7c622ec9fcf956c069543c87129834b2eefd1c75f5010256b382',
 'tests/town_sky_tests.inc': 'bf7877c3a83cd6b33ba94937d71b06b557bd34c61d4feec32795e1666edb1679',
 'tools/capture_town_skies.py': '32b83e723f3a5999db2050a22b196863a2450b8059593b9f81a6cd41b15f3f97',
 'docs/design/town-skies.md': '71f43efecca1b801a42cecea9b860fae555c889a5454de9832ab6d682cbd578a',
 '.github/workflows/town-skies.yml': '5a9c1b23868e0c38588893cded36f0e360145091a2070f17bf56653bb0a95018',
 'src/client/cc_local3d.c': 'b076ab6054468617085a209fa7d11384eeff22a501d692c241be8d4b14616877',
 'src/client/local3d/authored_places.inc': '71f9ac0d1dcb9699f9653a876d0b17c2157e7d839b8a8583406f387492d3e978',
 'src/client/local3d/road_book.inc': '83637231d34b50123cf41b33e3e799de251f26d088ceaa27ca47c817febe875f',
 'tests/renderer_regression_tests.c': 'cd5fcdb3bbaa6829904b7d742e728bf44eea4995da3174b9a341c8a47571fd04',
 'cmake/tests/80-client.cmake': 'd23d5e8227603cd4947be7f12c609b37c521c60253e8ac93144ba7e89abf60fb',
}
for path, digest in expected.items():
    actual = hashlib.sha256(Path(path).read_bytes()).hexdigest()
    print(path, actual)
    assert actual == digest, (path, actual, digest)
print('All final files match the locally reviewed and compiled implementation')
