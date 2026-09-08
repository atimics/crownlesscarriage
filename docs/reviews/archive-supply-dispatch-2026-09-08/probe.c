#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static CcSim sim;
static CcId orders[4096];
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    uint32_t seed = (uint32_t)strtoul(argv[1], NULL, 0);
    uint32_t schema = (uint32_t)strtoul(argv[2], NULL, 0);
    CcSimInit(&sim, seed); sim.schema_version = schema;
    int booked = 0, arrived = 0, lost = 0;
    CcId latest = 0;
    for (int day = 0; day <= 14600; ++day) {
        if (day > 0) CcSimAdvanceDays(&sim, 1);
        CcId next_latest = latest;
        for (int i = 0; i < sim.event_count; ++i) {
            const CcEvent *event = CcSimRecentEvent(&sim, sim.event_count - 1 - i);
            if (event->id <= latest) continue;
            if (event->id > next_latest) next_latest = event->id;
            if (event->kind == CC_EVENT_SHIPMENT_DEPARTED && strstr(event->text, "The archive at") == event->text) {
                if (booked >= 4096) return 3;
                orders[booked++] = event->subject_id;
            }
            if (event->kind == CC_EVENT_SHIPMENT_ARRIVED || event->kind == CC_EVENT_SHIPMENT_LOST) {
                for (int j = 0; j < booked; ++j) {
                    if (orders[j] != event->subject_id) continue;
                    if (event->kind == CC_EVENT_SHIPMENT_ARRIVED) arrived++; else lost++;
                    break;
                }
            }
        }
        latest = next_latest;
        if (day % 365 == 0) printf("%d %016" PRIx64 "\n", sim.current_day, CcSimHash(&sim));
    }
    fprintf(stderr, "seed=%" PRIu32 " schema=%" PRIu32 " booked=%d arrived=%d lost=%d\n", seed, schema, booked, arrived, lost);
    return 0;
}
