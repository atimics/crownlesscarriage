#include "sim/cc_sim.h"

#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
#include <string.h>

static void CheckReferences(const CcSim *sim)
{
    for (int32_t i = 0; i < sim->event_count; ++i) {
        const CcEvent *event = CcSimRecentEvent(sim, i);
        CC_CHECK(event != NULL);
        CC_CHECK(event->parent_id == 0U ||
                 CcSimEvent(sim, event->parent_id) != NULL);
    }
    for (int32_t i = 0; i < sim->situation_count; ++i) {
        const CcSituation *situation = &sim->situations[i];
        CC_CHECK(situation->cause_event_id == 0U ||
                 CcSimEvent(sim, situation->cause_event_id) != NULL);
    }
}

int main(void)
{
    char error[192];
    for (uint32_t seed = 1U; seed <= 8U; ++seed) {
        CcSim sim;
        CcSimInit(&sim, seed * UINT32_C(0x9e3779b9));
        CcSimAdvanceDays(&sim, 3650);
        CC_CHECK(sim.event_count == CC_MAX_EVENTS);
        CheckReferences(&sim);
        CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));
    }

    CcSim broken;
    CcSimInit(&broken, UINT32_C(0xca05a1));
    CcEvent *newest = (CcEvent *)CcSimRecentEvent(&broken, 0);
    CC_CHECK(newest != NULL);
    newest->parent_id = CcMakeId(CC_ENTITY_EVENT, UINT64_C(0xffffff));
    CC_CHECK(!CcSimValidate(&broken, error, sizeof(error)));
    CC_CHECK(strstr(error, "lost parent") != NULL);

    const char *save_path = "/tmp/crownless-causal-history-tests.ccsave";
    (void)remove(save_path);
    CcSim saved;
    CcSim loaded;
    CcSimInit(&saved, UINT32_C(42));
    CcSimAdvanceDays(&saved, 3650);
    CC_CHECK(CcSaveWrite(save_path, &saved, error, sizeof(error)));
    CC_CHECK(CcSaveRead(save_path, &loaded, error, sizeof(error)));
    CheckReferences(&loaded);
    CC_CHECK(CcSimHash(&saved) == CcSimHash(&loaded));
    CC_CHECK(remove(save_path) == 0);

    puts("Causal history retention tests passed");
    return 0;
}
