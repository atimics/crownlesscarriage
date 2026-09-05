#ifndef CROWNLESS_CREW_H
#define CROWNLESS_CREW_H
#include <stdint.h>

enum { CC_CREW_CAPACITY = 7, CC_CREW_POSE_FLOATS = 83 };
/* Position, facing, 23 joint positions, then 10 joint angles. */
typedef struct CcCrewMember {
    char id[17];
    char name[32];
    uint32_t appearance;
    float pose[CC_CREW_POSE_FLOATS];
} CcCrewMember;

typedef int32_t (*CcCrewExchange)(int32_t scene, const float *pose,
                                 CcCrewMember *crew);
#endif
