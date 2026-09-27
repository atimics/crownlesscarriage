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

    (void)printf("PASS character presentation: idle range %.4f m, "
                 "120 Hz chest step %.6f m, planted feet\n",
                 maximum_chest - minimum_chest, maximum_step);
}

/* Exercise real fixed updates with four independent display schedules. */
static void TestHumanoidPresentationSchedules(void)
{
    CcLocalAgent agents[4];
    for (int32_t i = 0; i < 4; ++i) {
        CcLocalAgentInit(&agents[i], (Vector2){4.0f, 5.5f}, true);
        RequireRenderer(CcLocalAgentSetExactTarget(&agents[i],
            (Vector3){8.0f, 0.0f, 5.5f}, true),
            "the presentation timing route must be reachable");
    }
    float deepest_braking_lean = 0.0f;
    for (int32_t tick = 0; tick < 600; ++tick) {
        for (int32_t i = 0; i < 4; ++i) {
            if (tick == 300) CcLocalAgentAttend(&agents[i],
                (Vector3){8.8f, 1.55f, 5.5f}, 2.0f, true);
            CcLocalAgentFixedStepInternal(&agents[i], 1.0f / 60.0f, true);
        }
        deepest_braking_lean = fminf(deepest_braking_lean,
                                     agents[0].performance.lean.value);
        CcLocalAgentInterpolateInternal(&agents[0], 1.0f);
        if (tick % 2 == 0) CcLocalAgentInterpolateInternal(&agents[1], 1.0f);
        CcLocalAgentInterpolateInternal(&agents[2], 0.5f);
        CcLocalAgentInterpolateInternal(&agents[2], 1.0f);
        if (tick % 7 == 0) {
            CcLocalAgentInterpolateInternal(&agents[3], 0.15f);
            CcLocalAgentInterpolateInternal(&agents[3], 0.65f);
        }
        if (tick % 6 != 0) continue;
        for (int32_t i = 0; i < 4; ++i) {
            CcLocalAgentInterpolateInternal(&agents[i], 0.37f);
            RequireRenderer(memcmp(&agents[0].humanoid, &agents[i].humanoid,
                sizeof(agents[0].humanoid)) == 0,
                "display schedules must share the same physical movement");
            RequireRenderer(memcmp(&agents[0].render_pose, &agents[i].render_pose,
                sizeof(agents[0].render_pose)) == 0,
                "30, 60, 120 and uneven display schedules must share poses");
            RequireRenderer(agents[0].render_head_yaw == agents[i].render_head_yaw &&
                            agents[0].render_head_pitch == agents[i].render_head_pitch,
                            "gaze and greeting must match across display rates");
        }
    }
    RequireRenderer(deepest_braking_lean < -0.015f && deepest_braking_lean > -0.066f,
                    "arrival must prepare a small backward weight shift");
    (void)printf("PASS prepared stop: lean %.4f rad\n", deepest_braking_lean);
    (void)puts("PASS character pose timing: exact match at 30, 60, 120 and uneven rates");
}

