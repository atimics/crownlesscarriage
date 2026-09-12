#ifndef CC_HRAKHOR_H
#define CC_HRAKHOR_H

#include "story/cc_core_account.h"

#define CC_HRAKHOR_VERSION 1
/* Apply Hra'khor to a completed English line from the core model or renderer.
   strength is 0..100. Named fields in the held account are copied verbatim.
   Input and output use separate buffers. Failure clears the output. */
bool CcHrakhorCorrupt(const CcCoreAccount *account, const char *english,
                      unsigned int strength, char *text, size_t capacity);

#endif
