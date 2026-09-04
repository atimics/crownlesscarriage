static float CombatClamp(float value, float minimum, float maximum)
{
    return fmaxf(minimum, fminf(value, maximum));
}

static bool CombatCanAct(const CcCombatState *combat)
{
    return combat != NULL && combat->life_state == CC_LIFE_ALIVE;
}

static bool CombatIsDefeated(const CcCombatState *combat)
{
    return combat != NULL &&
        (combat->life_state == CC_LIFE_DEAD ||
         combat->life_state == CC_LIFE_RESPAWNING);
}

static Vector3 CombatSubtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 CombatAdd(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 CombatScale(Vector3 value, float scale)
{
    return (Vector3){value.x * scale, value.y * scale, value.z * scale};
}

static float CombatDot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float CombatLengthSquared(Vector3 value)
{
    return CombatDot(value, value);
}

static Vector3 CombatNormalizeOr(Vector3 value, Vector3 fallback)
{
    float length_squared = CombatLengthSquared(value);
    if (length_squared <= 0.000001f) return fallback;
    return CombatScale(value, 1.0f / sqrtf(length_squared));
}

static Vector3 CombatDirectionBlend(Vector3 first, Vector3 second,
                                    float amount, Vector3 fallback)
{
    amount = CombatClamp(amount, 0.0f, 1.0f);
    return CombatNormalizeOr(
        CombatAdd(CombatScale(first, 1.0f - amount),
                  CombatScale(second, amount)), fallback);
}

static Vector3 CombatWeaponDirectionAt(const CcLocalAgent *agent,
                                       float action_time)
{
    Vector3 forward = {sinf(agent->facing_yaw), 0.0f,
                       cosf(agent->facing_yaw)};
    Vector3 right = {cosf(agent->facing_yaw), 0.0f,
                     -sinf(agent->facing_yaw)};
    const Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 guard = CombatNormalizeOr(
        CombatAdd(CombatScale(forward, 0.66f),
                  CombatAdd(CombatScale(right, 0.24f),
                            CombatScale(up, 0.62f))), forward);
    if (agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE) {
        if (agent->humanoid.action == CC_HUMANOID_ACTION_GUARD ||
            agent->combat.focus_valid) {
            return guard;
        }
        return CombatNormalizeOr(
            CombatAdd(CombatScale(up, -0.80f),
                      CombatAdd(CombatScale(right, 0.52f),
                                CombatScale(forward, 0.12f))), forward);
    }

    float phase = CombatClamp(action_time / 1.20f, 0.0f, 1.0f);
    Vector3 chamber;
    Vector3 cut;
    float chamber_end = 0.22f;
    float cut_end = 0.58f;
    if (agent->combat.active_skill == CC_COMBAT_SKILL_CRUSHING_BLOW) {
        chamber = CombatNormalizeOr(
            CombatAdd(CombatScale(up, 0.96f),
                      CombatAdd(CombatScale(right, 0.14f),
                                CombatScale(forward, -0.20f))), guard);
        cut = CombatNormalizeOr(
            CombatAdd(CombatScale(forward, 0.90f),
                      CombatAdd(CombatScale(up, -0.42f),
                                CombatScale(right, -0.08f))), forward);
        chamber_end = 0.32f;
        cut_end = 0.64f;
    } else if (agent->combat.active_skill == CC_COMBAT_SKILL_SUNDER) {
        chamber = CombatNormalizeOr(
            CombatAdd(CombatScale(right, 0.94f),
                      CombatAdd(CombatScale(forward, -0.22f),
                                CombatScale(up, 0.24f))), guard);
        cut = CombatNormalizeOr(
            CombatAdd(CombatScale(right, -0.82f),
                      CombatAdd(CombatScale(forward, 0.56f),
                                CombatScale(up, 0.08f))), forward);
        chamber_end = 0.26f;
        cut_end = 0.60f;
    } else {
        chamber = CombatNormalizeOr(
            CombatAdd(CombatScale(right, 0.72f),
                      CombatAdd(CombatScale(up, 0.66f),
                                CombatScale(forward, -0.20f))), guard);
        cut = CombatNormalizeOr(
            CombatAdd(CombatScale(right, -0.58f),
                      CombatAdd(CombatScale(forward, 0.78f),
                                CombatScale(up, 0.18f))), forward);
    }
    if (phase < chamber_end) {
        return CombatDirectionBlend(guard, chamber,
            SmoothStep01(phase / chamber_end), guard);
    }
    if (phase < cut_end) {
        return CombatDirectionBlend(chamber, cut,
            SmoothStep01((phase - chamber_end) /
                         (cut_end - chamber_end)), cut);
    }
    return CombatDirectionBlend(cut, guard,
        SmoothStep01((phase - cut_end) / (1.0f - cut_end)), guard);
}

static float CombatHorizontalDistanceSquared(const CcLocalAgent *a,
                                             const CcLocalAgent *b)
{
    float x = b->position.x - a->position.x;
    float z = b->position.z - a->position.z;
    return x * x + z * z;
}

static float CombatFacingDot(const CcLocalAgent *agent, Vector3 point)
{
    float x = point.x - agent->position.x;
    float z = point.z - agent->position.z;
    float length = sqrtf(x * x + z * z);
    if (length <= 0.0001f) return 1.0f;
    return (sinf(agent->facing_yaw) * x + cosf(agent->facing_yaw) * z) /
           length;
}

static float CombatSegmentDistanceSquared(Vector3 first_start,
                                          Vector3 first_end,
                                          Vector3 second_start,
                                          Vector3 second_end)
{
    const float epsilon = 0.000001f;
    Vector3 first = CombatSubtract(first_end, first_start);
    Vector3 second = CombatSubtract(second_end, second_start);
    Vector3 offset = CombatSubtract(first_start, second_start);
    float first_length = CombatDot(first, first);
    float second_length = CombatDot(second, second);
    float second_offset = CombatDot(second, offset);
    float first_amount = 0.0f;
    float second_amount = 0.0f;
    if (first_length <= epsilon && second_length <= epsilon) {
        return CombatLengthSquared(offset);
    }
    if (first_length <= epsilon) {
        second_amount = CombatClamp(second_offset / second_length, 0.0f, 1.0f);
    } else {
        float first_offset = CombatDot(first, offset);
        if (second_length <= epsilon) {
            first_amount = CombatClamp(-first_offset / first_length, 0.0f,
                                       1.0f);
        } else {
            float directions = CombatDot(first, second);
            float denominator = first_length * second_length -
                                directions * directions;
            if (fabsf(denominator) > epsilon) {
                first_amount = CombatClamp(
                    (directions * second_offset -
                     first_offset * second_length) / denominator,
                    0.0f, 1.0f);
            }
            float second_numerator = directions * first_amount + second_offset;
            if (second_numerator < 0.0f) {
                second_amount = 0.0f;
                first_amount = CombatClamp(-first_offset / first_length,
                                           0.0f, 1.0f);
            } else if (second_numerator > second_length) {
                second_amount = 1.0f;
                first_amount = CombatClamp(
                    (directions - first_offset) / first_length, 0.0f, 1.0f);
            } else {
                second_amount = second_numerator / second_length;
            }
        }
    }
    Vector3 first_closest = CombatAdd(first_start,
                                      CombatScale(first, first_amount));
    Vector3 second_closest = CombatAdd(second_start,
                                       CombatScale(second, second_amount));
    return CombatLengthSquared(CombatSubtract(first_closest, second_closest));
}

static Vector3 CombatClosestPointOnSegment(Vector3 start, Vector3 end,
                                           Vector3 point)
{
    Vector3 segment = CombatSubtract(end, start);
    float length_squared = CombatLengthSquared(segment);
    if (length_squared <= 0.000001f) return start;
    float amount = CombatClamp(
        CombatDot(CombatSubtract(point, start), segment) / length_squared,
        0.0f, 1.0f);
    return CombatAdd(start, CombatScale(segment, amount));
}

static float CombatWeaponExtension(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 0.78f;
    if (team == CC_COMBAT_RAIDER) return 0.86f;
    return 1.02f;
}

static float CombatStrikeReach(CcCombatTeam team)
{
    if (team == CC_COMBAT_GUARD) return 1.62f;
    if (team == CC_COMBAT_RAIDER) return 1.42f;
    return 1.56f;
}

static float CombatStrikeDamage(const CcLocalAgent *attacker)
{
    CcCombatTeam team = attacker != NULL ? attacker->combat.team :
                                           CC_COMBAT_NEUTRAL;
    float base = 24.0f;
    if (team == CC_COMBAT_GUARD) base = 21.0f;
    if (team == CC_COMBAT_RAIDER) base = 19.0f;
    return base * AthleticBonus(attacker, CC_ATHLETIC_POWER, 0.08f);
}

static bool CombatTeamsHostile(CcCombatTeam first, CcCombatTeam second)
{
    if (first == CC_COMBAT_NEUTRAL || second == CC_COMBAT_NEUTRAL) {
        return false;
    }
    bool first_raider = first == CC_COMBAT_RAIDER;
    bool second_raider = second == CC_COMBAT_RAIDER;
    return first_raider != second_raider;
}

static bool AgentPhysicalRagdollActive(const CcLocalAgent *agent)
{
    if (agent == NULL) return false;
    return agent->morphology == CC_MORPHOLOGY_BIPED ?
           agent->humanoid.ragdoll.active :
           agent->multileg_ragdoll.active;
}

static bool AgentPhysicalRecovering(const CcLocalAgent *agent)
{
    if (agent == NULL) return false;
    return agent->morphology == CC_MORPHOLOGY_BIPED ?
           agent->humanoid.recovering :
           agent->multileg_ragdoll.recovering;
}

static bool BeginMultiLegCollapse(CcLocalAgent *agent,
                                  Vector3 impact_direction,
                                  Vector3 impact_point,
                                  float impact_speed,
                                  bool recovery_allowed)
{
    if (agent == NULL ||
        agent->morphology == CC_MORPHOLOGY_BIPED ||
        agent->multileg_ragdoll.active) {
        return false;
    }
    Vector3 velocity = agent->velocity;
    Vector3 direction = PhysicsNormalizeOr(
        impact_direction, (Vector3){0.0f, 0.25f, 1.0f});
    velocity = PhysicsAdd(
        velocity, PhysicsScale(direction, impact_speed * 0.38f));
    bool collapsed = CcMultilegRagdollCollapse(
        &agent->multileg_ragdoll, &agent->limb_rig,
        ToLimbVector(ShellPodBaseCenter(agent)), agent->facing_yaw,
        ToLimbVector(velocity), ToLimbVector(direction),
        ToLimbVector(impact_point), impact_speed, recovery_allowed);
    if (!collapsed) return false;
    agent->climbing = false;
    agent->free_climbing = false;
    agent->climbing_down = false;
    agent->vaulting = false;
    agent->climb_training_pending = false;
    agent->exact_target_valid = false;
    agent->grounded = false;
    agent->traversal = CC_TRAVERSAL_RAGDOLL;
    if (recovery_allowed && agent->combat.life_state == CC_LIFE_ALIVE) {
        agent->combat.life_state = CC_LIFE_KNOCKED_DOWN;
    }
    if (agent->combat.weapon_mode == CC_WEAPON_HELD) {
        agent->combat.weapon_mode = CC_WEAPON_RAGDOLL_ATTACHED;
    }
    return true;
}

static void CombatApplyKnockback(CcLocalAgent *attacker,
                                 CcLocalAgent *defender,
                                 Vector3 direction, float speed)
{
    float x = direction.x;
    float z = direction.z;
    float length = sqrtf(x * x + z * z);
    if (length <= 0.0001f) {
        x = defender->position.x - attacker->position.x;
        z = defender->position.z - attacker->position.z;
        length = sqrtf(x * x + z * z);
        if (length <= 0.0001f) {
            x = sinf(attacker->facing_yaw);
            z = cosf(attacker->facing_yaw);
            length = 1.0f;
        }
    }
    defender->combat.knockback_velocity.x = x / length * speed;
    defender->combat.knockback_velocity.z = z / length * speed;
    if (defender->humanoid.initialized &&
        !defender->humanoid.ragdoll.active) {
        defender->humanoid.body.root.velocity.x += x / length * speed * 0.46f;
        defender->humanoid.body.root.velocity.z += z / length * speed * 0.46f;
        defender->humanoid.root_velocity.x =
            defender->humanoid.body.root.velocity.x;
        defender->humanoid.root_velocity.z =
            defender->humanoid.body.root.velocity.z;
    }
}

static void CombatDefeat(CcLocalAgent *agent, Vector3 impact_direction,
                         Vector3 impact_point, float impact_speed)
{
    agent->combat.health = 0.0f;
    agent->combat.life_state = CC_LIFE_DEAD;
    agent->combat.weapon_mode = CC_WEAPON_RAGDOLL_ATTACHED;
    agent->combat.focus_valid = false;
    agent->combat.target_index = -1;
    agent->combat.queued_skill = -1;
    agent->combat.active_skill = -1;
    agent->exact_target_valid = false;
    agent->combat.stagger_seconds = 1.10f;
    agent->combat.respawn_seconds = agent->combat.team == CC_COMBAT_PLAYER ?
                                    2.75f : 0.0f;
    agent->combat.knockback_velocity = (Vector3){0};
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitSetGuarded(&agent->humanoid, false);
        (void)CcHumanoidGaitDie(&agent->humanoid,
                                ToLimbVector(impact_direction),
                                ToLimbVector(impact_point), impact_speed);
    } else {
        if (agent->multileg_ragdoll.active) {
            agent->multileg_ragdoll.recovery_allowed = false;
            agent->multileg_ragdoll.recovering = false;
        } else {
            (void)BeginMultiLegCollapse(agent, impact_direction, impact_point,
                                        impact_speed, false);
        }
    }
}

bool CcLocalAgentApplyDamage(CcLocalAgent *agent, float health_damage,
                             float posture_damage,
                             Vector3 impact_direction,
                             Vector3 impact_point, float impact_speed)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        health_damage < 0.0f || posture_damage < 0.0f ||
        !isfinite(impact_speed)) {
        return false;
    }
    impact_speed = fmaxf(0.0f, impact_speed);
    agent->combat.health = fmaxf(
        0.0f, agent->combat.health - health_damage);
    agent->combat.posture = fmaxf(
        0.0f, agent->combat.posture - posture_damage);
    agent->combat.impact_point = impact_point;
    agent->combat.impact_direction = impact_direction;
    agent->combat.impact_speed = impact_speed;
    agent->combat.impact_valid = true;
    agent->combat.hit_flash_seconds = fmaxf(
        agent->combat.hit_flash_seconds, 0.14f);
    agent->combat.stagger_seconds = fmaxf(
        agent->combat.stagger_seconds, 0.34f);

    if (agent->combat.health <= 0.0f) {
        CombatDefeat(agent, impact_direction, impact_point, impact_speed);
        return true;
    }
    bool should_collapse = agent->combat.posture <= 0.0f ||
        (impact_speed >= 6.20f && agent->combat.health <= 55.0f);
    if (!should_collapse) {
        if (agent->morphology == CC_MORPHOLOGY_BIPED) {
            CcHumanoidGaitApplyImpact(
                &agent->humanoid, ToLimbVector(impact_direction),
                CombatClamp(impact_speed / 9.0f, 0.0f, 1.0f));
        }
        return true;
    }
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitApplyImpact(
            &agent->humanoid, ToLimbVector(impact_direction), 1.0f);
        (void)CcHumanoidGaitKnockDown(&agent->humanoid);
    } else {
        (void)BeginMultiLegCollapse(agent, impact_direction, impact_point,
                                    impact_speed, true);
    }
    return true;
}

void CcLocalCombatSetTeam(CcLocalAgent *agent, CcCombatTeam team)
{
    if (agent == NULL) return;
    agent->combat.team = team;
    agent->combat.weapon_mode = team == CC_COMBAT_PLAYER ||
                                team == CC_COMBAT_RAIDER ?
                                CC_WEAPON_HELD : CC_WEAPON_NONE;
}

void CcLocalCombatSetFocus(CcLocalAgent *agent,
                           const CcLocalAgent *target)
{
    if (agent == NULL) return;
    if (target == NULL || !CombatCanAct(&target->combat)) {
        CcLocalCombatClearFocus(agent);
        return;
    }
    agent->combat.focus_point = target->position;
    agent->combat.focus_valid = true;
}

void CcLocalCombatClearFocus(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->combat.focus_valid = false;
    agent->combat.target_index = -1;
}

void CcLocalCombatSetGuarded(CcLocalAgent *agent,
                             const CcLocalAgent *target, bool guarded)
{
    if (agent == NULL) return;
    if (guarded && target != NULL) CcLocalCombatSetFocus(agent, target);
    if (!CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f) {
        guarded = false;
    }
    CcHumanoidGaitSetGuarded(&agent->humanoid, guarded);
    if (!guarded && agent->humanoid.action != CC_HUMANOID_ACTION_STRIKE) {
        CcLocalCombatClearFocus(agent);
    }
}

bool CcLocalCombatBeginStrike(CcLocalAgent *agent,
                              const CcLocalAgent *target)
{
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f || !agent->grounded) return false;
    if (target != NULL) CcLocalCombatSetFocus(agent, target);
    if (!CcHumanoidGaitBeginStrike(&agent->humanoid, 1)) return false;
    agent->combat.strike_resolved = false;
    agent->combat.active_skill = -1;
    return true;
}

bool CcLocalAgentJump(CcLocalAgent *agent)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->grounded || agent->climbing || agent->swimming ||
        !CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f ||
        !CcHumanoidGaitBeginJump(&agent->humanoid)) {
        return false;
    }
    float takeoff_speed = 4.35f +
        (float)(AthleticLevel(agent, CC_ATHLETIC_MOBILITY) - 1) * 0.16f;
    agent->velocity.y = takeoff_speed;
    agent->grounded = false;
    agent->jump_training_pending = true;
    agent->humanoid.body.root.velocity.y = takeoff_speed;
    agent->humanoid.root_velocity.y = takeoff_speed;
    return true;
}

CcCombatOutcome CcLocalCombatResolveStrike(CcLocalAgent *attacker,
                                           CcLocalAgent *defender)
{
    if (attacker == NULL || attacker->combat.strike_resolved) {
        return CC_COMBAT_OUTCOME_NONE;
    }
    attacker->combat.strike_resolved = true;
    float damage_scale = attacker->combat.strike_damage_scale > 0.0f ?
                         attacker->combat.strike_damage_scale : 1.0f;
    float posture_scale = attacker->combat.strike_posture_scale > 0.0f ?
                          attacker->combat.strike_posture_scale : 1.0f;
    float knockback_scale = attacker->combat.strike_knockback_scale > 0.0f ?
                            attacker->combat.strike_knockback_scale : 1.0f;
    attacker->combat.strike_damage_scale = 0.0f;
    attacker->combat.strike_posture_scale = 0.0f;
    attacker->combat.strike_knockback_scale = 0.0f;
    if (defender == NULL || attacker == defender ||
        attacker->morphology != CC_MORPHOLOGY_BIPED ||
        !CombatCanAct(&attacker->combat) ||
        !CombatCanAct(&defender->combat) ||
        !CombatTeamsHostile(attacker->combat.team, defender->combat.team) ||
        AgentPhysicalRagdollActive(attacker) ||
        AgentPhysicalRecovering(attacker) ||
        AgentPhysicalRagdollActive(defender) ||
        AgentPhysicalRecovering(defender) ||
        defender->climbing || defender->swimming) {
        return CC_COMBAT_OUTCOME_MISS;
    }
    float reach = CombatStrikeReach(attacker->combat.team);
    if (CombatHorizontalDistanceSquared(attacker, defender) > reach * reach ||
        fabsf(attacker->position.y - defender->position.y) > 0.62f ||
        CombatFacingDot(attacker, defender->position) < 0.24f) {
        return CC_COMBAT_OUTCOME_MISS;
    }

    Vector3 world_contact = {0};
    Vector3 world_normal = {0};
    Vector3 attack_center = {
        attacker->position.x, attacker->position.y + 0.94f,
        attacker->position.z};
    Vector3 defense_center = defender->morphology == CC_MORPHOLOGY_BIPED ?
        (Vector3){defender->position.x, defender->position.y + 0.94f,
                  defender->position.z} : ShellPodCenter(defender);
    if (LocalWorldSphereSweep(attacker->scene, attack_center, defense_center,
                              0.045f, &world_contact, &world_normal)) {
        attacker->combat.impact_point = world_contact;
        attacker->combat.impact_direction = world_normal;
        attacker->combat.impact_speed = 0.0f;
        attacker->combat.impact_valid = true;
        return CC_COMBAT_OUTCOME_MISS;
    }

    int32_t arm = attacker->humanoid.strike_side == 0 ? 0 : 1;
    Vector3 previous_hand = FromLimbVector(
        attacker->humanoid.previous_pose.hand[arm]);
    Vector3 current_hand = FromLimbVector(attacker->humanoid.pose.hand[arm]);
    float extension = CombatWeaponExtension(attacker->combat.team);
    Vector3 previous_direction = CombatWeaponDirectionAt(
        attacker, fmaxf(0.0f, attacker->humanoid.action_time -
                              attacker->humanoid.last_delta_time));
    Vector3 current_direction = CombatWeaponDirectionAt(
        attacker, attacker->humanoid.action_time);
    Vector3 previous_tip = CombatAdd(previous_hand,
                                     CombatScale(previous_direction,
                                                 extension));
    Vector3 current_tip = CombatAdd(current_hand,
                                    CombatScale(current_direction,
                                                extension));
    const float weapon_radius = 0.045f;
    if (LocalWorldSphereSweep(attacker->scene, previous_hand, previous_tip,
                              weapon_radius, &world_contact, &world_normal) ||
        LocalWorldSphereSweep(attacker->scene, previous_tip, current_tip,
                              weapon_radius, &world_contact, &world_normal) ||
        LocalWorldSphereSweep(attacker->scene, current_hand, current_tip,
                              weapon_radius, &world_contact, &world_normal)) {
        attacker->combat.impact_point = world_contact;
        attacker->combat.impact_direction = world_normal;
        attacker->combat.impact_speed = 0.0f;
        attacker->combat.impact_valid = true;
        return CC_COMBAT_OUTCOME_MISS;
    }
    Vector3 body_bottom;
    Vector3 body_top;
    if (defender->morphology == CC_MORPHOLOGY_BIPED) {
        body_bottom = (Vector3){defender->position.x,
                                defender->position.y + 0.38f,
                                defender->position.z};
        body_top = (Vector3){defender->position.x,
                             defender->position.y + 1.52f,
                             defender->position.z};
    } else {
        Vector3 body = ShellPodCenter(defender);
        body_bottom = body;
        body_top = body;
        body_bottom.y -= 0.24f;
        body_top.y += 0.24f;
    }
    float collision_distance = CombatSegmentDistanceSquared(
        current_hand, current_tip, body_bottom, body_top);
    collision_distance = fminf(collision_distance,
        CombatSegmentDistanceSquared(previous_hand, previous_tip,
                                     body_bottom, body_top));
    collision_distance = fminf(collision_distance,
        CombatSegmentDistanceSquared(previous_tip, current_tip,
                                     body_bottom, body_top));

    const float combined_radius = 0.62f;
    if (collision_distance > combined_radius * combined_radius) {
        return CC_COMBAT_OUTCOME_MISS;
    }

    Vector3 tip_motion = CombatSubtract(current_tip, previous_tip);
    float tip_travel = sqrtf(CombatLengthSquared(tip_motion));
    Vector3 impact_direction = tip_travel > 0.001f ?
        CombatScale(tip_motion, 1.0f / tip_travel) : current_direction;
    float impact_speed = tip_travel /
        fmaxf(attacker->humanoid.last_delta_time, 1.0f / 240.0f);
    impact_speed = CombatClamp(impact_speed, 2.4f, 9.0f);
    float impact_scale = CombatClamp(0.66f + impact_speed * 0.09f,
                                     0.82f, 1.36f);
    impact_scale *= AthleticBonus(attacker, CC_ATHLETIC_POWER, 0.055f);

    Vector3 impact_sample = current_tip;
    Vector3 impact_point = CombatClosestPointOnSegment(
        body_bottom, body_top, impact_sample);
    Vector3 guard_left = FromLimbVector(defender->humanoid.pose.hand[0]);
    Vector3 guard_right = FromLimbVector(defender->humanoid.pose.hand[1]);
    float guard_distance = CombatSegmentDistanceSquared(
        current_hand, current_tip, guard_left, guard_right);
    guard_distance = fminf(guard_distance, CombatSegmentDistanceSquared(
        previous_hand, previous_tip, guard_left, guard_right));
    guard_distance = fminf(guard_distance, CombatSegmentDistanceSquared(
        previous_tip, current_tip, guard_left, guard_right));
    bool guarded = defender->morphology == CC_MORPHOLOGY_BIPED &&
                   defender->humanoid.action == CC_HUMANOID_ACTION_GUARD &&
                   defender->combat.posture > 0.0f &&
                   CombatFacingDot(defender, attacker->position) >= 0.28f &&
                   guard_distance <= 0.58f * 0.58f;
    if (guarded) {
        impact_point = CombatClosestPointOnSegment(
            guard_left, guard_right, impact_sample);
    }
    defender->combat.impact_point = impact_point;
    defender->combat.impact_direction = impact_direction;
    defender->combat.impact_speed = impact_speed;
    defender->combat.impact_valid = true;
    if (defender->morphology == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitApplyImpact(
            &defender->humanoid, ToLimbVector(impact_direction),
            guarded ? 0.24f :
            CombatClamp(impact_scale * 0.72f, 0.0f, 1.0f));
    }
    attacker->combat.hitstop_seconds = fmaxf(
        attacker->combat.hitstop_seconds, guarded ? 0.040f : 0.055f);
    defender->combat.hitstop_seconds = fmaxf(
        defender->combat.hitstop_seconds, guarded ? 0.050f : 0.075f);
    defender->combat.hit_flash_seconds = fmaxf(
        defender->combat.hit_flash_seconds, guarded ? 0.075f : 0.14f);
    if (guarded) {
        float posture_damage = attacker->combat.team == CC_COMBAT_GUARD ?
                               42.0f :
                               attacker->combat.team == CC_COMBAT_RAIDER ?
                               24.0f : 32.0f;
        posture_damage *= impact_scale * posture_scale;
        defender->combat.posture = fmaxf(0.0f,
                                         defender->combat.posture -
                                         posture_damage);
        CcHumanoidGaitApplyImpact(
            &attacker->humanoid, ToLimbVector(CombatScale(
                impact_direction, -1.0f)), 0.22f);
        CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 6.0f);
        CcLocalAgentTrainAthleticism(defender, CC_ATHLETIC_POWER, 7.0f);
        if (defender->combat.posture > 0.0f) {
            CombatApplyKnockback(attacker, defender, impact_direction,
                                 0.34f * impact_scale * knockback_scale);
            return CC_COMBAT_OUTCOME_BLOCKED;
        }
        CcHumanoidGaitSetGuarded(&defender->humanoid, false);
        defender->combat.health = fmaxf(
            0.0f, defender->combat.health -
            CombatStrikeDamage(attacker) * impact_scale * 0.65f *
            damage_scale);
        defender->combat.stagger_seconds = 0.78f;
        CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
        if (defender->combat.health <= 0.0f) {
            CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
            CombatDefeat(defender, impact_direction, impact_point,
                         1.22f * impact_scale * knockback_scale);
            return CC_COMBAT_OUTCOME_DEFEATED;
        }
        CombatApplyKnockback(attacker, defender, impact_direction,
                             1.22f * impact_scale * knockback_scale);
        return CC_COMBAT_OUTCOME_GUARD_BROKEN;
    }

    float health_damage = CombatStrikeDamage(attacker) *
                          impact_scale * damage_scale;
    float posture_damage = 12.0f * impact_scale * posture_scale;
    (void)CcLocalAgentApplyDamage(
        defender, health_damage, posture_damage, impact_direction,
        impact_point, impact_speed);
    defender->combat.stagger_seconds = 0.34f;
    CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
    if (defender->combat.health <= 0.0f) {
        CcLocalAgentTrainAthleticism(attacker, CC_ATHLETIC_POWER, 12.0f);
        return CC_COMBAT_OUTCOME_DEFEATED;
    }
    CombatApplyKnockback(attacker, defender, impact_direction,
                         1.06f * impact_scale * knockback_scale);
    return CC_COMBAT_OUTCOME_HIT;
}

const char *CcLocalCombatOutcomeName(CcCombatOutcome outcome)
{
    switch (outcome) {
        case CC_COMBAT_OUTCOME_MISS: return "WHIFF";
        case CC_COMBAT_OUTCOME_HIT: return "HIT";
        case CC_COMBAT_OUTCOME_BLOCKED: return "BLOCKED";
        case CC_COMBAT_OUTCOME_GUARD_BROKEN: return "GUARD BROKEN";
        case CC_COMBAT_OUTCOME_DEFEATED: return "DOWNED";
        case CC_COMBAT_OUTCOME_NONE:
        default: return "";
    }
}

const char *CcLocalCombatTeamName(CcCombatTeam team)
{
    switch (team) {
        case CC_COMBAT_PLAYER: return "YOU";
        case CC_COMBAT_GUARD: return "GUARD";
        case CC_COMBAT_RAIDER: return "RAIDER";
        case CC_COMBAT_NEUTRAL:
        default: return "";
    }
}

static bool CombatSkillValid(CcCombatSkill skill)
{
    return skill >= 0 && skill < CC_COMBAT_SKILL_COUNT;
}

const char *CcLocalCombatSkillName(CcCombatSkill skill)
{
    switch (skill) {
        case CC_COMBAT_SKILL_CRUSHING_BLOW: return "CRUSHING BLOW";
        case CC_COMBAT_SKILL_SUNDER: return "SUNDER";
        case CC_COMBAT_SKILL_SECOND_WIND: return "CATCH BREATH";
        case CC_COMBAT_SKILL_COUNT:
        default: return "SKILL";
    }
}

float CcLocalCombatSkillDuration(CcCombatSkill skill)
{
    switch (skill) {
        case CC_COMBAT_SKILL_CRUSHING_BLOW: return 6.0f;
        case CC_COMBAT_SKILL_SUNDER: return 8.0f;
        case CC_COMBAT_SKILL_SECOND_WIND: return 14.0f;
        case CC_COMBAT_SKILL_COUNT:
        default: return 0.0f;
    }
}

float CcLocalCombatSkillCooldown(const CcLocalAgent *player,
                                 CcCombatSkill skill)
{
    if (player == NULL || !CombatSkillValid(skill)) return 0.0f;
    return fmaxf(0.0f, player->combat.skill_cooldown[skill]);
}

