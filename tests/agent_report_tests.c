#define main AgentSweepMain
#include "../tools/agent_sweep.c"
#undef main
#include "test_support.h"

static void CheckField(const char *header, const char *row,
    const char *name, const char *expected)
{
    const char *key = header;
    const char *value = row;
    while (*key != '\0' && *value != '\0') {
        size_t key_length = strcspn(key, ",\n");
        size_t value_length = strcspn(value, ",\n");
        if (strlen(name) == key_length && strncmp(key, name, key_length) == 0) {
            CC_CHECK(strlen(expected) == value_length);
            CC_CHECK(strncmp(value, expected, value_length) == 0);
            return;
        }
        key += key_length; value += value_length;
        if (*key != '\0') ++key;
        if (*value != '\0') ++value;
    }
    CC_CHECK(false);
}

int main(void)
{
    static CcSim control, agent, control_before, agent_before;
    CcSimInit(&control, 123U);
    control.settlement_count = 3;
    control.settlements[0].population = 100;
    control.settlements[0].hunger = 10;
    control.settlements[1].population = 300;
    control.settlements[1].hunger = 20;
    control.settlements[2].population = 0;
    control.settlements[2].hunger = 100;
    agent = control;
    for (int32_t i = 0; i < agent.settlement_count; ++i)
        agent.settlements[i].population = 0;
    agent.current_day += 3;
    control_before = control; agent_before = agent;
    AgentStats stats = {0};
    FILE *file = tmpfile();
    CC_CHECK(file != NULL);
    WriteReportHeader(file);
    WriteReportRow(file, 7, 366, &control, &agent, &stats);
    rewind(file);
    char header[2048], row[2048];
    CC_CHECK(fgets(header, sizeof(header), file) != NULL);
    CC_CHECK(fgets(row, sizeof(row), file) != NULL);
    CC_CHECK(fgetc(file) == EOF);
    CheckField(header, row, "control_hunger", "15");
    CheckField(header, row, "control_maximum_hunger", "20");
    CheckField(header, row, "control_population_weighted_hunger", "17");
    CheckField(header, row, "control_active_settlements", "2");
    CheckField(header, row, "control_abandoned_settlements", "1");
    CheckField(header, row, "agent_hunger", "-1");
    CheckField(header, row, "agent_maximum_hunger", "-1");
    CheckField(header, row, "agent_population_weighted_hunger", "-1");
    CheckField(header, row, "agent_active_settlements", "0");
    CheckField(header, row, "agent_abandoned_settlements", "3");
    CheckField(header, row, "world_seed", "123");
    CheckField(header, row, "target_day", "366");
    CheckField(header, row, "control_day", "1");
    CheckField(header, row, "agent_day", "4");
    char hash[32];
    (void)snprintf(hash, sizeof(hash), "%016" PRIx64, CcSimHash(&control));
    CheckField(header, row, "control_hash", hash);
    (void)snprintf(hash, sizeof(hash), "%016" PRIx64, CcSimHash(&agent));
    CheckField(header, row, "agent_hash", hash);
    CC_CHECK(memcmp(&control, &control_before, sizeof(control)) == 0);
    CC_CHECK(memcmp(&agent, &agent_before, sizeof(agent)) == 0);
    CC_CHECK(fclose(file) == 0);
    return 0;
}
