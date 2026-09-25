#ifndef CROWNLESS_CHARACTER_SKIN_H
#define CROWNLESS_CHARACTER_SKIN_H

/* One skinned GLB per character, posed by the motion code.

   Every character and creature belongs to one skeleton family. The family
   fixes the bone names the GLB must carry and the solver that poses them.
   See docs/design/unified-characters.md. */

#include "locomotion/cc_creature.h"
#include "locomotion/cc_humanoid.h"

#include <stdbool.h>
#include <stdint.h>

/* The skinned shaders hold 32 bone matrices (boneMatrices[32]). */
#define CC_SKINNED_CHARACTER_MAX_BONES 32

typedef enum CcSkeletonFamily {
    CC_SKELETON_NONE,
    CC_SKELETON_HUMANOID,
    CC_SKELETON_QUADRUPED,
    CC_SKELETON_DRAGON,
    CC_SKELETON_FAMILY_COUNT
} CcSkeletonFamily;

const char *CcSkeletonFamilyName(CcSkeletonFamily family);
CcSkeletonFamily CcSkeletonFamilyFind(const char *name);
/* Zero for a family whose bone list is designed but not built yet. */
int32_t CcSkeletonFamilyBoneCount(CcSkeletonFamily family);
const char *CcSkeletonFamilyBoneName(CcSkeletonFamily family, int32_t bone);
int32_t CcSkeletonFamilyBoneFind(CcSkeletonFamily family, const char *name);

/* How a two-legged creature rig grows an upper body.

   The creature rig solves only the pelvis and the legs. The plan places the
   spine, head and arms on top of it, in body units (scale 1). The Blender
   builder authors the bind pose from the same numbers, so the idle pose of
   this mapping is the mesh's bind pose. */
typedef struct CcBipedBodyPlan {
    float spine_rise;
    float chest_rise;
    float neck_rise;
    float head_rise;
    float head_forward;
    float chest_sway;
    float shoulder_half_width;
    float shoulder_rise;
    float upper_arm_length;
    float forearm_length;
    float arm_spread;
    float elbow_bend;
    float arm_swing;
    float arm_swing_limit;
    float foot_length;
    float heel_length;
    float walk_bob;
    /* Both hands held forward in front of the chest (a carried load). The
       offsets are chest relative for the +x (.R) arm and mirror for .L. */
    bool carry;
    CcLimbVec3 carry_elbow;
    CcLimbVec3 carry_hand;
} CcBipedBodyPlan;

typedef enum CcBipedBodyKind {
    CC_BIPED_BODY_GOBLIN,
    CC_BIPED_BODY_GOBLIN_CARRIER,
    CC_BIPED_BODY_KIND_COUNT
} CcBipedBodyKind;

const CcBipedBodyPlan *CcBipedBodyPlanFor(CcBipedBodyKind kind);

/* Map a two-legged creature rig pose into a humanoid pose in model space:
   the origin moves to zero, the yaw turns to face +z, and the scale divides
   out, so the result matches the GLB bind pose units. Limb 0 is the .L
   (-x) leg and limb 1 is the .R (+x) leg, as the creature rig places them. */
bool CcHumanoidPoseFromBipedRig(const CcCreatureRigPose *rig,
                                const CcBipedBodyPlan *plan,
                                CcLimbVec3 origin, float yaw, float scale,
                                CcHumanoidPose *result);

#endif
