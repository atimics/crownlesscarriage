#ifndef CROWNLESS_MUSIC_NET_H
#define CROWNLESS_MUSIC_NET_H
#include <stdbool.h>
#include <stddef.h>

bool CcMusicNetStart(const char *url, size_t limit);
/* 0 pending, 1 complete, -1 failed. Caller owns completed data. */
int CcMusicNetPoll(unsigned char **data, size_t *size);
void CcMusicNetShutdown(void);

#endif
