#include "story/cc_core_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *GoalByName(const char *name)
{
    if (strcmp(name, "keep_order") == 0) return name;
    if (strcmp(name, "survive_crisis") == 0) return name;
    if (strcmp(name, "carry_news") == 0) return name;
    return "secure_livelihood";
}

static CcCoreGoal GoalId(const char *name)
{
    if (strcmp(name, "keep_order") == 0) return CC_CORE_GOAL_KEEP_ORDER;
    if (strcmp(name, "survive_crisis") == 0) return CC_CORE_GOAL_SURVIVE_CRISIS;
    if (strcmp(name, "carry_news") == 0) return CC_CORE_GOAL_CARRY_NEWS;
    return CC_CORE_GOAL_SECURE_LIVELIHOOD;
}

static CcCoreLevel LevelId(const char *name)
{
    if (strcmp(name, "low") == 0) return CC_CORE_LEVEL_LOW;
    if (strcmp(name, "high") == 0) return CC_CORE_LEVEL_HIGH;
    return CC_CORE_LEVEL_MEDIUM;
}

static CcCoreControl ControlId(const char *name)
{
    if (strcmp(name, "think") == 0) return CC_CORE_CONTROL_THINK;
    if (strcmp(name, "remember") == 0) return CC_CORE_CONTROL_REMEMBER;
    return CC_CORE_CONTROL_SAY;
}

int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "--encode") == 0) {
        int tokens[4096], n = CcCoreModelEncode(argv[2], tokens, 4096);
        if (n < 0) return 2;
        for (int i = 0; i < n; ++i) (void)printf("%s%d", i == 0 ? "" : " ", tokens[i]);
        (void)puts(""); return 0;
    }
    if (argc < 6) return 2;
    CcCoreModel *model = CcCoreModelLoad(argv[1]);
    if (model == NULL) return 3;
    char *end = NULL;
    long kind = strtol(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || kind < 0 || kind >= CC_EVENT_KIND_COUNT) { CcCoreModelFree(model); return 2; }
    long confidence = strtol(argv[3], &end, 10);
    if (end == argv[3] || *end != '\0' || confidence < 0 || confidence > 100) { CcCoreModelFree(model); return 2; }
    long retellings = strtol(argv[4], &end, 10);
    if (end == argv[4] || *end != '\0' || retellings < 0 || retellings > 100) { CcCoreModelFree(model); return 2; }
    CcCoreAccount account;
    bool okay = CcCoreAccountPrepare((CcEventKind)kind, argv[5], (int32_t)confidence, (int32_t)retellings, &account);
    CcCoreSpoken history[CC_CORE_HISTORY] = {0};
    int history_count = 0;
    CcCoreMind mind = {0};
    mind.goal = CC_CORE_GOAL_SECURE_LIVELIHOOD;
    mind.stress = CC_CORE_LEVEL_MEDIUM;
    mind.courage = CC_CORE_LEVEL_MEDIUM;
    CcCoreControl control = CC_CORE_CONTROL_SAY;
    bool use_mind = false;
    for (int i = 6; i < argc; ++i) {
        if (strcmp(argv[i], "--mind") == 0 && i + 1 < argc) {
            use_mind = true;
            char spec[256];
            (void)snprintf(spec, sizeof(spec), "%s", argv[++i]);
            char *voice = spec, *goal = NULL, *stress = NULL, *courage = NULL, *ctr = NULL;
            char *at = spec;
            for (int part = 0; part < 5 && at != NULL; ++part) {
                char *sep = strchr(at, ':');
                if (sep != NULL) *sep = '\0';
                if (part == 0) voice = at;
                else if (part == 1) goal = at;
                else if (part == 2) stress = at;
                else if (part == 3) courage = at;
                else ctr = at;
                at = sep != NULL ? sep + 1 : NULL;
            }
            mind.voice = voice[0] != '\0' ? voice : NULL;
            if (goal != NULL) mind.goal = GoalId(GoalByName(goal));
            if (stress != NULL) mind.stress = LevelId(stress);
            if (courage != NULL) mind.courage = LevelId(courage);
            if (ctr != NULL) control = ControlId(ctr);
        } else if (strcmp(argv[i], "--witnessed") == 0) {
            use_mind = true; mind.witnessed = true;
        } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
            use_mind = true;
            if (mind.memory_count < CC_CORE_MIND_LINES) mind.memories[mind.memory_count++] = argv[++i];
        } else if (strcmp(argv[i], "--thought") == 0 && i + 1 < argc) {
            use_mind = true;
            if (mind.thought_count < CC_CORE_MIND_LINES) mind.thoughts[mind.thought_count++] = argv[++i];
        } else {
            if (history_count >= CC_CORE_HISTORY || strlen(argv[i]) >= sizeof(history[0].text)) { CcCoreModelFree(model); return 2; }
            history[history_count].speaker = ((argc - 1 - i) % 2) == 0 ? 2U : 1U;
            (void)snprintf(history[history_count].text, sizeof(history[0].text), "%s", argv[i]);
            ++history_count;
        }
    }
    char text[CC_CORE_UTTERANCE];
    if (use_mind) okay = okay && CcCoreModelGenerateMind(model, &account, 1U, history,
        (size_t)history_count, &mind, control, text, sizeof(text));
    else okay = okay && CcCoreModelGenerate(model, &account, 1U, history,
        (size_t)history_count, text, sizeof(text));
    if (okay) (void)puts(text);
    CcCoreModelFree(model);
    return okay ? 0 : 1;
}