static void TestCharacterPerformance(void)
{
    CcLocalAgent agent;
    CcLocalAgentInit(&agent, (Vector2){4.0f, 5.5f}, true);
    agent.facing_yaw = 0.0f;
    CcLocalAgentSetMorphology(&agent, CC_MORPHOLOGY_BIPED, true);
    CcLocalAgentFixedStepInternal(&agent, 1.0f / 60.0f, true);
    CcLocalAgentAttend(&agent, (Vector3){6.0f, 1.55f, 5.5f}, 1.0f, true);
    float largest_nod = 0.0f;
    float maximum_yaw_step = 0.0f;
    float previous_yaw = 0.0f;
    for (int32_t tick = 0; tick < 240; ++tick) {
        CcLocalAgentFixedStepInternal(&agent, 1.0f / 60.0f, true);
        CcHumanoidGait saved = agent.humanoid;
        CcCharacterPerformance acting = agent.performance;
        CcLocalAgentInterpolateInternal(&agent, 1.0f);
        RequireRenderer(memcmp(&acting, &agent.performance, sizeof(acting)) == 0 &&
                        memcmp(&saved, &agent.humanoid, sizeof(saved)) == 0,
                        "rendering must preserve performance and physical state");
        largest_nod = fmaxf(largest_nod, agent.performance.nod.value);
        maximum_yaw_step = fmaxf(maximum_yaw_step,
            fabsf(agent.performance.gaze_yaw.value - previous_yaw));
        previous_yaw = agent.performance.gaze_yaw.value;
        if (tick == 5) {
            RequireRenderer(agent.performance.gaze_yaw.value > 0.55f &&
                agent.performance.chest_yaw.value > 0.08f &&
                agent.performance.gaze_yaw.value > agent.performance.chest_yaw.value * 2.0f,
                "a glance must lead the chest turn");
        }
        for (int32_t leg = 0; leg < 2; ++leg) {
            RequireRenderer(Vector3Distance(FromLimbVector(agent.render_pose.ankle[leg]),
                FromLimbVector(saved.pose.ankle[leg])) < 0.000001f,
                "acting must keep physical foot contacts");
        }
        for (int32_t arm = 0; arm < 2; ++arm) {
            float before = Vector3Distance(FromLimbVector(saved.pose.elbow[arm]),
                                            FromLimbVector(saved.pose.hand[arm]));
            float after = Vector3Distance(FromLimbVector(agent.pose_history.pose.elbow[arm]),
                                           FromLimbVector(agent.pose_history.pose.hand[arm]));
            RequireRenderer(fabsf(after - before) < 0.003f,
                            "acting must retain forearm length");
        }
    }
    RequireRenderer(largest_nod > 0.15f && largest_nod < 0.23f &&
                    maximum_yaw_step < 0.18f &&
                    fabsf(agent.performance.gaze_yaw.value) < 0.0001f &&
                    fabsf(agent.performance.chest_yaw.value) < 0.0001f &&
                    fabsf(agent.performance.nod.value) < 0.0001f,
                    "attention and greeting must ease into a settled pose");
    CcHumanoidSkinPose plain, gaze;
    CcHumanoidSkinPoseResolve(&agent.humanoid.pose, &plain);
    CcHumanoidSkinPoseResolveGaze(&agent.humanoid.pose, 0.7f, 0.2f, &gaze);
    RequireRenderer(gaze.valid &&
        Vector3Distance(FromLimbVector(plain.bones[CC_HUMANOID_SKIN_HEAD].forward),
                        FromLimbVector(gaze.bones[CC_HUMANOID_SKIN_HEAD].forward)) > 0.5f,
        "the head skin must turn with its gaze");
    RequireRenderer(fabsf(Vector3Distance(
        FromLimbVector(gaze.bones[CC_HUMANOID_SKIN_HEAD].head),
        FromLimbVector(gaze.bones[CC_HUMANOID_SKIN_HEAD].tail)) - 0.18f) < 0.000001f,
        "gaze must retain the head bone length");
    for (int32_t bone = CC_HUMANOID_SKIN_THIGH_LEFT;
         bone < CC_HUMANOID_SKIN_BONE_COUNT; ++bone) {
        RequireRenderer(memcmp(&plain.bones[bone], &gaze.bones[bone],
                        sizeof(plain.bones[bone])) == 0,
                        "head direction must preserve leg skin frames");
    }
    CcLocalAgentAttend(&agent, (Vector3){6.0f, 1.55f, 5.5f}, 1.0f, true);
    CcLocalRendererSetReducedMotion(true);
    CcLocalAgentFixedStepInternal(&agent, 1.0f / 60.0f, true);
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    RequireRenderer(agent.render_head_yaw == 0.0f && agent.render_head_pitch == 0.0f,
                    "reduced motion must retain the resting head pose");
    CcLocalRendererSetReducedMotion(false);
    CcLocalAgentAttend(&agent, (Vector3){6.0f, 1.55f, 5.5f}, 1.0f, true);
    CcHumanoidGaitSetGuarded(&agent.humanoid, true);
    CcLocalAgentFixedStepInternal(&agent, 1.0f / 60.0f, true);
    CcLocalAgentInterpolateInternal(&agent, 1.0f);
    RequireRenderer(agent.render_head_yaw == 0.0f && agent.render_head_pitch == 0.0f,
                    "guard must retain its action pose");
    (void)printf("PASS character acting: head leads chest, nod %.3f rad, "
                 "largest gaze step %.3f rad, planted feet and settled rest\n",
                 largest_nod, maximum_yaw_step);
}
