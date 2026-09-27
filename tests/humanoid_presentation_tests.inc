/* Check the production interpolation path without a graphics context. */
static void TestHumanoidPresentation(void)
{
    CcLocalAgent agent = {0};
    agent.crowned = true;
    agent.grounded = true;
    agent.morphology = CC_MORPHOLOGY_BIPED;
    agent.appearance = CcNpcCrownlessAppearance();
    CcHumanoidGaitInit(&agent.humanoid, (CcLimbVec3){0}, 0.0f,
                       FlatSkinProbe, NULL);
    float minimum_chest = 10.0f;
    float maximum_chest = -10.0f;
    float maximum_step = 0.0f;
    CcHumanoidPose previous = agent.humanoid.pose;
    for (int32_t tick = 0; tick < 600; ++tick) {
        CcHumanoidGaitAdvance(&agent.humanoid, (CcLimbVec3){0}, 0.0f,
            (CcLimbVec3){0}, true, 1.0f / 60.0f, FlatSkinProbe, NULL);
        CcHumanoidGait saved = agent.humanoid;
        for (int32_t frame = 0; frame < 2; ++frame) {
            CcLocalAgentInterpolateInternal(&agent, (float)frame * 0.5f);
            const CcHumanoidPose *pose = &agent.render_pose;
            RequireRenderer(memcmp(&saved, &agent.humanoid, sizeof(saved)) == 0,
                            "presentation must preserve the physical gait");
            RequireRenderer(Vector3Distance(FromLimbVector(pose->pelvis),
                FromLimbVector(saved.pose.pelvis)) < 0.000001f,
                "idle breathing must keep the pelvis planted");
            for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
                RequireRenderer(Vector3Distance(FromLimbVector(pose->ankle[leg]),
                    FromLimbVector(saved.pose.ankle[leg])) < 0.000001f &&
                    Vector3Distance(FromLimbVector(pose->toe[leg]),
                    FromLimbVector(saved.pose.toe[leg])) < 0.000001f,
                    "idle breathing must preserve foot contacts");
            }
            if (tick > 60) {
                minimum_chest = fminf(minimum_chest, pose->chest.y);
                maximum_chest = fmaxf(maximum_chest, pose->chest.y);
                maximum_step = fmaxf(maximum_step,
                    Vector3Distance(FromLimbVector(previous.chest),
                                     FromLimbVector(pose->chest)));
            }
            previous = *pose;
            CcLocalAgentInterpolateInternal(&agent, (float)frame * 0.5f);
            RequireRenderer(memcmp(&previous, &agent.render_pose,
                                    sizeof(previous)) == 0,
                            "repeated rendering must keep the same pose");
        }
    }
    RequireRenderer(maximum_chest - minimum_chest > 0.025f &&
                    maximum_chest - minimum_chest < 0.035f &&
                    maximum_step < 0.001f,
                    "resting characters need a small, smooth breathing cycle");
    CcLocalRendererSetReducedMotion(true);
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    RequireRenderer(memcmp(&agent.render_pose, &agent.humanoid.pose,
                            sizeof(agent.render_pose)) == 0,
                    "reduced motion must retain the settled idle pose");
    CcLocalRendererSetReducedMotion(false);
    CcHumanoidGaitSetGuarded(&agent.humanoid, true);
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    RequireRenderer(memcmp(&agent.render_pose, &agent.humanoid.pose,
                            sizeof(agent.render_pose)) == 0,
                    "guard must retain its physical pose");

    /* Cross the walk/idle threshold with a held hand pose still in flight. */
    CcHumanoidGaitSetGuarded(&agent.humanoid, false);
    agent.humanoid.speed.value = 0.7f;
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    agent.stepped_pose.from_local.hand[0].z += 0.15f;
    agent.stepped_pose.target_local.hand[0].z += 0.15f;
    agent.humanoid.speed.value = 0.1001f;
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    CcHumanoidPose before_stop = agent.render_pose;
    agent.humanoid.speed.value = 0.0999f;
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    RequireRenderer(Vector3Distance(FromLimbVector(before_stop.hand[0]),
        FromLimbVector(agent.render_pose.hand[0])) < 0.001f,
        "the held walk must ease into the resting pose at low speed");
    (void)printf("PASS character presentation: idle range %.4f m, "
                 "120 Hz chest step %.6f m, planted feet and eased stops\n",
                 maximum_chest - minimum_chest, maximum_step);
}
