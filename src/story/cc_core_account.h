#ifndef CC_CORE_ACCOUNT_H
#define CC_CORE_ACCOUNT_H

#include "sim/cc_sim.h"

#define CC_CORE_FIELDS 8
#define CC_CORE_MIND_LINES 2
typedef enum CcCoreRole {
    CC_CORE_NONE, CC_CORE_ACTOR, CC_CORE_RECIPIENT, CC_CORE_PLACE,
    CC_CORE_OBJECT, CC_CORE_GROUP, CC_CORE_MATERIAL, CC_CORE_DETAIL,
    CC_CORE_QUANTITY
} CcCoreRole;
typedef enum CcCoreKnowledge {
    CC_CORE_KNOWN, CC_CORE_COARSE, CC_CORE_UNCERTAIN, CC_CORE_UNKNOWN
} CcCoreKnowledge;
typedef enum CcCoreGoal {
    CC_CORE_GOAL_KEEP_ORDER, CC_CORE_GOAL_SECURE_LIVELIHOOD,
    CC_CORE_GOAL_SURVIVE_CRISIS, CC_CORE_GOAL_CARRY_NEWS
} CcCoreGoal;
typedef enum CcCoreLevel {
    CC_CORE_LEVEL_LOW, CC_CORE_LEVEL_MEDIUM, CC_CORE_LEVEL_HIGH
} CcCoreLevel;
typedef enum CcCoreControl {
    CC_CORE_CONTROL_SAY, CC_CORE_CONTROL_THINK, CC_CORE_CONTROL_REMEMBER
} CcCoreControl;
typedef struct CcCoreField {
    size_t start, length; /* UTF-8 byte offsets into the held text. */
    CcCoreRole role;
    CcCoreKnowledge knowledge;
    bool spoken;
} CcCoreField;
typedef struct CcCoreAccount {
    char text[CC_EVENT_TEXT_CAPACITY];
    CcCoreField fields[CC_CORE_FIELDS];
    size_t field_count, rule_index;
    int32_t confidence, retellings;
} CcCoreAccount;
typedef struct CcCoreMind {
    CcCoreGoal goal;
    CcCoreLevel stress;
    CcCoreLevel courage;
    bool witnessed;
    const char *voice;
    const char *memories[CC_CORE_MIND_LINES];
    size_t memory_count;
    const char *thoughts[CC_CORE_MIND_LINES];
    size_t thought_count;
} CcCoreMind;

/* Parse the supplied held telling. The caller owns its evidence. */
bool CcCoreAccountPrepare(CcEventKind kind, const char *held_text,
                          int32_t confidence, int32_t retellings,
                          CcCoreAccount *account);
bool CcCoreAccountRender(const CcCoreAccount *account, uint32_t variant,
                         char *text, size_t capacity);
const char *CcCoreAccountRule(const CcCoreAccount *account);
const char *CcCoreAccountGrammar(void);

#endif
