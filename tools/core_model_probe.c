#include "story/cc_core_model.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_HISTORY 8

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
    static const char *const names[CC_CORE_CONTROL_COUNT] = {
        "open", "answer", "remark", "affirm", "dispute", "hedge",
        "attribute", "defer", "settle", "part", "recall", "muse", "cite"
    };
    for (int i = 0; i < CC_CORE_CONTROL_COUNT; ++i)
        if (strcmp(name, names[i]) == 0) return (CcCoreControl)i;
    /* The cues that predate the move axis. */
    if (strcmp(name, "think") == 0) return CC_CORE_CONTROL_MUSE;
    if (strcmp(name, "remember") == 0) return CC_CORE_CONTROL_RECALL;
    return CC_CORE_CONTROL_REMARK;
}

static CcCoreFaction FactionId(const char *name)
{
    if (strcmp(name, "crown") == 0) return CC_CORE_FACTION_CROWN;
    if (strcmp(name, "guild") == 0) return CC_CORE_FACTION_GUILD;
    if (strcmp(name, "commons") == 0) return CC_CORE_FACTION_COMMONS;
    return CC_CORE_FACTION_NONE;
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
    bool dump_meta = false, dump_hidden = false;
    const char *history_text[MAX_HISTORY] = {0};
    int history_count = 0;
    char spec[256] = {0};
    CcCoreMind mind = {0};
    mind.goal = CC_CORE_GOAL_SECURE_LIVELIHOOD;
    mind.stress = CC_CORE_LEVEL_MEDIUM;
    mind.courage = CC_CORE_LEVEL_MEDIUM;
    mind.voice = NULL;
    /* Fed, sheltered, home: the default situation both runtimes agree on.
       Present spec parts overwrite these below; absent ones keep them. */
    mind.sheltered = true;
    CcCoreControl control = CC_CORE_CONTROL_SAY;
    bool use_mind = false, dump = false, faction_unknown = false;
    for (int i = 6; i < argc; ++i) {
        if (strcmp(argv[i], "--mind") == 0 && i + 1 < argc) {
            use_mind = true;
            /* mind.voice points into this buffer, so it has to outlive the
               branch that fills it: the prompt is built after the whole
               argument vector is parsed. */
            (void)snprintf(spec, sizeof(spec), "%s", argv[++i]);
            char *at = spec;
            int part = 0;
            while (at != NULL && part < 12) {
                char *sep = strchr(at, ':');
                if (sep != NULL) *sep = '\0';
                if (part == 0) mind.voice = at[0] != '\0' ? at : NULL;
                else if (part == 1) mind.goal = GoalId(at);
                else if (part == 2) mind.stress = LevelId(at);
                else if (part == 3) mind.courage = LevelId(at);
                else if (part == 4) control = ControlId(at);
                else if (part == 5) mind.hungry = at[0] == '1';
                else if (part == 6) mind.sheltered = at[0] == '1';
                else if (part == 7) mind.in_transit = at[0] == '1';
                else if (part == 8) mind.owes_listener = at[0] == '1';
                else if (part == 9) mind.trusts_listener = at[0] == '1';
                else if (part == 10) {
                    mind.faction = FactionId(at);
                    if (at[0] != '\0' && mind.faction == CC_CORE_FACTION_NONE)
                        faction_unknown = true;
                }
                else mind.far_from_home = at[0] == '1';
                at = sep != NULL ? sep + 1 : NULL;
                ++part;
            }
        } else if (strcmp(argv[i], "--witnessed") == 0) {
            use_mind = true; mind.witnessed = true;
        } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
            use_mind = true;
            if (mind.memory_count < CC_CORE_MIND_LINES) mind.memories[mind.memory_count++] = argv[++i];
        } else if (strcmp(argv[i], "--read") == 0 && i + 1 < argc) {
            use_mind = true;
            if (mind.read_count < CC_CORE_MIND_LINES) mind.read[mind.read_count++] = argv[++i];
        } else if (strcmp(argv[i], "--thought") == 0 && i + 1 < argc) {
            use_mind = true;
            if (mind.thought_count < CC_CORE_MIND_LINES) mind.thoughts[mind.thought_count++] = argv[++i];
        } else if (strcmp(argv[i], "--dump-meta") == 0) {
            dump = true; dump_meta = true;
        } else if (strcmp(argv[i], "--dump-hidden") == 0) {
            dump = true; dump_hidden = true;
        } else if (strcmp(argv[i], "--dump-prefix") == 0) {
            dump = true;
        } else {
            if (history_count >= MAX_HISTORY) { CcCoreModelFree(model); return 2; }
            history_text[history_count] = argv[i];
            ++history_count;
        }
    }
    CcCoreSpoken history[MAX_HISTORY] = {0};
    for (int i = 0; i < history_count; ++i) {
        /* The most recent spoken line is the other speaker (2); older lines
           alternate self (1). This matches the Python training convention. */
        size_t from_end = (size_t)(history_count - 1 - i);
        history[i].speaker = (from_end % 2U) == 0U ? 2U : 1U;
        if (strlen(history_text[i]) >= sizeof(history[0].text)) { CcCoreModelFree(model); return 2; }
        (void)snprintf(history[i].text, sizeof(history[0].text), "%s", history_text[i]);
    }
    /* The encoder keeps only the last CC_CORE_HISTORY lines. Speakers were
       assigned over the whole list above, so taking the tail here matches the
       Python reference rather than failing the longer cases outright. */
    int first = history_count > CC_CORE_HISTORY ? history_count - CC_CORE_HISTORY : 0;
    const CcCoreSpoken *spoken = history + first;
    size_t spoken_count = (size_t)(history_count - first);
    /* An unknown voice zeroes the stance block on both runtimes, which reads
       as the model ignoring goal, stress and courage too. Warn rather than
       fail: the prompt is still well-formed, but a typo here nukes all
       conditioning, and that should never pass silently. */
    if (use_mind && mind.voice != NULL && CcCoreModelVoiceId(mind.voice) == 0)
        (void)fprintf(stderr, "warning: unknown voice '%s' conditions on no stance\n",
                      mind.voice);
    if (use_mind && faction_unknown)
        (void)fprintf(stderr, "warning: unknown faction gates the faction table off\n");
    if (dump) {
        int out[4096];
        bool works = use_mind ? CcCoreModelBeginMind(model, &account, 1U, spoken, spoken_count, &mind, control)
                              : CcCoreModelBegin(model, &account, 1U, spoken, spoken_count);
        if (works) {
            if (dump_hidden) {
                if (!CcCoreModelRunPrefix(model)) { CcCoreModelFree(model); return 1; }
                float hidden[256];
                int count = CcCoreModelHidden(model, hidden, 256);
                for (int i = 0; i < count; ++i) (void)printf("%s%.6f", i == 0 ? "" : " ", (double)hidden[i]);
                (void)puts("");
            } else if (dump_meta) {
                /* Sixteen fields now: the five that mark a copied span, the four
                   stance ids, hungry, sheltered, in_transit, owes, trusts,
                   faction, far. A wrong id is invisible in the decoded prompt,
                   so this dump is the only way parity stays honest. */
                enum { PROBE_META = 16 };
                int meta[4096 * PROBE_META];
                int count = CcCoreModelPrefixMeta(model, meta, 4096 * PROBE_META);
                for (int i = 0; i < count; ++i) {
                    (void)printf("%s", i == 0 ? "" : " ");
                    for (int k = 0; k < PROBE_META; ++k)
                        (void)printf("%s%d", k == 0 ? "" : ",", meta[i * PROBE_META + k]);
                }
                (void)puts("");
            } else {
                int count = CcCoreModelPrefixTokens(model, out, 4096);
                for (int i = 0; i < count; ++i) (void)printf("%s%d", i == 0 ? "" : " ", out[i]);
                (void)puts("");
            }
        }
        CcCoreModelFree(model);
        return works ? 0 : 1;
    }
    char text[CC_CORE_UTTERANCE];
    if (use_mind) okay = okay && CcCoreModelGenerateMind(model, &account, 1U, spoken,
        spoken_count, &mind, control, text, sizeof(text));
    else okay = okay && CcCoreModelGenerate(model, &account, 1U, spoken,
        spoken_count, text, sizeof(text));
    if (okay) (void)puts(text);
    CcCoreModelFree(model);
    return okay ? 0 : 1;
}