void CcLocalCourseInit(CcLocalCourse *course)
{
    static const int32_t starts[CC_LOCAL_COURSE_RUNNER_COUNT] = {0, 6, 12};
    static const Color colors[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        {50, 151, 160, 255}, {166, 91, 132, 255}, {176, 122, 54, 255}
    };
    static const Vector2 traveller_entries[CC_LOCAL_TRAVELLER_COUNT] = {
        {18.00f, 0.72f}, {31.00f, 71.28f},
        {43.00f, 0.72f}, {64.00f, 71.28f}
    };
    static const Vector2 traveller_exits[CC_LOCAL_TRAVELLER_COUNT] = {
        {18.00f, 71.28f}, {31.00f, 0.72f},
        {43.00f, 71.28f}, {64.00f, 0.72f}
    };
    static const Color traveller_colors[CC_LOCAL_TRAVELLER_COUNT] = {
        {118, 134, 145, 255}, {150, 105, 91, 255},
        {73, 137, 121, 255}, {139, 102, 153, 255}
    };
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    *course = (CcLocalCourse){0};
    course->scene = CC_LOCAL_SCENE_STREET;
    course->raider_initial_resolve = 78;
    course->raider_resolve = course->raider_initial_resolve;
    (void)snprintf(course->raider_company_name,
                   sizeof(course->raider_company_name), "Road company");
    (void)snprintf(course->raider_names[0],
                   sizeof(course->raider_names[0]), "The Captain");
    (void)snprintf(course->raider_names[1],
                   sizeof(course->raider_names[1]), "The Forager");
    course->raider_roles[0] = CC_LOCAL_RAIDER_CAPTAIN;
    course->raider_roles[1] = CC_LOCAL_RAIDER_FORAGER;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        const Vector3 start = COURSE_WAYPOINTS[starts[i]];
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, (Vector2){start.x, start.z}, false);
        CcLocalCombatSetTeam(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &runner->agent, UINT32_C(0x47554100) + (uint32_t)i,
            CC_NPC_ROLE_GUARD, colors[i]);
        runner->marker_color = colors[i];
        runner->duty = CC_GUARD_TRAINING;
        runner->next_waypoint = (starts[i] + 1) % waypoint_count;
        runner->pause_seconds = 0.20f + (float)i * 0.12f;
        runner->attack_cooldown = 0.28f + (float)i * 0.18f;
        if (CcLocalAgentSetExactTarget(
                &runner->agent, COURSE_WAYPOINTS[runner->next_waypoint], false)) {
            runner->next_waypoint = (runner->next_waypoint + 1) % waypoint_count;
        }
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInit(&course->raiders[i],
                         (Vector2){CC_LOCAL_WORLD_WIDTH - 0.80f,
                                   38.60f + (float)i * 2.80f}, false);
        CcLocalCombatSetTeam(&course->raiders[i], CC_COMBAT_RAIDER);
        course->raiders[i].crowned = false;
        CcLocalAgentSetNpcAppearance(
            &course->raiders[i], UINT32_C(0x52414900) + (uint32_t)i,
            CC_NPC_ROLE_RAIDER, (Color){126, 55, 61, 255});
        course->raider_entry[i] = course->raiders[i].position;
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        CcLocalTraveller *traveller = &course->travellers[i];
        traveller->entry = (Vector3){traveller_entries[i].x, 0.0f,
                                     traveller_entries[i].y};
        traveller->exit = (Vector3){traveller_exits[i].x, 0.0f,
                                    traveller_exits[i].y};
        CcLocalAgentInit(&traveller->agent, traveller_entries[i], false);
        traveller->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &traveller->agent, UINT32_C(0x54524100) + (uint32_t)i,
            i == 3 ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
            traveller_colors[i]);
        traveller->active = i < 2;
        traveller->respawn_delay = 1.8f + (float)i * 1.4f;
        if (traveller->active) {
            (void)CcLocalAgentSetExactTarget(&traveller->agent,
                                             traveller->exit, false);
        }
    }
    CcLocalAgentInit(&course->situation_witness,
                     (Vector2){CC_LOCAL_NOTICE_X + 0.92f,
                               CC_LOCAL_NOTICE_Z + 0.72f}, false);
    course->situation_witness.crowned = false;
    CcLocalAgentSetNpcAppearance(
        &course->situation_witness, UINT32_C(0x57495400),
        CC_NPC_ROLE_LABORER, (Color){173, 112, 76, 255});
    course->situation_witness_active = false;

    course->alarm_countdown = 24.0f;
    course->automatic_alarm = true;
    course->raider_attack_cooldown[0] = 0.52f;
    course->raider_attack_cooldown[1] = 0.78f;
}

const char *CcLocalRaiderRoleName(CcLocalRaiderRole role)
{
    switch (role) {
        case CC_LOCAL_RAIDER_CAPTAIN: return "CAPTAIN";
        case CC_LOCAL_RAIDER_FORAGER: return "FORAGER";
    }
    return "OUTLAW";
}

static const CcBanditGroup *CourseRaiderCompany(const CcSim *sim)
{
    if (sim == NULL) return NULL;
    if (sim->journey.active) {
        return CcSimBanditGroupOnRoute(sim, sim->journey.route_id);
    }
    const CcBanditGroup *strongest = NULL;
    for (int32_t i = 0; i < sim->bandit_count; ++i) {
        if (strongest == NULL ||
            sim->bandits[i].influence > strongest->influence) {
            strongest = &sim->bandits[i];
        }
    }
    return strongest;
}

static void CourseApplyRaiderIdentity(CcLocalCourse *course, int32_t index)
{
    if (course == NULL || index < 0 || index >= CC_LOCAL_RAIDER_COUNT) return;
    uint64_t mark = course->raider_company_id != 0U ?
        course->raider_company_id : UINT64_C(0x524149444552);
    uint32_t seed = (uint32_t)(mark ^ (mark >> 32U)) ^
                    (index == 0 ? UINT32_C(0x43415054) :
                                  UINT32_C(0x464f5247));
    Color accent = index == 0 ? (Color){168, 126, 58, 255} :
                                (Color){112, 65, 78, 255};
    CcLocalAgentSetNpcAppearance(&course->raiders[index], seed,
                                 CC_NPC_ROLE_RAIDER, accent);
    if (index == 0) {
        CcLocalAgentSetAthleticLevel(&course->raiders[index],
                                     CC_ATHLETIC_POWER, 3);
        CcLocalAgentSetAthleticLevel(&course->raiders[index],
                                     CC_ATHLETIC_GRIP, 3);
    } else {
        CcLocalAgentSetAthleticLevel(&course->raiders[index],
                                     CC_ATHLETIC_MOBILITY, 3);
    }
}

void CcLocalCourseBindRaiderCompany(CcLocalCourse *course,
                                    const CcSim *sim)
{
    if (course == NULL) return;
    const CcBanditGroup *bandits = CourseRaiderCompany(sim);
    if (bandits == NULL) return;
    static const char *captain_names[] = {
        "Maud Vey", "Harl Rook", "Ysabet Pike", "Osric Dunn",
        "Ren Tallow", "Tavin Grey"
    };
    static const char *forager_names[] = {
        "Kerrin", "Sable", "Noll", "Brin", "Edda", "Moss"
    };
    uint64_t serial = bandits->id & UINT64_C(0x00ffffffffffffff);
    course->raider_company_id = bandits->id;
    (void)snprintf(course->raider_company_name,
                   sizeof(course->raider_company_name), "%s", bandits->name);
    (void)snprintf(course->raider_names[0],
                   sizeof(course->raider_names[0]), "%s",
                   captain_names[serial % 6U]);
    (void)snprintf(course->raider_names[1],
                   sizeof(course->raider_names[1]), "%s",
                   forager_names[(serial / 3U + 2U) % 6U]);
    course->raider_initial_resolve = (int32_t)lroundf(CombatClamp(
        38.0f + (float)bandits->members * 0.45f +
        (float)bandits->influence * 0.34f +
        (float)bandits->supplies * 0.16f, 48.0f, 100.0f));
    course->raider_resolve = course->raider_initial_resolve;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CourseApplyRaiderIdentity(course, i);
    }
}

static bool CourseCombatOriginOpen(CcLocalSceneKind scene, Vector3 origin)
{
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        float z = origin.z + ((float)i - 1.0f) * COURSE_GUARD_SPACING;
        if (StaticBodyBlocked(scene, origin.x - 2.05f, z,
                              PLAYER_COLLISION_RADIUS)) return false;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        float z = origin.z + ((float)i - 0.5f) * COURSE_RAIDER_SPACING;
        if (StaticBodyBlocked(scene, origin.x + 1.85f, z,
                              PLAYER_COLLISION_RADIUS)) return false;
    }
    return !StaticBodyBlocked(scene, origin.x, origin.z,
                              PLAYER_COLLISION_RADIUS);
}

static void CoursePrepareCombatant(CcLocalAgent *agent, CcCombatTeam team)
{
    agent->combat = (CcCombatState){
        .health = CC_LOCAL_COMBAT_MAX_HEALTH,
        .posture = CC_LOCAL_COMBAT_MAX_POSTURE,
        .target_index = -1,
        .queued_skill = -1,
        .active_skill = -1,
        .team = team,
        .weapon_mode = team == CC_COMBAT_PLAYER || team == CC_COMBAT_RAIDER ?
                       CC_WEAPON_HELD : CC_WEAPON_NONE,
    };
    agent->exact_target_valid = false;
    agent->target_valid = false;
    CcHumanoidGaitSetGuarded(&agent->humanoid, false);
}

void CcLocalCourseStageRoadEncounter(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     bool hostile)
{
    if (course == NULL || player == NULL) return;
    static const Vector2 guard_positions[CC_LOCAL_COURSE_RUNNER_COUNT] = {
        {43.10f, 36.85f}, {42.40f, 40.20f}, {43.35f, 43.15f}
    };
    course->road_encounter = true;
    course->scene = CC_LOCAL_SCENE_ROAD;
    CcLocalAgentSetScene(player, CC_LOCAL_SCENE_ROAD);
    course->situation_witness_active = false;
    course->situation_witness_id = 0U;
    course->situation_witness_character_id = 0U;
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        course->travellers[i].active = false;
    }
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgentInit(&runner->agent, guard_positions[i], false);
        CcLocalAgentSetScene(&runner->agent, CC_LOCAL_SCENE_ROAD);
        CcLocalCombatSetTeam(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &runner->agent, UINT32_C(0x47554100) + (uint32_t)i,
            CC_NPC_ROLE_GUARD, runner->marker_color);
        runner->duty = hostile ? CC_GUARD_RESPONDING : CC_GUARD_TRAINING;
        runner->response_stage = 0;
        runner->response_waypoint_active = false;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        Vector2 raider_position = hostile ?
            (Vector2){54.60f + (float)i * 0.35f,
                      i == 0 ? 39.35f : 40.65f} :
            (Vector2){i == 0 ? 50.45f : 52.90f,
                      i == 0 ? 39.45f : 40.65f};
        CcLocalAgentInit(raider, raider_position, false);
        CcLocalAgentSetScene(raider, CC_LOCAL_SCENE_ROAD);
        CcLocalCombatSetTeam(raider, CC_COMBAT_RAIDER);
        if (!hostile) raider->combat.weapon_mode = CC_WEAPON_NONE;
        raider->crowned = false;
        CcLocalAgentSetNpcAppearance(
            raider, UINT32_C(0x52414900) + (uint32_t)i,
            i == 0 && !hostile ? CC_NPC_ROLE_MERCHANT : CC_NPC_ROLE_RAIDER,
            (Color){126, 55, 61, 255});
        course->raider_entry[i] = raider->position;
        course->raider_response_stage[i] = 0;
        course->raider_response_waypoint_active[i] = false;
    }
    course->combat_origin = (Vector3){52.10f, 0.0f, 40.00f};
    course->combat_origin_valid = true;
    if (hostile) CcLocalCourseRaiseAlarmNear(course, player);
}

void CcLocalCourseRaiseAlarmNear(CcLocalCourse *course,
                                 const CcLocalAgent *player)
{
    if (course->alarm_active) return;
    Vector3 origin = course->road_encounter ?
        (Vector3){47.20f, 0.0f, 40.00f} : player != NULL ?
        (Vector3){player->position.x + 4.65f, 0.0f,
                  player->position.z + 0.60f} :
        (Vector3){CC_LOCAL_START_X + 4.65f, 0.0f,
                  CC_LOCAL_START_Z + 0.60f};
    if (!CourseCombatOriginOpen(course->scene, origin)) {
        origin = (Vector3){CC_LOCAL_START_X + 4.65f, 0.0f,
                           CC_LOCAL_START_Z + 0.60f};
    }
    if (!CourseCombatOriginOpen(course->scene, origin)) {
        origin = course->road_encounter ?
            (Vector3){47.20f, 0.0f, 40.20f} :
            (Vector3){44.80f, 0.0f, 40.20f};
    }
    course->combat_origin = origin;
    course->combat_origin_valid = true;
    course->alarm_active = true;
    course->raiders_retreating = false;
    course->engagement_time = 0.0f;
    course->raider_resolve = course->raider_initial_resolve > 0 ?
        course->raider_initial_resolve : 78;
    course->last_outcome = CC_COMBAT_OUTCOME_NONE;
    course->last_attacker_team = CC_COMBAT_NEUTRAL;
    course->last_defender_team = CC_COMBAT_NEUTRAL;
    course->last_health_damage = 0.0f;
    course->last_posture_damage = 0.0f;
    course->combat_event_seconds = 0.0f;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        course->guard_entry[i] = runner->agent.position;
        CoursePrepareCombatant(&runner->agent, CC_COMBAT_GUARD);
        runner->agent.crowned = false;
        CcLocalAgentSetNpcAppearance(
            &runner->agent, UINT32_C(0x47554100) + (uint32_t)i,
            CC_NPC_ROLE_GUARD, runner->marker_color);
        runner->duty = CC_GUARD_RESPONDING;
        runner->response_stage = 0;
        runner->response_waypoint_active = false;
        runner->attack_cooldown = 0.22f + (float)i * 0.20f;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        course->raider_entry[i] = raider->position;
        CoursePrepareCombatant(raider, CC_COMBAT_RAIDER);
        raider->crowned = false;
        CourseApplyRaiderIdentity(course, i);
        course->raider_response_stage[i] = 0;
        course->raider_response_waypoint_active[i] = false;
        course->raider_attack_cooldown[i] = i == 0 ? 0.74f : 0.38f;
    }
}

void CcLocalCourseRaiseAlarm(CcLocalCourse *course)
{
    CcLocalCourseRaiseAlarmNear(course, NULL);
}

static bool CourseAgentBusy(const CcLocalAgent *agent)
{
    return agent->climbing || agent->humanoid.ragdoll.active ||
           agent->humanoid.recovering;
}

static bool CourseAgentCanSeparate(const CcLocalAgent *agent)
{
    return agent != NULL && CombatCanAct(&agent->combat) &&
           !agent->climbing && !agent->swimming &&
           !agent->humanoid.ragdoll.active && !agent->humanoid.recovering;
}

static void CourseAddSeparationPair(CcLocalAgent *first,
                                    CcLocalAgent *second,
                                    int32_t pair_index)
{
    if (!CourseAgentCanSeparate(first) ||
        !CourseAgentCanSeparate(second)) return;
    bool hostile = CombatTeamsHostile(first->combat.team,
                                      second->combat.team);
    bool bystander = first->combat.team == CC_COMBAT_NEUTRAL ||
                     second->combat.team == CC_COMBAT_NEUTRAL;
    float minimum = hostile ? COMBAT_PERSONAL_SPACE :
                    bystander ? COMBAT_BYSTANDER_SPACE : COMBAT_ALLY_SPACE;
    CcLimbVec3 first_correction = {0};
    CcLimbVec3 second_correction = {0};
    if (!CcRobotPredictiveAvoidance(
            ToLimbVector(first->position), ToLimbVector(first->velocity),
            ToLimbVector(second->position), ToLimbVector(second->velocity),
            minimum, 0.85f, pair_index,
            &first_correction, &second_correction)) {
        return;
    }
    first->separation_velocity.x += first_correction.x;
    first->separation_velocity.z += first_correction.z;
    second->separation_velocity.x += second_correction.x;
    second->separation_velocity.z += second_correction.z;
}

static void CourseLimitSeparation(CcLocalAgent *agent)
{
    float x = agent->separation_velocity.x;
    float z = agent->separation_velocity.z;
    float speed = sqrtf(x * x + z * z);
    const float maximum = 0.68f;
    if (speed > maximum) {
        agent->separation_velocity.x = x * maximum / speed;
        agent->separation_velocity.z = z * maximum / speed;
    }
}

static void CourseConfigureSituationWitness(CcLocalCourse *course,
                                            const CcSim *sim)
{
    if (course == NULL || sim == NULL) return;
    if (course->road_encounter) {
        course->situation_witness_active = false;
        course->situation_witness_id = 0U;
        course->situation_witness_character_id = 0U;
        return;
    }
    const CcSituation *situation = CcSimAcceptedSituation(sim);
    const CcCharacter *character = CcSimSituationConversationCharacter(
        sim, situation, sim->player.location_id);
    if (situation == NULL || character == NULL) {
        situation = NULL;
        character = NULL;
        for (int32_t i = 0; i < sim->situation_count; ++i) {
            const CcSituation *candidate = &sim->situations[i];
            if (candidate->status != CC_SITUATION_ACTIVE) continue;
            const CcCharacter *participant =
                CcSimSituationConversationCharacter(
                    sim, candidate, sim->player.location_id);
            if (participant != NULL) {
                situation = candidate;
                character = participant;
                break;
            }
        }
    }
    if (situation == NULL) {
        for (int32_t i = 0; i < sim->situation_count; ++i) {
            const CcSituation *candidate = &sim->situations[i];
            if (candidate->status == CC_SITUATION_ACTIVE ||
                candidate->created_day < sim->current_day - 56) continue;
            const CcCharacter *participant =
                CcSimSituationConversationCharacter(
                    sim, candidate, sim->player.location_id);
            if (participant == NULL) continue;
            if (situation == NULL ||
                candidate->created_day > situation->created_day) {
                situation = candidate;
                character = participant;
            }
        }
    }
    if (situation == NULL) {
        for (int32_t offset = 0; offset < sim->event_count; ++offset) {
            const CcEvent *event = CcSimRecentEvent(sim, offset);
            if (event == NULL || event->kind != CC_EVENT_DELAYED_ECHO ||
                event->location_id != sim->player.location_id) continue;
            situation = CcSimSituation(sim, event->subject_id);
            character = CcSimSituationConversationCharacter(
                sim, situation, sim->player.location_id);
            if (situation != NULL && character != NULL) break;
            situation = NULL;
            character = NULL;
        }
    }
    if (situation == NULL) {
        course->situation_witness_active = false;
        course->situation_witness_id = 0U;
        course->situation_witness_character_id = 0U;
        course->situation_witness.separation_velocity = (Vector3){0};
        return;
    }
    if (character == NULL) {
        course->situation_witness_active = false;
        course->situation_witness_id = 0U;
        course->situation_witness_character_id = 0U;
        return;
    }
    if (course->situation_witness_id != situation->id ||
        course->situation_witness_character_id != character->id) {
        CcLocalAgentInit(&course->situation_witness,
                         (Vector2){CC_LOCAL_NOTICE_X + 0.92f,
                                   CC_LOCAL_NOTICE_Z + 0.72f}, false);
        course->situation_witness.crowned = false;
        Color witness_accent =
            situation->status == CC_SITUATION_RESOLVED ?
                (Color){74, 145, 126, 255} :
            situation->kind == CC_SITUATION_BLACK_MARKET_DELIVERY ?
                (Color){91, 68, 111, 255} :
            situation->kind == CC_SITUATION_MONSTER_EXPEDITION ?
                (Color){128, 82, 66, 255} :
                (Color){173, 112, 76, 255};
        CcNpcRole witness_role = character->role == CC_CHARACTER_OFFICIAL ?
                CC_NPC_ROLE_MERCHANT :
            character->role == CC_CHARACTER_SCOUT ?
                CC_NPC_ROLE_SCOUT :
            character->role == CC_CHARACTER_TRAVELLER ||
            character->role == CC_CHARACTER_COURIER ?
                CC_NPC_ROLE_TRAVELLER :
            character->role == CC_CHARACTER_REFUGEE ?
                CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_LABORER;
        CcLocalAgentSetNpcAppearance(
            &course->situation_witness,
            character->appearance_seed,
            witness_role, witness_accent);
        course->situation_witness_id = situation->id;
        course->situation_witness_character_id = character->id;
        course->situation_witness_activity = character->activity;
        course->situation_witness_activity_seconds = 0.0f;
        course->situation_witness_activity_stage = 0;
    } else if (course->situation_witness_activity != character->activity) {
        course->situation_witness_activity = character->activity;
        course->situation_witness_activity_seconds = 0.0f;
        course->situation_witness_activity_stage = 0;
        course->situation_witness.exact_target_valid = false;
        course->situation_witness.target_valid = false;
    }
    course->situation_witness_active = true;
}

static void CourseUpdateSituationWitnessBehavior(CcLocalCourse *course,
                                                 float delta_time)
{
    if (course == NULL || !course->situation_witness_active) return;
    CcLocalAgent *witness = &course->situation_witness;
    course->situation_witness_activity_seconds += delta_time;
    Vector3 target = witness->position;
    bool wants_target = false;
    if (course->alarm_active) {
        target = (Vector3){37.55f, 0.0f, 24.65f};
        wants_target = true;
    } else if (course->situation_witness_activity ==
               CC_CHARACTER_ACTIVITY_PREPARING ||
               course->situation_witness_activity ==
               CC_CHARACTER_ACTIVITY_WORKING) {
        if (!witness->exact_target_valid && !CourseAgentBusy(witness) &&
            course->situation_witness_activity_seconds >= 1.2f) {
            static const Vector3 work_points[2] = {
                {42.20f, 0.0f, 28.52f},
                {44.10f, 0.0f, 29.30f}
            };
            target = work_points[course->situation_witness_activity_stage % 2];
            course->situation_witness_activity_stage += 1;
            course->situation_witness_activity_seconds = 0.0f;
            wants_target = true;
        }
    } else if (course->situation_witness_activity ==
               CC_CHARACTER_ACTIVITY_RECOVERING) {
        target = (Vector3){39.62f, 0.0f, 26.54f};
        wants_target = true;
    } else if (course->situation_witness_activity ==
               CC_CHARACTER_ACTIVITY_HIDING) {
        target = (Vector3){38.10f, 0.0f, 24.90f};
        wants_target = true;
    } else {
        target = (Vector3){CC_LOCAL_NOTICE_X + 0.92f, 0.0f,
                           CC_LOCAL_NOTICE_Z + 0.72f};
        wants_target = true;
    }
    float dx = target.x - witness->position.x;
    float dz = target.z - witness->position.z;
    if (wants_target && dx * dx + dz * dz > 0.20f &&
        !witness->exact_target_valid && !CourseAgentBusy(witness)) {
        (void)CcLocalAgentSetExactTarget(witness, target, false);
    }
}

static void CoursePlanActorSeparation(CcLocalCourse *course,
                                      CcLocalAgent *player)
{
    CcLocalAgent *actors[1 + CC_LOCAL_COURSE_RUNNER_COUNT +
                         CC_LOCAL_RAIDER_COUNT + CC_LOCAL_TRAVELLER_COUNT + 1];
    int32_t count = 0;
    if (player != NULL) actors[count++] = player;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        actors[count++] = &course->runners[i].agent;
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        actors[count++] = &course->raiders[i];
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (course->travellers[i].active) {
            actors[count++] = &course->travellers[i].agent;
        }
    }
    if (course->situation_witness_active) {
        actors[count++] = &course->situation_witness;
    }
    for (int32_t i = 0; i < count; ++i) {
        actors[i]->separation_velocity = (Vector3){0};
    }
    int32_t pair_index = 0;
    for (int32_t first = 0; first < count; ++first) {
        for (int32_t second = first + 1; second < count; ++second) {
            CourseAddSeparationPair(actors[first], actors[second],
                                    pair_index++);
        }
    }
    for (int32_t i = 0; i < count; ++i) CourseLimitSeparation(actors[i]);
}

static Vector3 CourseThreatCenter(const CcLocalCourse *course)
{
    Vector3 center = {0};
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        center.x += course->raiders[i].position.x;
        center.y += course->raiders[i].position.y;
        center.z += course->raiders[i].position.z;
    }
    center.x /= (float)CC_LOCAL_RAIDER_COUNT;
    center.y /= (float)CC_LOCAL_RAIDER_COUNT;
    center.z /= (float)CC_LOCAL_RAIDER_COUNT;
    return center;
}

static float CourseAlarmInterval(const CcSim *sim)
{
    int32_t influence = 0;
    if (sim != NULL) {
        for (int32_t i = 0; i < sim->bandit_count; ++i) {
            if (sim->bandits[i].influence > influence) {
                influence = sim->bandits[i].influence;
            }
        }
    }
    return fmaxf(9.0f, 22.0f - (float)influence * 0.16f);
}

static bool SetStreetClickTarget(CcLocalAgent *agent, Vector3 target);

static bool CourseRaiderResponseWaypoint(const CcLocalCourse *course,
                                         int32_t raider, int32_t stage,
                                         Vector3 *waypoint)
{
    static const Vector3 routes[CC_LOCAL_RAIDER_COUNT][3] = {
        {{72.00f, 0.0f, 38.60f}, {64.20f, 0.0f, 38.60f},
         {53.20f, 0.0f, 38.60f}},
        {{72.00f, 0.0f, 40.40f}, {64.20f, 0.0f, 40.40f},
         {53.20f, 0.0f, 40.40f}},
    };
    if (raider < 0 || raider >= CC_LOCAL_RAIDER_COUNT || stage < 0) {
        return false;
    }
    if (course != NULL && course->road_encounter) {
        static const Vector3 road_routes[CC_LOCAL_RAIDER_COUNT][4] = {
            {{55.10f, 0.0f, 39.35f}, {53.25f, 0.0f, 39.35f},
             {51.25f, 0.0f, 39.35f}, {48.80f, 0.0f, 39.35f}},
            {{55.45f, 0.0f, 40.65f}, {53.55f, 0.0f, 40.65f},
             {52.20f, 0.0f, 39.70f}, {49.50f, 0.0f, 39.70f}},
        };
        if (stage >= 4) return false;
        *waypoint = road_routes[raider][stage];
        return true;
    }
    if (stage >= 3) return false;
    *waypoint = routes[raider][stage];
    return true;
}

static bool CourseGuardIngressWaypoint(const CcLocalCourse *course,
                                       int32_t guard, int32_t stage,
                                       Vector3 *waypoint)
{
    if (course == NULL || waypoint == NULL || guard < 0 ||
        guard >= CC_LOCAL_COURSE_RUNNER_COUNT || stage < 0 || stage >= 4) {
        return false;
    }
    Vector3 entry = course->guard_entry[guard];
    if (course->road_encounter) {
        static const float road_guard_lanes[CC_LOCAL_COURSE_RUNNER_COUNT] = {
            38.35f, 40.95f, 42.10f
        };
        float lane_z = road_guard_lanes[guard];
        if (stage == 0) {
            *waypoint = (Vector3){44.75f, 0.0f, lane_z};
        } else if (stage == 1) {

            *waypoint = (Vector3){45.35f, 0.0f, lane_z};
        } else if (stage == 2) {
            *waypoint = (Vector3){47.70f, 0.0f, lane_z};
        } else {
            *waypoint = (Vector3){course->combat_origin.x - 1.45f, 0.0f,
                                  course->combat_origin.z +
                                  ((float)guard - 1.0f) * 0.72f};
        }
        return true;
    }
    float exit_x = entry.x < 11.80f ? 8.35f : 15.90f;
    float lane_z = 26.50f + (float)guard * 0.35f;
    if (stage == 0) {
        *waypoint = (Vector3){exit_x, 0.0f, entry.z};
    } else if (stage == 1) {
        *waypoint = (Vector3){exit_x, 0.0f, 12.80f};
    } else if (stage == 2) {
        *waypoint = (Vector3){16.20f + (float)guard * 0.50f, 0.0f,
                              lane_z};
    } else {
        *waypoint = (Vector3){42.80f, 0.0f, lane_z};
    }
    return true;
}

static bool CourseAgentReachedWaypoint(const CcLocalAgent *agent,
                                       Vector3 waypoint)
{
    if (agent == NULL) return false;
    float x = agent->position.x - waypoint.x;
    float z = agent->position.z - waypoint.z;

    return !agent->exact_target_valid || x * x + z * z <= 0.20f * 0.20f;
}

static void UpdateCourseTraining(CcLocalCourse *course, float delta_time)
{
    const int32_t waypoint_count = (int32_t)(sizeof(COURSE_WAYPOINTS) /
                                              sizeof(COURSE_WAYPOINTS[0]));
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        runner->duty = CC_GUARD_TRAINING;
        CcLocalAgentFixedStepInternal(&runner->agent, delta_time, false);
        if (!runner->agent.exact_target_valid &&
            !CourseAgentBusy(&runner->agent)) {
            runner->pause_seconds -= delta_time;
            if (runner->pause_seconds <= 0.0f &&
                CcLocalAgentSetExactTarget(
                    &runner->agent,
                    COURSE_WAYPOINTS[runner->next_waypoint], false)) {
                runner->next_waypoint =
                    (runner->next_waypoint + 1) % waypoint_count;
                runner->pause_seconds = 0.28f;
            }
        }
    }
}

static int32_t CourseTravellerDemand(const CcSim *sim)
{
    int32_t count = 2;
    if (sim == NULL) return count;
    for (int32_t i = 0; i < sim->shipment_count && count <
         CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (sim->shipments[i].status == CC_SHIPMENT_TRAVELLING) count += 1;
    }
    return count;
}

static void UpdateCourseTravellers(CcLocalCourse *course, const CcSim *sim,
                                   float delta_time)
{
    if (course->scene != CC_LOCAL_SCENE_STREET) return;
    int32_t demand = CourseTravellerDemand(sim);
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        CcLocalTraveller *traveller = &course->travellers[i];
        if (!traveller->active) {
            if (i >= demand) continue;
            traveller->respawn_delay -= delta_time;
            if (traveller->respawn_delay > 0.0f) continue;
            CcLocalAgentInit(&traveller->agent,
                             (Vector2){traveller->entry.x,
                                       traveller->entry.z}, false);
            traveller->agent.scene = course->scene;
            traveller->agent.crowned = false;
            static const Color colors[CC_LOCAL_TRAVELLER_COUNT] = {
                {118, 134, 145, 255}, {150, 105, 91, 255},
                {73, 137, 121, 255}, {139, 102, 153, 255}
            };
            CcLocalAgentSetNpcAppearance(
                &traveller->agent, UINT32_C(0x54524100) + (uint32_t)i,
                i == 3 ? CC_NPC_ROLE_REFUGEE : CC_NPC_ROLE_TRAVELLER,
                colors[i]);
            traveller->active = CcLocalAgentSetExactTarget(
                &traveller->agent, traveller->exit, false);
            continue;
        }
        CcLocalAgentFixedStepInternal(&traveller->agent, delta_time, false);
        if (traveller->agent.exact_target_valid) continue;
        float exit_x = traveller->agent.position.x - traveller->exit.x;
        float exit_z = traveller->agent.position.z - traveller->exit.z;
        if (exit_x * exit_x + exit_z * exit_z > 0.20f * 0.20f) continue;
        traveller->active = false;
        traveller->respawn_delay = 5.5f + (float)i * 1.7f;
    }
}

