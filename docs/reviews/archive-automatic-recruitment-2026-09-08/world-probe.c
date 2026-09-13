#include "sim/cc_sim.h"
#include "sim/cc_archive_recruitment.h"
#include "sim/cc_archive_staff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static CcSim sim;
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    unsigned seed = (unsigned)strtoul(argv[1], NULL, 0);
    unsigned schema = (unsigned)strtoul(argv[2], NULL, 0);
    CcSimInit(&sim, seed); sim.schema_version = schema;
    int commissions = 0, patrons = 0, appointments = 0, staffed_days = 0, peak = 0;
    int gates[CC_ARCHIVE_RECRUIT_STORAGE + 1] = {0};
    char error[256];
    for (int day = 0; day < 40 * 365; ++day) {
        CcSimAdvanceDays(&sim, 1);
        int named = 0;
        for (int i = 0; i < CC_MAX_SCRIBES; ++i) named += CcSimArchiveStaffWorking(&sim, sim.archive_staff.person_ids[i]);
        if (named > 0) ++staffed_days;
        if (named > peak) peak = named;
        for (int i = 0; i < sim.event_count; ++i) {
            const CcEvent *event = CcSimRecentEvent(&sim, i);
            if (event->day != sim.current_day) break;
            if (strstr(event->text, "The archive commissions ") != NULL) {
                ++commissions; patrons += sim.archive_recruitment.donor_ids[0] != 0;
            }
            if (strstr(event->text, " joins the archive.") != NULL) ++appointments;
        }
        CcArchiveRecruitmentGate gate = sim.archive_recruitment.status == 5 ? CcSimArchiveAppointmentPlan(&sim).gate :
            sim.archive_recruitment.status == 3 ? CcSimArchiveRecruitmentTrainingGate(&sim) :
            sim.archive_recruitment.status != 0 ? CcSimArchiveRecruitmentJourneyGate(&sim) :
            CcSimArchiveRecruitmentPlan(&sim).gate;
        if (gate >= 0 && gate <= CC_ARCHIVE_RECRUIT_STORAGE) ++gates[gate];
        if (day % 365 == 0 && !CcSimValidate(&sim, error, sizeof(error))) {
            fprintf(stderr, "day %d: %s\n", sim.current_day, error); return 1;
        }
    }
    printf("{\"seed\":%u,\"schema\":%u,\"years\":40,\"commissions\":%d,\"patron_orders\":%d,\"appointments\":%d,\"days_with_named_staff\":%d,\"peak_named_staff\":%d,\"gate_days\":{",
        seed, schema, commissions, patrons, appointments, staffed_days, peak);
    for (int i = 0; i <= CC_ARCHIVE_RECRUIT_STORAGE; ++i)
        printf("%s\"%s\":%d", i == 0 ? "" : ",", CcArchiveRecruitmentGateName((CcArchiveRecruitmentGate)i), gates[i]);
    puts("}}");
    return 0;
}
