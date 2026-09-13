#ifndef CROWNLESS_ARCHIVE_VOLUMES_INTERNAL_H
#define CROWNLESS_ARCHIVE_VOLUMES_INTERNAL_H

#include "sim/cc_sim.h"

/* Classify an archive title, including a title stripped from a ruined book. */
bool CcArchiveVolumeHasTitle(const char *name);
bool CcArchiveVolumeIsLive(const CcTreasure *treasure);
/* Compare live array slots; a negative second slot starts a selection. */
bool CcArchiveVolumeEarlier(const CcSim *sim, int32_t first, int32_t second);
/* Select four books with shared custody, ordered by creation day and slot. */
bool CcArchiveFindVolumesToBind(const CcSim *sim, int32_t slots[4]);

#endif