static int32_t CourseClosestRaider(const CcLocalCourse *course,
                                   const CcLocalAgent *agent,
                                   float maximum_distance,
                                   bool require_forward,
                                   int32_t excluded_index)
{
    int32_t closest = -1;
    float closest_distance = maximum_distance * maximum_distance;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (i == excluded_index) continue;
        const CcLocalAgent *raider = &course->raiders[i];
        if (!CombatCanAct(&raider->combat)) continue;
        float distance = CombatHorizontalDistanceSquared(agent, raider);
        if (distance >= closest_distance) continue;
        if (require_forward && distance > 1.15f * 1.15f &&
            CombatFacingDot(agent, raider->position) < -0.05f) continue;
        closest = i;
        closest_distance = distance;
    }
    return closest;
}

static CcLocalAgent *CourseClosestDefender(CcLocalCourse *course,
                                           CcLocalAgent *player,
                                           const CcLocalAgent *raider)
{
    CcLocalAgent *closest = NULL;
    float closest_distance = 3.20f * 3.20f;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgent *guard = &course->runners[i].agent;
        if (!CombatCanAct(&guard->combat) || CourseAgentBusy(guard)) continue;
        float distance = CombatHorizontalDistanceSquared(raider, guard);
        if (distance < closest_distance) {
            closest = guard;
            closest_distance = distance;
        }
    }
    if (player != NULL && CombatCanAct(&player->combat)) {
        float distance = CombatHorizontalDistanceSquared(raider, player);
        if (distance < closest_distance) closest = player;
    }
    return closest;
}

static void CourseRecordOutcome(CcLocalCourse *course,
                                const CcLocalAgent *attacker,
                                const CcLocalAgent *defender,
                                float previous_health,
                                float previous_posture,
                                CcCombatOutcome outcome)
{
    if (outcome == CC_COMBAT_OUTCOME_NONE) return;
    course->last_outcome = outcome;
    course->last_attacker_team = attacker->combat.team;
    course->last_defender_team = defender != NULL ?
        defender->combat.team : CC_COMBAT_NEUTRAL;
    course->last_health_damage = defender != NULL ?
        fmaxf(0.0f, previous_health - defender->combat.health) : 0.0f;
    course->last_posture_damage = defender != NULL ?
        fmaxf(0.0f, previous_posture - defender->combat.posture) : 0.0f;
    course->combat_event_seconds = 0.72f;
}

static void CourseResolveImpact(CcLocalCourse *course,
                                CcLocalAgent *attacker,
                                CcLocalAgent *defender)
{
    if (!CcHumanoidGaitConsumeStrikeImpact(&attacker->humanoid)) return;
    float previous_health = defender != NULL ? defender->combat.health : 0.0f;
    float previous_posture = defender != NULL ?
        defender->combat.posture : 0.0f;
    CourseRecordOutcome(course, attacker, defender,
                        previous_health, previous_posture,
                        CcLocalCombatResolveStrike(attacker, defender));
}

static void CourseRefreshRaiderResolve(CcLocalCourse *course,
                                       int32_t engaged_guards)
{
    float lost_health = 0.0f;
    int32_t defeated = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        lost_health += CC_LOCAL_COMBAT_MAX_HEALTH -
                       course->raiders[i].combat.health;
        if (CombatIsDefeated(&course->raiders[i].combat)) defeated += 1;
    }
    float pressure = lost_health * 0.28f + (float)defeated * 12.0f;
    if (CombatIsDefeated(&course->raiders[0].combat)) pressure += 28.0f;
    if (engaged_guards >= 2) pressure += 7.0f;
    if (course->engagement_time > 8.0f) {
        pressure += fminf(12.0f, course->engagement_time - 8.0f);
    }
    course->raider_resolve = (int32_t)lroundf(CombatClamp(
        (float)course->raider_initial_resolve - pressure,
        0.0f, CC_LOCAL_COMBAT_MAX_HEALTH));
}

static bool CourseRaidersBroken(const CcLocalCourse *course)
{
    float total_health = 0.0f;
    int32_t standing = 0;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        total_health += course->raiders[i].combat.health;
        standing += CombatIsDefeated(&course->raiders[i].combat) ? 0 : 1;
    }
    return standing == 0 || total_health <= 60.0f ||
           course->raider_resolve <= 35;
}

static void CourseBeginRetreat(CcLocalCourse *course, CcLocalAgent *player)
{
    course->raiders_retreating = true;
    course->raider_resolve = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        runner->duty = CC_GUARD_RETURNING;
        CcLocalCombatSetGuarded(&runner->agent, NULL, false);
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        if (CombatIsDefeated(&raider->combat)) {
            course->raider_response_waypoint_active[i] = false;
            continue;
        }
        raider->combat.stagger_seconds = 0.0f;
        raider->combat.hitstop_seconds = 0.0f;
        CcLocalCombatSetGuarded(raider, NULL, false);
        CcLocalCombatClearFocus(raider);
        course->raider_response_stage[i] = 2;
        course->raider_response_waypoint_active[i] = false;
    }
    if (player != NULL) CcLocalCourseClearPlayerTarget(player);
}

static CcLocalAgent *CoursePlayerTarget(CcLocalCourse *course,
                                        const CcLocalAgent *player)
{
    if (course == NULL || player == NULL ||
        player->combat.target_index < 0 ||
        player->combat.target_index >= CC_LOCAL_RAIDER_COUNT) {
        return NULL;
    }
    CcLocalAgent *target = &course->raiders[player->combat.target_index];
    return CombatCanAct(&target->combat) &&
           target->combat.team == CC_COMBAT_RAIDER ? target : NULL;
}

bool CcLocalCourseCanPlayerEngage(const CcLocalCourse *course,
                                  const CcLocalAgent *player,
                                  int32_t target_index)
{
    if (course == NULL || player == NULL || !course->alarm_active ||
        course->raiders_retreating || !CombatCanAct(&player->combat) ||
        target_index < 0 || target_index >= CC_LOCAL_RAIDER_COUNT) {
        return false;
    }
    const CcLocalAgent *target = &course->raiders[target_index];
    return target->combat.team == CC_COMBAT_RAIDER &&
           CombatCanAct(&target->combat) &&
           CombatHorizontalDistanceSquared(player, target) <=
               COMBAT_PLAYER_ENGAGEMENT_RADIUS *
               COMBAT_PLAYER_ENGAGEMENT_RADIUS;
}

bool CcLocalCourseHasNearbyHostile(const CcLocalCourse *course,
                                   const CcLocalAgent *player)
{
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        if (CcLocalCourseCanPlayerEngage(course, player, i)) return true;
    }
    return false;
}

static Vector3 CourseCombatApproachPoint(const CcLocalAgent *mover,
                                         const CcLocalAgent *target,
                                         float standoff,
                                         float orbit_angle)
{
    float x = mover->position.x - target->position.x;
    float z = mover->position.z - target->position.z;
    float distance = sqrtf(x * x + z * z);
    if (distance <= 0.0001f) {
        x = -sinf(mover->facing_yaw);
        z = -cosf(mover->facing_yaw);
        distance = 1.0f;
    }
    x /= distance;
    z /= distance;
    float cosine = cosf(orbit_angle);
    float sine = sinf(orbit_angle);
    float orbit_x = x * cosine - z * sine;
    float orbit_z = x * sine + z * cosine;
    return (Vector3){target->position.x + orbit_x * standoff,
                     target->position.y,
                     target->position.z + orbit_z * standoff};
}

void CcLocalCourseClearPlayerTarget(CcLocalAgent *player)
{
    if (player == NULL) return;
    player->combat.queued_skill = -1;
    CcLocalCombatSetGuarded(player, NULL, false);

    CcLocalCombatClearFocus(player);
}

bool CcLocalCourseSelectPlayerTarget(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     int32_t target_index)
{
    if (!CcLocalCourseCanPlayerEngage(course, player, target_index)) {
        return false;
    }
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    player->combat.target_index = target_index;
    player->combat.queued_skill = -1;
    player->exact_target_valid = false;
    player->target_valid = false;
    CcLocalCombatSetFocus(player, &course->raiders[target_index]);
    return true;
}

int32_t CcLocalCoursePickPlayerTarget(CcLocalCourse *course,
                                      CcLocalAgent *player,
                                      Vector2 screen_point,
                                      RenderTexture2D target,
                                      Rectangle destination)
{
    if (course == NULL || player == NULL || !course->alarm_active ||
        course->raiders_retreating ||
        !CheckCollisionPointRec(screen_point, destination)) {
        return -1;
    }
    Vector2 local = {
        (screen_point.x - destination.x) / destination.width *
            (float)target.texture.width,
        (screen_point.y - destination.y) / destination.height *
            (float)target.texture.height
    };
    Camera3D camera = {0};
    if (!PresentedCameraForInput(course->scene, player,
                                 target.texture.width,
                                 target.texture.height, &camera)) {
        Camera3D base_camera = course->scene == CC_LOCAL_SCENE_ROAD ?
            RoadCamera(player->position, false, 0.0f, false,
                       target.texture.height) :
            CcLocalStreetCameraInternal(player, 0.0f, false,
                                        target.texture.height);
        camera = CcLocalCombatCameraInternal(
            base_camera, player, course, 0.0f, false,
            target.texture.height);
    }
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    int32_t picked = -1;
    float nearest = FLT_MAX;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        const CcLocalAgent *raider = &course->raiders[i];
        if (!CombatCanAct(&raider->combat)) continue;
        BoundingBox body = {
            .min = {raider->position.x - 0.48f, raider->position.y,
                    raider->position.z - 0.48f},
            .max = {raider->position.x + 0.48f,
                    raider->position.y + 2.15f,
                    raider->position.z + 0.48f}
        };
        RayCollision collision = GetRayCollisionBox(ray, body);
        if (!collision.hit || collision.distance >= nearest) continue;
        picked = i;
        nearest = collision.distance;
    }

    if (picked < 0) {
        float best_score = FLT_MAX;
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            const CcLocalAgent *raider = &course->raiders[i];
            if (!CombatCanAct(&raider->combat)) continue;
            Vector3 center = Vector3Add(
                raider->position, (Vector3){0.0f, 1.10f, 0.0f});
            if (!CameraPointInFront(camera, center)) continue;
            Vector2 center_screen = GetWorldToScreenEx(
                center, camera, target.texture.width, target.texture.height);
            Vector2 head_screen = GetWorldToScreenEx(
                Vector3Add(raider->position,
                           (Vector3){0.0f, 2.12f, 0.0f}),
                camera, target.texture.width, target.texture.height);
            Vector2 feet_screen = GetWorldToScreenEx(
                Vector3Add(raider->position,
                           (Vector3){0.0f, 0.08f, 0.0f}),
                camera, target.texture.width, target.texture.height);
            if (center_screen.x < 0.0f ||
                center_screen.x > (float)target.texture.width ||
                center_screen.y < 0.0f ||
                center_screen.y > (float)target.texture.height) continue;
            float projected_height = fabsf(feet_screen.y - head_screen.y);
            float radius_x = CombatClamp(projected_height * 0.34f,
                                         18.0f, 24.0f);
            float radius_y = CombatClamp(projected_height * 0.58f,
                                         18.0f, 30.0f);
            float dx = (local.x - center_screen.x) / radius_x;
            float dy = (local.y - center_screen.y) / radius_y;
            float score = dx * dx + dy * dy;
            if (score > 1.0f || score >= best_score) continue;
            best_score = score;
            picked = i;
        }
    }
    return CcLocalCourseSelectPlayerTarget(course, player, picked) ?
           picked : -1;
}

bool CcLocalCourseUsePlayerSkill(CcLocalCourse *course,
                                 CcLocalAgent *player,
                                 CcCombatSkill skill)
{
    if (course == NULL || player == NULL || !CombatSkillValid(skill) ||
        !CombatCanAct(&player->combat) ||
        player->combat.skill_cooldown[skill] > 0.0f) {
        return false;
    }
    if (!course->alarm_active || course->raiders_retreating ||
        CoursePlayerTarget(course, player) == NULL ||
        !CcLocalCourseCanPlayerEngage(
            course, player, player->combat.target_index)) {
        return false;
    }
    if (skill == CC_COMBAT_SKILL_SECOND_WIND) {
        if (player->combat.posture >= CC_LOCAL_COMBAT_MAX_POSTURE) {
            return false;
        }
        player->combat.posture = fminf(CC_LOCAL_COMBAT_MAX_POSTURE,
                                       player->combat.posture + 65.0f);
        player->combat.skill_cooldown[skill] =
            CcLocalCombatSkillDuration(skill);
        return true;
    }
    player->combat.queued_skill = (int32_t)skill;
    return true;
}

static bool CourseBeginPlayerAttack(CcLocalAgent *player,
                                    CcLocalAgent *target,
                                    CcCombatSkill skill)
{
    if (!CcLocalCombatBeginStrike(player, target)) return false;
    player->combat.active_skill = CombatSkillValid(skill) ?
                                  (int32_t)skill : -1;
    CcHumanoidGaitSetStrikeStyle(
        &player->humanoid,
        skill == CC_COMBAT_SKILL_CRUSHING_BLOW ? CC_HUMANOID_STRIKE_HEAVY :
        skill == CC_COMBAT_SKILL_SUNDER ? CC_HUMANOID_STRIKE_SWEEP :
                                         CC_HUMANOID_STRIKE_CUT);
    player->combat.strike_damage_scale = 1.0f;
    player->combat.strike_posture_scale = 1.0f;
    player->combat.strike_knockback_scale = 1.0f;
    if (skill == CC_COMBAT_SKILL_CRUSHING_BLOW) {
        player->combat.strike_damage_scale = 1.75f;
        player->combat.strike_posture_scale = 1.15f;
        player->combat.strike_knockback_scale = 1.45f;
    } else if (skill == CC_COMBAT_SKILL_SUNDER) {
        player->combat.strike_damage_scale = 0.80f;
        player->combat.strike_posture_scale = 2.65f;
        player->combat.strike_knockback_scale = 0.82f;
    }
    return true;
}

static void CourseUpdatePlayerCombat(CcLocalCourse *course,
                                     CcLocalAgent *player,
                                     float delta_time)
{
    if (player == NULL) return;
    player->combat.auto_attack_cooldown = fmaxf(
        0.0f, player->combat.auto_attack_cooldown - delta_time);
    for (int32_t skill = 0; skill < CC_COMBAT_SKILL_COUNT; ++skill) {
        player->combat.skill_cooldown[skill] = fmaxf(
            0.0f, player->combat.skill_cooldown[skill] - delta_time);
    }
    if (!course->alarm_active) {
        CcLocalCourseClearPlayerTarget(player);
        return;
    }
    CcLocalAgent *target = CoursePlayerTarget(course, player);
    if (target == NULL || !CombatCanAct(&player->combat) ||
        course->raiders_retreating ||
        !CcLocalCourseCanPlayerEngage(
            course, player, player->combat.target_index)) {
        CcLocalCourseClearPlayerTarget(player);
        return;
    }

    CcLocalCombatSetFocus(player, target);
    float x = player->position.x - target->position.x;
    float z = player->position.z - target->position.z;
    float distance = sqrtf(x * x + z * z);
    bool can_reposition = !CourseAgentBusy(player) &&
        player->humanoid.action != CC_HUMANOID_ACTION_STRIKE;
    if ((distance > COMBAT_PLAYER_STANDOFF + 0.10f ||
         distance < COMBAT_MIN_STRIKE_DISTANCE) &&
        can_reposition) {

        Vector3 approach = CourseCombatApproachPoint(
            player, target, COMBAT_PLAYER_STANDOFF, 0.0f);
        (void)CcLocalAgentSetExactTarget(player, approach, false);
        return;
    }
    if (distance <= COMBAT_PLAYER_STANDOFF + 0.10f) {
        player->exact_target_valid = false;
        player->target_valid = false;
    }
    if (distance < COMBAT_MIN_STRIKE_DISTANCE || distance > 1.56f ||
        player->combat.auto_attack_cooldown > 0.0f) {
        return;
    }

    CcCombatSkill queued = (CcCombatSkill)player->combat.queued_skill;
    if (CombatSkillValid(queued)) {
        if (player->combat.skill_cooldown[queued] > 0.0f) {
            player->combat.queued_skill = -1;
            return;
        }
        if (CourseBeginPlayerAttack(player, target, queued)) {
            player->combat.skill_cooldown[queued] =
                CcLocalCombatSkillDuration(queued);
            player->combat.auto_attack_cooldown = 1.12f;
            player->combat.queued_skill = -1;
        }
        return;
    }
    if (CourseBeginPlayerAttack(player, target, CC_COMBAT_SKILL_COUNT)) {
        player->combat.auto_attack_cooldown = 0.92f;
    }
}

bool CcLocalCourseBeginPlayerStrike(CcLocalCourse *course,
                                    CcLocalAgent *player)
{
    if (course == NULL || player == NULL) return false;
    player->combat.queued_skill = -1;
    CcLocalAgent *target = CoursePlayerTarget(course, player);
    if (!course->alarm_active || course->raiders_retreating ||
        target == NULL || !CcLocalCourseCanPlayerEngage(
                              course, player,
                              player->combat.target_index)) {
        CcLocalCombatClearFocus(player);
        return false;
    }
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    return CourseBeginPlayerAttack(player, target, CC_COMBAT_SKILL_COUNT);
}

bool CcLocalCourseSetPlayerGuarded(CcLocalCourse *course,
                                   CcLocalAgent *player, bool guarded)
{
    if (course == NULL || player == NULL) return false;
    CcLocalAgent *target = CoursePlayerTarget(course, player);
    if (!course->alarm_active || course->raiders_retreating ||
        target == NULL || !CcLocalCourseCanPlayerEngage(
                              course, player,
                              player->combat.target_index)) {
        CcLocalCombatSetGuarded(player, NULL, false);
        return false;
    }
    int32_t target_index = player->combat.target_index;
    CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    CcLocalCombatSetGuarded(player, target, guarded);
    if (!guarded) {
        player->combat.target_index = target_index;
        CcLocalCombatSetFocus(player, target);
    }
    return true;
}

void CcLocalCourseFixedStepInternal(CcLocalCourse *course,
                                    CcLocalAgent *player,
                                    const CcSim *sim, float delta_time)
{
    UpdateCourseTravellers(course, sim, delta_time);
    if (course->scene == CC_LOCAL_SCENE_STREET) {
        CourseConfigureSituationWitness(course, sim);
    } else {
        course->situation_witness_active = false;
    }
    CourseUpdateSituationWitnessBehavior(course, delta_time);
    CoursePlanActorSeparation(course, player);
    if (course->situation_witness_active) {
        CcLocalAgentFixedStepInternal(&course->situation_witness,
                                      delta_time, false);
    }
    course->combat_event_seconds = fmaxf(
        0.0f, course->combat_event_seconds - delta_time);
    if (player != NULL && player->combat.team == CC_COMBAT_NEUTRAL) {
        CcLocalCombatSetTeam(player, CC_COMBAT_PLAYER);
    }
    CourseUpdatePlayerCombat(course, player, delta_time);
    if (!course->alarm_active) {
        if (player != NULL) CourseResolveImpact(course, player, NULL);
        if (course->automatic_alarm) course->alarm_countdown -= delta_time;
        if (course->automatic_alarm && course->alarm_countdown <= 0.0f) {
            if (course->raider_company_id == 0U) {
                CcLocalCourseBindRaiderCompany(course, sim);
            }
            CcLocalCourseRaiseAlarmNear(course, player);
        } else {
            UpdateCourseTraining(course, delta_time);
            return;
        }
    }
    if (player != NULL && player->combat.target_index >= 0 &&
        player->combat.target_index < CC_LOCAL_RAIDER_COUNT &&
        CombatCanAct(&course->raiders[player->combat.target_index].combat)) {
        CcLocalCombatSetFocus(
            player, &course->raiders[player->combat.target_index]);
    } else if (player != NULL &&
               player->humanoid.action != CC_HUMANOID_ACTION_STRIKE &&
               player->humanoid.action != CC_HUMANOID_ACTION_GUARD) {
        CcLocalCombatClearFocus(player);
    }

    if (course->raiders_retreating) {
        for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
            CcLocalCourseRunner *runner = &course->runners[i];
            runner->duty = CC_GUARD_RETURNING;
            CcLocalCombatSetGuarded(&runner->agent, NULL, false);
            (void)CcLocalAgentSetExactTarget(
                &runner->agent,
                (Vector3){course->combat_origin.x - 1.30f, 0.0f,
                          course->combat_origin.z +
                          ((float)i - 1.0f) * COURSE_GUARD_SPACING}, false);
            CcLocalAgentFixedStepInternal(&runner->agent, delta_time, false);
        }
        bool raiders_clear = true;
        for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
            CcLocalAgent *raider = &course->raiders[i];
            if (CombatIsDefeated(&raider->combat)) {
                CcLocalAgentFixedStepInternal(raider, delta_time, false);
                continue;
            }
            Vector3 retreat_waypoint;
            bool has_retreat_waypoint = CourseRaiderResponseWaypoint(
                course, i, course->raider_response_stage[i],
                &retreat_waypoint);
            if (has_retreat_waypoint &&
                !course->raider_response_waypoint_active[i]) {
                if (SetStreetClickTarget(raider, retreat_waypoint)) {
                    course->raider_response_waypoint_active[i] = true;
                }
            } else if (has_retreat_waypoint &&
                       course->raider_response_waypoint_active[i] &&
                       CourseAgentReachedWaypoint(raider,
                                                  retreat_waypoint)) {
                raider->exact_target_valid = false;
                raider->target_valid = false;
                course->raider_response_stage[i] -= 1;
                course->raider_response_waypoint_active[i] = false;
            } else if (!has_retreat_waypoint) {
                (void)CcLocalAgentSetExactTarget(
                    raider, course->raider_entry[i], false);
            }
            CcLocalAgentFixedStepInternal(raider, delta_time, false);
            float entry_x = raider->position.x - course->raider_entry[i].x;
            float entry_z = raider->position.z - course->raider_entry[i].z;
            if (entry_x * entry_x + entry_z * entry_z > 0.18f * 0.18f) {
                raiders_clear = false;
            }
        }
        if (raiders_clear) {
            course->alarm_active = false;
            course->raiders_retreating = false;
            course->defenses_completed += 1;
            course->alarm_countdown = CourseAlarmInterval(sim);
            for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
                course->runners[i].duty = CC_GUARD_TRAINING;
                course->runners[i].pause_seconds = 0.18f + (float)i * 0.08f;
                course->runners[i].agent.exact_target_valid = false;
            }
        }
        return;
    }

    int32_t engaged_guards = 0;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalCourseRunner *runner = &course->runners[i];
        CcLocalAgent *guard = &runner->agent;
        int32_t reserved_target = -1;
        if (player != NULL && CoursePlayerTarget(course, player) != NULL) {
            int32_t selected = player->combat.target_index;
            for (int32_t raider = 0; raider < CC_LOCAL_RAIDER_COUNT; ++raider) {
                if (raider != selected &&
                    CombatCanAct(&course->raiders[raider].combat)) {
                    reserved_target = selected;
                    break;
                }
            }
        }
        int32_t target_index = CourseClosestRaider(
            course, guard, 3.20f, false, reserved_target);
        CcLocalAgent *target = target_index >= 0 ?
                               &course->raiders[target_index] : NULL;
        bool engaged = target != NULL &&
            CombatHorizontalDistanceSquared(guard, target) < 3.20f * 3.20f;
        float target_distance_squared = target != NULL ?
            CombatHorizontalDistanceSquared(guard, target) : FLT_MAX;
        bool in_attack_range = target != NULL &&
            target_distance_squared < 1.62f * 1.62f &&
            target_distance_squared >= COMBAT_MIN_STRIKE_DISTANCE *
                                       COMBAT_MIN_STRIKE_DISTANCE;

        if (CombatCanAct(&guard->combat) && !CourseAgentBusy(guard)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseGuardIngressWaypoint(
                course, i, runner->response_stage, &response_waypoint);
            if (has_response_waypoint &&
                !runner->response_waypoint_active) {
                if (CcLocalAgentSetExactTarget(guard, response_waypoint,
                                               false)) {
                    runner->response_waypoint_active = true;
                }
            } else if (has_response_waypoint &&
                       runner->response_waypoint_active &&
                       CourseAgentReachedWaypoint(guard,
                                                  response_waypoint)) {
                guard->exact_target_valid = false;
                guard->target_valid = false;
                runner->response_stage += 1;
                runner->response_waypoint_active = false;
            } else if (!has_response_waypoint && !in_attack_range) {
                Vector3 guard_target = target != NULL ?
                    CourseCombatApproachPoint(guard, target,
                                              COMBAT_NPC_STANDOFF,
                                              ((float)i - 1.0f) * 0.32f) :
                    (Vector3){course->combat_origin.x - 1.30f, 0.0f,
                              course->combat_origin.z +
                              ((float)i - 1.0f) * COURSE_GUARD_SPACING};
                (void)CcLocalAgentSetExactTarget(guard, guard_target,
                                                 false);
            }
        }
        if (in_attack_range && CombatCanAct(&guard->combat)) {
            runner->duty = CC_GUARD_ENGAGED;
            guard->exact_target_valid = false;
            guard->combat.target_index = target_index;
            CcLocalCombatSetFocus(guard, target);
            CcLocalCombatSetGuarded(guard, target, true);
            runner->attack_cooldown -= delta_time;
            if (runner->attack_cooldown <= 0.0f &&
                CcLocalCombatBeginStrike(guard, target)) {
                runner->attack_cooldown = 0.88f + (float)i * 0.07f;
            }
            engaged_guards += 1;
        } else if (CombatCanAct(&guard->combat)) {
            runner->duty = engaged || runner->response_stage >= 4 ?
                           CC_GUARD_ENGAGED : CC_GUARD_RESPONDING;
            CcLocalCombatSetGuarded(guard, target, false);
        }
        CcLocalAgentFixedStepInternal(guard, delta_time, false);
        target_index = guard->combat.target_index;
        target = target_index >= 0 && target_index < CC_LOCAL_RAIDER_COUNT ?
                 &course->raiders[target_index] : NULL;
        CourseResolveImpact(course, guard, target);
    }

    if (engaged_guards > 0) course->engagement_time += delta_time;
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgent *raider = &course->raiders[i];
        if (!CombatCanAct(&raider->combat)) {
            CcLocalAgentFixedStepInternal(raider, delta_time, false);
            continue;
        }
        CcLocalAgent *eligible_player = player != NULL &&
            (player->combat.target_index < 0 ||
             player->combat.target_index == i) ? player : NULL;
        CcLocalAgent *target = CourseClosestDefender(
            course, eligible_player, raider);
        float target_distance_squared = target != NULL ?
            CombatHorizontalDistanceSquared(raider, target) : FLT_MAX;
        bool engaged = target != NULL &&
            target_distance_squared < 1.52f * 1.52f &&
            target_distance_squared >= COMBAT_MIN_STRIKE_DISTANCE *
                                       COMBAT_MIN_STRIKE_DISTANCE;
        if (!CourseAgentBusy(raider)) {
            Vector3 response_waypoint;
            bool has_response_waypoint = CourseRaiderResponseWaypoint(
                course, i, course->raider_response_stage[i],
                &response_waypoint);
            if (has_response_waypoint &&
                !course->raider_response_waypoint_active[i]) {
                if (CcLocalAgentSetExactTarget(raider, response_waypoint,
                                               false)) {
                    course->raider_response_waypoint_active[i] = true;
                }
            } else if (has_response_waypoint &&
                       course->raider_response_waypoint_active[i] &&
                       CourseAgentReachedWaypoint(raider,
                                                  response_waypoint)) {
                raider->exact_target_valid = false;
                raider->target_valid = false;
                course->raider_response_stage[i] += 1;
                course->raider_response_waypoint_active[i] = false;
            } else if (!has_response_waypoint) {
                Vector3 target_point = engaged ? raider->position :
                    target != NULL ? CourseCombatApproachPoint(
                        raider, target, COMBAT_NPC_STANDOFF,
                        ((float)i - 0.5f) * 0.28f) :
                    (Vector3){course->combat_origin.x + 1.30f, 0.0f,
                              course->combat_origin.z +
                              ((float)i - 0.5f) * COURSE_RAIDER_SPACING};
                (void)CcLocalAgentSetExactTarget(raider, target_point, false);
            }
        }
        if (engaged) {
            raider->exact_target_valid = false;
            CcLocalCombatSetFocus(raider, target);
            CcLocalCombatSetGuarded(raider, target, true);
            course->raider_attack_cooldown[i] -= delta_time;
            if (course->raider_attack_cooldown[i] <= 0.0f &&
                CcLocalCombatBeginStrike(raider, target)) {
                course->raider_attack_cooldown[i] = 1.02f +
                                                    (float)i * 0.11f;
            }
        } else {
            CcLocalCombatSetGuarded(raider, target, false);
        }
        CcLocalAgentFixedStepInternal(raider, delta_time, false);
        CourseResolveImpact(course, raider, target);
    }

    if (player != NULL) {
        CcLocalAgent *target = NULL;
        if (player->combat.target_index >= 0 &&
            player->combat.target_index < CC_LOCAL_RAIDER_COUNT) {
            target = &course->raiders[player->combat.target_index];
            if (!CombatCanAct(&target->combat)) target = NULL;
        }
        CourseResolveImpact(course, player, target);
    }
    CourseRefreshRaiderResolve(course, engaged_guards);
    if (CourseRaidersBroken(course)) {
        CourseBeginRetreat(course, player);
    }
}

void CcLocalCourseInterpolateInternal(CcLocalCourse *course, float amount)
{
    if (course == NULL) return;
    for (int32_t i = 0; i < CC_LOCAL_COURSE_RUNNER_COUNT; ++i) {
        CcLocalAgentInterpolateInternal(&course->runners[i].agent, amount);
    }
    for (int32_t i = 0; i < CC_LOCAL_RAIDER_COUNT; ++i) {
        CcLocalAgentInterpolateInternal(&course->raiders[i], amount);
    }
    for (int32_t i = 0; i < CC_LOCAL_TRAVELLER_COUNT; ++i) {
        if (course->travellers[i].active) {
            CcLocalAgentInterpolateInternal(&course->travellers[i].agent,
                                            amount);
        }
    }
    if (course->situation_witness_active) {
        CcLocalAgentInterpolateInternal(&course->situation_witness, amount);
    }
}

typedef struct ResolvedStreetPortal {
    const StreetTraversalLink *link;
    const StreetBoundaryExit *exit;
    int32_t destination_room;
    int32_t exit_index;
    bool reverse;
} ResolvedStreetPortal;

#define STREET_CLICK_NAVIGATION_DESTINATION INT32_MIN

