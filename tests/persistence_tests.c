#include "persistence/cc_save.h"
#include "sim/cc_sim.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    const char *path = "persistence-test.ccsave";
    (void)remove(path);

    CcSim original;
    CcSimInit(&original, UINT32_C(0xa11ce5ed));
    CcSimAdvanceDays(&original, 23);
    char error[256];
    CcCommand command = {
        .kind = CC_COMMAND_TRAVEL,
        .target_id = original.settlements[1].id
    };
    assert(CcSimApply(&original, &command, error, sizeof(error)));
    uint64_t expected = CcSimHash(&original);
    assert(CcSaveWrite(path, &original, error, sizeof(error)));

    CcSim restored;
    assert(CcSaveRead(path, &restored, error, sizeof(error)));
    assert(CcSimHash(&restored) == expected);
    assert(restored.current_day == original.current_day);
    assert(restored.player.location_id == original.player.location_id);
    assert(restored.event_count == original.event_count);

    (void)remove(path);
    puts("SQLite persistence tests passed");
    return 0;
}
