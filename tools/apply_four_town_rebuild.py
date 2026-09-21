from pathlib import Path
import re
root=Path.cwd()
import hashlib
EXPECTED_INPUT = {'src/client/cc_local_place.c': 'a6f4d04ddc226b6f5f98560122349d455fbbab89e7ef2968f15eb49e51e6a811', 'src/client/local3d/authored_places.inc': 'c79ed2d557e2c8e3d4db910f5e598ac07398aa3a014d7a05431695ead40a890d', 'tests/local_movement_tests.c': 'ebbf3b19f57ea73ee3859043a8b34c5b20938fccd0e74391d4d8ba0bb9bb0977', 'tests/local_place_tests.c': 'b466f2abf494cd14c8d98584d4cfe91a5d5918773468f16f3645f54c0831313a'}
for name,digest in EXPECTED_INPUT.items():
    if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:
        raise SystemExit(f'Unexpected input revision: {name}')
p=root/'src/client/cc_local_place.c'; s=p.read_text()
scenes={
'FARMING': [
 ('ARRIVAL','RIVER CROSSING',[82,34,80,.16,29,-18,.64,28,28]),
 ('HEART','THRESHING GREEN',[44,29,45,.14,25,-15,.64,23,17]),
 ('LANDMARK','GRANARY RISE',[78,27,77,.16,18,-13,.64,20,17]),
 ('CLOSE_FIRST',"DROVERS' CLOSE",[32,38,30,.12,39,-6,.62,8,6.3]),
 ('CLOSE_SECOND',"MILLER'S BEND",[58,50,60.5,.12,50,6,.62,8,6.3]),
 ('CARRIAGE_YARD','CARTWRIGHT YARD',[42.4,55.2,40.5,.12,53,7,.62,8,8.4])],
'MARKET': [
 ('ARRIVAL','CUSTOMS ARCH',[82,34,61,.14,34,-26,.64,32,32]),
 ('HEART','MARKET CIRCLE',[44,29,46,.14,34,-19,.64,26,24]),
 ('LANDMARK','ARCHIVE STEPS',[39.5,23,34,.18,16,-14,.64,22,17]),
 ('CLOSE_FIRST','CLOTH YARD',[24,28,22,.14,25,6,.64,8,6.5]),
 ('CLOSE_SECOND','EXCHANGE DOOR',[50,27.25,49.5,.14,23.5,-6,.64,8,6.1]),
 ('CARRIAGE_YARD','COACH COURT',[42.4,55.2,38,.14,54,-5,.64,8,9])],
'MINING': [
 ('ARRIVAL','FOUNDRY TERRACE',[82,34,74,.14,24,-24,.66,30,28]),
 ('HEART','COMPANY STORE',[44,29,50,.16,23,-11,.66,19,18]),
 ('LANDMARK','LOWER SILVERWORKS',[78,29,78,.14,20,-12,.64,18,16]),
 ('CLOSE_FIRST',"WORKERS' LANE",[33,25,32,.12,24.5,6,.66,8,6.5]),
 ('CLOSE_SECOND','FURNACE ALLEY',[27,50,24,.12,47.5,6,.66,8,6.3]),
 ('CARRIAGE_YARD','ORE WAGON YARD',[42.4,55.2,39.5,.12,52,5,.66,8,6.5])],
'FORTRESS': [
 ('ARRIVAL','CONTESTED BRIDGE',[82,34,85,.16,33,16,.62,24,26]),
 ('HEART','MUSTER SPINE',[44,29,47,.16,27,-11,.62,20,19]),
 ('LANDMARK','ALDERWATCH KEEP',[78.5,27,78,.18,20,14,.60,20,22]),
 ('CLOSE_FIRST',"ARMOURERS' ROW",[33,25,34,.16,24,6,.62,8,6.3]),
 ('CLOSE_SECOND','ALDER GATE PASS',[73.5,32.5,78.5,.18,29.5,0,.60,9,6.6]),
 ('CARRIAGE_YARD','LOWER BAILEY',[42.4,55.2,39.5,.16,52,-5,.62,8,6.5])]
}
for fn,entries in scenes.items():
    a=s.index(f'.function = CC_SETTLEMENT_{fn},'); start=s.index('        .scene = {',a); end=s.index('        },',start)+len('        },')
    def fmt(v): return f'{v:.2f}f'
    block='        .scene = {\n'
    for kind,name,nums in entries:
        block+=f'            {{CC_LOCAL_TOWN_SCENE_{kind}, "{name}",\n'
        block+='             '+', '.join(map(fmt,nums[:5]))+',\n'
        block+='             '+', '.join(map(fmt,nums[5:]))+'},\n'
    block+='        },'
    s=s[:start]+block+s[end:]
