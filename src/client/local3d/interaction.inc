#include "client/cc_interaction.h"

void CcLocalAgentStop(CcLocalAgent *agent)
{
    if (agent == NULL) return;
    agent->navigation_active = false;
    agent->navigation_point_count = 0;
    agent->navigation_point_index = 0;
    agent->navigation_world_exit = false;
    agent->exact_target_valid = false;
    agent->command_point_valid = false;
    agent->target_valid = false;
    CcLocalAgentClearWorldTarget(agent);
}

/* Use the camera that drew the scene. Small targets retain a usable click area. */
void CcLocalProjectInteraction(const CcLocalAgent *agent,
    RenderTexture2D texture, Rectangle destination, CcInteractionTarget *target)
{
    if (target == NULL) return;
    target->visible = false;
    Camera3D camera;
    if (agent == NULL || destination.width <= 0 || destination.height <= 0 ||
        !PresentedCameraForInput(agent->scene, agent, texture.texture.width,
                                  texture.texture.height, &camera)) return;
    float base = agent->scene == CC_LOCAL_SCENE_STREET ?
        CcLocalTerrainHeightAt(target->x, target->z) : 0.0f;
    target->y += base;
    Vector3 middle = {target->x, target->y + target->height * 0.65f, target->z};
    if (!CameraPointInFront(camera, middle)) return;
    LocalCollisionBox boxes[CC_LOCAL_COLLISION_BOX_CAPACITY];
    LocalProbeContext context = {.scene = agent->scene};
    int32_t count = GatherLocalCollisionBoxes(&context, middle, boxes);
    bool exposed = false;
    for (int32_t sample = 0; sample < 3; ++sample) {
        Vector3 point = {middle.x, target->y + target->height *
            (0.35f + (float)sample * 0.3f), middle.z};
        Ray ray = {camera.position, Vector3Normalize(Vector3Subtract(point, camera.position))};
        float distance = Vector3Distance(camera.position, point);
        bool occluded = false;
        for (int32_t i = 0; i < count; ++i) {
            BoundingBox box = {boxes[i].minimum, boxes[i].maximum};
            /* A building, counter or carriage may own the surface being picked. */
            if (point.x >= box.min.x - target->width * 0.5f &&
                point.x <= box.max.x + target->width * 0.5f &&
                point.z >= box.min.z - target->depth * 0.5f &&
                point.z <= box.max.z + target->depth * 0.5f &&
                target->key.kind != CC_INTERACTION_PERSON) continue;
            RayCollision hit = GetRayCollisionBox(ray, box);
            if (hit.hit && hit.distance + 0.12f < distance) { occluded = true; break; }
        }
        if (!occluded) { exposed = true; break; }
    }
    if (!exposed) return;
    float left = 1e9f, top = 1e9f, right = -1e9f, bottom = -1e9f;
    for (int32_t corner = 0; corner < 8; ++corner) {
        Vector3 point = {target->x + ((corner & 1) ? 0.5f : -0.5f) * target->width,
            target->y + ((corner & 2) ? target->height : 0.0f),
            target->z + ((corner & 4) ? 0.5f : -0.5f) * target->depth};
        Vector2 pixel = GetWorldToScreenEx(point, camera, texture.texture.width, texture.texture.height);
        float x = destination.x + pixel.x * destination.width / (float)texture.texture.width;
        float y = destination.y + pixel.y * destination.height / (float)texture.texture.height;
        left = fminf(left, x); right = fmaxf(right, x);
        top = fminf(top, y); bottom = fmaxf(bottom, y);
    }
    float pad = fmaxf(0.0f, (28.0f - (right - left)) * 0.5f);
    left -= pad; right += pad;
    if (right < destination.x || left > destination.x + destination.width ||
        bottom < destination.y || top > destination.y + destination.height) return;
    target->left = fmaxf(left, destination.x);
    target->right = fminf(right, destination.x + destination.width);
    target->top = fmaxf(top, destination.y);
    target->bottom = fminf(bottom, destination.y + destination.height);
    target->camera_distance = Vector3Distance(camera.position, middle);
    target->visible = true;
}
