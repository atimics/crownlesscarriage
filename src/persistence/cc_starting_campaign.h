#ifndef CC_STARTING_CAMPAIGN_H
#define CC_STARTING_CAMPAIGN_H
#include "sim/cc_prophecy.h"
#define CC_DEEP_WYRM_TITLE "The Day of the Deep Wyrm"
#define CC_DEEP_WYRM_ASSET "assets/campaigns/deep-wyrm.ccsave"
/* Load the fixed historical world and place a fresh company in Gloamgate.
   Failure leaves the caller's world intact. */
bool CcStartingCampaignDeepWyrm(CcSim *sim, const char *path,
                               char *error, size_t capacity);
#endif
