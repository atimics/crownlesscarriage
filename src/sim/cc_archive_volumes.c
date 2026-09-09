#include "sim/cc_archive_volumes_internal.h"
#include "sim/cc_archive_relocation.h"

#include <limits.h>
#include <string.h>

bool CcArchiveVolumeHasTitle(const char *name)
{
    return strncmp(name, "Chronicle ", 10) == 0 ||
           strncmp(name, "Ledger ", 7) == 0 ||
           strncmp(name, "Annal ", 6) == 0 ||
           strncmp(name, "Register ", 9) == 0 ||
           strncmp(name, "Codex of ", 9) == 0;
}

bool CcArchiveVolumeIsLive(const CcTreasure *treasure)
{
    return treasure != NULL && !treasure->destroyed && CcArchiveVolumeHasTitle(treasure->name);
}

int32_t CcSimArchivePhysicalLore(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int64_t lore = 0;
    for (int32_t i = 0; i < sim->treasure_count; ++i) {
        const CcTreasure *volume = &sim->treasures[i];
        if (CcArchiveVolumeIsLive(volume)) lore += volume->craft_work;
    }
    return lore > INT32_MAX ? INT32_MAX : (int32_t)lore;
}

void CcSimUpgradeArchivePhysicalLore(CcSim *sim)
{
    if (sim == NULL) return;
    sim->archives.lore_stored = CcSimArchivePhysicalLore(sim);
}

bool CcArchiveVolumeEarlier(const CcSim *sim, int32_t first,
                                 int32_t second)
{
    if (second < 0) return true;
    const CcTreasure *a = &sim->treasures[first];
    const CcTreasure *b = &sim->treasures[second];
    return a->created_day < b->created_day ||
        (a->created_day == b->created_day && first < second);
}

bool CcArchiveFindVolumesToBind(const CcSim *sim, int32_t slots[4])
{
    int32_t best[4] = {-1, -1, -1, -1};
    for (int32_t anchor = 0; anchor < sim->treasure_count; ++anchor) {
        const CcTreasure *volume = &sim->treasures[anchor];
        if (!CcArchiveVolumeIsLive(volume) || CcSimArchiveConvoyCarriesBook(sim, volume->id) ||
            volume->owner_id == sim->player.id) continue;

        int32_t candidate[4] = {-1, -1, -1, -1};
        for (int32_t i = 0; i < sim->treasure_count; ++i) {
            const CcTreasure *other = &sim->treasures[i];
            if (!CcArchiveVolumeIsLive(other) || CcSimArchiveConvoyCarriesBook(sim, other->id) ||
                other->owner_id == sim->player.id ||
                other->owner_id != volume->owner_id ||
                other->location_id != volume->location_id) continue;
            for (int32_t position = 0; position < 4; ++position) {
                if (!CcArchiveVolumeEarlier(sim, i, candidate[position])) {
                    continue;
                }
                for (int32_t move = 3; move > position; --move) {
                    candidate[move] = candidate[move - 1];
                }
                candidate[position] = i;
                break;
            }
        }
        if (candidate[3] < 0) continue;
        if (best[0] < 0 ||
            CcArchiveVolumeEarlier(sim, candidate[0], best[0])) {
            for (int32_t i = 0; i < 4; ++i) best[i] = candidate[i];
        }
    }
    if (best[0] < 0) return false;
    for (int32_t i = 0; i < 4; ++i) slots[i] = best[i];
    return true;
}