static int32_t StreetRoomForAgent(const CcLocalAgent *agent)
{
    if (agent == NULL || agent->scene != CC_LOCAL_SCENE_STREET) return -1;
    return StreetNavigationRoomFor(agent->position, -1);
}

static bool ResolveStreetPortal(const CcLocalAgent *agent,
                                int32_t portal_index,
                                ResolvedStreetPortal *resolved)
{
    int32_t room = StreetRoomForAgent(agent);
    if (room < 0 || portal_index < 0 || resolved == NULL) return false;
    int32_t ordinal = 0;
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_TRAVERSAL_LINKS) /
                       sizeof(STREET_TRAVERSAL_LINKS[0])); ++i) {
        const StreetTraversalLink *link = &STREET_TRAVERSAL_LINKS[i];
        if (link->room_a != room && link->room_b != room) continue;
        if (ordinal++ != portal_index) continue;
        *resolved = (ResolvedStreetPortal){
            .link = link,
            .destination_room = link->room_a == room ? link->room_b :
                                                        link->room_a,
            .exit_index = -1,
            .reverse = link->room_b == room,
        };
        return true;
    }
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_BOUNDARY_EXITS) /
                       sizeof(STREET_BOUNDARY_EXITS[0])); ++i) {
        const StreetBoundaryExit *exit = &STREET_BOUNDARY_EXITS[i];
        if (exit->room != room) continue;
        if (ordinal++ != portal_index) continue;
        *resolved = (ResolvedStreetPortal){
            .exit = exit,
            .destination_room = -1,
            .exit_index = i,
        };
        return true;
    }
    return false;
}

int32_t CcLocalAgentStreetPortalCount(const CcLocalAgent *agent)
{
    int32_t room = StreetRoomForAgent(agent);
    if (room < 0) return 0;
    int32_t count = 0;
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_TRAVERSAL_LINKS) /
                       sizeof(STREET_TRAVERSAL_LINKS[0])); ++i) {
        if (STREET_TRAVERSAL_LINKS[i].room_a == room ||
            STREET_TRAVERSAL_LINKS[i].room_b == room) count += 1;
    }
    for (int32_t i = 0;
         i < (int32_t)(sizeof(STREET_BOUNDARY_EXITS) /
                       sizeof(STREET_BOUNDARY_EXITS[0])); ++i) {
        if (STREET_BOUNDARY_EXITS[i].room == room) count += 1;
    }
    return count;
}

const char *CcLocalAgentStreetPortalName(const CcLocalAgent *agent,
                                         int32_t portal_index)
{
    ResolvedStreetPortal portal = {0};
    if (!ResolveStreetPortal(agent, portal_index, &portal)) return NULL;
    if (portal.destination_room >= 0) {
        return CcLocalPlaceRoomName(active_place_function,
                                    portal.destination_room);
    }
    return portal.exit != NULL ? portal.exit->name : NULL;
}

static void ClearAgentNavigation(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->navigation_repath_count = 0;
    agent->navigation_destination_room = -1;
    agent->navigation_active = false;
    agent->navigation_world_exit = false;
    agent->world_exit_requested = false;
}

static bool SetAgentExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior)
{
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        StaticBodyBlocked(scene, target.x, target.z, agent->radius)) {
        return false;
    }
    agent->scene = scene;
    target.y = SurfaceHeightAt(scene, target.x, target.z);
    agent->target_point = target;
    agent->target_valid = true;
    agent->exact_target_valid = true;
    agent->movement_stall_seconds = 0.0f;
    return true;
}

bool CcLocalAgentSetExactTarget(CcLocalAgent *agent, Vector3 target,
                                bool market_interior)
{
    ClearAgentNavigation(agent);
    if (agent == NULL) return false;
    agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    if (!SetAgentExactTarget(agent, target, market_interior)) return false;
    agent->command_origin = agent->position;
    agent->command_point = agent->target_point;
    agent->command_point_valid = true;
    return true;
}

static bool AppendStreetNavigationPoint(CcLocalAgent *agent, Vector2 point)
{
    if (agent->navigation_point_count >=
        CC_LOCAL_NAVIGATION_POINT_CAPACITY ||
        StaticBodyBlocked(CC_LOCAL_SCENE_STREET, point.x, point.y,
                          agent->radius)) {
        return false;
    }
    Vector3 world = {
        point.x,
        SurfaceHeightAt(CC_LOCAL_SCENE_STREET, point.x, point.y),
        point.y,
    };
    Vector3 previous = agent->navigation_point_count > 0 ?
        agent->navigation_point[agent->navigation_point_count - 1] :
        agent->position;
    float x = world.x - previous.x;
    float z = world.z - previous.z;
    if (x * x + z * z < 0.12f * 0.12f) return true;
    agent->navigation_point[agent->navigation_point_count++] = world;
    return true;
}

static bool QueueStreetNavigationPointWithProjection(
    CcLocalAgent *agent, Vector2 point, float maximum_projection,
    bool *target_projected_out)
{
    if (agent == NULL) return false;
    Vector3 previous = agent->navigation_point_count > 0 ?
        agent->navigation_point[agent->navigation_point_count - 1] :
        agent->position;
    Vector2 from = {previous.x, previous.z};
    Vector2 path[CC_LOCAL_NAVIGATION_POINT_CAPACITY];
    int32_t remaining = CC_LOCAL_NAVIGATION_POINT_CAPACITY -
                        agent->navigation_point_count;
    int32_t path_count = FindStreetPath(
        from, point, agent->radius, path, remaining, maximum_projection,
        target_projected_out);
    if (path_count <= 0) return false;
    for (int32_t index = 0; index < path_count; ++index) {
        if (!AppendStreetNavigationPoint(agent, path[index])) return false;
    }
    return true;
}

static bool QueueStreetNavigationPoint(CcLocalAgent *agent, Vector2 point)
{
    return QueueStreetNavigationPointWithProjection(
        agent, point, STREET_SCRIPTED_MAX_PROJECTION, NULL);
}

static bool SetStreetClickTarget(CcLocalAgent *agent, Vector3 target)
{
    if (agent == NULL || !CombatCanAct(&agent->combat)) return false;
    ClearAgentNavigation(agent);
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    float target_land = CcLocalTerrainHeightAt(target.x, target.z);
    float agent_land = CcLocalTerrainHeightAt(agent->position.x,
                                               agent->position.z);
    bool target_on_platform = target.y > target_land + 0.24f;
    bool agent_on_platform = agent->position.y > agent_land + 0.24f;
    if (target_on_platform || agent_on_platform) {
        if (!SetAgentExactTarget(agent, target, false)) return false;
        agent->command_origin = agent->position;
        agent->command_point = agent->target_point;
        agent->command_point_valid = true;
        return true;
    }
    if (active_world_stream != NULL) {
        if (!SetAgentExactTarget(agent, target, false)) return false;
        agent->command_origin = agent->position;
        agent->command_point = agent->target_point;
        agent->command_point_valid = true;
        return true;
    }
    agent->scene = CC_LOCAL_SCENE_STREET;
    if (!QueueStreetNavigationPointWithProjection(
            agent, (Vector2){target.x, target.z},
            STREET_CLICK_MAX_PROJECTION, NULL) ||
        agent->navigation_point_count <= 0) {
        ClearAgentNavigation(agent);
        return false;
    }
    agent->navigation_destination_room =
        STREET_CLICK_NAVIGATION_DESTINATION;
    agent->navigation_active = true;
    if (!SetAgentExactTarget(agent, agent->navigation_point[0], false)) {
        ClearAgentNavigation(agent);
        return false;
    }

    target = agent->navigation_point[agent->navigation_point_count - 1];
    agent->command_origin = agent->position;
    agent->command_point = target;
    agent->command_point_valid = true;
    return true;
}

bool CcLocalAgentSetStreetTarget(CcLocalAgent *agent, Vector3 target)
{
    if (agent != NULL) agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
    return SetStreetClickTarget(agent, target);
}

Vector2 CcLocalWorldTargetApproachPoint(CcLocalWorldTargetKind target)
{
    if (target == CC_LOCAL_WORLD_TARGET_CARRIAGE) {
        return (Vector2){CC_LOCAL_CARRIAGE_APPROACH_X,
                         CC_LOCAL_CARRIAGE_APPROACH_Z};
    }
    return (Vector2){0};
}

const char *CcLocalWorldTargetName(CcLocalWorldTargetKind target)
{
    if (target == CC_LOCAL_WORLD_TARGET_CARRIAGE) return "Carriage";
    return NULL;
}

void CcLocalAgentClearWorldTarget(CcLocalAgent *agent)
{
    if (agent != NULL) agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
}

bool CcLocalAgentApproachWorldTarget(CcLocalAgent *agent,
                                     CcLocalWorldTargetKind target)
{
    if (agent == NULL || target != CC_LOCAL_WORLD_TARGET_CARRIAGE ||
        active_opening_step != CC_LOCAL_OPENING_COMPLETE) {
        return false;
    }
    agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
    Vector2 approach = CcLocalWorldTargetApproachPoint(target);
    Vector3 world = {
        approach.x, CcLocalTerrainHeightAt(approach.x, approach.y), approach.y
    };
    if (!SetStreetClickTarget(agent, world)) return false;
    agent->world_target = target;
    return true;
}

static bool StartStreetPortalTraversal(CcLocalAgent *agent,
                                       int32_t portal_index,
                                       bool include_room_trigger)
{
    ResolvedStreetPortal portal = {0};
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        !ResolveStreetPortal(agent, portal_index, &portal)) return false;

    int32_t room = StreetRoomForAgent(agent);
    agent->command_point_valid = false;
    ClearAgentNavigation(agent);
    Vector3 room_trigger = {
        STREET_CAMERA_SHOTS[room].trigger.x, 0.0f,
        STREET_CAMERA_SHOTS[room].trigger.y,
    };
    float trigger_x = room_trigger.x - agent->position.x;
    float trigger_z = room_trigger.z - agent->position.z;
    if (include_room_trigger &&
        trigger_x * trigger_x + trigger_z * trigger_z > 1.0f) {
        if (!QueueStreetNavigationPoint(
                agent, STREET_CAMERA_SHOTS[room].trigger)) {
            ClearAgentNavigation(agent);
            return false;
        }
    }

    bool queued = true;
    if (portal.link != NULL) {
        if (!portal.reverse) {
            for (int32_t i = 0; i < portal.link->via_count; ++i) {
                queued = queued && QueueStreetNavigationPoint(
                    agent, portal.link->via[i]);
            }
        } else {
            for (int32_t i = portal.link->via_count - 1; i >= 0; --i) {
                queued = queued && QueueStreetNavigationPoint(
                    agent, portal.link->via[i]);
            }
        }
        queued = queued && QueueStreetNavigationPoint(
            agent, STREET_CAMERA_SHOTS[portal.destination_room].trigger);
    } else if (portal.exit != NULL) {
        for (int32_t i = 0; i < portal.exit->via_count; ++i) {
            queued = queued && QueueStreetNavigationPoint(
                agent, portal.exit->via[i]);
        }
        queued = queued && QueueStreetNavigationPoint(
            agent, portal.exit->endpoint);
    }
    if (!queued || agent->navigation_point_count <= 0) {
        ClearAgentNavigation(agent);
        return false;
    }

    agent->navigation_point_index = 0;
    agent->navigation_destination_room = portal.destination_room >= 0 ?
        portal.destination_room : -1 - portal.exit_index;
    agent->navigation_active = true;
    agent->navigation_world_exit = portal.exit != NULL;
    if (!SetAgentExactTarget(agent, agent->navigation_point[0], false)) {
        ClearAgentNavigation(agent);
        return false;
    }
    return true;
}

bool CcLocalAgentFollowStreetPortal(CcLocalAgent *agent,
                                    int32_t portal_index)
{
    return StartStreetPortalTraversal(agent, portal_index, true);
}

const char *CcLocalAgentNavigationName(const CcLocalAgent *agent)
{
    if (agent == NULL || !agent->navigation_active) return NULL;
    if (agent->navigation_destination_room ==
        STREET_CLICK_NAVIGATION_DESTINATION) return NULL;
    if (agent->navigation_destination_room >= 0) {
        return CcLocalPlaceRoomName(active_place_function,
                                    agent->navigation_destination_room);
    }
    int32_t exit_index = -1 - agent->navigation_destination_room;
    if (exit_index >= (int32_t)(sizeof(STREET_BOUNDARY_EXITS) /
                                sizeof(STREET_BOUNDARY_EXITS[0]))) return NULL;
    return STREET_BOUNDARY_EXITS[exit_index].name;
}

bool CcLocalAgentConsumeWorldExit(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->world_exit_requested) return false;
    agent->world_exit_requested = false;
    return true;
}

static bool AdvanceAgentNavigation(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->navigation_active) return false;
    agent->navigation_point_index += 1;
    if (agent->navigation_point_index < agent->navigation_point_count) {
        bool targeted = SetAgentExactTarget(
            agent, agent->navigation_point[agent->navigation_point_index],
            false);
        if (!targeted) {
            ClearAgentNavigation(agent);
            agent->exact_target_valid = false;
            agent->target_valid = false;
        }
        return targeted;
    }

    bool world_exit = agent->navigation_world_exit;
    agent->navigation_active = false;
    agent->navigation_world_exit = false;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    agent->world_exit_requested = world_exit;
    return false;
}

static bool AgentCanAdvanceNavigation(const CcLocalAgent *agent,
                                      float target_distance)
{
    if (agent == NULL || !agent->navigation_active ||
        target_distance >= 0.35f) {
        return false;
    }
    int32_t next_index = agent->navigation_point_index + 1;
    if (next_index >= agent->navigation_point_count ||
        agent->scene != CC_LOCAL_SCENE_STREET) {
        return true;
    }
    Vector3 next = agent->navigation_point[next_index];
    return StreetSegmentClear(
        (Vector2){agent->position.x, agent->position.z},
        (Vector2){next.x, next.z}, agent->radius);
}

static float AgentCornerSpeedScale(const CcLocalAgent *agent,
                                   float target_distance)
{
    if (agent == NULL || !agent->navigation_active ||
        agent->navigation_point_index + 1 >= agent->navigation_point_count ||
        target_distance >= 1.25f) {
        return 1.0f;
    }
    Vector3 corner = agent->navigation_point[agent->navigation_point_index];
    Vector3 next = agent->navigation_point[agent->navigation_point_index + 1];
    Vector3 incoming = PhysicsSubtract(corner, agent->position);
    Vector3 outgoing = PhysicsSubtract(next, corner);
    float incoming_length = sqrtf(
        incoming.x * incoming.x + incoming.z * incoming.z);
    float outgoing_length = sqrtf(
        outgoing.x * outgoing.x + outgoing.z * outgoing.z);
    if (incoming_length < 0.001f || outgoing_length < 0.001f) return 1.0f;
    float alignment =
        (incoming.x * outgoing.x + incoming.z * outgoing.z) /
        (incoming_length * outgoing_length);
    alignment = fmaxf(-1.0f, fminf(1.0f, alignment));
    float severity = (1.0f - alignment) * 0.5f;
    float proximity = fmaxf(
        0.0f, fminf(1.0f, (1.25f - target_distance) / 1.25f));
    proximity = proximity * proximity * (3.0f - 2.0f * proximity);
    return fmaxf(0.34f, 1.0f - severity * proximity * 0.72f);
}

static Vector3 StreetPortalWorldPoint(const ResolvedStreetPortal *portal)
{
    Vector2 point = portal->destination_room >= 0 ?
        STREET_CAMERA_SHOTS[portal->destination_room].trigger :
        portal->exit->endpoint;
    return (Vector3){point.x,
                     CcLocalTerrainHeightAt(point.x, point.y) + 0.42f,
                     point.y};
}

static Vector3 StreetPortalApproachWorldPoint(
    const ResolvedStreetPortal *portal)
{
    if (portal->link != NULL && portal->link->via_count > 0) {
        int32_t index = portal->reverse ? portal->link->via_count - 1 : 0;
        Vector2 point = portal->link->via[index];
        return (Vector3){point.x,
                         CcLocalTerrainHeightAt(point.x, point.y) + 0.42f,
                         point.y};
    }
    if (portal->exit != NULL && portal->exit->via_count > 0) {
        Vector2 point = portal->exit->via[0];
        return (Vector3){point.x,
                         CcLocalTerrainHeightAt(point.x, point.y) + 0.42f,
                         point.y};
    }
    return StreetPortalWorldPoint(portal);
}

static Vector2 StreetPortalEdgePoint(const ResolvedStreetPortal *portal,
                                     Camera3D camera, int32_t width,
                                     int32_t height)
{
    Vector2 projected = GetWorldToScreenEx(
        StreetPortalWorldPoint(portal), camera, width, height);
    Vector2 center = {(float)width * 0.5f, (float)height * 0.5f};
    Vector2 direction = {projected.x - center.x,
                         projected.y - center.y};
    if (fabsf(direction.x) + fabsf(direction.y) < 0.001f) {
        direction = (Vector2){1.0f, 0.0f};
    }
    float horizontal = ((float)width * 0.5f - 13.0f) /
                       fmaxf(0.001f, fabsf(direction.x));
    float vertical = ((float)height * 0.5f - 13.0f) /
                     fmaxf(0.001f, fabsf(direction.y));
    float scale = fminf(horizontal, vertical);
    return (Vector2){center.x + direction.x * scale,
                     center.y + direction.y * scale};
}

static float StreetEdgeStripScore(Vector2 point, Vector2 marker,
                                  int32_t width, int32_t height,
                                  float maximum_inward, float maximum_cross)
{
    float left = marker.x;
    float right = (float)width - marker.x;
    float top = marker.y;
    float bottom = (float)height - marker.y;
    float inward = 0.0f;
    float cross = 0.0f;
    if (left <= right && left <= top && left <= bottom) {
        inward = point.x - marker.x;
        cross = fabsf(point.y - marker.y);
    } else if (right <= top && right <= bottom) {
        inward = marker.x - point.x;
        cross = fabsf(point.y - marker.y);
    } else if (top <= bottom) {
        inward = point.y - marker.y;
        cross = fabsf(point.x - marker.x);
    } else {
        inward = marker.y - point.y;
        cross = fabsf(point.x - marker.x);
    }
    if (inward < -6.0f || inward > maximum_inward ||
        cross > maximum_cross) return FLT_MAX;
    return inward / maximum_inward + cross / maximum_cross;
}

static float StreetPortalProximityScore(const CcLocalAgent *agent,
                                        const ResolvedStreetPortal *portal,
                                        int32_t room)
{
    enum { ART_WIDTH = 457, ART_HEIGHT = 285 };
    if (agent == NULL || portal == NULL || room < 0 ||
        room >= (int32_t)(sizeof(STREET_CAMERA_SHOTS) /
                          sizeof(STREET_CAMERA_SHOTS[0]))) return FLT_MAX;
    const StreetCameraShot *composition = &STREET_CAMERA_SHOTS[room];
    Vector3 camera_target = composition->target;
    camera_target.y += CcLocalTerrainHeightAt(camera_target.x,
                                               camera_target.z);
    Camera3D camera = SnapCameraToArtPixels(
        ExteriorCameraComposed(camera_target,
                               composition->camera_offset,
                               composition->fovy),
        ART_HEIGHT);
    Vector2 projected = GetWorldToScreenEx(
        StreetPortalApproachWorldPoint(portal), camera, ART_WIDTH,
        ART_HEIGHT);
    Vector2 center = {(float)ART_WIDTH * 0.5f,
                      (float)ART_HEIGHT * 0.5f};
    Vector2 direction = {projected.x - center.x,
                         projected.y - center.y};
    if (fabsf(direction.x) + fabsf(direction.y) < 0.001f) {
        direction = (Vector2){1.0f, 0.0f};
    }
    float horizontal = ((float)ART_WIDTH * 0.5f - 13.0f) /
                       fmaxf(0.001f, fabsf(direction.x));
    float vertical = ((float)ART_HEIGHT * 0.5f - 13.0f) /
                     fmaxf(0.001f, fabsf(direction.y));
    float marker_scale = fminf(horizontal, vertical);
    Vector2 marker = {center.x + direction.x * marker_scale,
                      center.y + direction.y * marker_scale};
    Vector2 player = GetWorldToScreenEx(
        (Vector3){agent->position.x,
                  CcLocalTerrainHeightAt(agent->position.x,
                                         agent->position.z) + 0.42f,
                  agent->position.z}, camera,
        ART_WIDTH, ART_HEIGHT);


    float edge_score = StreetEdgeStripScore(
        player, marker, ART_WIDTH, ART_HEIGHT, 82.0f, 64.0f);
    Vector3 approach = StreetPortalApproachWorldPoint(portal);
    float approach_x = agent->position.x - approach.x;
    float approach_z = agent->position.z - approach.z;
    float approach_distance = sqrtf(approach_x * approach_x +
                                    approach_z * approach_z);
    float corridor_x = composition->trigger.x - approach.x;
    float corridor_z = composition->trigger.y - approach.z;
    float corridor_length = sqrtf(corridor_x * corridor_x +
                                  corridor_z * corridor_z);
    float physical_radius = corridor_length >= 4.0f ?
        fminf(7.2f, corridor_length * 0.55f) : 0.0f;
    float physical_score = physical_radius > 0.0f &&
                           approach_distance <= physical_radius ?
        0.15f + approach_distance / physical_radius : FLT_MAX;
    return fminf(edge_score, physical_score);
}

static float StreetPortalCommandAlignment(
    const CcLocalAgent *agent, const ResolvedStreetPortal *portal)
{
    if (agent == NULL || portal == NULL || !agent->command_point_valid) {
        return -1.0f;
    }
    Vector3 portal_point = StreetPortalWorldPoint(portal);
    float intent_x = agent->command_point.x - agent->command_origin.x;
    float intent_z = agent->command_point.z - agent->command_origin.z;
    float portal_x = portal_point.x - agent->command_origin.x;
    float portal_z = portal_point.z - agent->command_origin.z;
    float intent_length = sqrtf(intent_x * intent_x + intent_z * intent_z);
    float portal_length = sqrtf(portal_x * portal_x + portal_z * portal_z);
    if (intent_length < 0.08f || portal_length < 0.08f) return -1.0f;
    return (intent_x * portal_x + intent_z * portal_z) /
           (intent_length * portal_length);
}

static void UpdateStreetPortalProximity(CcLocalAgent *agent)
{
    if (active_world_stream != NULL) return;
    bool following_click_path = agent != NULL && agent->navigation_active &&
        agent->navigation_destination_room ==
            STREET_CLICK_NAVIGATION_DESTINATION;
    if (agent == NULL || !agent->crowned ||
        (agent->navigation_active && !following_click_path) ||
        !agent->exact_target_valid || agent->scene != CC_LOCAL_SCENE_STREET) {
        return;
    }
    int32_t room = StreetRoomForAgent(agent);
    int32_t count = CcLocalAgentStreetPortalCount(agent);
    int32_t nearest = -1;
    float nearest_score = FLT_MAX;
    for (int32_t portal_index = 0; portal_index < count; ++portal_index) {
        ResolvedStreetPortal portal = {0};
        if (!ResolveStreetPortal(agent, portal_index, &portal)) continue;
        float alignment = StreetPortalCommandAlignment(agent, &portal);
        if (alignment < 0.35f) continue;
        float score = StreetPortalProximityScore(agent, &portal, room);
        if (score < FLT_MAX) score += (1.0f - alignment) * 0.50f;
        if (score >= nearest_score) continue;
        nearest = portal_index;
        nearest_score = score;
    }
    if (nearest >= 0) {

        (void)StartStreetPortalTraversal(agent, nearest, false);
    }
}

static float RayFootprintDistance(Ray ray, Rectangle footprint, float height)
{
    BoundingBox box = {
        .min = {footprint.x, 0.0f, footprint.y},
        .max = {footprint.x + footprint.width, height,
                footprint.y + footprint.height}
    };
    RayCollision collision = GetRayCollisionBox(ray, box);
    return collision.hit ? collision.distance : FLT_MAX;
}

static float RoomArtObstacleRayDistance(Ray ray, Vector3 focus)
{
    float nearest = FLT_MAX;
    for (int32_t i = 0; i < (int32_t)(sizeof(ROOM_ART_OBSTACLES) /
                                      sizeof(ROOM_ART_OBSTACLES[0])); ++i) {
        Rectangle footprint = ROOM_ART_OBSTACLES[i];
        float center_x = footprint.x + footprint.width * 0.5f;
        float center_z = footprint.y + footprint.height * 0.5f;
        if (!RoomDetailPointVisible(center_x, center_z, focus)) continue;
        nearest = fminf(nearest,
                        RayFootprintDistance(ray, footprint, 4.90f));
    }
    return nearest;
}

float CcLocalRoomArtRayDistanceInternal(Ray ray, Vector3 focus)
{
    return RoomArtObstacleRayDistance(ray, focus);
}

static bool SetNearestClickTarget(CcLocalAgent *agent, Vector3 picked_point,
                                  bool market_interior)
{
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    bool targeted = scene == CC_LOCAL_SCENE_STREET &&
                    active_world_stream == NULL ?
        SetStreetClickTarget(agent, picked_point) :
        CcLocalAgentSetExactTarget(agent, picked_point, market_interior);
    if (targeted) {
        return true;
    }


    float toward_x = agent->position.x - picked_point.x;
    float toward_z = agent->position.z - picked_point.z;
    float toward_length = sqrtf(toward_x * toward_x + toward_z * toward_z);
    if (toward_length > 0.0001f) {
        toward_x /= toward_length;
        toward_z /= toward_length;
    } else {
        toward_x = sinf(agent->facing_yaw + PI);
        toward_z = cosf(agent->facing_yaw + PI);
    }
    static const int32_t angle_steps[] = {0, 1, -1, 2, -2, 3, -3};
    for (int32_t ring = 1; ring <= 3; ++ring) {
        float radius = (float)ring * 0.24f;
        for (int32_t i = 0; i < (int32_t)(sizeof(angle_steps) /
                                          sizeof(angle_steps[0])); ++i) {
            float angle = (float)angle_steps[i] * PI / 6.0f;
            float cosine = cosf(angle);
            float sine = sinf(angle);
            Vector3 candidate = picked_point;
            candidate.x += (toward_x * cosine - toward_z * sine) * radius;
            candidate.z += (toward_x * sine + toward_z * cosine) * radius;
            targeted = scene == CC_LOCAL_SCENE_STREET &&
                       active_world_stream == NULL ?
                SetStreetClickTarget(agent, candidate) :
                CcLocalAgentSetExactTarget(agent, candidate,
                                           market_interior);
            if (targeted) {
                return true;
            }
        }
    }
    return false;
}

static RayCollision RayTerrainCollision(Ray ray)
{
    RayCollision nearest = {0};
    nearest.distance = FLT_MAX;
    if (active_world_stream != NULL) {
        float previous_distance = 0.0f;
        float previous_delta = FLT_MAX;
        bool saw_world = false;
        for (float distance = 0.0f; distance <= 1800.0f; distance += 0.75f) {
            Vector3 point = {
                ray.position.x + ray.direction.x * distance,
                ray.position.y + ray.direction.y * distance,
                ray.position.z + ray.direction.z * distance,
            };
            if (!CcWorldManifestContains(&active_world_stream->manifest,
                                         point.x, point.z)) {
                if (saw_world) break;
                continue;
            }
            saw_world = true;
            float delta = point.y - CcLocalTerrainHeightAt(point.x, point.z);
            if (previous_delta > 0.0f && delta <= 0.0f) {
                float low = previous_distance;
                float high = distance;
                for (int32_t iteration = 0; iteration < 8; ++iteration) {
                    float middle = (low + high) * 0.5f;
                    Vector3 sample = {
                        ray.position.x + ray.direction.x * middle,
                        ray.position.y + ray.direction.y * middle,
                        ray.position.z + ray.direction.z * middle,
                    };
                    float sample_delta = sample.y -
                        CcLocalTerrainHeightAt(sample.x, sample.z);
                    if (sample_delta > 0.0f) low = middle;
                    else high = middle;
                }
                nearest.hit = true;
                nearest.distance = high;
                nearest.point = (Vector3){
                    ray.position.x + ray.direction.x * high,
                    ray.position.y + ray.direction.y * high,
                    ray.position.z + ray.direction.z * high,
                };
                nearest.point.y = CcLocalTerrainHeightAt(
                    nearest.point.x, nearest.point.z);
                nearest.normal = CcLocalTerrainNormalAt(
                    nearest.point.x, nearest.point.z);
                return nearest;
            }
            previous_distance = distance;
            previous_delta = delta;
        }
        return nearest;
    }
    const float step = 1.0f;
    for (float z = 0.0f; z < CC_LOCAL_WORLD_DEPTH; z += step) {
        float far_z = fminf(z + step, CC_LOCAL_WORLD_DEPTH);
        for (float x = 0.0f; x < CC_LOCAL_WORLD_WIDTH; x += step) {
            float far_x = fminf(x + step, CC_LOCAL_WORLD_WIDTH);
            Vector3 p00 = {x, CcLocalTerrainHeightAt(x, z), z};
            Vector3 p10 = {far_x, CcLocalTerrainHeightAt(far_x, z), z};
            Vector3 p01 = {x, CcLocalTerrainHeightAt(x, far_z), far_z};
            Vector3 p11 = {far_x,
                           CcLocalTerrainHeightAt(far_x, far_z), far_z};
            RayCollision hit = GetRayCollisionTriangle(ray, p00, p11, p10);
            if (hit.hit && hit.distance < nearest.distance) nearest = hit;
            hit = GetRayCollisionTriangle(ray, p00, p01, p11);
            if (hit.hit && hit.distance < nearest.distance) nearest = hit;
        }
    }
    return nearest;
}

CcLocalWorldTargetKind CcLocalAgentPickWorldTarget(
    const CcLocalAgent *agent, Vector2 screen_point, RenderTexture2D target,
    Rectangle destination, bool market_interior)
{
    if (active_world_stream != NULL) return CC_LOCAL_WORLD_TARGET_NONE;
    if (agent == NULL || market_interior ||
        agent->scene != CC_LOCAL_SCENE_STREET ||
        active_opening_step != CC_LOCAL_OPENING_COMPLETE ||
        !CombatCanAct(&agent->combat) ||
        !CheckCollisionPointRec(screen_point, destination)) {
        return CC_LOCAL_WORLD_TARGET_NONE;
    }
    Vector2 local = {
        (screen_point.x - destination.x) / destination.width *
            (float)target.texture.width,
        (screen_point.y - destination.y) / destination.height *
            (float)target.texture.height
    };
    Camera3D camera = {0};
    if (!PresentedCameraForInput(CC_LOCAL_SCENE_STREET, agent,
                                 target.texture.width,
                                 target.texture.height, &camera)) {
        camera = CcLocalStreetCameraInternal(agent, 0.0f, false,
                                              target.texture.height);
    }
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    float carriage = RayFootprintDistance(ray, CARRIAGE_FOOTPRINT, 2.65f);
    return carriage < FLT_MAX ? CC_LOCAL_WORLD_TARGET_CARRIAGE :
                                CC_LOCAL_WORLD_TARGET_NONE;
}

