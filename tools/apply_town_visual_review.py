"""One-shot, hash-checked edits from the sixteen-scene review; removed after use."""
from pathlib import Path
import hashlib

EXPECTED = {
    'src/client/cc_local_place.c': '4bc27658de399a412a7481962c04cebbec0dd7f6',
    'src/client/local3d/authored_places.inc': 'a46d6ce28328ee2df37f86cf022e6a98055f07e9',
    'src/client/local3d/authored_town_architecture.inc': '6611f40dfc8abdc85b2bd83b9cb1c36ef7c3b814',
    'tests/local_movement_tests.c': '1347b6b77288b23a859dfccb40aa7dfc1ed18d0a',
    'tests/local_place_tests.c': '4ae092749122d6c46322939a9615d1b1bef3ba89',
    'tools/capture_four_towns.py': '2e68044ee325416d6ea743cf3e38f248b65b6702',
}
texts = {}
for name, expected in EXPECTED.items():
    data = Path(name).read_bytes()
    actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
    if actual != expected:
        raise SystemExit(f'Unexpected source revision: {name}: {actual}')
    texts[name] = data.decode()


def replace(name, old, new):
    if texts[name].count(old) != 1:
        raise SystemExit(f'Non-unique edit in {name}: {old!r}')
    texts[name] = texts[name].replace(old, new, 1)


place = 'src/client/cc_local_place.c'
for old, new in (
    ('-13.00f, 0.64f, 20.00f, 17.00f}', '-13.00f, 0.64f, 20.00f, 22.00f}'),
    ('-15.00f, 0.64f, 23.00f, 17.00f}', '-15.00f, 0.64f, 23.00f, 20.00f}'),
    ('-19.00f, 0.64f, 26.00f, 24.00f}', '-19.00f, 0.64f, 26.00f, 30.00f}'),
    ('6.00f, 0.64f, 8.00f, 6.50f}', '6.00f, 0.64f, 12.00f, 9.80f}'),
    ('-24.00f, 0.66f, 30.00f, 28.00f}', '-24.00f, 0.66f, 30.00f, 34.00f}'),
    ('-11.00f, 0.66f, 19.00f, 18.00f}', '-11.00f, 0.66f, 19.00f, 22.00f}'),
    ('6.00f, 0.66f, 8.00f, 6.50f}', '6.00f, 0.66f, 12.00f, 9.80f}'),
    ('16.00f, 0.62f, 24.00f, 26.00f}', '20.00f, 0.62f, 28.00f, 36.00f}'),
    ('14.00f, 0.60f, 20.00f, 22.00f}', '20.00f, 0.60f, 28.00f, 34.00f}'),
):
    replace(place, old, new)

# These two authored street views intentionally show more than an NPC close-up.
replace('tests/local_place_tests.c',
    '                CHECK(camera->fovy <= (pony_yard ? 9.0f : 6.6f));',
    '''                bool authored_street =
                    (profile->function == CC_SETTLEMENT_MARKET ||
                     profile->function == CC_SETTLEMENT_MINING) &&
                    camera->kind == CC_LOCAL_TOWN_SCENE_CLOSE_FIRST;
                CHECK(camera->fovy <=
                      (authored_street ? 10.0f : pony_yard ? 9.0f : 6.6f));''')
replace('tests/local_place_tests.c',
    '    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_FORTRESS, 2)->fovy >= 20.0f);',
    '''    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_FORTRESS, 2)->fovy >= 30.0f);
    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_MARKET, 3)->fovy >= 9.5f);
    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_MINING, 3)->fovy >= 9.5f);''')

# A direct preview never sets navigation_active. The old loop did zero updates.
replace('tests/local_movement_tests.c',
    'frame < 2400 && miller_click_agent.navigation_active; ++frame)',
    '''frame < 2400 && (miller_click_agent.navigation_active ||
                          miller_click_agent.exact_target_valid); ++frame)''')
replace('tests/local_movement_tests.c',
    '    if (miller_click_agent.navigation_active ||\n        VectorDistance2(',
    '    if (miller_click_agent.navigation_active ||\n        miller_click_agent.exact_target_valid ||\n        VectorDistance2(')

