#include "locomotion/cc_footing.h"
#include "locomotion/cc_creature.h"
#include "locomotion/cc_humanoid.h"
#include "locomotion/cc_quadruped.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void Require(bool condition, const char *message)
{
    if (condition) return;
    fprintf(stderr, "%s\n", message);
    exit(1);
}

typedef struct Terrain {
    float slope;
    float curb;
    bool left_missing;
    bool absent;
} Terrain;

static float Height(const Terrain *t, float z)
{
    return t->slope * z + (z >= 0.35f && z <= 0.65f ? t->curb : 0.0f);
}

static bool Probe(void *context, CcLimbVec3 origin, float drop,
                   CcLimbVec3 *point, CcLimbVec3 *normal)
{
    const Terrain *t = context;
    if (t->absent || (t->left_missing && origin.x < 7.0f)) return false;
    float height = Height(t, origin.z);
    if (height > origin.y || origin.y - height > drop) return false;
    float n = sqrtf(1.0f + t->slope * t->slope);
    *point = (CcLimbVec3){origin.x, height, origin.z};
    *normal = (CcLimbVec3){0.0f, 1.0f / n, -t->slope / n};
    return true;
}

static float Distance(CcLimbVec3 a, CcLimbVec3 b)
{
    return sqrtf((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z));
}

static void CheckSegments(const CcLimbRig *rig)
{
    for (int32_t i = 0; i < rig->morphology.limb_count; ++i) {
        const CcLimbSpec *spec = &rig->morphology.limbs[i];
        const CcLimbRuntime *limb = &rig->limbs[i];
        for (int32_t j = 0; j < spec->segment_count; ++j) {
            Require(fabsf(Distance(limb->joints[j], limb->joints[j+1]) -
                           spec->segment_length[j]) < 0.001f, "course changed a limb length");
        }
        if (limb->state == CC_LIMB_STANCE) {
            Require(Distance(limb->joints[spec->segment_count], limb->planted_contact) < 0.01f,
                    "supporting limb ends above its contact");
        }
    }
}

static void LostSupport(void)
{
    for (int32_t profile = 0; profile < CC_CREATURE_RIG_PROFILE_COUNT; ++profile) {
        Terrain t = {0};
        CcCreatureRigController c;
        CcCreatureRigPose p;
        Require(CcCreatureRigControllerInit(&c, (CcCreatureRigProfile)profile, 0.0f, 1.0f), "init failed");
        CcCreatureRigWorldCommand cmd = {.ground_position = {7.0f, 0.0f, 5.0f}, .grounded = true};
        for (int32_t i = 0; i < 60; ++i) {
            Require(CcCreatureRigControllerStepWorld(&c, &cmd, 1.0f/60.0f, Probe, &t, &p), "step failed");
        }
        t.left_missing = true;
        Require(CcCreatureRigControllerStepWorld(&c, &cmd, 1.0f/60.0f, Probe, &t, &p), "step failed");
        Require(p.planted_count == p.limb_count / 2, "lost feet still contribute support");
        for (int32_t i = 0; i < p.limb_count; ++i) {
            if (c.skeleton.limbs[i].state == CC_LIMB_STANCE) {
                Require(c.skeleton.limbs[i].planted_contact.x >= 7.0f, "ledge supports the wrong foot");
            }
        }
        t.absent = true;
        for (int32_t i = 0; i < 30; ++i) {
            Require(CcCreatureRigControllerStepWorld(&c, &cmd, 1.0f/60.0f, Probe, &t, &p), "step failed");
        }
        Require(p.planted_count == 0 && p.support_state == CC_LIMB_SUPPORT_UNSUPPORTED,
                "missing ground should transfer control to support recovery");
    }
    Terrain t = {0};
    CcHumanoidGait human;
    CcLimbVec3 body = {7.0f, 0.0f, 5.0f};
    CcHumanoidGaitInit(&human, body, 0.0f, Probe, &t);
    t.left_missing = true;
    CcHumanoidGaitAdvance(&human, body, 0.0f, (CcLimbVec3){0}, true, 1.0f/60.0f, Probe, &t);
    Require(human.feet[0].contact == CC_HUMANOID_CONTACT_AIR && human.planted_count == 1,
            "human idle keeps a foot planted beyond the ledge");
    t.absent = true;
    CcHumanoidGaitInit(&human, body, 0.0f, Probe, &t);
    Require(human.planted_count == 0, "human initialization invents support");
}