static bool PickAgentTargetInternal(
    CcLocalAgent *agent, Vector2 screen_point, RenderTexture2D target,
    Rectangle destination, bool market_interior,
    Vector3 *requested_point_out, bool *picked_point_valid_out)
{
    if (requested_point_out != NULL) *requested_point_out = (Vector3){0};
    if (picked_point_valid_out != NULL) *picked_point_valid_out = false;
    if (agent == NULL || !CombatCanAct(&agent->combat) ||
        !CheckCollisionPointRec(screen_point, destination)) return false;
    agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    bool interior = scene == CC_LOCAL_SCENE_MARKET;
    Vector2 local = {
        (screen_point.x - destination.x) / destination.width * (float)target.texture.width,
        (screen_point.y - destination.y) / destination.height * (float)target.texture.height
    };
    Camera3D camera = {0};
    if (!PresentedCameraForInput(scene, agent, target.texture.width,
                                 target.texture.height, &camera)) {
        camera = interior ? LocalCamera(true, agent->position) :
            scene == CC_LOCAL_SCENE_ROAD ?
                RoadCamera(agent->position, false, 0.0f, false,
                           target.texture.height) :
                CcLocalStreetCameraInternal(agent, 0.0f, false,
                                            target.texture.height);
    }
    Ray ray = GetScreenToWorldRayEx(local, camera, target.texture.width,
                                    target.texture.height);
    float nearest = FLT_MAX;
    Vector3 picked_point = {0};
    const NavPlatform *picked_platform = NULL;
    RayCollision collision = {0};
    if (scene == CC_LOCAL_SCENE_STREET) {
        collision = RayTerrainCollision(ray);
    } else {
        BoundingBox ground = {
            .min = {0.0f, -0.08f, 0.0f},
            .max = {interior ? 9.0f : CC_LOCAL_WORLD_WIDTH, 0.01f,
                    interior ? 7.0f : CC_LOCAL_WORLD_DEPTH}
        };
        collision = GetRayCollisionBox(ray, ground);
    }
    if (collision.hit) {
        nearest = collision.distance;
        picked_point = collision.point;
    }
    if (scene == CC_LOCAL_SCENE_STREET && active_world_stream == NULL) {
        for (int32_t i = 0; i < StreetPhysicsPlatformCount(); ++i) {
            const NavPlatform *platform = StreetPhysicsPlatformAt(i);
            if (platform == NULL) continue;
            float base = PlatformBaseHeight(platform);
            float top = PlatformTopHeight(platform);
            BoundingBox box = {
                .min = {platform->x, base - 0.02f, platform->z},
                .max = {platform->x + platform->width,
                        top + 0.02f,
                        platform->z + platform->depth}
            };
            collision = GetRayCollisionBox(ray, box);
            if (!collision.hit || collision.distance >= nearest) continue;
            nearest = collision.distance;
            picked_point = collision.point;
            picked_platform = platform;
        }
    }
    if (nearest == FLT_MAX) return false;
    if (picked_platform != NULL) {

        float half_extent = fminf(picked_platform->width,
                                  picked_platform->depth) * 0.5f;
        float inset = fminf(agent->radius + 0.035f,
                            fmaxf(0.02f, half_extent - 0.02f));
        picked_point.x = fmaxf(
            picked_platform->x + inset,
            fminf(picked_point.x,
                  picked_platform->x + picked_platform->width - inset));
        picked_point.z = fmaxf(
            picked_platform->z + inset,
            fminf(picked_point.z,
                  picked_platform->z + picked_platform->depth - inset));
        picked_point.y = PlatformTopHeight(picked_platform);
        if (requested_point_out != NULL) *requested_point_out = picked_point;
        if (picked_point_valid_out != NULL) *picked_point_valid_out = true;
        return SetStreetClickTarget(agent, picked_point);
    }
    float occluder = FLT_MAX;
    if (interior) {
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, MARKET_COUNTER_FOOTPRINT, 0.92f));
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, MARKET_SHELF_FOOTPRINT, 1.90f));
        occluder = fminf(occluder, RayFootprintDistance(
            ray, (Rectangle){0.0f, 0.0f, 9.0f, 0.50f}, 2.60f));
        occluder = fminf(occluder, RayFootprintDistance(
            ray, (Rectangle){0.0f, 0.0f, 0.50f, 7.0f}, 2.60f));
    } else if (scene == CC_LOCAL_SCENE_STREET &&
               active_world_stream == NULL) {
        for (int32_t i = 0; i < ActiveWorldBuildingCount(); ++i) {
            WorldBuilding building = ActiveWorldBuildingAt(i);
            occluder = fminf(occluder,
                             RayFootprintDistance(
                                 ray, building.footprint,
                                 building.height));
        }
        for (int32_t i = 0; i < ActiveCompoundStructureCount(); ++i) {
            WorldStructure structure = ActiveCompoundStructureAt(i);
            occluder = fminf(occluder,
                             RayFootprintDistance(
                                 ray, structure.footprint,
                                 structure.height));
        }
        for (int32_t i = 0; i < CC_LOCAL_PLACE_LANDMARK_COUNT; ++i) {
            const CcLocalPlaceLandmark *landmark =
                ActivePlaceLandmarkAt(i);
            if (landmark == NULL) continue;
            occluder = fminf(
                occluder,
                RayFootprintDistance(ray, PlaceLandmarkFootprint(landmark),
                                     landmark->height));
        }
        if (active_opening_step == CC_LOCAL_OPENING_COMPLETE) {
            occluder = fminf(
                occluder,
                RayFootprintDistance(ray, CARRIAGE_FOOTPRINT, 1.92f));
        }
        occluder = fminf(occluder,
                         RayFootprintDistance(ray, DUNGEON_FOOTPRINT, 2.45f));
        occluder = fminf(occluder,
                         RoomArtObstacleRayDistance(ray, camera.target));
    } else {
        for (int32_t i = 0; i < RoadObstacleCount(); ++i) {
            occluder = fminf(
                occluder,
                RayFootprintDistance(ray, RoadObstacleAt(i), 2.20f));
        }
    }

    bool street_fixed_room = scene == CC_LOCAL_SCENE_STREET &&
                             camera.projection == CAMERA_ORTHOGRAPHIC;
    if (!street_fixed_room && occluder < nearest) return false;
    if (requested_point_out != NULL) *requested_point_out = picked_point;
    if (picked_point_valid_out != NULL) *picked_point_valid_out = true;
    return SetNearestClickTarget(agent, picked_point, market_interior);
}

bool CcLocalAgentPickTarget(CcLocalAgent *agent, Vector2 screen_point,
                            RenderTexture2D target, Rectangle destination,
                            bool market_interior)
{
    return PickAgentTargetInternal(
        agent, screen_point, target, destination, market_interior, NULL, NULL);
}

static CcLocalAgent *MovementPreviewAgent(const CcLocalAgent *agent)
{
    CcLocalAgent *preview = &movement_probe_agent_scratch;
    *preview = (CcLocalAgent){0};
    if (agent == NULL) return preview;
    preview->position = agent->position;
    preview->velocity = agent->velocity;
    preview->facing_yaw = agent->facing_yaw;
    preview->radius = agent->radius;
    preview->scene = agent->scene;
    preview->crowned = agent->crowned;
    preview->combat = agent->combat;
    return preview;
}

static void MovementPreviewFromAgent(
    const CcLocalAgent *preview_agent, Vector2 screen_point,
    Vector3 requested_point, bool picked_point_valid,
    CcLocalMovementPreview *preview)
{
    if (preview == NULL || preview_agent == NULL) return;
    preview->screen_point = screen_point;
    preview->origin = preview_agent->position;
    preview->requested_point = requested_point;
    preview->resolved_point = picked_point_valid ? requested_point :
                                                    preview_agent->position;
    preview->scene = preview_agent->scene;
    preview->valid = picked_point_valid;
    preview->accepted = preview_agent->command_point_valid;
    preview->navigation = preview_agent->navigation_active;
    if (!preview->accepted) return;

    preview->origin = preview_agent->command_origin;
    preview->resolved_point = preview_agent->command_point;
    float adjusted_x = preview->resolved_point.x - requested_point.x;
    float adjusted_z = preview->resolved_point.z - requested_point.z;
    preview->adjusted = adjusted_x * adjusted_x + adjusted_z * adjusted_z >
                        0.18f * 0.18f;
    if (preview_agent->navigation_active) {
        for (int32_t index = preview_agent->navigation_point_index;
             index < preview_agent->navigation_point_count &&
             preview->path_count < CC_LOCAL_NAVIGATION_POINT_CAPACITY;
             ++index) {
            preview->path[preview->path_count++] =
                preview_agent->navigation_point[index];
        }
    } else {
        preview->path[preview->path_count++] = preview->resolved_point;
    }
}

bool CcLocalAgentProbeTarget(
    const CcLocalAgent *agent, Vector2 screen_point, RenderTexture2D target,
    Rectangle destination, bool market_interior,
    CcLocalMovementPreview *preview)
{
    if (preview == NULL) return false;
    *preview = (CcLocalMovementPreview){0};
    if (agent == NULL) return false;

    CcLocalAgent *preview_agent = MovementPreviewAgent(agent);
    Vector3 requested_point = {0};
    bool picked_point_valid = false;
    (void)PickAgentTargetInternal(
        preview_agent, screen_point, target, destination, market_interior,
        &requested_point, &picked_point_valid);
    MovementPreviewFromAgent(
        preview_agent, screen_point, requested_point, picked_point_valid,
        preview);
    return preview->accepted;
}

bool CcLocalAgentApplyMovementPreview(
    CcLocalAgent *agent, const CcLocalMovementPreview *preview,
    bool market_interior)
{
    if (agent == NULL || preview == NULL || !preview->valid ||
        !preview->accepted || !CombatCanAct(&agent->combat) ||
        preview->scene != AgentSceneForCall(agent, market_interior)) {
        return false;
    }
    float origin_x = agent->position.x - preview->origin.x;
    float origin_z = agent->position.z - preview->origin.z;
    if (origin_x * origin_x + origin_z * origin_z > 0.65f * 0.65f) {
        return false;
    }

    agent->world_target = CC_LOCAL_WORLD_TARGET_NONE;
    ClearAgentNavigation(agent);
    agent->exact_target_valid = false;
    agent->target_valid = false;
    agent->command_point_valid = false;
    if (preview->navigation) {
        if (preview->path_count <= 0 ||
            preview->path_count > CC_LOCAL_NAVIGATION_POINT_CAPACITY) {
            return false;
        }
        for (int32_t index = 0; index < preview->path_count; ++index) {
            agent->navigation_point[index] = preview->path[index];
        }
        agent->navigation_point_count = preview->path_count;
        agent->navigation_point_index = 0;
        agent->navigation_destination_room =
            STREET_CLICK_NAVIGATION_DESTINATION;
        agent->navigation_active = true;
        if (!SetAgentExactTarget(agent, agent->navigation_point[0], false)) {
            ClearAgentNavigation(agent);
            return false;
        }
    } else if (!SetAgentExactTarget(
                   agent, preview->resolved_point, market_interior)) {
        return false;
    }
    agent->command_origin = agent->position;
    agent->command_point = preview->resolved_point;
    agent->command_point_valid = true;
    return true;
}

static float WrapAngle(float angle)
{
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

static float Approach(float current, float target, float maximum_change)
{
    if (current < target) return fminf(target, current + maximum_change);
    return fmaxf(target, current - maximum_change);
}

static float SmoothStep01(float amount)
{
    amount = fmaxf(0.0f, fminf(amount, 1.0f));
    return amount * amount * (3.0f - 2.0f * amount);
}

static Vector3 PhysicsAdd(Vector3 a, Vector3 b)
{
    return (Vector3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 PhysicsSubtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector3 PhysicsScale(Vector3 value, float scale)
{
    return (Vector3){value.x * scale, value.y * scale, value.z * scale};
}

static float PhysicsDot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float PhysicsLength(Vector3 value)
{
    return sqrtf(PhysicsDot(value, value));
}

static Vector3 PhysicsCross(Vector3 a, Vector3 b)
{
    return (Vector3){a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x};
}

static Vector3 PhysicsNormalizeOr(Vector3 value, Vector3 fallback)
{
    float length = PhysicsLength(value);
    return length > 0.0001f ? PhysicsScale(value, 1.0f / length) : fallback;
}

static Vector3 PhysicsLerp(Vector3 a, Vector3 b, float amount)
{
    return PhysicsAdd(a, PhysicsScale(PhysicsSubtract(b, a), amount));
}

static void ResetHeroCape(CcLocalAgent *agent,
                          const CcHumanoidSkinPose *skin,
                          Vector3 anchor)
{
    static const float SEGMENT_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcLocalCapeState *cape = &agent->cape;
    Vector3 up = FromLimbVector(skin->body_up);
    Vector3 backward = PhysicsScale(FromLimbVector(skin->body_forward), -1.0f);
    Vector3 rest_direction = PhysicsNormalizeOr(
        PhysicsAdd(PhysicsScale(up, -0.975f),
                   PhysicsScale(backward, 0.22f)),
        (Vector3){0.0f, -1.0f, 0.0f});
    cape->point[0] = anchor;
    cape->previous[0] = anchor;
    for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        cape->point[point] = PhysicsAdd(
            cape->point[point - 1],
            PhysicsScale(rest_direction, SEGMENT_LENGTHS[point - 1]));
        cape->previous[point] = cape->point[point];
    }
    cape->anchor = anchor;
    cape->initialized = true;
}

static void UpdateHeroCape(CcLocalAgent *agent, float delta_time)
{
    if (agent == NULL || agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized) return;
    static const float SEGMENT_LENGTHS[] = {0.270f, 0.275f, 0.285f, 0.285f};
    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(&agent->humanoid.pose, &skin);
    if (!skin.valid) return;
    Vector3 up = FromLimbVector(skin.body_up);
    Vector3 forward = FromLimbVector(skin.body_forward);
    Vector3 anchor = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(up, 0.14f));
    CcLocalCapeState *cape = &agent->cape;
    if (!cape->initialized ||
        PhysicsLength(PhysicsSubtract(anchor, cape->anchor)) > 0.72f) {
        ResetHeroCape(agent, &skin, anchor);
        return;
    }

    float step = fmaxf(1.0f / 240.0f, fminf(delta_time, 1.0f / 30.0f));
    float damping = expf(-3.2f * step);
    Vector3 acceleration = {
        -agent->velocity.x * 2.15f,
        agent->swimming ? 0.45f : -7.25f,
        -agent->velocity.z * 2.15f,
    };
    acceleration = PhysicsAdd(
        acceleration,
        PhysicsScale(forward, -0.30f * PhysicsLength(agent->velocity)));
    for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        Vector3 current = cape->point[point];
        Vector3 velocity = PhysicsScale(
            PhysicsSubtract(current, cape->previous[point]), damping);
        cape->previous[point] = current;
        cape->point[point] = PhysicsAdd(
            PhysicsAdd(current, velocity),
            PhysicsScale(acceleration, step * step));
    }
    cape->point[0] = anchor;
    cape->previous[0] = anchor;

    Vector3 chest = FromLimbVector(skin.bones[CC_HUMANOID_SKIN_CHEST].head);
    Vector3 pelvis = FromLimbVector(skin.bones[CC_HUMANOID_SKIN_PELVIS].head);
    for (int32_t iteration = 0; iteration < 9; ++iteration) {
        cape->point[0] = anchor;
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            Vector3 delta = PhysicsSubtract(cape->point[point],
                                            cape->point[point - 1]);
            float distance = PhysicsLength(delta);
            if (distance <= 0.0001f) continue;
            Vector3 correction = PhysicsScale(
                delta, (distance - SEGMENT_LENGTHS[point - 1]) / distance);
            if (point == 1) {
                cape->point[point] = PhysicsSubtract(cape->point[point],
                                                     correction);
            } else {
                cape->point[point - 1] = PhysicsAdd(
                    cape->point[point - 1], PhysicsScale(correction, 0.5f));
                cape->point[point] = PhysicsSubtract(
                    cape->point[point], PhysicsScale(correction, 0.5f));
            }
        }
        for (int32_t point = 2; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float rest_span = (SEGMENT_LENGTHS[point - 2] +
                               SEGMENT_LENGTHS[point - 1]) * 0.985f;
            Vector3 delta = PhysicsSubtract(cape->point[point],
                                            cape->point[point - 2]);
            float distance = PhysicsLength(delta);
            if (distance <= rest_span || distance <= 0.0001f) continue;
            Vector3 correction = PhysicsScale(
                delta, (distance - rest_span) / distance * 0.16f);
            if (point == 2) {
                cape->point[point] = PhysicsSubtract(cape->point[point],
                                                     correction);
            } else {
                cape->point[point - 2] = PhysicsAdd(
                    cape->point[point - 2], PhysicsScale(correction, 0.5f));
                cape->point[point] = PhysicsSubtract(
                    cape->point[point], PhysicsScale(correction, 0.5f));
            }
        }
        for (int32_t point = 1; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            float amount = (float)point /
                           (float)(CC_LOCAL_CAPE_POINT_COUNT - 1);
            Vector3 torso = PhysicsLerp(chest, pelvis, amount * 0.86f);
            float front_distance = PhysicsDot(
                PhysicsSubtract(cape->point[point], torso), forward);
            float rear_limit = -0.115f - amount * 0.035f;
            if (front_distance > rear_limit) {
                cape->point[point] = PhysicsAdd(
                    cape->point[point],
                    PhysicsScale(forward, rear_limit - front_distance));
            }
        }
    }
    cape->point[0] = anchor;
    cape->anchor = anchor;
}

static Vector3 FramePoint(Vector3 base, float x, float y, float z, float yaw)
{
    float cosine = cosf(yaw);
    float sine = sinf(yaw);
    return (Vector3){base.x + x * cosine + z * sine,
                     base.y + y,
                     base.z - x * sine + z * cosine};
}

bool CcLocalAgentBeginFreeClimb(CcLocalAgent *agent,
                                Vector3 surface_point,
                                Vector3 surface_normal)
{
    if (agent == NULL ||
        agent->morphology == CC_MORPHOLOGY_BIPED ||
        !agent->limb_rig.initialized ||
        agent->multileg_ragdoll.active ||
        !CombatCanAct(&agent->combat)) {
        return false;
    }
    float normal_length = PhysicsLength(surface_normal);
    if (normal_length <= 0.50f) return false;
    surface_normal = PhysicsScale(surface_normal, 1.0f / normal_length);

    float maximum_reach = FLT_MAX;
    for (int32_t limb = 0;
         limb < agent->limb_rig.morphology.limb_count; ++limb) {
        if (agent->limb_rig.limbs[limb].health <= 0.0f) continue;
        maximum_reach = fminf(
            maximum_reach,
            CcLimbChainLength(&agent->limb_rig, limb) * 0.92f);
    }
    if (maximum_reach == FLT_MAX) return false;
    maximum_reach = fmaxf(maximum_reach, 0.38f);
    float step_length = fminf(0.44f, maximum_reach * 0.62f);

    CcLimbVec3 start_point = ToLimbVector(agent->position);
    CcLimbVec3 start_normal = {0.0f, 1.0f, 0.0f};
    if (agent->free_climbing && agent->climb_route.count > 0) {
        int32_t index = agent->climb_route_index;
        if (index < 0) index = 0;
        if (index >= agent->climb_route.count) {
            index = agent->climb_route.count - 1;
        }
        start_point = agent->climb_route.nodes[index].point;
        start_normal = agent->climb_route.nodes[index].normal;
    }
    LocalProbeContext context = {.scene = agent->scene};
    CcRobotClimbRoute route;
    if (!CcRobotPlanFreeClimb(
            start_point, start_normal, ToLimbVector(surface_point),
            ToLimbVector(surface_normal), step_length, maximum_reach,
            ProbeLocalClimbSurface, &context, &route)) {
        return false;
    }

    agent->climb_route = route;
    agent->climb_route_index = route.count > 1 ? 1 : 0;
    agent->climb_start = agent->position;
    agent->climb_end = surface_point;
    agent->climb_face = FromLimbVector(route.nodes[0].point);
    agent->climb_normal = FromLimbVector(route.nodes[0].normal);
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climb_duration = fmaxf(0.60f, route.length / 0.82f);
    agent->climbing = true;
    agent->free_climbing = true;
    agent->climbing_down = false;
    agent->vaulting = false;
    agent->climb_training_pending = true;
    agent->grounded = route.nodes[0].surface == CC_ROBOT_SURFACE_FLOOR;
    agent->velocity = (Vector3){0};
    agent->traversal = CC_TRAVERSAL_CLIMB;
    agent->support_state = CC_HUMANOID_SUPPORT_HANDS;
    (void)CcLimbRigRequestPace(&agent->limb_rig, CC_LIMB_PACE_WALK);
    return true;
}

static bool BeginClimb(CcLocalAgent *agent, const NavPlatform *platform)
{
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        (!agent->humanoid.initialized || agent->humanoid.ragdoll.active ||
         agent->humanoid.recovering || agent->humanoid.climbing)) {
        return false;
    }
    float platform_top = PlatformTopHeight(platform);
    float left = fabsf(agent->position.x - platform->x);
    float right_edge = fabsf(agent->position.x -
                             (platform->x + platform->width));
    float near_edge = fabsf(agent->position.z - platform->z);
    float far_edge = fabsf(agent->position.z -
                           (platform->z + platform->depth));
    float nearest = left;
    Vector3 normal = {-1.0f, 0.0f, 0.0f};
    Vector3 face = {platform->x, platform_top, agent->position.z};
    if (right_edge < nearest) {
        nearest = right_edge;
        normal = (Vector3){1.0f, 0.0f, 0.0f};
        face = (Vector3){platform->x + platform->width, platform_top,
                         agent->position.z};
    }
    if (near_edge < nearest) {
        nearest = near_edge;
        normal = (Vector3){0.0f, 0.0f, -1.0f};
        face = (Vector3){agent->position.x, platform_top,
                         platform->z};
    }
    if (far_edge < nearest) {
        normal = (Vector3){0.0f, 0.0f, 1.0f};
        face = (Vector3){agent->position.x, platform_top,
                         platform->z + platform->depth};
    }
    if (normal.x != 0.0f) {
        face.z = fmaxf(platform->z + 0.24f,
                       fminf(face.z,
                             platform->z + platform->depth - 0.24f));
    } else {
        face.x = fmaxf(platform->x + 0.24f,
                       fminf(face.x,
                             platform->x + platform->width - 0.24f));
    }
    Vector3 forward = PhysicsScale(normal, -1.0f);
    float facing = atan2f(forward.x, forward.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 body = {agent->position.x,
                    agent->position.y + agent->limb_rig.morphology.body_height,
                    agent->position.z};
    Vector3 shoulder_left = FramePoint(body, -0.25f, 0.48f, 0.0f, facing);
    Vector3 shoulder_right = FramePoint(body, 0.25f, 0.48f, 0.0f, facing);
    Vector3 hand_left = PhysicsAdd(PhysicsAdd(face, PhysicsScale(normal, 0.025f)),
                                  PhysicsScale(frame_right, -0.16f));
    Vector3 hand_right = PhysicsAdd(PhysicsAdd(face, PhysicsScale(normal, 0.025f)),
                                   PhysicsScale(frame_right, 0.16f));
    hand_left.y += 0.035f;
    hand_right.y += 0.035f;
    float arm_reach = 0.69f +
        (float)(AthleticLevel(agent, CC_ATHLETIC_GRIP) - 1) * 0.025f;
    float ledge_rise = platform_top - agent->position.y;
    bool low_ledge = ledge_rise <= 0.72f;
    if (!low_ledge) {
        if (agent->morphology == CC_MORPHOLOGY_BIPED) {
            if (PhysicsLength(PhysicsSubtract(hand_left, shoulder_left)) >
                    arm_reach ||
                PhysicsLength(PhysicsSubtract(hand_right, shoulder_right)) >
                    arm_reach) {
                return false;
            }
        } else {
            float maximum_leg_reach = 0.0f;
            for (int32_t limb = 0;
                 limb < agent->limb_rig.morphology.limb_count; ++limb) {
                maximum_leg_reach = fmaxf(
                    maximum_leg_reach,
                    CcLimbChainLength(&agent->limb_rig, limb));
            }
            float scramble_reach =
                agent->limb_rig.morphology.body_height +
                maximum_leg_reach * 0.85f;
            if (ledge_rise > scramble_reach) return false;
        }
    }

    agent->climb_start = agent->position;
    agent->climb_end = PhysicsAdd(face,
                                  PhysicsScale(normal, -(agent->radius + 0.18f)));
    agent->climb_end.y = platform_top;
    if (StaticBodyBlocked(agent->scene, agent->climb_end.x,
                          agent->climb_end.z,
                          agent->radius)) return false;
    agent->climb_face = face;
    agent->climb_normal = normal;
    agent->climb_hand_left = hand_left;
    agent->climb_hand_right = hand_right;
    if (agent->morphology != CC_MORPHOLOGY_BIPED) {
        agent->facing_yaw = facing;
        return CcLocalAgentBeginFreeClimb(
            agent, agent->climb_end, (Vector3){0.0f, 1.0f, 0.0f});
    }
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        agent->humanoid.initialized) {
        agent->climb_foot_left = FromLimbVector(
            agent->humanoid.feet[0].current_point);
        agent->climb_foot_right = FromLimbVector(
            agent->humanoid.feet[1].current_point);
    }
    agent->facing_yaw = facing;
    float rise = agent->climb_end.y - agent->climb_start.y;
    agent->vaulting = agent->morphology == CC_MORPHOLOGY_BIPED &&
        rise <= 1.08f &&
        AthleticLevel(agent, CC_ATHLETIC_MOBILITY) >= 2;
    float base_duration = agent->vaulting ? 0.76f + rise * 0.10f :
                                            1.70f + rise * 0.18f;
    float athletic_rate =
        AthleticBonus(agent, CC_ATHLETIC_GRIP, 0.07f) *
        AthleticBonus(agent, CC_ATHLETIC_MOBILITY, 0.035f);
    if (agent->vaulting) {
        agent->climb_duration = fmaxf(0.56f,
                                      base_duration / athletic_rate);
    } else {

        float mantle_rate = fminf(athletic_rate, 1.35f);
        agent->climb_duration = fmaxf(1.35f,
                                      base_duration / mantle_rate);
    }
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climbing = true;
    agent->climbing_down = false;
    agent->climb_training_pending = true;
    agent->grounded = true;
    agent->velocity = (Vector3){0};
    agent->traversal = agent->vaulting ? CC_TRAVERSAL_VAULT :
                                         CC_TRAVERSAL_CLIMB;
    agent->support_state = CC_HUMANOID_SUPPORT_HANDS;
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        CcHumanoidGaitBeginClimb(&agent->humanoid);
        agent->humanoid_needs_reset = false;
    } else {
        (void)CcLimbRigRequestPace(&agent->limb_rig, CC_LIMB_PACE_WALK);
    }
    return true;
}

static bool BeginDownClimb(CcLocalAgent *agent,
                           const NavPlatform *platform,
                           bool move_x, float amount)
{
    if (!agent->allow_downclimb ||
        agent->morphology != CC_MORPHOLOGY_BIPED ||
        !agent->humanoid.initialized || agent->humanoid.ragdoll.active ||
        agent->humanoid.recovering || agent->humanoid.climbing) {
        return false;
    }
    float platform_top = PlatformTopHeight(platform);
    Vector3 normal = {0.0f, 0.0f, 0.0f};
    Vector3 face = {agent->position.x, platform_top, agent->position.z};
    if (move_x) {
        normal.x = amount < 0.0f ? -1.0f : 1.0f;
        face.x = amount < 0.0f ? platform->x :
                                 platform->x + platform->width;
        face.z = fmaxf(platform->z + 0.24f,
                       fminf(face.z,
                             platform->z + platform->depth - 0.24f));
    } else {
        normal.z = amount < 0.0f ? -1.0f : 1.0f;
        face.z = amount < 0.0f ? platform->z :
                                 platform->z + platform->depth;
        face.x = fmaxf(platform->x + 0.24f,
                       fminf(face.x,
                             platform->x + platform->width - 0.24f));
    }
    Vector3 descent_end = PhysicsAdd(
        face, PhysicsScale(normal, agent->radius + 0.42f));
    descent_end.y = BodySurfaceHeightAt(agent->scene, descent_end.x,
                                        descent_end.z);
    if (descent_end.y >= platform_top - 0.24f ||
        StaticBodyBlocked(agent->scene, descent_end.x, descent_end.z,
                          agent->radius)) {
        return false;
    }
    float facing = atan2f(-normal.x, -normal.z);
    Vector3 frame_right = {cosf(facing), 0.0f, -sinf(facing)};
    Vector3 hand_center = PhysicsAdd(face, PhysicsScale(normal, 0.018f));
    hand_center.y = platform_top + 0.035f;

    agent->climb_start = agent->position;
    agent->climb_end = descent_end;
    agent->climb_face = face;
    agent->climb_normal = normal;
    agent->climb_hand_left = PhysicsAdd(
        hand_center, PhysicsScale(frame_right, -0.16f));
    agent->climb_hand_right = PhysicsAdd(
        hand_center, PhysicsScale(frame_right, 0.16f));
    agent->climb_start_yaw = agent->facing_yaw;
    agent->climb_end_yaw = facing;
    agent->climb_duration = 1.42f +
        (platform_top - descent_end.y) * 0.18f;
    agent->climb_duration /= AthleticBonus(agent, CC_ATHLETIC_GRIP, 0.06f);
    agent->climb_progress = 0.0f;
    agent->climb_settle = 0.0f;
    agent->climbing = true;
    agent->climbing_down = true;
    agent->vaulting = false;
    agent->climb_training_pending = true;
    agent->grounded = true;
    agent->velocity = (Vector3){0};
    agent->traversal = CC_TRAVERSAL_DESCEND;
    agent->support_state = CC_HUMANOID_SUPPORT_HANDS;
    CcHumanoidGaitBeginClimb(&agent->humanoid);
    agent->humanoid_needs_reset = false;
    return true;
}

static bool ReleaseClimbToPhysicalFall(CcLocalAgent *agent)
{
    if (agent == NULL || !agent->humanoid.ragdoll.active) return false;
    agent->position = FromLimbVector(CcHumanoidGaitAuthoritativePosition(
        &agent->humanoid, ToLimbVector(agent->position)));
    agent->velocity = FromLimbVector(CcHumanoidGaitAuthoritativeVelocity(
        &agent->humanoid, ToLimbVector(agent->velocity)));
    agent->climbing = false;
    agent->climbing_down = false;
    agent->vaulting = false;
    agent->climb_training_pending = false;
    agent->grounded = false;
    agent->support_state = CC_HUMANOID_SUPPORT_UNCONTROLLED_FALL;
    agent->traversal = CC_TRAVERSAL_RAGDOLL;
    return true;
}