p.write_text(s)
# Close interaction shots retain their bounds; the fortress gains establishing views.
p=root/'tests/local_place_tests.c'; s=p.read_text()
s=s.replace('profile->function == CC_SETTLEMENT_MINING) &&','profile->function == CC_SETTLEMENT_MINING ||\n                                profile->function == CC_SETTLEMENT_FORTRESS) &&',1)
start=s.index('int main(void)')
contract='''static int FourAuthoredSceneContracts(void)
{
    static const struct {
        CcSettlementFunction function;
        int32_t index[4];
        const char *name[4];
    } towns[] = {
        {CC_SETTLEMENT_FARMING, {0, 1, 2, 5},
         {"RIVER CROSSING", "THRESHING GREEN", "GRANARY RISE", "CARTWRIGHT YARD"}},
        {CC_SETTLEMENT_MARKET, {1, 2, 3, 5},
         {"MARKET CIRCLE", "ARCHIVE STEPS", "CLOTH YARD", "COACH COURT"}},
        {CC_SETTLEMENT_MINING, {0, 1, 3, 5},
         {"FOUNDRY TERRACE", "COMPANY STORE", "WORKERS' LANE", "ORE WAGON YARD"}},
        {CC_SETTLEMENT_FORTRESS, {0, 1, 2, 5},
         {"CONTESTED BRIDGE", "MUSTER SPINE", "ALDERWATCH KEEP", "LOWER BAILEY"}},
    };
    for (size_t town = 0; town < sizeof(towns) / sizeof(towns[0]); ++town) {
        const CcLocalPlaceProfile *profile =
            CcLocalPlaceProfileForFunction(towns[town].function);
        for (int32_t scene = 0; scene < 4; ++scene) {
            const CcLocalTownScene *view = &profile->scene[towns[town].index[scene]];
            CHECK(strcmp(view->name, towns[town].name[scene]) == 0);
        }
        /* Service and parked-carriage anchors stay compatible with saved walks. */
        const CcLocalPlaceBuilding *hall = &profile->building[profile->primary_building];
        CHECK(hall->x + hall->width * 0.5f == 50.0f);
        CHECK(profile->scene[CC_LOCAL_TOWN_SCENE_CARRIAGE_YARD].trigger_x == 42.4f);
        CHECK(profile->scene[CC_LOCAL_TOWN_SCENE_CARRIAGE_YARD].trigger_z == 55.2f);
    }
    /* Market discovery is no longer another shot of the customs keep. */
    const CcLocalTownScene *archive = CcLocalTownSceneAt(
        CC_SETTLEMENT_MARKET, CC_LOCAL_TOWN_SCENE_LANDMARK);
    CHECK(archive->trigger_x < 42.0f && archive->target_x < 38.0f);
    /* A fortress establishes the crossing and keep before its close-up walks. */
    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_FORTRESS, 0)->fovy >= 20.0f);
    CHECK(CcLocalTownSceneAt(CC_SETTLEMENT_FORTRESS, 2)->fovy >= 20.0f);
    return 0;
}

'''
s=s[:start]+contract+s[start:]; s=s.replace('    if (ProfileContract() != 0) return 1;','    if (ProfileContract() != 0) return 1;\n    if (FourAuthoredSceneContracts() != 0) return 1;',1); p.write_text(s)
p=root/'tests/local_movement_tests.c'; s=p.read_text(); old='''        miller_click_agent.navigation_point_count !=
            miller_preview.path_count) {'''; new='''        miller_click_agent.navigation_active != miller_preview.navigation ||
        (miller_preview.navigation && miller_click_agent.navigation_point_count !=
            miller_preview.path_count) ||
        (!miller_preview.navigation && miller_click_agent.navigation_point_count != 0)) {'''; assert s.count(old)==1
s=s.replace(old,new); p.write_text(s)
p=root/'src/client/local3d/authored_places.inc'; s=p.read_text()
at=s.index('static void DrawSettlementBuilding(')
s=s[:at]+'#include "client/local3d/authored_town_architecture.inc"\n\n'+s[at:]
start=s.index('        case CC_SETTLEMENT_FARMING:',s.index('static void DrawSettlementBuilding('))
end=s.index('        case CC_SETTLEMENT_CAPITAL:',start)
replacement=""
for town,fn in [('FARMING','Thornford'),('MINING','Silverwick'),('MARKET','Gloamgate'),('FORTRESS','Alderwatch')]:
    replacement+=f'        case CC_SETTLEMENT_{town}:\n            Draw{fn}Architecture(building, wall, roof, building_index, burnt);\n            break;\n'
s=s[:start]+replacement+s[end:]; p.write_text(s)
EXPECTED_OUTPUT = {'src/client/cc_local_place.c': '9a97b2e09e5d0260753a5abbf54ef4e37457fae876dee1938cbcc090aa05c255', 'src/client/local3d/authored_places.inc': 'fdf15ffd6e312b74df375c9a69fe32d132a5f48ee1e4933f34371fbd08b7fa61', 'tests/local_movement_tests.c': '9d4c2bf9ec0bbf11c726f4cd4f440ca3c3b6d470e4bf904c500c4c539442b90d', 'tests/local_place_tests.c': '2c32324b3ebc5122a51e4570b85d310d6f3ae0e0f8b8132e18fe93a0ad22dae8'}
for name,digest in EXPECTED_OUTPUT.items():
    if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest:
        raise SystemExit(f'Prepared source mismatch: {name}')
print('Four authored town source edits verified')
