"""Compile temporary instrumented objects; the checked-in simulation stays intact."""
import argparse
from pathlib import Path
import shutil
import subprocess

parser=argparse.ArgumentParser()
parser.add_argument('source',type=Path)
parser.add_argument('library',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
root=args.source.resolve(); output=args.output.resolve(); output.mkdir(parents=True,exist_ok=True)
probe=Path(__file__).with_name('probe.c').resolve()

def replace_once(text,old,new):
    assert text.count(old)==1, old
    return text.replace(old,new)

source=(root/'src/sim/cc_sim.c').read_text()
source=replace_once(source,'static void GenerateSituations(CcSim *sim);', '''void StudyStaff(const CcSim *sim);
void StudyPurchase(const CcSim *sim, const CcShipment *shipment, CcMoney charge);
void StudyEvent(const CcSim *sim, CcEventKind kind, CcId id, CcId location, int32_t quantity);
static void GenerateSituations(CcSim *sim);''')
source=replace_once(source,'    CompactEventLedger(sim, parent);','    StudyEvent(sim, kind, subject, location, magnitude);\n    CompactEventLedger(sim, parent);')
source=replace_once(source,'    CcArchives *archives = &sim->archives;\n\n    int32_t target_scribes', '    StudyStaff(sim);\n    CcArchives *archives = &sim->archives;\n\n    int32_t target_scribes')
source=replace_once(source,'    if (supply != NULL) {\n        supply->shipment_id = shipment->id;', '    if (archive != NULL) StudyPurchase(sim, shipment, total_charge);\n    if (supply != NULL) {\n        supply->shipment_id = shipment->id;')
for arm in ('protected','unprotected','paused'):
    folder=output/arm;folder.mkdir(exist_ok=True)
    code=source
    if arm=='paused':
        code=replace_once(code,'    if (sim == NULL || sim->schema_version < 75U) return false;', '    if (sim == NULL || sim->schema_version < 75U) return false;\n    return false; /* Controlled archive dispatch pause. */')
    (folder/'cc_sim.c').write_text(code)
    supply=(root/'src/sim/cc_archive_supply.c').read_text()
    if arm=='unprotected':
        supply=replace_once(supply,'    if (sim->schema_version < 85U) return sim->iron_ledger_reserve;','    return sim->iron_ledger_reserve; /* Controlled release of the staffing floor. */')
    (folder/'cc_archive_supply.c').write_text(supply)
    library=folder/'libstudy.a';shutil.copyfile(args.library,library)
    for name in ('cc_sim','cc_archive_supply'):
        obj=folder/(name+'.c.o')
        subprocess.run(['cc','-O3','-DNDEBUG','-std=c17','-Wall','-Wextra','-Werror','-I'+str(root/'src'),'-I'+str(root/'src/sim'),'-c',str(folder/(name+'.c')),'-o',str(obj)],check=True)
        subprocess.run(['ar','r',str(library),str(obj)],check=True)
    subprocess.run(['cc','-O2','-std=c17','-Wall','-Wextra','-Werror','-I'+str(root/'src'),str(probe),str(library),'-lm','-o',str(folder/'probe')],check=True)
    print(arm,folder/'probe',flush=True)