static void UpdateDownClimb(CcLocalAgent *agent, float delta_time,
                            bool market_interior)
{
    Vector3 previous_position = agent->position;
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    float turn = SmoothStep01(amount / 0.20f);
    float yaw_delta = WrapAngle(agent->climb_end_yaw -
                                agent->climb_start_yaw);
    agent->facing_yaw = WrapAngle(agent->climb_start_yaw + yaw_delta * turn);

    Vector3 outside_top = PhysicsAdd(
        agent->climb_face,
        PhysicsScale(agent->climb_normal, agent->radius + 0.09f));
    outside_top.y = agent->climb_face.y - 0.10f;
    Vector3 hang = outside_top;
    hang.y = fmaxf(agent->climb_end.y + 0.34f,
                   agent->climb_face.y - 1.08f);
    float edge_transfer = SmoothStep01((amount - 0.02f) / 0.22f);
    float lower_transfer = SmoothStep01((amount - 0.08f) / 0.30f);
    float land_transfer = SmoothStep01((amount - 0.64f) / 0.28f);
    Vector3 target = PhysicsLerp(agent->climb_start, outside_top,
                                 edge_transfer);
    target = PhysicsLerp(target, hang, lower_transfer);
    target = PhysicsLerp(target, agent->climb_end, land_transfer);

    Vector3 acceleration = PhysicsSubtract(
        PhysicsScale(PhysicsSubtract(target, agent->position), 31.0f),
        PhysicsScale(agent->velocity, 9.5f));
    agent->velocity = PhysicsAdd(agent->velocity,
                                 PhysicsScale(acceleration, delta_time));
    float speed = PhysicsLength(agent->velocity);
    if (speed > 2.7f) {
        agent->velocity = PhysicsScale(agent->velocity, 2.7f / speed);
    }
    agent->position = PhysicsAdd(agent->position,
                                 PhysicsScale(agent->velocity, delta_time));
    agent->grounded = amount < 0.10f;

    float outside_distance = PhysicsDot(
        PhysicsSubtract(agent->position, agent->climb_face),
        agent->climb_normal);
    float clearance_weight = SmoothStep01((amount - 0.18f) / 0.24f);
    float required_outside = (agent->radius + 0.035f) * clearance_weight;
    if (amount < 0.86f && outside_distance < required_outside) {
        agent->position = PhysicsAdd(
            agent->position,
            PhysicsScale(agent->climb_normal,
                         required_outside - outside_distance));
        float into_face = PhysicsDot(agent->velocity, agent->climb_normal);
        if (into_face < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(agent->climb_normal, into_face));
        }
    }
    if (agent->position.y < agent->climb_end.y) {
        agent->position.y = agent->climb_end.y;
        if (agent->velocity.y < 0.0f) agent->velocity.y = 0.0f;
    }

    Vector3 resolved_position = agent->position;
    Vector3 collision_normal = {0};
    if (ResolveLocalAgentCapsuleMove(
            AgentSceneForCall(agent, market_interior), previous_position,
            agent->position, agent->radius, -FLT_MAX, &resolved_position,
            &collision_normal)) {
        agent->position = resolved_position;
        float into_surface = PhysicsDot(agent->velocity, collision_normal);
        if (into_surface < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(collision_normal, into_surface));
        }
    }

    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.64f);
    }

    LocalProbeContext context = {
        .scene = AgentSceneForCall(agent, market_interior)
    };
    Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                           -sinf(agent->facing_yaw)};
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 top_left = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, -0.18f)),
        PhysicsScale(frame_right, -0.13f));
    Vector3 top_right = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, -0.18f)),
        PhysicsScale(frame_right, 0.13f));
    top_left.y = agent->climb_face.y + 0.035f;
    top_right.y = agent->climb_face.y + 0.035f;

    Vector3 wall_left = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, 0.018f)),
        PhysicsScale(frame_right, -0.13f));
    Vector3 wall_right = PhysicsAdd(
        PhysicsAdd(agent->climb_face,
                   PhysicsScale(agent->climb_normal, 0.018f)),
        PhysicsScale(frame_right, 0.13f));
    wall_left.y = fmaxf(agent->climb_end.y + 0.22f,
                        agent->climb_face.y - 0.50f);
    wall_right.y = fmaxf(agent->climb_end.y + 0.30f,
                         agent->climb_face.y - 0.68f);
    Vector3 ground_left = PhysicsAdd(
        agent->climb_end, PhysicsScale(frame_right, -0.13f));
    Vector3 ground_right = PhysicsAdd(
        agent->climb_end, PhysicsScale(frame_right, 0.13f));
    ground_left.y = agent->climb_end.y + 0.035f;
    ground_right.y = agent->climb_end.y + 0.035f;
    float left_wall_transfer = SmoothStep01((amount - 0.16f) / 0.22f);
    float right_wall_transfer = SmoothStep01((amount - 0.28f) / 0.22f);
    float left_ground_transfer = SmoothStep01((amount - 0.52f) / 0.34f);
    float right_ground_transfer = SmoothStep01((amount - 0.64f) / 0.34f);
    Vector3 foot_targets[CC_HUMANOID_LEG_COUNT] = {
        PhysicsLerp(PhysicsLerp(top_left, wall_left, left_wall_transfer),
                    ground_left, left_ground_transfer),
        PhysicsLerp(PhysicsLerp(top_right, wall_right, right_wall_transfer),
                    ground_right, right_ground_transfer)
    };
    Vector3 wall_normal_left = PhysicsNormalizeOr(
        PhysicsLerp(up, agent->climb_normal, left_wall_transfer), up);
    Vector3 wall_normal_right = PhysicsNormalizeOr(
        PhysicsLerp(up, agent->climb_normal, right_wall_transfer), up);
    Vector3 foot_normals[CC_HUMANOID_LEG_COUNT] = {
        PhysicsNormalizeOr(
            PhysicsLerp(wall_normal_left, up, left_ground_transfer), up),
        PhysicsNormalizeOr(
            PhysicsLerp(wall_normal_right, up, right_ground_transfer), up)
    };
    CcLimbVec3 hands[CC_HUMANOID_ARM_COUNT] = {
        ToLimbVector(agent->climb_hand_left),
        ToLimbVector(agent->climb_hand_right)
    };
    CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
    };
    CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
    };
    CcLocalSceneKind climb_scene = AgentSceneForCall(agent, market_interior);
    const float hand_support[CC_HUMANOID_ARM_COUNT] = {
        LocalClimbContactSupport(climb_scene, agent->climb_hand_left, up,
                                 1.0f),
        LocalClimbContactSupport(climb_scene, agent->climb_hand_right, up,
                                 1.0f),
    };
    const float support[CC_HUMANOID_LEG_COUNT] = {
        LocalClimbContactSupport(climb_scene, foot_targets[0],
                                 foot_normals[0], 1.0f),
        LocalClimbContactSupport(climb_scene, foot_targets[1],
                                 foot_normals[1], 1.0f),
    };
    float standing_convergence = SmoothStep01(agent->climb_settle);
    float biomech_progress = amount < 0.78f ? amount :
        0.78f + 0.22f * standing_convergence;
    CcHumanoidGaitAdvanceClimb(
        &agent->humanoid, ToLimbVector(agent->position),
        agent->facing_yaw, hands, hand_support, feet, normals, support,
        biomech_progress, delta_time, ProbeLocalSurface, &context);
    if (ReleaseClimbToPhysicalFall(agent)) return;

    agent->traversal = CC_TRAVERSAL_DESCEND;
    if (agent->climb_settle >= 1.0f &&
        CcHumanoidGaitClimbReady(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
        if (agent->climb_training_pending) {
            CcLocalAgentTrainAthleticism(agent, CC_ATHLETIC_GRIP, 14.0f);
            agent->climb_training_pending = false;
        }
        agent->climbing = false;
        agent->climbing_down = false;
        agent->vaulting = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
        CcHumanoidGaitFinishClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context);
    }
}

static Vector3 ClimbTopOutArc(Vector3 wall_contact, Vector3 top_contact,
                              Vector3 wall_normal, float amount)
{
    amount = fmaxf(0.0f, fminf(1.0f, amount));
    Vector3 lip_clearance = wall_contact;
    lip_clearance.y = top_contact.y + 0.13f;
    if (amount < 0.46f) {
        float rise = SmoothStep01(amount / 0.46f);
        Vector3 result = PhysicsLerp(wall_contact, lip_clearance, rise);
        result = PhysicsAdd(
            result, PhysicsScale(wall_normal, sinf(PI * rise) * 0.075f));
        return result;
    }
    float cross = SmoothStep01((amount - 0.46f) / 0.54f);
    Vector3 result = PhysicsLerp(lip_clearance, top_contact, cross);
    result.y += sinf(PI * cross) * 0.065f;
    return result;
}

static Vector3 ClimbWallStepArc(Vector3 takeoff, Vector3 wall_contact,
                                Vector3 wall_normal, Vector3 frame_right,
                                float side, float amount)
{
    amount = SmoothStep01(amount);
    Vector3 control = PhysicsLerp(takeoff, wall_contact, 0.50f);

    control = PhysicsAdd(control, PhysicsScale(wall_normal, 0.11f));
    control = PhysicsAdd(control, PhysicsScale(frame_right, side * 0.035f));
    control.y += 0.17f;
    float inverse = 1.0f - amount;
    Vector3 result = PhysicsScale(takeoff, inverse * inverse);
    result = PhysicsAdd(
        result, PhysicsScale(control, 2.0f * inverse * amount));
    return PhysicsAdd(result, PhysicsScale(wall_contact, amount * amount));
}

static float ClimbSwingSupport(float amount)
{
    if (amount <= 0.0f || amount >= 1.0f) return 1.0f;
    return 1.0f - sinf(amount * PI);
}

static int32_t UpdateMultiLegClimbContacts(
    CcLocalAgent *agent, float amount, float delta_time,
    const LocalProbeContext *context)
{
    CcLimbRig *rig = &agent->limb_rig;
    Vector3 body = ShellPodBaseCenter(agent);
    CcLimbVec3 root = ToLimbVector(body);
    Vector3 up = {0.0f, 1.0f, 0.0f};
    Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                           -sinf(agent->facing_yaw)};
    float maximum_longitudinal = 0.001f;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        maximum_longitudinal = fmaxf(
            maximum_longitudinal,
            fabsf(rig->morphology.limbs[limb].rest_contact_local.z));
    }

    int32_t supported_count = 0;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        CcLimbRuntime *runtime = &rig->limbs[limb];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        if (runtime->health <= 0.0f) continue;

        int32_t pair = limb / 2;
        float longitudinal = spec->rest_contact_local.z /
                             maximum_longitudinal;
        float lateral = fmaxf(-0.25f,
                              fminf(spec->rest_contact_local.x * 0.36f,
                                    0.25f));
        float wall_start = 0.10f + (float)pair * 0.035f +
                           ((limb & 1) != 0 ? 0.022f : 0.0f);
        float wall_step = (amount - wall_start) / 0.16f;
        float wall_arrival = SmoothStep01(wall_step);

        Vector3 wall_contact = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, lateral));
        float climbing_rise = SmoothStep01((amount - 0.22f) / 0.42f);
        wall_contact.y = fmaxf(
            agent->climb_start.y + 0.18f,
            agent->climb_face.y - 0.82f + longitudinal * 0.14f +
                climbing_rise * 0.20f);

        float depth = 0.19f + (1.0f - longitudinal) * 0.035f;
        Vector3 top_contact = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -depth)),
            PhysicsScale(frame_right, lateral));
        top_contact.y = agent->climb_face.y + 0.035f;

        float top_start = 0.48f + (float)pair * 0.045f +
                          ((limb & 1) != 0 ? 0.025f : 0.0f);
        float top_step = (amount - top_start) / 0.20f;
        Vector3 target;
        Vector3 normal;
        float requested_support;
        if (wall_step <= 0.0f) {
            target = FromLimbVector(runtime->planted_contact);
            normal = FromLimbVector(runtime->contact_normal);
            requested_support = 1.0f;
        } else if (top_step > 0.0f) {
            target = ClimbTopOutArc(wall_contact, top_contact,
                                    agent->climb_normal, top_step);
            float top_turn = SmoothStep01((top_step - 0.42f) / 0.58f);
            normal = PhysicsNormalizeOr(
                PhysicsLerp(agent->climb_normal, up, top_turn), up);
            requested_support = fminf(
                ClimbSwingSupport(wall_step),
                ClimbSwingSupport(top_step));
        } else {
            Vector3 current = FromLimbVector(runtime->planted_contact);
            target = PhysicsLerp(current, wall_contact, wall_arrival);
            normal = PhysicsNormalizeOr(
                PhysicsLerp(FromLimbVector(runtime->contact_normal),
                            agent->climb_normal, wall_arrival),
                up);
            requested_support = ClimbSwingSupport(wall_step);
        }

        float support = LocalClimbContactSupport(
            context->scene, target, normal, requested_support);
        Vector3 socket = FramePoint(
            body, spec->socket_local.x, spec->socket_local.y,
            spec->socket_local.z, agent->facing_yaw);
        if (PhysicsLength(PhysicsSubtract(target, socket)) >
            CcLimbChainLength(rig, limb) * 1.03f) {
            support = 0.0f;
        }
        if (support >= 0.58f) {
            CcLimbRigPinContact(rig, limb, root, agent->facing_yaw,
                                ToLimbVector(target), ToLimbVector(normal));
            supported_count += 1;
        } else {
            runtime->state = CC_LIMB_SEARCHING;
        }
    }

    CcLimbRigEvaluateSupport(rig, root, agent->facing_yaw,
                             supported_count > 0, delta_time);
    (void)CcLimbRigRequestPace(rig, CC_LIMB_PACE_WALK);
    return supported_count;
}

static Vector3 FreeClimbRootTarget(const CcLocalAgent *agent,
                                   const CcRobotClimbNode *node)
{
    Vector3 point = FromLimbVector(node->point);
    Vector3 normal = FromLimbVector(node->normal);
    float upward = fmaxf(0.0f, normal.y);
    float clearance = agent->radius + 0.14f;
    clearance += (agent->limb_rig.morphology.body_height - clearance) *
                 upward;
    Vector3 center = PhysicsAdd(point, PhysicsScale(normal, clearance));
    Vector3 root = center;
    root.y -= agent->limb_rig.morphology.body_height;
    if (node->surface == CC_ROBOT_SURFACE_WALL) {
        float floor = BodySurfaceHeightAt(agent->scene, root.x, root.z);
        root.y = fmaxf(root.y, floor);
    }
    return root;
}

static int32_t UpdateFreeClimbContacts(
    CcLocalAgent *agent, const CcRobotClimbNode *node,
    float delta_time, const LocalProbeContext *context)
{
    CcLimbRig *rig = &agent->limb_rig;
    CcLimbVec3 root = ToLimbVector(ShellPodBaseCenter(agent));
    Vector3 normal = FromLimbVector(node->normal);
    int32_t current = agent->climb_route_index;
    int32_t previous = current > 0 ? current - 1 : current;
    int32_t next = current + 1 < agent->climb_route.count ?
                   current + 1 : current;
    Vector3 tangent = PhysicsSubtract(
        FromLimbVector(agent->climb_route.nodes[next].point),
        FromLimbVector(agent->climb_route.nodes[previous].point));
    tangent = PhysicsSubtract(tangent,
                              PhysicsScale(normal,
                                           PhysicsDot(tangent, normal)));
    Vector3 fallback = fabsf(normal.y) < 0.82f ?
        (Vector3){0.0f, 1.0f, 0.0f} :
        (Vector3){0.0f, 0.0f, 1.0f};
    tangent = PhysicsNormalizeOr(tangent, fallback);
    Vector3 right = PhysicsNormalizeOr(
        Vector3CrossProduct(tangent, normal),
        (Vector3){cosf(agent->facing_yaw), 0.0f,
                  -sinf(agent->facing_yaw)});
    float maximum_longitudinal = 0.001f;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        maximum_longitudinal = fmaxf(
            maximum_longitudinal,
            fabsf(rig->morphology.limbs[limb].rest_contact_local.z));
    }
    float route_phase = agent->climb_progress *
                        (float)fmaxf((float)agent->climb_route.count, 2.0f);
    int32_t swing_limb = ((int32_t)floorf(route_phase * 1.7f)) %
                         rig->morphology.limb_count;
    float swing_phase = fmodf(route_phase * 1.7f, 1.0f);

    int32_t supported_count = 0;
    for (int32_t limb = 0; limb < rig->morphology.limb_count; ++limb) {
        CcLimbRuntime *runtime = &rig->limbs[limb];
        const CcLimbSpec *spec = &rig->morphology.limbs[limb];
        if (runtime->health <= 0.0f) continue;
        float lateral = fmaxf(-0.30f, fminf(
            spec->rest_contact_local.x * 0.48f, 0.30f));
        float longitudinal = spec->rest_contact_local.z /
                             maximum_longitudinal * 0.16f;
        Vector3 sample = FromLimbVector(node->point);
        sample = PhysicsAdd(sample, PhysicsScale(right, lateral));
        sample = PhysicsAdd(sample, PhysicsScale(tangent, longitudinal));
        sample = PhysicsAdd(sample, PhysicsScale(normal, 0.025f));
        CcLimbVec3 contact_point = {0};
        CcLimbVec3 contact_normal = {0};
        bool contact_found = ProbeLocalClimbSurface(
            (void *)context, ToLimbVector(sample), 0.24f,
            &contact_point, &contact_normal);
        bool swinging = limb == swing_limb &&
                        swing_phase > 0.20f && swing_phase < 0.72f;
        Vector3 socket = FramePoint(
            ShellPodBaseCenter(agent), spec->socket_local.x,
            spec->socket_local.y, spec->socket_local.z,
            agent->facing_yaw);
        bool within_reach = contact_found &&
            PhysicsLength(PhysicsSubtract(
                FromLimbVector(contact_point), socket)) <=
            CcLimbChainLength(rig, limb) * 1.08f;
        float support = within_reach && !swinging ?
            LocalClimbContactSupport(
                context->scene, FromLimbVector(contact_point),
                FromLimbVector(contact_normal), 1.0f) : 0.0f;
        if (support >= 0.58f) {
            CcLimbRigPinContact(rig, limb, root, agent->facing_yaw,
                                contact_point, contact_normal);
            supported_count += 1;
        } else {
            runtime->state = CC_LIMB_SEARCHING;
        }
    }
    CcLimbRigEvaluateSupport(rig, root, agent->facing_yaw,
                             supported_count > 0, delta_time);
    (void)CcLimbRigRequestPace(rig, CC_LIMB_PACE_WALK);
    return supported_count;
}

static void UpdateFreeClimb(CcLocalAgent *agent, float delta_time,
                            bool market_interior)
{
    if (agent->climb_route.count <= 0 || agent->climb_route_index < 0 ||
        agent->climb_route_index >= agent->climb_route.count) {
        agent->climbing = false;
        agent->free_climbing = false;
        agent->grounded = false;
        agent->traversal = CC_TRAVERSAL_DROP;
        return;
    }
    agent->scene = AgentSceneForCall(agent, market_interior);
    const CcRobotClimbNode *node =
        &agent->climb_route.nodes[agent->climb_route_index];
    agent->climb_face = FromLimbVector(node->point);
    agent->climb_normal = FromLimbVector(node->normal);
    if (node->surface == CC_ROBOT_SURFACE_WALL) {
        agent->facing_yaw = atan2f(-node->normal.x, -node->normal.z);
    }
    Vector3 desired = FreeClimbRootTarget(agent, node);
    Vector3 previous = agent->position;
    Vector3 offset = PhysicsSubtract(desired, agent->position);
    float distance = PhysicsLength(offset);
    float speed = 0.82f * AthleticBonus(
        agent, CC_ATHLETIC_GRIP, 0.055f);
    float step = fminf(distance, speed * delta_time);
    if (distance > 0.0001f) {
        agent->position = PhysicsAdd(
            agent->position, PhysicsScale(offset, step / distance));
    }
    agent->velocity = delta_time > 0.00001f ?
        PhysicsScale(PhysicsSubtract(agent->position, previous),
                     1.0f / delta_time) : (Vector3){0};
    agent->grounded = node->surface == CC_ROBOT_SURFACE_FLOOR &&
                      fabsf(agent->position.y - desired.y) < 0.08f;
    int32_t edge_count = agent->climb_route.count > 1 ?
                         agent->climb_route.count - 1 : 1;
    float edge_fraction = distance > 0.0001f ?
        1.0f - fminf(distance / fmaxf(step + distance, 0.001f), 1.0f) : 1.0f;
    agent->climb_progress = fminf(
        1.0f, ((float)agent->climb_route_index - 1.0f + edge_fraction) /
                  (float)edge_count);

    LocalProbeContext context = {.scene = agent->scene};
    int32_t contacts = UpdateFreeClimbContacts(
        agent, node, delta_time, &context);
    if (contacts < agent->limb_rig.morphology.minimum_supports) {
        agent->climb_settle += delta_time;
        if (agent->climb_settle > 0.34f) {
            agent->climbing = false;
            agent->free_climbing = false;
            agent->climb_training_pending = false;
            agent->grounded = false;
            agent->velocity.y = fminf(agent->velocity.y, -0.20f);
            agent->traversal = CC_TRAVERSAL_DROP;
            return;
        }
    } else {
        agent->climb_settle = 0.0f;
    }

    if (distance < 0.075f &&
        agent->climb_route_index + 1 < agent->climb_route.count) {
        agent->climb_route_index += 1;
        agent->climb_settle = 0.0f;
    } else if (distance < 0.035f &&
               agent->climb_route_index + 1 == agent->climb_route.count) {
        if (node->surface == CC_ROBOT_SURFACE_FLOOR) {
            agent->position = desired;
            agent->velocity = (Vector3){0};
            agent->grounded = true;
            agent->climbing = false;
            agent->free_climbing = false;
            agent->traversal = CC_TRAVERSAL_IDLE;
            if (agent->climb_training_pending) {
                CcLocalAgentTrainAthleticism(
                    agent, CC_ATHLETIC_GRIP, 22.0f);
                agent->climb_training_pending = false;
            }
        } else {
            agent->velocity = (Vector3){0};
            agent->grounded = false;
        }
    }
    if (agent->climbing) agent->traversal = CC_TRAVERSAL_CLIMB;
}

static void UpdateHighMantle(CcLocalAgent *agent, float delta_time,
                             bool market_interior)
{
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    CcMotionMantleSample motion;
    if (!CcMotionClipSampleMantle(amount, &motion)) return;

    Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                           -sinf(agent->facing_yaw)};
    Vector3 previous = agent->position;
    Vector3 target = PhysicsLerp(agent->climb_start, agent->climb_end,
                                 motion.root_depth_progress);
    target.y = agent->climb_start.y +
        (agent->climb_end.y - agent->climb_start.y) * motion.root_progress;
    target = PhysicsAdd(target,
                        PhysicsScale(agent->climb_normal,
                                     motion.root_outward));
    target = PhysicsAdd(target,
                        PhysicsScale(frame_right, motion.root_lateral));
    target.y += motion.root_vertical;


    if (amount < 0.90f) {
        float outside_distance = PhysicsDot(
            PhysicsSubtract(target, agent->climb_face),
            agent->climb_normal);
        float required_outside = agent->radius + 0.030f;
        if (outside_distance < required_outside) {
            float collision_weight = 1.0f - SmoothStep01(
                (amount - 0.78f) / 0.12f);
            target = PhysicsAdd(
                target, PhysicsScale(agent->climb_normal,
                                     (required_outside - outside_distance) *
                                         collision_weight));
        }
    }
    if (amount >= 1.0f) target = agent->climb_end;
    Vector3 resolved_target = target;
    Vector3 collision_normal = {0};
    (void)ResolveLocalAgentCapsuleMove(
        AgentSceneForCall(agent, market_interior), previous, target,
        agent->radius, amount >= 0.78f ? agent->climb_face.y : -FLT_MAX,
        &resolved_target, &collision_normal);
    target = resolved_target;
    agent->position = target;
    agent->velocity = delta_time > 0.00001f ?
        PhysicsScale(PhysicsSubtract(target, previous), 1.0f / delta_time) :
        (Vector3){0};
    agent->grounded = amount < 0.10f;
    agent->traversal = CC_TRAVERSAL_CLIMB;

    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.42f);
    }

    LocalProbeContext context = {
        .scene = AgentSceneForCall(agent, market_interior)
    };
    CcLimbVec3 takeoff[CC_HUMANOID_LEG_COUNT] = {
        ToLimbVector(agent->climb_foot_left),
        ToLimbVector(agent->climb_foot_right)
    };
    CcHumanoidGaitAdvanceMantle(
        &agent->humanoid, ToLimbVector(agent->position),
        agent->facing_yaw, ToLimbVector(agent->climb_face),
        ToLimbVector(agent->climb_normal), takeoff, amount, delta_time,
        ProbeLocalSurface, &context);

    if (agent->climb_settle >= 1.0f &&
        CcHumanoidGaitClimbReady(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
        if (agent->climb_training_pending) {
            CcLocalAgentTrainAthleticism(
                agent, CC_ATHLETIC_GRIP, 22.0f);
            agent->climb_training_pending = false;
        }
        agent->climbing = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
        CcHumanoidGaitFinishClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, ProbeLocalSurface, &context);
    }
}

