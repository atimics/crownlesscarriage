/* Small acting motions follow fixed simulation time and retain their speed. */
static void StepPerformanceSpring(CcHumanoidSpring *spring, float target,
                                  float response, float delta_time)
{
    float offset = spring->value - target;
    float impulse = spring->velocity + response * offset;
    float decay = expf(-response * delta_time);
    spring->value = target + (offset + impulse * delta_time) * decay;
    spring->velocity = (spring->velocity - response * impulse * delta_time) * decay;
}

void CcLocalAgentAttend(CcLocalAgent *agent, Vector3 point, float seconds,
                        bool greeting)
{
    if (agent == NULL || !isfinite(point.x) || !isfinite(point.y) ||
        !isfinite(point.z) || !isfinite(seconds)) return;
    agent->performance.attention_point = point;
    agent->performance.attention_seconds = Clamp(seconds, 0.0f, 10.0f);
    if (greeting) {
        agent->performance.greeting_time = 0.0f;
        agent->performance.greeting_active = true;
    }
}

static bool CharacterPerformanceActive(const CcLocalAgent *agent)
{
    return !reduced_motion_enabled && agent->grounded && !agent->climbing &&
        !agent->swimming && !agent->humanoid.ragdoll.active &&
        agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION;
}

static void AdvanceCharacterPerformance(CcLocalAgent *agent, float delta_time)
{
    CcCharacterPerformance *acting = &agent->performance;
    if (!CharacterPerformanceActive(agent)) {
        *acting = (CcCharacterPerformance){0};
        return;
    }
    float dt = Clamp(delta_time, 0.0f, 1.0f / 30.0f);
    float gaze = 0.0f;
    float pitch = 0.0f;
    float lean = 0.0f;
    Vector3 attention = agent->target_point;
    bool has_attention = agent->exact_target_valid;
    if (has_attention) {
        float distance = Vector2Length((Vector2){attention.x - agent->position.x,
                                                attention.z - agent->position.z});
        float moving = SmoothStep01(agent->humanoid.speed.value / 0.5f);
        lean = -0.065f * (1.0f - SmoothStep01(distance / 0.85f)) * moving;
        /* Look into the next path leg while the feet complete this one. */
        int32_t next = agent->navigation_point_index + 1;
        if (agent->navigation_active && next < agent->navigation_point_count) {
            float preview = 1.0f - SmoothStep01(distance / 1.2f);
            attention = Vector3Lerp(attention, agent->navigation_point[next], preview);
        }
    }
    if (acting->attention_seconds > 0.0f) {
        attention = acting->attention_point;
        has_attention = true;
        float distance = Vector2Length((Vector2){attention.x - agent->position.x,
                                                attention.z - agent->position.z});
        pitch = Clamp(-atan2f(attention.y - agent->humanoid.pose.head.y,
                              fmaxf(0.10f, distance)), -0.30f, 0.30f);
        acting->attention_seconds = fmaxf(0.0f, acting->attention_seconds - dt);
    }
    if (has_attention) {
        float dx = attention.x - agent->position.x;
        float dz = attention.z - agent->position.z;
        if (dx * dx + dz * dz > 0.0025f) {
            gaze = Clamp(WrapAngle(atan2f(dx, dz) - agent->facing_yaw), -1.05f, 1.05f);
        }
    }
    float nod = 0.0f;
    if (acting->greeting_active && agent->humanoid.speed.value < 0.15f) {
        acting->greeting_time += dt;
        float time = acting->greeting_time;
        if (time < 0.24f) {
            float preparation = sinf(PI * time / 0.24f);
            nod = -0.06f * preparation * preparation;
        } else if (time < 0.90f) {
            float greeting = sinf(PI * (time - 0.24f) / 0.66f);
            nod = 0.22f * greeting * greeting;
        } else {
            acting->greeting_active = false;
        }
    }
    StepPerformanceSpring(&acting->gaze_yaw, gaze, 26.0f, dt);
    StepPerformanceSpring(&acting->gaze_pitch, pitch, 26.0f, dt);
    StepPerformanceSpring(&acting->chest_yaw, gaze * 0.30f, 14.0f, dt);
    StepPerformanceSpring(&acting->lean, lean, 12.0f, dt);
    StepPerformanceSpring(&acting->nod, nod, 24.0f, dt);
}

static CcLimbVec3 TurnPerformancePoint(CcLimbVec3 point, CcLimbVec3 pivot,
                                       float yaw)
{
    float x = point.x - pivot.x;
    float z = point.z - pivot.z;
    return (CcLimbVec3){pivot.x + cosf(yaw) * x + sinf(yaw) * z, point.y,
                        pivot.z - sinf(yaw) * x + cosf(yaw) * z};
}

static CcLimbVec3 LeanPerformancePoint(CcLimbVec3 point, CcLimbVec3 pivot,
                                       float yaw, float lean)
{
    CcLimbVec3 right = {cosf(yaw), 0, -sinf(yaw)};
    CcLimbVec3 forward = {sinf(yaw), 0, cosf(yaw)};
    float dx = point.x - pivot.x, dz = point.z - pivot.z;
    float depth = dx * forward.x + dz * forward.z;
    float height = point.y - pivot.y;
    float side = dx * right.x + dz * right.z;
    float tilted_depth = cosf(lean) * depth + sinf(lean) * height;
    return (CcLimbVec3){pivot.x + side * right.x + tilted_depth * forward.x,
        pivot.y + cosf(lean) * height - sinf(lean) * depth,
        pivot.z + side * right.z + tilted_depth * forward.z};
}

static void ApplyCharacterPerformance(const CcLocalAgent *agent, CcHumanoidPose *pose)
{
    if (!CharacterPerformanceActive(agent)) return;
    const CcCharacterPerformance *acting = &agent->performance;
    CcLimbVec3 pivot = pose->spine;
#define ACT_POINT(point) \
    pose->point = TurnPerformancePoint(pose->point, pivot, acting->chest_yaw.value); \
    pose->point = LeanPerformancePoint(pose->point, pose->pelvis, \
        agent->facing_yaw, acting->lean.value)
    ACT_POINT(chest);
    ACT_POINT(neck);
    ACT_POINT(head);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        ACT_POINT(shoulder[arm]);
        ACT_POINT(elbow[arm]);
        ACT_POINT(hand[arm]);
    }
#undef ACT_POINT
    pose->spine = LeanPerformancePoint(pose->spine, pose->pelvis,
        agent->facing_yaw, acting->lean.value);
    pose->chest_yaw += acting->chest_yaw.value;
    pose->chest_pitch += acting->lean.value;
}
