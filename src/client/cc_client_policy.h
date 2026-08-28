#ifndef CROWNLESS_CLIENT_POLICY_H
#define CROWNLESS_CLIENT_POLICY_H

#include <stdbool.h>

float CcClientConvoyPaceStep(float pace, bool road_phase,
                             bool urge, bool rein_in, bool stopped,
                             float delta_time);
bool CcClientMapCommandEnabled(bool viewing_map, bool road_local,
                               bool market_interior,
                               float carriage_distance);
bool CcClientPromiseCanBeAccepted(bool market_interior,
                                  float notice_distance);

#endif