static void UpdateClimb(CcLocalAgent *agent, float delta_time,
                        bool market_interior)
{
    if (agent->free_climbing &&
        agent->morphology != CC_MORPHOLOGY_BIPED) {
        UpdateFreeClimb(agent, delta_time, market_interior);
        return;
    }
    if (agent->climbing_down) {
        UpdateDownClimb(agent, delta_time, market_interior);
        return;
    }
    if (agent->morphology == CC_MORPHOLOGY_BIPED && !agent->vaulting) {
        UpdateHighMantle(agent, delta_time, market_interior);
        return;
    }
    Vector3 previous_position = agent->position;
    agent->climb_progress = fminf(1.0f, agent->climb_progress +
                                  delta_time / agent->climb_duration);
    float amount = agent->climb_progress;
    float acquire = SmoothStep01((amount - 0.10f) / 0.12f);
    float left_push = SmoothStep01((amount - 0.26f) / 0.18f);
    float right_push = SmoothStep01((amount - 0.46f) / 0.18f);
    float stand = SmoothStep01((amount - 0.86f) / 0.14f);
    Vector3 outside = PhysicsAdd(agent->climb_face,
                                 PhysicsScale(agent->climb_normal,
                                              agent->radius + 0.08f));
    Vector3 hang = outside;
    hang.y = fmaxf(agent->climb_start.y + 0.18f,
                   agent->climb_face.y - 1.04f);
    Vector3 middle = PhysicsLerp(hang, agent->climb_end, 0.16f);
    middle.y = agent->climb_face.y - 0.80f;
    Vector3 high = PhysicsLerp(hang, agent->climb_end, 0.38f);
    high.y = agent->climb_face.y - 0.54f;
    Vector3 target = agent->climb_start;
    if (amount < 0.10f) {
        agent->position = agent->climb_start;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
    } else {
        target = PhysicsLerp(agent->climb_start, hang, acquire);
        target = PhysicsLerp(target, middle, left_push);
        target = PhysicsLerp(target, high, right_push);
        target = PhysicsLerp(target, agent->climb_end, stand);
        Vector3 acceleration = PhysicsSubtract(
            PhysicsScale(PhysicsSubtract(target, agent->position), 34.0f),
            PhysicsScale(agent->velocity, 9.0f));
        agent->velocity = PhysicsAdd(agent->velocity,
                                     PhysicsScale(acceleration, delta_time));
        float speed = PhysicsLength(agent->velocity);
        if (speed > 3.0f) agent->velocity = PhysicsScale(agent->velocity, 3.0f / speed);
        agent->position = PhysicsAdd(agent->position,
                                     PhysicsScale(agent->velocity, delta_time));
        agent->grounded = false;
    }

    if (amount >= 0.10f && amount < 0.82f) {
        Vector3 body = {agent->position.x,
                        agent->position.y + agent->limb_rig.morphology.body_height,
                        agent->position.z};
        Vector3 shoulders[2] = {
            FramePoint(body, -0.25f, 0.48f, 0.0f, agent->facing_yaw),
            FramePoint(body, 0.25f, 0.48f, 0.0f, agent->facing_yaw)
        };
        Vector3 hands[2] = {agent->climb_hand_left, agent->climb_hand_right};
        float desired_reach = 0.67f - right_push * 0.15f;
        Vector3 correction = {0};
        for (int32_t hand = 0; hand < 2; ++hand) {
            Vector3 toward = PhysicsSubtract(hands[hand], shoulders[hand]);
            float distance = PhysicsLength(toward);
            if (distance <= desired_reach || distance <= 0.0001f) continue;
            correction = PhysicsAdd(correction,
                                    PhysicsScale(toward,
                                                 (distance - desired_reach) /
                                                 distance * 0.32f));
        }
        agent->position = PhysicsAdd(agent->position, PhysicsScale(correction, 0.5f));
    }

    float outside_distance = PhysicsDot(
        PhysicsSubtract(agent->position, agent->climb_face), agent->climb_normal);
    float acquisition_clearance =
        (1.0f - SmoothStep01(amount / 0.28f)) * 0.135f;
    float required_outside = agent->radius + 0.035f + acquisition_clearance;
    if ((amount < 0.72f || agent->position.y < agent->climb_face.y - 0.12f) &&
        outside_distance < required_outside) {
        agent->position = PhysicsAdd(
            agent->position,
            PhysicsScale(agent->climb_normal, required_outside - outside_distance));
        float into_face = PhysicsDot(agent->velocity, agent->climb_normal);
        if (into_face < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity, PhysicsScale(agent->climb_normal, into_face));
        }
    }
    if (agent->position.y > agent->climb_end.y) {
        agent->position.y = agent->climb_end.y;
        if (agent->velocity.y > 0.0f) agent->velocity.y = 0.0f;
    }
    Vector3 resolved_position = agent->position;
    Vector3 collision_normal = {0};
    if (ResolveLocalAgentCapsuleMove(
            AgentSceneForCall(agent, market_interior), previous_position,
            agent->position, agent->radius,
            amount >= 0.82f ? agent->climb_face.y : -FLT_MAX,
            &resolved_position,
            &collision_normal)) {
        agent->position = resolved_position;
        float into_surface = PhysicsDot(agent->velocity, collision_normal);
        if (into_surface < 0.0f) {
            agent->velocity = PhysicsSubtract(
                agent->velocity,
                PhysicsScale(collision_normal, into_surface));
        }
    }
    agent->traversal = agent->vaulting ? CC_TRAVERSAL_VAULT :
                                         CC_TRAVERSAL_CLIMB;
    float end_distance = PhysicsLength(
        PhysicsSubtract(agent->position, agent->climb_end));
    if (amount >= 1.0f && end_distance < 0.020f) {
        agent->position = agent->climb_end;
        agent->velocity = (Vector3){0};
        agent->grounded = true;
        agent->climb_settle = fminf(1.0f, agent->climb_settle +
                                    delta_time / 0.70f);
    }
    LocalProbeContext context = {
        .scene = AgentSceneForCall(agent, market_interior)
    };
    if (agent->morphology == CC_MORPHOLOGY_BIPED) {
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 frame_right = {cosf(agent->facing_yaw), 0.0f,
                               -sinf(agent->facing_yaw)};
        Vector3 wall_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, -0.20f));
        Vector3 wall_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, 0.018f)),
            PhysicsScale(frame_right, 0.18f));
        wall_left.y = fmaxf(agent->climb_start.y + 0.28f,
                            agent->climb_face.y - 0.70f);
        wall_right.y = fmaxf(agent->climb_start.y + 0.36f,
                             agent->climb_face.y - 0.52f);
        Vector3 top_left = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -0.30f)),
            PhysicsScale(frame_right, -0.16f));
        Vector3 top_right = PhysicsAdd(
            PhysicsAdd(agent->climb_face,
                       PhysicsScale(agent->climb_normal, -0.26f)),
            PhysicsScale(frame_right, 0.14f));
        top_left.y = agent->climb_face.y + 0.035f;
        top_right.y = agent->climb_face.y + 0.035f;

        float left_wall_step = (amount - 0.08f) / 0.18f;
        float right_wall_step = (amount - 0.28f) / 0.18f;
        float left_top_step = (amount - 0.50f) / 0.18f;
        float right_top_step = (amount - 0.70f) / 0.20f;
        Vector3 left_wall_target = ClimbWallStepArc(
            agent->climb_foot_left, wall_left, agent->climb_normal,
            frame_right, -1.0f, left_wall_step);
        Vector3 right_wall_target = ClimbWallStepArc(
            agent->climb_foot_right, wall_right, agent->climb_normal,
            frame_right, 1.0f, right_wall_step);
        Vector3 left_target = ClimbTopOutArc(
            wall_left, top_left, agent->climb_normal, left_top_step);
        Vector3 right_target = ClimbTopOutArc(
            wall_right, top_right, agent->climb_normal, right_top_step);
        float left_to_top = SmoothStep01(
            (left_top_step - 0.42f) / 0.58f);
        float right_to_top = SmoothStep01(
            (right_top_step - 0.42f) / 0.58f);

        float left_to_wall = SmoothStep01(
            (left_wall_step - 0.52f) / 0.48f);
        float right_to_wall = SmoothStep01(
            (right_wall_step - 0.52f) / 0.48f);
        Vector3 left_wall_normal = PhysicsNormalizeOr(
            PhysicsLerp(up, agent->climb_normal, left_to_wall), up);
        Vector3 right_wall_normal = PhysicsNormalizeOr(
            PhysicsLerp(up, agent->climb_normal, right_to_wall), up);
        Vector3 foot_targets[CC_HUMANOID_LEG_COUNT] = {
            left_top_step > 0.0f ? left_target : left_wall_target,
            right_top_step > 0.0f ? right_target : right_wall_target
        };
        Vector3 foot_normals[CC_HUMANOID_LEG_COUNT] = {
            PhysicsNormalizeOr(
                PhysicsLerp(left_wall_normal, up, left_to_top), up),
            PhysicsNormalizeOr(
                PhysicsLerp(right_wall_normal, up, right_to_top), up)
        };
        Vector3 top_hand_left = PhysicsAdd(
            agent->climb_hand_left,
            PhysicsScale(agent->climb_normal, -0.20f));
        Vector3 top_hand_right = PhysicsAdd(
            agent->climb_hand_right,
            PhysicsScale(agent->climb_normal, -0.20f));
        top_hand_left.y = agent->climb_face.y + 0.040f;
        top_hand_right.y = agent->climb_face.y + 0.040f;
        float left_hand_press = SmoothStep01((amount - 0.48f) / 0.20f);
        float right_hand_press = SmoothStep01((amount - 0.60f) / 0.18f);
        Vector3 hand_points[CC_HUMANOID_ARM_COUNT] = {
            PhysicsLerp(agent->climb_hand_left,
                        top_hand_left, left_hand_press),
            PhysicsLerp(agent->climb_hand_right,
                        top_hand_right, right_hand_press),
        };
        CcLimbVec3 hand_targets[CC_HUMANOID_ARM_COUNT] = {
            ToLimbVector(hand_points[0]), ToLimbVector(hand_points[1])
        };
        const float hand_support[CC_HUMANOID_ARM_COUNT] = {
            LocalClimbContactSupport(
                context.scene, hand_points[0],
                PhysicsNormalizeOr(
                    PhysicsLerp(agent->climb_normal, up, left_hand_press),
                    up), 1.0f),
            LocalClimbContactSupport(
                context.scene, hand_points[1],
                PhysicsNormalizeOr(
                    PhysicsLerp(agent->climb_normal, up, right_hand_press),
                    up), 1.0f),
        };
        CcLimbVec3 feet[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_targets[0]), ToLimbVector(foot_targets[1])
        };
        CcLimbVec3 normals[CC_HUMANOID_LEG_COUNT] = {
            ToLimbVector(foot_normals[0]), ToLimbVector(foot_normals[1])
        };
        float support[CC_HUMANOID_LEG_COUNT] = {
            LocalClimbContactSupport(
                context.scene, foot_targets[0], foot_normals[0],
                fminf(ClimbSwingSupport(left_wall_step),
                      ClimbSwingSupport(left_top_step))),
            LocalClimbContactSupport(
                context.scene, foot_targets[1], foot_normals[1],
                fminf(ClimbSwingSupport(right_wall_step),
                      ClimbSwingSupport(right_top_step)))
        };
        float standing_convergence = SmoothStep01(agent->climb_settle);
        float biomech_progress = amount < 0.78f ? amount :
            0.78f + 0.22f * standing_convergence;
        CcHumanoidGaitAdvanceClimb(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw, hand_targets, hand_support,
            feet, normals, support,
            biomech_progress, delta_time, ProbeLocalSurface, &context);
        if (ReleaseClimbToPhysicalFall(agent)) return;
        if (agent->climb_settle >= 1.0f &&
            CcHumanoidGaitClimbReady(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw, ProbeLocalSurface, &context, 0.025f)) {
            if (agent->climb_training_pending) {
                CcLocalAgentTrainAthleticism(
                    agent, CC_ATHLETIC_GRIP,
                    agent->vaulting ? 12.0f : 22.0f);
                agent->climb_training_pending = false;
            }
            agent->climbing = false;
            agent->grounded = true;
            agent->traversal = CC_TRAVERSAL_IDLE;
        }
        if (!agent->climbing) {
            agent->vaulting = false;
            CcHumanoidGaitFinishClimb(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw, ProbeLocalSurface, &context);
        }
        return;
    }
    if (agent->climb_settle >= 1.0f) {
        if (agent->climb_training_pending) {
            CcLocalAgentTrainAthleticism(
                agent, CC_ATHLETIC_GRIP,
                agent->vaulting ? 12.0f : 22.0f);
            agent->climb_training_pending = false;
        }
        agent->climbing = false;
        agent->vaulting = false;
        agent->grounded = true;
        agent->traversal = CC_TRAVERSAL_IDLE;
    }
    CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                    agent->facing_yaw, ToLimbVector(agent->velocity),
                    agent->grounded, agent->climbing ? 0.0f : delta_time,
                    ProbeLocalSurface, &context);
    if (agent->climbing) {
        (void)UpdateMultiLegClimbContacts(
            agent, amount, delta_time, &context);
        if (agent->limb_rig.support_state == CC_LIMB_SUPPORT_UNSUPPORTED) {
            agent->climbing = false;
            agent->climbing_down = false;
            agent->vaulting = false;
            agent->climb_training_pending = false;
            agent->grounded = false;
            agent->velocity.y = fminf(agent->velocity.y, -0.20f);
            agent->traversal = CC_TRAVERSAL_DROP;
        }
    }
}

static bool TryHorizontalAxis(CcLocalAgent *agent, bool market_interior,
                              bool move_x, float amount)
{
    if (agent->climbing) return false;
    if (fabsf(amount) <= 0.000001f) return true;
    float candidate_x = agent->position.x + (move_x ? amount : 0.0f);
    float candidate_z = agent->position.z + (move_x ? 0.0f : amount);
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    if (scene == CC_LOCAL_SCENE_STREET && active_world_stream == NULL) {
        const NavPlatform *support = SupportingPlatformAt(
            agent->position.x, agent->position.z, agent->position.y);
        float candidate_surface = BodySurfaceHeightAt(
            scene, candidate_x, candidate_z);
        float current_surface = BodySurfaceHeightAt(
            scene, agent->position.x, agent->position.z);
        float horizontal = fabsf(amount);
        Vector3 candidate_normal = SurfaceNormalAt(
            scene, candidate_x, candidate_z);
        if (agent->grounded && horizontal > 0.0001f &&
            candidate_surface > current_surface + 0.002f &&
            candidate_normal.y < 0.60f) {
            return false;
        }
        bool moving_toward_target = agent->exact_target_valid &&
            amount * ((move_x ? agent->target_point.x : agent->target_point.z) -
                      (move_x ? agent->position.x : agent->position.z)) > 0.0f;
        bool target_requests_descent = agent->exact_target_valid &&
            agent->target_point.y < agent->position.y - 0.24f;
        if (support != NULL && agent->grounded && moving_toward_target &&
            target_requests_descent &&
            candidate_surface < agent->position.y - 0.24f &&
            BeginDownClimb(agent, support, move_x, amount)) {
            return false;
        }
        const NavPlatform *platform = ClimbPlatformAt(scene,
                                                      candidate_x, candidate_z,
                                                      LocalClimbTriggerRadius(
                                                          agent),
                                                      agent->position.y);
        if (platform != NULL) {
            bool target_requests_climb = agent->exact_target_valid &&
                agent->target_point.y >= PlatformTopHeight(platform) - 0.10f;
            if (agent->grounded && target_requests_climb) {
                if (BeginClimb(agent, platform)) return false;
            }
            if (agent->grounded) {
                Rectangle footprint = {platform->x, platform->z,
                                       platform->width, platform->depth};
                float current_distance = FootprintDistanceSquared(
                    agent->position.x, agent->position.z, footprint);
                float candidate_distance = FootprintDistanceSquared(
                    candidate_x, candidate_z, footprint);
                if (candidate_distance <= current_distance + 0.000001f) {
                    return false;
                }
            }
        }
    }
    Vector3 candidate = agent->position;
    candidate.x = candidate_x;
    candidate.z = candidate_z;
    if (StaticBodyBlocked(scene, candidate_x, candidate_z, agent->radius) ||
        LocalAgentCapsuleBlocked(scene, agent->position, candidate,
                                 agent->radius) ||
        CcLocalAgentPointSpaceBlockedInternal(agent, candidate)) {
        return false;
    }
    if (move_x) agent->position.x = candidate_x;
    else agent->position.z = candidate_z;
    return true;
}

static bool UpdateCombatClock(CcLocalAgent *agent, float delta_time)
{
    CcCombatState *combat = &agent->combat;
    combat->hit_flash_seconds = fmaxf(0.0f,
                                      combat->hit_flash_seconds - delta_time);
    if (combat->hitstop_seconds > 0.0f && agent->grounded &&
        !agent->climbing && !agent->swimming) {
        combat->hitstop_seconds = fmaxf(0.0f,
                                        combat->hitstop_seconds - delta_time);
        return true;
    }
    combat->hitstop_seconds = fmaxf(0.0f,
                                    combat->hitstop_seconds - delta_time);
    combat->stagger_seconds = fmaxf(0.0f,
                                    combat->stagger_seconds - delta_time);
    if (combat->life_state == CC_LIFE_DEAD &&
        combat->team == CC_COMBAT_PLAYER) {
        combat->respawn_seconds = fmaxf(0.0f,
                                        combat->respawn_seconds - delta_time);
        if (combat->respawn_seconds <= 0.0f) {
            combat->life_state = CC_LIFE_RESPAWNING;
            if (agent->morphology == CC_MORPHOLOGY_BIPED) {
                CcHumanoidGaitBeginResurrection(&agent->humanoid);
            } else {
                CcMultilegRagdollAllowRecovery(
                    &agent->multileg_ragdoll);
            }
        }
    }
    if (combat->life_state == CC_LIFE_RESPAWNING &&
        !AgentPhysicalRagdollActive(agent) &&
        !AgentPhysicalRecovering(agent)) {
        combat->life_state = CC_LIFE_ALIVE;
        combat->health = 45.0f;
        combat->posture = 72.0f;
        combat->stagger_seconds = 0.42f;
        combat->hit_flash_seconds = 0.22f;
        combat->weapon_mode = combat->team == CC_COMBAT_PLAYER ||
                              combat->team == CC_COMBAT_RAIDER ?
                              CC_WEAPON_HELD : CC_WEAPON_NONE;
    }
    if (CombatCanAct(combat) && combat->stagger_seconds <= 0.0f) {
        float regeneration = agent->morphology == CC_MORPHOLOGY_BIPED &&
                             agent->humanoid.action ==
                                 CC_HUMANOID_ACTION_GUARD ? 4.0f : 15.0f;
        combat->posture = fminf(CC_LOCAL_COMBAT_MAX_POSTURE,
                                combat->posture + regeneration * delta_time);
    }
    return false;
}

static void SyncPhysicalLifeState(CcLocalAgent *agent)
{
    CcCombatState *combat = &agent->combat;
    if (combat->life_state == CC_LIFE_ALIVE &&
        AgentPhysicalRagdollActive(agent)) {
        combat->life_state = CC_LIFE_KNOCKED_DOWN;
        if (combat->weapon_mode == CC_WEAPON_HELD) {
            combat->weapon_mode = CC_WEAPON_RAGDOLL_ATTACHED;
        }
    } else if (combat->life_state == CC_LIFE_KNOCKED_DOWN &&
               !AgentPhysicalRagdollActive(agent)) {
        combat->life_state = CC_LIFE_ALIVE;
        combat->weapon_mode = combat->team == CC_COMBAT_PLAYER ||
                              combat->team == CC_COMBAT_RAIDER ?
                              CC_WEAPON_HELD : CC_WEAPON_NONE;
    }
}

static void ApplyRagdollWaterResponse(CcHumanoidGait *gait,
                                      float water_surface, float immersion,
                                      float delta_time)
{
    if (gait == NULL || !gait->ragdoll.active ||
        !isfinite(delta_time) || delta_time <= 0.0f) return;
    float water_amount = fmaxf(0.0f, fminf(immersion, 1.0f));
    for (int32_t index = 0; index < gait->ragdoll.particle_count; ++index) {
        CcBiomechRagdollParticle *particle = &gait->ragdoll.particles[index];
        if (particle->inverse_mass <= 0.0f) continue;
        float diameter = fmaxf(particle->radius * 2.0f, 0.08f);
        float submerged = fmaxf(0.0f, fminf(
            (water_surface + particle->radius - particle->position.y) /
                diameter,
            1.0f)) * water_amount;
        if (submerged <= 0.0f) continue;


        particle->acceleration.y += 19.62f * submerged;
        float retained_velocity = expf(-4.8f * submerged * delta_time);
        CcBiomechVec3 velocity = {
            particle->position.x - particle->previous_position.x,
            particle->position.y - particle->previous_position.y,
            particle->position.z - particle->previous_position.z,
        };
        particle->previous_position = (CcBiomechVec3){
            particle->position.x - velocity.x * retained_velocity,
            particle->position.y - velocity.y * retained_velocity,
            particle->position.z - velocity.z * retained_velocity,
        };
    }
}

void CcLocalAgentFixedStepInternal(CcLocalAgent *agent, float delta_time,
                                   bool market_interior)
{
    const float gravity = 9.81f;
    delta_time = fminf(delta_time, 1.0f / 30.0f);
    agent->previous_cape = agent->cape;
    if (UpdateCombatClock(agent, delta_time)) {
        UpdateHeroCape(agent, delta_time);
        return;
    }
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        UpdateHeroCape(agent, delta_time);
        return;
    }
    CcLocalSceneKind scene = AgentSceneForCall(agent, market_interior);
    agent->scene = scene;
    UpdateStreetPortalProximity(agent);
    bool biped = agent->morphology == CC_MORPHOLOGY_BIPED;
    LocalProbeContext context = {.scene = scene};
    if (biped && agent->humanoid_needs_reset) {
        CcHumanoidGaitInit(&agent->humanoid, ToLimbVector(agent->position),
                            agent->facing_yaw, ProbeLocalSurface, &context);
        ApplyAgentWalkingProfile(agent);
        agent->humanoid_needs_reset = false;
    }
    if (!biped && agent->multileg_ragdoll.active) {
        bool active = CcMultilegRagdollStep(
            &agent->multileg_ragdoll, &agent->limb_rig, delta_time,
            ProbeLocalSurface, ProbeLocalCollision, &context);
        CcLimbVec3 center = agent->multileg_ragdoll.body_center;
        agent->position = (Vector3){
            center.x,
            center.y - agent->limb_rig.morphology.body_height,
            center.z,
        };
        agent->velocity = FromLimbVector(
            agent->multileg_ragdoll.body_velocity);
        agent->facing_yaw = agent->multileg_ragdoll.body_yaw;
        agent->grounded =
            CcMultilegRagdollSupportContactCount(
                &agent->multileg_ragdoll) > 0;
        agent->traversal = agent->multileg_ragdoll.recovering ?
                           CC_TRAVERSAL_GET_UP : CC_TRAVERSAL_RAGDOLL;
        if (!active) {
            agent->position = FromLimbVector(
                agent->multileg_ragdoll.recovery_ground);
            agent->velocity = (Vector3){0};
            agent->grounded = true;
            agent->traversal = CC_TRAVERSAL_IDLE;
        }
        SyncPhysicalLifeState(agent);
        agent->cape = (CcLocalCapeState){0};
        agent->previous_cape = (CcLocalCapeState){0};
        agent->render_cape = (CcLocalCapeState){0};
        return;
    }
    bool was_swimming = agent->swimming;
    bool in_water = biped &&
                    CourseWaterContains(scene, agent->position.x,
                                        agent->position.z);
    if (was_swimming && !in_water) {
        float surface = BodySurfaceHeightAt(scene, agent->position.x,
                                            agent->position.z);
        agent->position.y = surface;
        agent->velocity.y = 0.0f;
        agent->grounded = true;
        CcHumanoidGaitEndSwim(&agent->humanoid,
                              ToLimbVector(agent->position),
                              agent->facing_yaw, ProbeLocalSurface, &context);
    }
    agent->swimming = in_water;
    agent->immersion = Approach(agent->immersion, in_water ? 1.0f : 0.0f,
                                delta_time * 2.8f);
    float traction = agent->limb_rig.initialized ? agent->limb_rig.traction : 1.0f;
    float base_speed = in_water ? 0.76f : biped ? 1.45f : 2.35f;
    if (biped) {
        base_speed *= AthleticBonus(agent, CC_ATHLETIC_MOBILITY, 0.045f);
    }
    float maximum_speed = biped ? base_speed :
                          base_speed * (0.68f + traction * 0.32f);
    if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_GUARD) {
        maximum_speed *= 0.54f;
    } else if (biped &&
               agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {
        maximum_speed *= 0.16f;
    }
    if (!CombatCanAct(&agent->combat) ||
        agent->combat.stagger_seconds > 0.0f) {
        maximum_speed = 0.0f;
    }
    float acceleration = 10.5f * traction *
        AthleticBonus(agent, CC_ATHLETIC_MOBILITY, 0.035f);
    Vector3 direction = {0};
    float target_distance = 0.0f;
    if (agent->exact_target_valid) {
        direction.x = agent->target_point.x - agent->position.x;
        direction.z = agent->target_point.z - agent->position.z;
        target_distance = sqrtf(direction.x * direction.x + direction.z * direction.z);
        if (AgentCanAdvanceNavigation(agent, target_distance) &&
            !agent->climbing && !agent->humanoid.ragdoll.active) {
            (void)AdvanceAgentNavigation(agent);
            if (agent->exact_target_valid) {
                direction.x = agent->target_point.x - agent->position.x;
                direction.z = agent->target_point.z - agent->position.z;
                target_distance = sqrtf(direction.x * direction.x +
                                        direction.z * direction.z);
            } else {
                direction = (Vector3){0};
                target_distance = 0.0f;
            }
        }
        float physical_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                     agent->velocity.z * agent->velocity.z);
        bool gait_settled = !biped ||
                            (!agent->humanoid.ragdoll.active &&
                             agent->humanoid.speed.value < 0.03f &&
                             physical_speed < 0.02f);
        if (target_distance < 0.025f && gait_settled &&
            fabsf(agent->position.y - agent->target_point.y) < 0.12f) {
            agent->exact_target_valid = false;
            agent->command_point_valid = false;
            target_distance = 0.0f;
        }
    }
    float desired_x = 0.0f;
    float desired_z = 0.0f;
    if (target_distance > 0.001f) {
        float desired_speed = fminf(maximum_speed, target_distance * 3.2f);
        if (biped) {
            desired_speed *= AgentCornerSpeedScale(agent, target_distance);
            if (!agent->combat.focus_valid) {
                float target_x = direction.x / target_distance;
                float target_z = direction.z / target_distance;
                float facing_x = sinf(agent->facing_yaw);
                float facing_z = cosf(agent->facing_yaw);
                float alignment = fmaxf(
                    -1.0f, fminf(1.0f,
                        target_x * facing_x + target_z * facing_z));
                float turn_amount = SmoothStep01(
                    (alignment + 0.12f) / 1.12f);
                desired_speed *= 0.18f + turn_amount * 0.82f;
            }
        }
        desired_x = direction.x / target_distance * desired_speed;
        desired_z = direction.z / target_distance * desired_speed;
    }
    if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_STRIKE) {

        desired_x = 0.0f;
        desired_z = 0.0f;
    } else if (biped && agent->combat.focus_valid &&
               !agent->humanoid.ragdoll.active) {
        float away_x = agent->position.x - agent->combat.focus_point.x;
        float away_z = agent->position.z - agent->combat.focus_point.z;
        float focus_distance = sqrtf(away_x * away_x + away_z * away_z);
        if (focus_distance > 0.0001f &&
            focus_distance < COMBAT_PERSONAL_SPACE) {
            float retreat_speed = fminf(
                maximum_speed,
                0.34f + (COMBAT_PERSONAL_SPACE - focus_distance) * 2.4f);
            desired_x = away_x / focus_distance * retreat_speed;
            desired_z = away_z / focus_distance * retreat_speed;
        }
    }
    if (CombatCanAct(&agent->combat) && agent->grounded &&
        !agent->climbing && !agent->swimming) {

        desired_x += agent->separation_velocity.x;
        desired_z += agent->separation_velocity.z;
    }
    if (!CombatCanAct(&agent->combat)) {
        desired_x = 0.0f;
        desired_z = 0.0f;
    } else if (agent->combat.stagger_seconds > 0.0f) {
        desired_x = agent->combat.knockback_velocity.x;
        desired_z = agent->combat.knockback_velocity.z;
    } else {
        desired_x += agent->combat.knockback_velocity.x;
        desired_z += agent->combat.knockback_velocity.z;
    }
    if (!(biped && agent->humanoid.ragdoll.active)) {
        float face_x = direction.x;
        float face_z = direction.z;
        bool should_face = target_distance > 0.001f;
        if (agent->combat.focus_valid) {
            face_x = agent->combat.focus_point.x - agent->position.x;
            face_z = agent->combat.focus_point.z - agent->position.z;
            should_face = face_x * face_x + face_z * face_z > 0.0001f;
        }
        if (should_face) {
            float target_yaw = atan2f(face_x, face_z);
            float difference = WrapAngle(target_yaw - agent->facing_yaw);
            float turn_response = 7.55f +
                (float)(AthleticLevel(agent, CC_ATHLETIC_MOBILITY) - 1) *
                    0.55f;
            agent->facing_yaw = WrapAngle(
                agent->facing_yaw +
                difference * fminf(1.0f, delta_time * turn_response));
        }
    }
    bool ragdoll_was_active = biped && agent->humanoid.ragdoll.active;
    if (biped && in_water && !agent->humanoid.ragdoll.active) {
        CcHumanoidGaitAdvanceSwim(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw,
            (CcLimbVec3){desired_x, 0.0f, desired_z},
            COURSE_WATER_SURFACE, agent->immersion, delta_time);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else if (biped) {
        if (in_water && agent->humanoid.ragdoll.active) {
            ApplyRagdollWaterResponse(&agent->humanoid,
                                      COURSE_WATER_SURFACE,
                                      agent->immersion, delta_time);
        }
        CcHumanoidGaitAdvancePhysical(
            &agent->humanoid, ToLimbVector(agent->position),
            agent->facing_yaw,
            (CcLimbVec3){desired_x, 0.0f, desired_z},
            agent->grounded, delta_time, ProbeLocalSurface,
            ProbeLocalCollision, &context);
        agent->velocity.x = agent->humanoid.root_velocity.x;
        agent->velocity.z = agent->humanoid.root_velocity.z;
    } else {
        agent->velocity.x = Approach(agent->velocity.x, desired_x,
                                     acceleration * delta_time);
        agent->velocity.z = Approach(agent->velocity.z, desired_z,
                                     acceleration * delta_time);
        float support_weight = fminf(1.0f, target_distance / 0.45f);
        agent->velocity.x += agent->limb_rig.body_acceleration.x * delta_time *
                             support_weight;
        agent->velocity.z += agent->limb_rig.body_acceleration.z * delta_time *
                             support_weight;
    }

    bool landed = false;
    if (biped && (ragdoll_was_active || agent->humanoid.ragdoll.active)) {
        CcLimbVec3 physical_position = CcHumanoidGaitAuthoritativePosition(
            &agent->humanoid, ToLimbVector(agent->position));
        CcLimbVec3 physical_velocity = CcHumanoidGaitAuthoritativeVelocity(
            &agent->humanoid, ToLimbVector(agent->velocity));
        agent->position = FromLimbVector(physical_position);
        agent->velocity = FromLimbVector(physical_velocity);
        agent->grounded = !agent->humanoid.ragdoll.active ||
                          agent->humanoid.support_state ==
                              CC_HUMANOID_SUPPORT_STABLE;
        agent->swimming = false;
        float knockback_decay = expf(-7.5f * delta_time);
        agent->combat.knockback_velocity.x *= knockback_decay;
        agent->combat.knockback_velocity.z *= knockback_decay;
        goto local_body_resolved;
    }

    Vector3 previous_position = agent->position;
    bool moved_x = TryHorizontalAxis(agent, market_interior, true,
                                     agent->velocity.x * delta_time);
    bool moved_z = TryHorizontalAxis(agent, market_interior, false,
                                     agent->velocity.z * delta_time);
    if (!moved_x) agent->velocity.x = 0.0f;
    if (!moved_z) agent->velocity.z = 0.0f;
    if (agent->climbing) {
        UpdateClimb(agent, delta_time, market_interior);
        UpdateHeroCape(agent, delta_time);
        return;
    }
    float main_progress = 0.0f;
    if (target_distance > 0.001f) {
        main_progress =
            ((agent->position.x - previous_position.x) * direction.x +
             (agent->position.z - previous_position.z) * direction.z) /
            target_distance;
    }
    bool forward_stalled = main_progress <
        fmaxf(0.0010f, maximum_speed * delta_time * 0.12f);
    if (target_distance > 0.001f && forward_stalled &&
        (!moved_x || !moved_z)) {
        Vector3 forward = {direction.x / target_distance, 0.0f,
                           direction.z / target_distance};
        float side_sign = (agent->navigation_repath_count & 1) != 0 ?
                          -1.0f : 1.0f;
        Vector3 side = {-forward.z * side_sign, 0.0f,
                        forward.x * side_sign};
        float sidestep = maximum_speed * 0.72f * delta_time;
        bool side_x = TryHorizontalAxis(agent, market_interior, true,
                                        side.x * sidestep);
        bool side_z = TryHorizontalAxis(agent, market_interior, false,
                                        side.z * sidestep);
        if ((!side_x || !side_z) && !agent->climbing) {
            if (side_x) agent->position.x -= side.x * sidestep;
            if (side_z) agent->position.z -= side.z * sidestep;
            (void)TryHorizontalAxis(agent, market_interior, true,
                                    -side.x * sidestep);
            (void)TryHorizontalAxis(agent, market_interior, false,
                                    -side.z * sidestep);
        }
        if (agent->climbing) {
            UpdateClimb(agent, delta_time, market_interior);
            UpdateHeroCape(agent, delta_time);
            return;
        }
    }
    agent->velocity.x = (agent->position.x - previous_position.x) / delta_time;
    agent->velocity.z = (agent->position.z - previous_position.z) / delta_time;
    float training_step = sqrtf(
        (agent->position.x - previous_position.x) *
            (agent->position.x - previous_position.x) +
        (agent->position.z - previous_position.z) *
            (agent->position.z - previous_position.z));
    bool requested_progress = agent->crowned && agent->exact_target_valid &&
        target_distance > 0.18f && maximum_speed > 0.01f;
    if (requested_progress && forward_stalled) {
        agent->movement_stall_seconds += delta_time;
    } else {
        agent->movement_stall_seconds = 0.0f;
    }
    if (agent->movement_stall_seconds > 0.72f) {
        bool click_path = agent->scene == CC_LOCAL_SCENE_STREET &&
            agent->navigation_active &&
            agent->navigation_destination_room ==
                STREET_CLICK_NAVIGATION_DESTINATION &&
            agent->command_point_valid;
        Vector3 retry_target = agent->command_point;
        int32_t retry_count = agent->navigation_repath_count;
        if (click_path && retry_count < 3 &&
            SetStreetClickTarget(agent, retry_target)) {

            agent->navigation_repath_count = retry_count + 1;
            agent->movement_stall_seconds = 0.0f;
            goto local_navigation_recovered;
        }

        agent->exact_target_valid = false;
        agent->target_valid = false;
        agent->command_point_valid = false;
        ClearAgentNavigation(agent);
        agent->movement_stall_seconds = 0.0f;
        agent->velocity.x = 0.0f;
        agent->velocity.z = 0.0f;
        if (biped && !agent->humanoid.ragdoll.active) {
            CcHumanoidGaitConstrainMotion(
                &agent->humanoid, ToLimbVector(agent->position),
                ToLimbVector(agent->velocity), agent->grounded);
        }
    }
local_navigation_recovered:
    if (agent->grounded && !agent->climbing && training_step < 0.20f) {
        agent->athletics.travel_training_distance += training_step;
        while (agent->athletics.travel_training_distance >= 12.0f) {
            agent->athletics.travel_training_distance -= 12.0f;
            CcLocalAgentTrainAthleticism(agent, CC_ATHLETIC_MOBILITY, 5.0f);
        }
    }
    float knockback_decay = expf(-7.5f * delta_time);
    agent->combat.knockback_velocity.x *= knockback_decay;
    agent->combat.knockback_velocity.z *= knockback_decay;

    bool occupies_water = biped &&
                          CourseWaterContains(scene, agent->position.x,
                                              agent->position.z);
    if (occupies_water) {
        agent->swimming = true;
        agent->immersion = Approach(agent->immersion, 1.0f,
                                    delta_time * 2.8f);
        if (!in_water) {
            CcHumanoidGaitAdvanceSwim(
                &agent->humanoid, ToLimbVector(agent->position),
                agent->facing_yaw,
                (CcLimbVec3){agent->velocity.x, 0.0f, agent->velocity.z},
                COURSE_WATER_SURFACE, agent->immersion, delta_time);
            agent->velocity.x = agent->humanoid.root_velocity.x;
            agent->velocity.z = agent->humanoid.root_velocity.z;
        }
        float float_base = COURSE_WATER_SURFACE - 0.82f;
        agent->position.y = Approach(agent->position.y, float_base,
                                     delta_time * 1.8f);
        agent->velocity.y = Approach(agent->velocity.y, 0.0f,
                                     delta_time * 4.0f);
        agent->grounded = false;
    } else {
        if (agent->swimming) {
            agent->swimming = false;
            CcHumanoidGaitEndSwim(&agent->humanoid,
                                  ToLimbVector(agent->position),
                                  agent->facing_yaw, ProbeLocalSurface,
                                  &context);
        }
        bool followed_ground = agent->grounded;
        agent->velocity.y -= gravity * delta_time;
        agent->position.y += agent->velocity.y * delta_time;
        float surface = BodySurfaceHeightAt(scene,
                                            agent->position.x,
                                            agent->position.z);
        if (agent->position.y <= surface && agent->velocity.y <= 0.0f) {
            landed = !agent->grounded;
            agent->position.y = surface;
            agent->velocity.y = 0.0f;
            agent->grounded = true;
        } else if (followed_ground && agent->velocity.y <= 0.0f &&
                   agent->position.y - surface <= 0.12f) {

            agent->position.y = surface;
            agent->velocity.y = 0.0f;
            agent->grounded = true;
        } else {
            agent->grounded = false;
        }
    }
local_body_resolved:
    if (biped && !agent->swimming) {
        CcHumanoidGaitConstrainMotion(&agent->humanoid,
                                      ToLimbVector(agent->position),
                                      ToLimbVector(agent->velocity),
                                      agent->grounded);
        agent->support_state = agent->humanoid.support_state;
        agent->grounded = agent->support_state == CC_HUMANOID_SUPPORT_STABLE ||
                          agent->support_state == CC_HUMANOID_SUPPORT_MARGINAL ||
                          agent->support_state == CC_HUMANOID_SUPPORT_HANDS;
        SyncPhysicalLifeState(agent);
    } else if (agent->swimming) {
        agent->support_state = CC_HUMANOID_SUPPORT_MARGINAL;
    }

    float horizontal_speed = sqrtf(agent->velocity.x * agent->velocity.x +
                                   agent->velocity.z * agent->velocity.z);
    if (agent->swimming) {
        agent->traversal = CC_TRAVERSAL_SWIM;
    } else if (biped && agent->humanoid.ragdoll.active) {
        agent->traversal = agent->humanoid.recovering ?
                           CC_TRAVERSAL_GET_UP : CC_TRAVERSAL_RAGDOLL;
    } else if (biped && agent->humanoid.action == CC_HUMANOID_ACTION_JUMP) {
        agent->traversal = CC_TRAVERSAL_JUMP;
    } else if (agent->grounded) {
        agent->traversal = horizontal_speed > 0.08f ? CC_TRAVERSAL_WALK :
                                                     CC_TRAVERSAL_IDLE;
    } else {
        agent->traversal = CC_TRAVERSAL_DROP;
    }
    if (landed && horizontal_speed < 0.08f &&
        !(biped && agent->humanoid.ragdoll.active)) {
        agent->traversal = CC_TRAVERSAL_IDLE;
    }
    if (landed && agent->jump_training_pending &&
        !(biped && agent->humanoid.ragdoll.active)) {
        CcLocalAgentTrainAthleticism(agent, CC_ATHLETIC_MOBILITY, 12.0f);
        agent->jump_training_pending = false;
    }
    if (biped) {
        CcHumanoidGaitResolvePose(&agent->humanoid,
                                  ToLimbVector(agent->position),
                                  agent->facing_yaw);
        float ragdoll_target = agent->humanoid.ragdoll.active ? 1.0f : 0.0f;
        agent->ragdoll_visual_blend = Approach(
            agent->ragdoll_visual_blend, ragdoll_target,
            delta_time * 4.5f);
        UpdateHeroCape(agent, delta_time);
    } else {
        float requested_speed = sqrtf(desired_x * desired_x +
                                      desired_z * desired_z);
        float speed_ratio = maximum_speed > 0.0001f ?
            requested_speed / maximum_speed : 0.0f;
        CcLimbPace requested_pace = speed_ratio < 0.45f ?
            CC_LIMB_PACE_WALK : speed_ratio < 0.82f ?
            CC_LIMB_PACE_RUN : CC_LIMB_PACE_SPRINT;
        (void)CcLimbRigRequestPace(&agent->limb_rig, requested_pace);
        CcLimbRigUpdate(&agent->limb_rig, ToLimbVector(ShellPodBaseCenter(agent)),
                        agent->facing_yaw, ToLimbVector(agent->velocity),
                        agent->grounded, delta_time, ProbeLocalSurface, &context);
        agent->cape = (CcLocalCapeState){0};
        agent->previous_cape = (CcLocalCapeState){0};
        agent->render_cape = (CcLocalCapeState){0};
    }
}

