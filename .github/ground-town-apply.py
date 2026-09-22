from pathlib import Path
import re

root = Path('.')
p = root / 'src/client/cc_local_place.c'
s = p.read_text()
# target x/y/z, camera offset x, eye above camera terrain, offset z, frame span.
# Trigger coordinates, streets, service entrances and parked carriage stay fixed.
tables = {
 'FARMING': [(81,3.8,31,11,3.1,20,16),(47,3.2,27,-5,2.9,20,12),(77,3.8,24,10,3.1,18,14),(30,2.8,39,5,2.7,13,9),(60.5,2.8,50,6,2.7,12,9),(40.5,2.8,53,9,2.9,12,10.5)],
 'MINING': [(78,4.2,31,12,3.1,23,18),(49,3.3,28,-6,2.9,18,12.5),(78,4.0,25,10,3.0,18,15),(32,3.0,26,5,2.8,13,10),(25,2.8,48,9,2.7,9,9),(39.5,2.8,52,11,2.8,13,10.5)],
 'MARKET': [(78,3.5,34,15,3.0,12,14),(46,3.7,31,14,3.1,14,14),(35,3.5,21,9,2.9,15,11.5),(24,3.0,25,11,2.8,7,10),(49.5,3.1,25,-7,2.9,13,10),(38,2.8,54,12,2.8,6,10.5)],
 'FORTRESS': [(83,5.0,33,9,3.4,23,21),(47,3.5,29,2,3.0,18,12.5),(78,5.0,26,3,3.0,21,18),(34,3.0,26,8,2.8,12,10),(78.5,3.8,29.5,2,2.9,14,12),(39.5,2.8,52,7,2.9,16,11)],
 'CAPITAL': [(81,4.5,34,-10,3.1,19,16),(46,3.5,29,9,2.9,17,13),(78,5,20,-11,3.2,22,19),(36,3,24.5,-7,2.8,14,10),(64,3,47,8,2.8,12,10),(39.5,2.8,52,9,2.9,13,10.5)],
 'DUNGEON_TOWN': [(81,3.6,34,-10,2.9,17,14),(46,3,30,8,2.8,16,12),(79,4,20,-10,3,20,17),(33,2.8,38.5,7,2.7,12,9),(28.5,2.8,49.5,-7,2.7,12,9),(39.5,2.8,52,10,2.8,12,10.5)],
}
for town, rows in tables.items():
 a = s.index('.function = CC_SETTLEMENT_' + town + ',')
 start = s.index('        .scene = {', a)
 end = s.index('\n        },', start) + len('\n        },')
 block = s[start:end]
 scenes = list(re.finditer(r'(\{CC_LOCAL_TOWN_SCENE_\w+, "[^"]+",\s*)([\d.f,\s-]+)(\})', block))
 assert len(scenes) == 6, (town, len(scenes))
 for m, values in reversed(list(zip(scenes, rows))):
  orig = [x.strip() for x in m.group(2).split(',') if x.strip()]
  assert len(orig) == 9
  numbers = f'{orig[0]}, {orig[1]}, ' + ', '.join(f'{v:.2f}f' for v in values[:3]) + ',\n             ' + ', '.join(f'{v:.2f}f' for v in values[3:])
  block = block[:m.start(2)] + numbers + block[m.end(2):]
 s = s[:start] + block + s[end:]
p.write_text(s)

p = root / 'src/client/local3d/camera_composition.inc'
s = p.read_text()
start = s.index('static float StreetCameraClearanceHeight(')
end = s.index('\nstatic Vector3 CameraHermite', start)
s = s[:start] + '''/* Town cameras stand in the scene; a wall is not a licence to lift the
   camera above its roof. Find the nearest clear ground-level station instead. */
static bool StreetCameraStationClear(Vector3 station)
{
    for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
        WorldBuilding building = ActiveWorldBuildingAt(i);
        Vector2 local = BuildingUnrotatedPoint(i, station.x, station.z);
        if (TerrainDistanceToRectangle(local.x, local.y, building.footprint) < 0.8f &&
            station.y < TerrainFootprintHeight(building.footprint) + building.height + 0.8f)
            return false;
    }
    for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
        WorldStructure structure = ActiveCompoundStructureAt(i);
        if (TerrainDistanceToRectangle(station.x, station.z, structure.footprint) < 0.8f &&
            station.y < TerrainFootprintHeight(structure.footprint) + structure.height + 0.5f)
            return false;
    }
    return true;
}

static Camera3D StreetCameraClearVolume(Camera3D camera)
{
    float eye = CombatClamp(camera.position.y - CcLocalTerrainHeightAt(
        camera.position.x, camera.position.z), 1.8f, 4.0f);
    Vector3 station = camera.position;
    station.y = CcLocalTerrainHeightAt(station.x, station.z) + eye;
    if (!StreetCameraStationClear(station)) {
        bool found = false;
        for (int32_t ring = 1; ring <= 12 && !found; ++ring) {
            for (int32_t side = 0; side < 16; ++side) {
                float angle = (float)side * (2.0f * PI / 16.0f);
                Vector3 candidate = {station.x + cosf(angle) * (float)ring,
                    0.0f, station.z + sinf(angle) * (float)ring};
                candidate.y = CcLocalTerrainHeightAt(candidate.x, candidate.z) + eye;
                if (!StreetCameraStationClear(candidate)) continue;
                station = candidate;
                found = true;
                break;
            }
        }
    }
    camera.position = station;
    return camera;
}

/* The frame guard may pan along the ground and aim the lens, but never crane
   the camera back into the aerial projection this town mode replaces. */
static Camera3D GroundStreetFrameHero(Camera3D camera, Vector3 hero,
                                      int32_t art_height, Rectangle safe_area)
{
    float eye = CombatClamp(camera.position.y - CcLocalTerrainHeightAt(
        camera.position.x, camera.position.z), 1.8f, 4.0f);
    camera = KeepHeroInsideStreetFrame(camera, hero, art_height, safe_area);
    camera.position.y = CcLocalTerrainHeightAt(camera.position.x, camera.position.z) + eye;
    return camera;
}
''' + s[end:]
old = '''    float horizontal_span = sqrtf(camera_offset.x * camera_offset.x +
                                   camera_offset.z * camera_offset.z);
    /* Keep a view down onto the street when the camera stands on lower land. */
    camera_offset.y = fmaxf(
        camera_ground + composition->camera_offset.y - destination.y,
        horizontal_span * 0.55f);'''