# raylib TakeScreenshot prefixes the process working directory to its argument.
replace('tools/capture_four_towns.py', 'import json\n', 'import json\nimport os\n')
replace('tools/capture_four_towns.py', '               str(image), state]',
    '''               os.path.relpath(image, Path.cwd()), state]''')
replace('tools/capture_four_towns.py',
    '    command = [str(binary),',
    '''    # The shipped screenshot API prefixes cwd; absolute names duplicate it.
    command = [str(binary),''')

architecture = 'src/client/local3d/authored_town_architecture.inc'
replace(architecture, 'A wide raised food store, cross-braced gallery and an unloading hatch.',
    'A wide raised food store, timber gallery and an unloading hatch.')
replace(architecture,
    'stone stairs within the existing solid plot and a central record door.',
    'stepped masonry within the existing solid plot and a central record door.')
replace(architecture,
    '''            DrawBox((Vector3){px + bw * 0.5f,2.40f,z + d + 0.12f},
                    (Vector3){bw - 0.22f,0.45f,0.045f}, cloth);''',
    '''            DrawBox((Vector3){px + bw * 0.5f,2.40f,z + d + 0.12f},
                    (Vector3){bw - 0.22f,0.45f,0.045f}, cloth);
            /* Fixed shop dress, not a representation of saleable cloth stock. */
            for (int32_t hanging = 0; hanging < 2; ++hanging) {
                float hx = px + bw * (hanging == 0 ? 0.18f : 0.82f);
                Color fabric = hanging == 0 ? cloth :
                    BlendColor(cloth, WORLD_DANGER, 0.54f);
                if (burnt) fabric = roof;
                DrawBox((Vector3){hx,3.5f,z + d + 0.11f},
                        (Vector3){bw * 0.19f,1.45f,0.04f}, fabric);
                DrawBox((Vector3){hx,4.28f,z + d + 0.12f},
                        (Vector3){bw * 0.25f,0.10f,0.16f}, trim);
            }''')
texts[architecture] += '''
/* Replace the mine compound's inherited military towers with industrial stacks.
 * The broad base occupies exactly the old solid footprint. No lanes, collision
 * extents, production state or simulated emissions are added by this facade. */
static void DrawSilverwickStack(const WorldStructure *structure, Color stone)
{
    Rectangle plot = structure->footprint;
    float h = structure->height;
    float x = plot.x + plot.width * 0.5f;
    float z = plot.y + plot.height * 0.5f;
    Color soot = BlendColor(stone, WORLD_METAL_SHADOW, 0.62f);
    DrawBox((Vector3){x,h * 0.18f,z},
            (Vector3){plot.width,h * 0.36f,plot.height}, stone);
    DrawBox((Vector3){x,h * 0.67f,z},
            (Vector3){plot.width * 0.64f,h * 0.66f,plot.height * 0.64f}, soot);
    for (int32_t band = 0; band < 3; ++band) {
        DrawBox((Vector3){x,h * (0.44f + (float)band * 0.25f),z},
                (Vector3){plot.width * 0.72f,0.20f,plot.height * 0.72f},
                WORLD_METAL_SHADOW);
    }
    DrawBox((Vector3){x,h + 0.08f,z},
            (Vector3){plot.width * 0.74f,0.16f,plot.height * 0.74f}, stone);
    DrawBox((Vector3){x,h + 0.17f,z},
            (Vector3){plot.width * 0.44f,0.025f,plot.height * 0.44f}, WORLD_INK);
    DrawBox((Vector3){x,h * 0.18f,plot.y + plot.height + 0.025f},
            (Vector3){plot.width * 0.36f,h * 0.18f,0.05f}, WORLD_INK);
}
'''
replace('src/client/local3d/authored_places.inc',
    '''    bool tower = structure->kind == CC_LOCAL_COMPOUND_TOWER;
    Color base = ShadeColor(stone, 0.70f);''',
    '''    bool tower = structure->kind == CC_LOCAL_COMPOUND_TOWER;
    Color base = ShadeColor(stone, 0.70f);

    if (tower && profile != NULL && profile->function == CC_SETTLEMENT_MINING) {
        DrawSilverwickStack(structure, stone);
        return;
    }''')

# All preconditions and replacement counts are checked before writing any source.
for name, text in texts.items():
    Path(name).write_text(text)
print('Applied reviewed town framing, industrial stacks, hanging cloth, and fixture repairs')