static CcLimbVec3 BlendLimbPoint(CcLimbVec3 before, CcLimbVec3 after,
                                 float amount)
{
    return (CcLimbVec3){
        before.x + (after.x - before.x) * amount,
        before.y + (after.y - before.y) * amount,
        before.z + (after.z - before.z) * amount,
    };
}

static float BlendPoseAngle(float before, float after, float amount)
{
    return WrapAngle(before + WrapAngle(after - before) * amount);
}

static void BlendHumanoidPose(CcHumanoidPose *result,
                              const CcHumanoidPose *before,
                              const CcHumanoidPose *after, float amount)
{
#define BLEND_POSE_POINT(point) \
    result->point = BlendLimbPoint(before->point, after->point, amount)
    BLEND_POSE_POINT(pelvis);
    BLEND_POSE_POINT(spine);
    BLEND_POSE_POINT(chest);
    BLEND_POSE_POINT(neck);
    BLEND_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = BlendLimbPoint(before->hip[leg], after->hip[leg],
                                          amount);
        result->knee[leg] = BlendLimbPoint(before->knee[leg],
                                           after->knee[leg], amount);
        result->ankle[leg] = BlendLimbPoint(before->ankle[leg],
                                            after->ankle[leg], amount);
        result->heel[leg] = BlendLimbPoint(before->heel[leg],
                                           after->heel[leg], amount);
        result->ball[leg] = BlendLimbPoint(before->ball[leg],
                                           after->ball[leg], amount);
        result->toe[leg] = BlendLimbPoint(before->toe[leg], after->toe[leg],
                                          amount);
        result->foot_pitch[leg] = BlendPoseAngle(before->foot_pitch[leg],
                                                 after->foot_pitch[leg],
                                                 amount);
        result->knee_flexion[leg] =
            before->knee_flexion[leg] +
            (after->knee_flexion[leg] - before->knee_flexion[leg]) * amount;
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = BlendLimbPoint(before->shoulder[arm],
                                               after->shoulder[arm], amount);
        result->elbow[arm] = BlendLimbPoint(before->elbow[arm],
                                            after->elbow[arm], amount);
        result->hand[arm] = BlendLimbPoint(before->hand[arm], after->hand[arm],
                                           amount);
    }
#undef BLEND_POSE_POINT
    result->pelvis_yaw = BlendPoseAngle(before->pelvis_yaw,
                                        after->pelvis_yaw, amount);
    result->pelvis_roll = BlendPoseAngle(before->pelvis_roll,
                                         after->pelvis_roll, amount);
    result->pelvis_pitch = BlendPoseAngle(before->pelvis_pitch,
                                          after->pelvis_pitch, amount);
    result->chest_yaw = BlendPoseAngle(before->chest_yaw,
                                       after->chest_yaw, amount);
    result->chest_roll = BlendPoseAngle(before->chest_roll,
                                        after->chest_roll, amount);
    result->chest_pitch = BlendPoseAngle(before->chest_pitch,
                                         after->chest_pitch, amount);
}

static CcLimbVec3 PosePointToLocal(CcLimbVec3 point, CcLimbVec3 origin,
                                   CcLimbVec3 right, CcLimbVec3 up,
                                   CcLimbVec3 forward)
{
    CcLimbVec3 offset = {
        point.x - origin.x,
        point.y - origin.y,
        point.z - origin.z,
    };
    return (CcLimbVec3){
        offset.x * right.x + offset.y * right.y + offset.z * right.z,
        offset.x * up.x + offset.y * up.y + offset.z * up.z,
        offset.x * forward.x + offset.y * forward.y + offset.z * forward.z,
    };
}

static CcLimbVec3 PosePointToWorld(CcLimbVec3 point, CcLimbVec3 origin,
                                   CcLimbVec3 right, CcLimbVec3 up,
                                   CcLimbVec3 forward)
{
    return (CcLimbVec3){
        origin.x + right.x * point.x + up.x * point.y + forward.x * point.z,
        origin.y + right.y * point.x + up.y * point.y + forward.y * point.z,
        origin.z + right.z * point.x + up.z * point.y + forward.z * point.z,
    };
}

static void HumanoidPoseToLocal(CcHumanoidPose *result,
                                const CcHumanoidPose *world,
                                CcLimbVec3 origin, float yaw)
{
    CcLimbVec3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    CcLimbVec3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    *result = *world;
#define LOCALIZE_POSE_POINT(point) \
    result->point = PosePointToLocal(world->point, origin, right, up, forward)
    LOCALIZE_POSE_POINT(pelvis);
    LOCALIZE_POSE_POINT(spine);
    LOCALIZE_POSE_POINT(chest);
    LOCALIZE_POSE_POINT(neck);
    LOCALIZE_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = PosePointToLocal(
            world->hip[leg], origin, right, up, forward);
        result->knee[leg] = PosePointToLocal(
            world->knee[leg], origin, right, up, forward);
        result->ankle[leg] = PosePointToLocal(
            world->ankle[leg], origin, right, up, forward);
        result->heel[leg] = PosePointToLocal(
            world->heel[leg], origin, right, up, forward);
        result->ball[leg] = PosePointToLocal(
            world->ball[leg], origin, right, up, forward);
        result->toe[leg] = PosePointToLocal(
            world->toe[leg], origin, right, up, forward);
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = PosePointToLocal(
            world->shoulder[arm], origin, right, up, forward);
        result->elbow[arm] = PosePointToLocal(
            world->elbow[arm], origin, right, up, forward);
        result->hand[arm] = PosePointToLocal(
            world->hand[arm], origin, right, up, forward);
    }
#undef LOCALIZE_POSE_POINT
}

static void HumanoidPoseToWorld(CcHumanoidPose *result,
                                const CcHumanoidPose *local,
                                CcLimbVec3 origin, float yaw)
{
    CcLimbVec3 right = {cosf(yaw), 0.0f, -sinf(yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    CcLimbVec3 forward = {sinf(yaw), 0.0f, cosf(yaw)};
    *result = *local;
#define WORLDIZE_POSE_POINT(point) \
    result->point = PosePointToWorld(local->point, origin, right, up, forward)
    WORLDIZE_POSE_POINT(pelvis);
    WORLDIZE_POSE_POINT(spine);
    WORLDIZE_POSE_POINT(chest);
    WORLDIZE_POSE_POINT(neck);
    WORLDIZE_POSE_POINT(head);
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        result->hip[leg] = PosePointToWorld(
            local->hip[leg], origin, right, up, forward);
        result->knee[leg] = PosePointToWorld(
            local->knee[leg], origin, right, up, forward);
        result->ankle[leg] = PosePointToWorld(
            local->ankle[leg], origin, right, up, forward);
        result->heel[leg] = PosePointToWorld(
            local->heel[leg], origin, right, up, forward);
        result->ball[leg] = PosePointToWorld(
            local->ball[leg], origin, right, up, forward);
        result->toe[leg] = PosePointToWorld(
            local->toe[leg], origin, right, up, forward);
    }
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        result->shoulder[arm] = PosePointToWorld(
            local->shoulder[arm], origin, right, up, forward);
        result->elbow[arm] = PosePointToWorld(
            local->elbow[arm], origin, right, up, forward);
        result->hand[arm] = PosePointToWorld(
            local->hand[arm], origin, right, up, forward);
    }
#undef WORLDIZE_POSE_POINT
}

static void PreserveSteppedFootContacts(CcHumanoidPose *stepped,
                                        const CcHumanoidPose *physical)
{
    for (int32_t leg = 0; leg < CC_HUMANOID_LEG_COUNT; ++leg) {
        CcLimbVec3 correction = {
            physical->ankle[leg].x - stepped->ankle[leg].x,
            physical->ankle[leg].y - stepped->ankle[leg].y,
            physical->ankle[leg].z - stepped->ankle[leg].z,
        };
        stepped->knee[leg].x += correction.x * 0.46f;
        stepped->knee[leg].y += correction.y * 0.46f;
        stepped->knee[leg].z += correction.z * 0.46f;
        stepped->ankle[leg] = physical->ankle[leg];
        stepped->heel[leg] = physical->heel[leg];
        stepped->ball[leg] = physical->ball[leg];
        stepped->toe[leg] = physical->toe[leg];
        stepped->foot_pitch[leg] = physical->foot_pitch[leg];
    }
}

static bool ApplySteppedLocomotionPose(CcLocalAgent *agent,
                                       const CcHumanoidPose *physical,
                                       CcHumanoidPose *result)
{
    const int32_t pose_count = 8;
    const float transition_fraction = 0.24f;
    bool active = agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION &&
                  agent->grounded && !agent->climbing && !agent->swimming &&
                  !agent->humanoid.ragdoll.active &&
                  agent->humanoid.speed.value > 0.10f;
    if (!active) {
        agent->stepped_pose.initialized = false;
        return false;
    }

    float phase = agent->humanoid.phase - floorf(agent->humanoid.phase);
    float stepped_phase = phase * (float)pose_count;
    int32_t bin = (int32_t)floorf(stepped_phase);
    if (bin < 0) bin = 0;
    if (bin >= pose_count) bin = pose_count - 1;
    CcHumanoidPose local;
    HumanoidPoseToLocal(&local, physical, physical->pelvis,
                        agent->facing_yaw);
    CcSteppedPoseState *state = &agent->stepped_pose;
    if (!state->initialized) {
        state->from_local = local;
        state->target_local = local;
        state->locomotion_bin = bin;
        state->initialized = true;
    } else if (state->locomotion_bin != bin) {
        state->from_local = state->target_local;
        state->target_local = local;
        state->locomotion_bin = bin;
    }

    float within = stepped_phase - floorf(stepped_phase);
    float transition = SmoothStep01(within / transition_fraction);
    CcHumanoidPose local_result;
    BlendHumanoidPose(&local_result, &state->from_local,
                      &state->target_local, transition);
    HumanoidPoseToWorld(result, &local_result, physical->pelvis,
                        agent->facing_yaw);
    PreserveSteppedFootContacts(result, physical);
    return true;
}

static CcLimbVec3 OffsetPosePoint(CcLimbVec3 point, CcLimbVec3 right,
                                  CcLimbVec3 up, CcLimbVec3 forward,
                                  float side, float rise, float depth)
{
    point.x += right.x * side + up.x * rise + forward.x * depth;
    point.y += right.y * side + up.y * rise + forward.y * depth;
    point.z += right.z * side + up.z * rise + forward.z * depth;
    return point;
}

static bool ApplyLocomotionPostureCorrection(const CcLocalAgent *agent,
                                              CcHumanoidPose *pose)
{
    bool active = agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION &&
                  agent->grounded && !agent->climbing && !agent->swimming &&
                  !agent->humanoid.ragdoll.active &&
                  agent->humanoid.speed.value > 0.08f;
    if (!active) return false;

    CcLimbVec3 right = {cosf(agent->facing_yaw), 0.0f,
                        -sinf(agent->facing_yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    float momentum_x = agent->humanoid.root_velocity.x;
    float momentum_z = agent->humanoid.root_velocity.z;
    float momentum_speed = sqrtf(momentum_x * momentum_x +
                                 momentum_z * momentum_z);
    CcLimbVec3 forward = {sinf(agent->facing_yaw), 0.0f,
                          cosf(agent->facing_yaw)};
    float facing_momentum = forward.x * momentum_x +
                            forward.z * momentum_z;
    if (momentum_speed > 0.035f &&
        facing_momentum < momentum_speed * 0.985f) {
        forward = (CcLimbVec3){momentum_x / momentum_speed, 0.0f,
                               momentum_z / momentum_speed};
    }
    float weight = SmoothStep01((momentum_speed - 0.06f) / 0.34f);
    float depth = 0.058f * weight;


    pose->spine = OffsetPosePoint(pose->spine, right, up, forward,
                                  0.0f, 0.0f, depth * 0.24f);
    pose->chest = OffsetPosePoint(pose->chest, right, up, forward,
                                  0.0f, 0.0f, depth * 0.70f);
    pose->neck = OffsetPosePoint(pose->neck, right, up, forward,
                                 0.0f, 0.0f, depth * 0.88f);
    pose->head = OffsetPosePoint(pose->head, right, up, forward,
                                 0.0f, 0.0f, depth);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        pose->shoulder[arm] = OffsetPosePoint(
            pose->shoulder[arm], right, up, forward,
            0.0f, 0.0f, depth * 0.70f);
        pose->elbow[arm] = OffsetPosePoint(
            pose->elbow[arm], right, up, forward,
            0.0f, 0.0f, depth * 0.38f);
        pose->hand[arm] = OffsetPosePoint(
            pose->hand[arm], right, up, forward,
            0.0f, 0.0f, depth * 0.16f);
    }
    return true;
}

static float HeldGestureWeight(const CcLocalAgent *agent)
{
    float enter = SmoothStep01((agent->humanoid.idle.still_time - 0.24f) /
                               0.24f);
    float seed_offset = (float)(agent->appearance.seed & UINT32_C(0xff)) /
                        255.0f * 3.2f;
    float cycle = fmodf(agent->humanoid.idle.still_time + seed_offset, 3.2f);
    float held = 0.0f;
    if (cycle < 0.18f) {
        held = SmoothStep01(cycle / 0.18f);
    } else if (cycle < 2.34f) {
        held = 1.0f;
    } else if (cycle < 2.56f) {
        held = 1.0f - SmoothStep01((cycle - 2.34f) / 0.22f);
    }
    return enter * held;
}

static bool ApplyRoleIdleGesture(const CcLocalAgent *agent,
                                 CcHumanoidPose *pose)
{
    bool active = !agent->crowned && agent->grounded &&
                  agent->humanoid.idle.stable && !agent->climbing &&
                  !agent->swimming && !agent->humanoid.ragdoll.active &&
                  agent->humanoid.action == CC_HUMANOID_ACTION_LOCOMOTION &&
                  agent->humanoid.speed.value < 0.10f;
    if (!active) return false;

    CcLimbVec3 right = {cosf(agent->facing_yaw), 0.0f,
                        -sinf(agent->facing_yaw)};
    CcLimbVec3 up = {0.0f, 1.0f, 0.0f};
    CcLimbVec3 forward = {sinf(agent->facing_yaw), 0.0f,
                          cosf(agent->facing_yaw)};
    float enter = SmoothStep01((agent->humanoid.idle.still_time - 0.18f) /
                               0.30f);
    float gesture = HeldGestureWeight(agent);
    float lean = agent->appearance.idle_lean * enter;
    pose->spine = OffsetPosePoint(pose->spine, right, up, forward,
                                  0.0f, 0.0f, lean * 0.36f);
    pose->chest = OffsetPosePoint(pose->chest, right, up, forward,
                                  0.0f, 0.0f, lean * 0.74f);
    pose->neck = OffsetPosePoint(pose->neck, right, up, forward,
                                 0.0f, 0.0f, lean * 0.94f);
    pose->head = OffsetPosePoint(pose->head, right, up, forward,
                                 0.0f, 0.0f, lean);
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        pose->shoulder[arm] = OffsetPosePoint(
            pose->shoulder[arm], right, up, forward,
            0.0f, 0.0f, lean * 0.72f);
    }

    CcLimbVec3 elbow_target[CC_HUMANOID_ARM_COUNT];
    CcLimbVec3 hand_target[CC_HUMANOID_ARM_COUNT];
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        elbow_target[arm] = pose->elbow[arm];
        hand_target[arm] = pose->hand[arm];
    }
#define ROLE_TARGET(side, rise, depth) \
    OffsetPosePoint(pose->chest, right, up, forward, side, rise, depth)
    switch (agent->appearance.role) {
        case CC_NPC_ROLE_GUARD:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.12f, 0.05f);
            hand_target[0] = ROLE_TARGET(-0.24f, -0.22f, 0.25f);
            elbow_target[1] = ROLE_TARGET(0.30f, -0.20f, 0.02f);
            hand_target[1] = ROLE_TARGET(0.17f, -0.38f, 0.12f);
            pose->chest_yaw += 0.08f * gesture;
            break;
        case CC_NPC_ROLE_RAIDER:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.08f, -0.01f);
            hand_target[0] = ROLE_TARGET(-0.22f, -0.34f, 0.08f);
            elbow_target[1] = ROLE_TARGET(0.34f, -0.02f, 0.10f);
            hand_target[1] = ROLE_TARGET(0.21f, 0.04f, 0.22f);
            pose->chest_yaw -= 0.10f * gesture;
            break;
        case CC_NPC_ROLE_MERCHANT:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.10f, 0.08f);
            hand_target[0] = ROLE_TARGET(-0.42f, -0.17f, 0.25f);
            elbow_target[1] = ROLE_TARGET(0.34f, -0.10f, 0.08f);
            hand_target[1] = ROLE_TARGET(0.42f, -0.17f, 0.25f);
            break;
        case CC_NPC_ROLE_LABORER:
            elbow_target[0] = ROLE_TARGET(-0.28f, -0.26f, 0.02f);
            hand_target[0] = ROLE_TARGET(-0.09f, -0.45f, 0.18f);
            elbow_target[1] = ROLE_TARGET(0.28f, -0.26f, 0.02f);
            hand_target[1] = ROLE_TARGET(0.09f, -0.45f, 0.18f);
            break;
        case CC_NPC_ROLE_SCOUT:
            elbow_target[0] = ROLE_TARGET(-0.34f, -0.10f, 0.04f);
            hand_target[0] = ROLE_TARGET(-0.20f, -0.36f, 0.09f);
            elbow_target[1] = ROLE_TARGET(0.37f, 0.02f, 0.13f);
            hand_target[1] = OffsetPosePoint(
                pose->head, right, up, forward, 0.16f, 0.02f, 0.23f);
            break;
        case CC_NPC_ROLE_HEALER:
            elbow_target[0] = ROLE_TARGET(-0.30f, -0.08f, 0.05f);
            hand_target[0] = ROLE_TARGET(-0.10f, -0.12f, 0.23f);
            elbow_target[1] = ROLE_TARGET(0.30f, -0.08f, 0.05f);
            hand_target[1] = ROLE_TARGET(0.10f, -0.12f, 0.23f);
            break;
        case CC_NPC_ROLE_REFUGEE:
            elbow_target[0] = ROLE_TARGET(-0.31f, -0.16f, 0.00f);
            hand_target[0] = ROLE_TARGET(-0.18f, -0.31f, 0.06f);
            elbow_target[1] = ROLE_TARGET(0.27f, -0.02f, 0.05f);
            hand_target[1] = ROLE_TARGET(0.10f, -0.08f, 0.16f);
            break;
        case CC_NPC_ROLE_TRAVELLER:
        case CC_NPC_ROLE_WAYFARER:
        default:
            elbow_target[0] = ROLE_TARGET(-0.32f, -0.16f, 0.01f);
            hand_target[0] = ROLE_TARGET(-0.20f, -0.36f, 0.07f);
            elbow_target[1] = ROLE_TARGET(0.29f, -0.06f, 0.04f);
            hand_target[1] = ROLE_TARGET(0.13f, -0.18f, 0.18f);
            break;
    }
#undef ROLE_TARGET

    float arm_weight = gesture * 0.82f;
    for (int32_t arm = 0; arm < CC_HUMANOID_ARM_COUNT; ++arm) {
        pose->elbow[arm] = BlendLimbPoint(
            pose->elbow[arm], elbow_target[arm], arm_weight);
        pose->hand[arm] = BlendLimbPoint(
            pose->hand[arm], hand_target[arm], arm_weight);
    }
    return true;
}


static bool ApplyHeroKnockdownPresentationPose(const CcLocalAgent *agent,
                                               CcHumanoidPose *pose)
{
    if (agent == NULL || pose == NULL || !agent->crowned ||
        !agent->humanoid.ragdoll.active) {
        return false;
    }

    float weight = SmoothStep01(agent->ragdoll_visual_blend) * 0.68f;
    if (weight <= 0.001f) return false;

    float floor_y = fminf(pose->ankle[0].y, pose->ankle[1].y);
    floor_y = fminf(floor_y, fminf(pose->hand[0].y, pose->hand[1].y));
    CcLimbVec3 origin = pose->pelvis;
    origin.y = fmaxf(origin.y, floor_y + 0.27f);

    CcHumanoidPose local = {0};
    local.pelvis = (CcLimbVec3){0.0f, 0.0f, 0.0f};
    local.spine = (CcLimbVec3){0.0f, 0.10f, 0.01f};
    local.chest = (CcLimbVec3){0.0f, 0.43f, 0.07f};
    local.neck = (CcLimbVec3){0.0f, 0.66f, 0.10f};
    local.head = (CcLimbVec3){0.0f, 0.84f, 0.13f};

    local.hip[0] = (CcLimbVec3){-0.155f, -0.01f, 0.0f};
    local.knee[0] = (CcLimbVec3){-0.37f, -0.12f, 0.28f};
    local.ankle[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.08f,
                                  0.45f};
    local.heel[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.02f,
                                 0.37f};
    local.ball[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.02f,
                                 0.53f};
    local.toe[0] = (CcLimbVec3){-0.62f, floor_y - origin.y + 0.02f,
                                0.59f};

    local.hip[1] = (CcLimbVec3){0.155f, -0.01f, 0.0f};
    local.knee[1] = (CcLimbVec3){0.34f, -0.14f, -0.18f};
    local.ankle[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.08f,
                                  -0.38f};
    local.heel[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.02f,
                                 -0.46f};
    local.ball[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.02f,
                                 -0.30f};
    local.toe[1] = (CcLimbVec3){0.51f, floor_y - origin.y + 0.02f,
                                -0.24f};

    local.shoulder[0] = (CcLimbVec3){-0.285f, 0.49f, 0.07f};
    local.elbow[0] = (CcLimbVec3){-0.43f, 0.24f, 0.25f};
    local.hand[0] = (CcLimbVec3){-0.49f, floor_y - origin.y + 0.07f,
                                 0.39f};
    local.shoulder[1] = (CcLimbVec3){0.285f, 0.49f, 0.07f};
    local.elbow[1] = (CcLimbVec3){0.40f, 0.35f, 0.20f};
    local.hand[1] = (CcLimbVec3){0.22f, 0.31f, 0.31f};

    CcHumanoidPose target;
    HumanoidPoseToWorld(&target, &local, origin, agent->facing_yaw);
    CcHumanoidPose blended;
    BlendHumanoidPose(&blended, pose, &target, weight);
    *pose = blended;
    return true;
}

static void UpdateRenderCape(CcLocalAgent *agent, float amount)
{
    if (!agent->cape.initialized) {
        agent->render_cape = (CcLocalCapeState){0};
        return;
    }
    if (!agent->previous_cape.initialized) {
        agent->render_cape = agent->cape;
    } else {
        CcLocalCapeState blended = {.initialized = true};
        for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
            blended.point[point] = PhysicsLerp(
                agent->previous_cape.point[point], agent->cape.point[point],
                amount);
            blended.previous[point] = PhysicsLerp(
                agent->previous_cape.previous[point],
                agent->cape.previous[point], amount);
        }
        blended.anchor = PhysicsLerp(agent->previous_cape.anchor,
                                     agent->cape.anchor, amount);
        agent->render_cape = blended;
    }

    CcHumanoidSkinPose skin;
    CcHumanoidSkinPoseResolve(&agent->render_pose, &skin);
    if (!skin.valid) return;
    Vector3 render_anchor = PhysicsAdd(
        FromLimbVector(skin.sockets[CC_HUMANOID_SOCKET_BACK].position),
        PhysicsScale(FromLimbVector(skin.body_up), 0.14f));
    Vector3 correction = PhysicsSubtract(render_anchor,
                                          agent->render_cape.anchor);
    for (int32_t point = 0; point < CC_LOCAL_CAPE_POINT_COUNT; ++point) {
        agent->render_cape.point[point] = PhysicsAdd(
            agent->render_cape.point[point], correction);
        agent->render_cape.previous[point] = PhysicsAdd(
            agent->render_cape.previous[point], correction);
    }
    agent->render_cape.anchor = render_anchor;
}

void CcLocalAgentInterpolateInternal(CcLocalAgent *agent, float amount)
{
    if (agent == NULL) return;
    if (agent->morphology == CC_MORPHOLOGY_BIPED &&
        agent->humanoid.initialized) {
        amount = fmaxf(0.0f, fminf(amount, 1.0f));
        const CcHumanoidPoseSnapshot *before =
            CcHumanoidGaitPreviousSnapshot(&agent->humanoid);
        const CcHumanoidPoseSnapshot *after =
            CcHumanoidGaitCurrentSnapshot(&agent->humanoid);
        const CcHumanoidPose *before_pose = before != NULL ? &before->pose :
            &agent->humanoid.previous_pose;
        const CcHumanoidPose *after_pose = after != NULL ? &after->pose :
            &agent->humanoid.pose;
        CcHumanoidPose physical_pose;
        BlendHumanoidPose(&physical_pose, before_pose, after_pose, amount);
        agent->render_pose = physical_pose;
        (void)ApplySteppedLocomotionPose(
            agent, &physical_pose, &agent->render_pose);
        (void)ApplyLocomotionPostureCorrection(agent, &agent->render_pose);
        (void)ApplyRoleIdleGesture(agent, &agent->render_pose);
        (void)ApplyHeroKnockdownPresentationPose(agent,
                                                 &agent->render_pose);
        agent->render_pose_valid = true;
        UpdateRenderCape(agent, amount);
    } else {
        agent->render_pose_valid = false;
        agent->render_cape = (CcLocalCapeState){0};
    }
}

static const CcHumanoidPose *AgentRenderPose(const CcLocalAgent *agent)
{
    return agent->render_pose_valid ? &agent->render_pose :
                                      &agent->humanoid.pose;
}