assert old in s
s = s.replace(old, '''    /* The authored vertical component is eye height over the camera's own
       ground, not a pitch ratio. Do not impose the former 29-degree dive. */
    camera_offset.y = camera_ground + composition->camera_offset.y - destination.y;''')
s = s.replace('''    Camera3D camera = ExteriorCameraComposed(
        street_camera_rig.displayed_target,
        street_camera_rig.displayed_offset,
        street_camera_rig.displayed_fovy);''', '''    Camera3D camera = PerspectiveCameraComposed(
        street_camera_rig.displayed_target,
        street_camera_rig.displayed_offset, 40.0f);''')
a = s.index('Camera3D CcLocalStreetCameraInternal(')
b = s.index('static Camera3D TownArrivalCamera', a)
block = s[a:b]
start = block.index('        Camera3D authored_camera = camera;')
end = block.index('        Vector3 before_guard = camera.target;', start)
block = block[:start] + '        camera = StreetCameraClearVolume(camera);\n' + block[end:]
block = block.replace('camera = KeepHeroInsideStreetFrame(', 'camera = GroundStreetFrameHero(')
block = block.replace('    bool fresh_view = !street_camera_rig.initialized;\n', '')
s = s[:a] + block + s[b:]
a = s.index('    typedef struct TownTravelCamera', s.index('static Camera3D TownArrivalCamera'))
b = s.index('    if (convoy == NULL || arrival == NULL)', a)
s = s[:a] + '''    const CcLocalTownScene *arrival = CcLocalTownSceneAt(
        active_place_function, CC_LOCAL_TOWN_SCENE_ARRIVAL);
    float target_height = arrival != NULL ? arrival->target_y : 3.0f;
    Vector3 travel_offset = arrival != NULL ?
        (Vector3){arrival->camera_offset_x, 0.0f, arrival->camera_offset_z} :
        (Vector3){-8.0f, 0.0f, 20.0f};
''' + s[b:]
a = s.index('static Camera3D TownArrivalCamera')
b = s.index('static Camera3D RoadCamera', a)
block = s[a:b].replace('travel.target_height', 'target_height').replace('travel.offset', 'travel_offset')
block = block.replace('(Vector3){0.0f, 5.2f, 0.0f}', '(Vector3){0.0f, 3.0f, 0.0f}')
block = block.replace('    Vector3 wide_position = Vector3Add(wide_target, travel_offset);', '''    Vector3 wide_position = Vector3Add(wide_target, travel_offset);
    wide_position.y = CcLocalTerrainHeightAt(wide_position.x, wide_position.z) + 3.0f;''')
block = block.replace('.displayed_offset = Vector3Scale(arrival_offset, 1.0f / 2.5f),', '.displayed_offset = arrival_offset,')
s = s[:a] + block + s[b:]
p.write_text(s)
p = root / 'src/client/local3d/road_book.inc'
s = p.read_text()
a = s.index('void CcLocalDrawStreet3D(')
s = s[:a] + s[a:].replace('camera = KeepHeroInsideStreetFrame(', 'camera = GroundStreetFrameHero(', 1)
p.write_text(s)
p = root / 'src/client/cc_local_place.h'
s = p.read_text().replace('    float target_y;', '    float target_y; /* Look-at height above target terrain. */', 1).replace('    float camera_offset_y;', '    float camera_offset_y; /* Eye height above terrain at the camera station. */', 1).replace('    float fovy;', '    float fovy; /* Vertical framing span at target, not degrees or an ortho lens. */', 1)
p.write_text(s)
print('Applied ground-level camera candidate')