static void SlopeAndSkin(void)
{
    const float scales[] = {0.42f, 0.84f, 0.96f, 1.0f};
    const CcQuadrupedBone hooves[] = {CC_QUADRUPED_HOOF_FL, CC_QUADRUPED_HOOF_FR,
                                     CC_QUADRUPED_HOOF_HL, CC_QUADRUPED_HOOF_HR};
    for (int32_t scale_index = 0; scale_index < 4; ++scale_index) {
        float scale = scales[scale_index];
        for (int32_t ramp = 0; ramp < 3; ++ramp) {
            Terrain t = {.slope = tanf((float)ramp * 15.0f * 3.14159265f / 180.0f)};
            CcCreatureRigController c;
            CcCreatureRigPose p;
            Require(CcCreatureRigControllerInit(&c, CC_CREATURE_RIG_HORSE, 0.0f, scale), "pony init failed");
            CcCreatureRigWorldCommand cmd = {.ground_position = {7.0f, Height(&t, 5.0f), 5.0f}, .grounded = true};
            for (int32_t frame = 0; frame < 180; ++frame) {
                Require(CcCreatureRigControllerStepWorld(&c, &cmd, 1.0f/60.0f, Probe, &t, &p), "slope step failed");
                CheckSegments(&c.skeleton);
            }
            Require(p.planted_count == 4, "standing pony lost a reachable hoof on a slope");
            CcCreatureRigPose unit = p;
            unit.body = (CcLimbVec3){p.body.x/scale, p.body.y/scale, p.body.z/scale};
            for (int32_t i = 0; i < 4; ++i) {
                for (int32_t j = 0; j <= 2; ++j) {
                    CcLimbVec3 v = unit.limbs[i].joints[j];
                    unit.limbs[i].joints[j] = (CcLimbVec3){v.x/scale, v.y/scale, v.z/scale};
                }
            }
            CcQuadrupedPose skin;
            CcQuadrupedPoseResolveFromRig(CC_QUADRUPED_HORSE, &unit, &skin);
            Require(skin.valid, "physical skin did not resolve");
            CcLimbVec3 a = skin.bones[CC_QUADRUPED_BODY].head;
            CcLimbVec3 b = skin.bones[CC_QUADRUPED_BODY].tail;
            Require(fabsf(atan2f(b.y-a.y, b.z-a.z) - atanf(t.slope)) < 0.01f,
                    "visible pony body stays level on a slope");
            for (int32_t i = 0; i < 4; ++i) {
                CcLimbVec3 foot = skin.bones[hooves[i]].head;
                foot.x = cmd.ground_position.x + scale * (foot.x - unit.up.x * 0.10f);
                foot.y = cmd.ground_position.y + scale * (foot.y - unit.up.y * 0.10f);
                foot.z = cmd.ground_position.z + scale * (foot.z - unit.up.z * 0.10f);
                Require(Distance(foot, c.skeleton.limbs[i].planted_contact) < 0.001f,
                        "scaled skin moved a planted hoof");
            }
        }
    }
}

static bool Wall(void *context, CcBiomechVec3 previous, CcBiomechVec3 next,
                  float radius, CcBiomechVec3 *resolved, CcBiomechVec3 *normal)
{
    (void)context;
    (void)resolved;
    (void)normal;
    return previous.z < 0.5f && next.z + radius >= 0.5f;
}

static void Clearance(void)
{
    Terrain t = {.curb = 0.20f};
    CcLimbVec3 start = {0.0f, 0.0f, 0.0f};
    CcLimbVec3 target = {0.0f, 0.0f, 1.0f};
    float lift = 0.0f;
    Require(CcFootingPlanSwing(start, target, 0.105f, 0.38f, 0.02f, Probe, NULL, &t, &lift),
            "walkable curb rejected");
    Require(lift > 0.20f, "curb should raise the foot path");
    for (int32_t i = 0; i <= 1000; ++i) {
        CcLimbVec3 p = CcFootingSwingPoint(start, target, (float)i/1000.0f, lift);
        Require(p.y + 0.002f >= Height(&t, p.z), "swing foot crosses a curb face");
    }
    Require(!CcFootingPlanSwing(start, target, 0.105f, 0.38f, 0.02f, Probe, Wall, &t, &lift),
            "swing route should detect a solid wall");
    t.curb = 0.8f;
    Require(!CcFootingPlanSwing(start, target, 0.105f, 0.38f, 0.02f, Probe, NULL, &t, &lift),
            "high obstacle should require another step target");
}

int main(void)
{
    LostSupport();
    SlopeAndSkin();
    Clearance();
    puts("verified footing course passed");
    return 0;
}
