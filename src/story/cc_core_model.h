#ifndef CC_CORE_MODEL_H
#define CC_CORE_MODEL_H
#include "story/cc_core_account.h"
#define CC_CORE_HISTORY 4
#define CC_CORE_UTTERANCE 512

typedef struct CcCoreModel CcCoreModel;
typedef struct CcCoreSpoken {
    CcId speaker;
    char text[CC_CORE_UTTERANCE];
} CcCoreSpoken;

/* One owned runtime per caller. Weights and attention memory stay outside CcSim. */
CcCoreModel *CcCoreModelLoad(const char *path);
void CcCoreModelFree(CcCoreModel *model);
/* Start and step allow a client to spread generation across frames. */
bool CcCoreModelBegin(CcCoreModel *model, const CcCoreAccount *account,
                      CcId speaker, const CcCoreSpoken *history, size_t count);
/* Mind-aware start: the character's goal, stress, courage, voice, memories,
   and thoughts precede the spoken history; the control cue names the output
   line (# say:, # think:, # remember:). Pass NULL for the plain form. */
bool CcCoreModelBeginMind(CcCoreModel *model, const CcCoreAccount *account,
                          CcId speaker, const CcCoreSpoken *history, size_t count,
                          const CcCoreMind *mind, CcCoreControl control);
/* Returns 0 while working, 1 on a complete sentence, -1 on failure. */
int CcCoreModelStep(CcCoreModel *model, unsigned int budget);
const char *CcCoreModelText(const CcCoreModel *model);
bool CcCoreModelGenerate(CcCoreModel *model, const CcCoreAccount *account,
                         CcId speaker, const CcCoreSpoken *history, size_t count,
                         char *text, size_t capacity);
bool CcCoreModelGenerateMind(CcCoreModel *model, const CcCoreAccount *account,
                             CcId speaker, const CcCoreSpoken *history, size_t count,
                             const CcCoreMind *mind, CcCoreControl control,
                             char *text, size_t capacity);
/* Tokenizer inspection for parity tests. */
int CcCoreModelEncode(const char *text, int *tokens, int capacity);
/* Prefix token inspection for parity tests. */
int CcCoreModelPrefixTokens(const CcCoreModel *model, int *tokens, int capacity);
/* Prefix meta inspection for parity tests. */
int CcCoreModelPrefixMeta(const CcCoreModel *model, int *meta, int capacity);
/* Hidden-state inspection for parity tests. */
int CcCoreModelHidden(const CcCoreModel *model, float *hidden, int capacity);
/* Run the prefix so the final hidden state can be read. */
bool CcCoreModelRunPrefix(CcCoreModel *model);
#endif
