/* A house carries one surface state through every roof part. Both town and
 * world views set it from the same saved plot before drawing the building. */
typedef struct BuildingRoofSurface {
    float snow;
    float wear;
    float fresh;
    uint32_t seed;
} BuildingRoofSurface;

static BuildingRoofSurface building_roof_surface;
static const Color BUILDING_SNOW = {222, 232, 236, 255};

static void DrawBuildingRoofSlope(Vector3 center, Vector3 size, Vector3 axis,
                                   float degrees, Color roof)
{
    rlPushMatrix();
    rlTranslatef(center.x, center.y, center.z);
    rlRotatef(degrees, axis.x, axis.y, axis.z);
    DrawBox((Vector3){0}, size, roof);
    const BuildingRoofSurface *surface = &building_roof_surface;
    if (surface->wear > 0.20f || surface->fresh > 0.01f) {
        float side = (surface->seed & 1U) != 0U ? 1.0f : -1.0f;
        Color patch = BlendColor(roof, WORLD_WOOD_LIGHT,
                                  0.22f + surface->fresh * 0.28f);
        DrawBox((Vector3){size.x * 0.23f * side, size.y * 0.5f + 0.018f,
                          size.z * -0.20f},
                (Vector3){size.x * 0.25f, 0.035f, size.z * 0.30f}, patch);
        if (surface->wear > 0.65f) {
            DrawBox((Vector3){size.x * -0.20f * side, size.y * 0.5f + 0.022f,
                              size.z * 0.24f},
                    (Vector3){size.x * 0.18f, 0.04f, size.z * 0.22f},
                    BlendColor(roof, WORLD_WOOD_SHADOW, 0.62f));
        }
    }
    if (surface->snow > 0.02f) {
        /* Wind leaves a thinner edge on one slope. Roof parts retain their
         * actual pitch, so the cover follows low barns and tall gables alike. */
        float shelter = degrees < 0.0f ? 1.0f : 0.78f;
        float pitch = fminf(1.0f, fabsf(degrees) / 90.0f);
        float cover = surface->snow * shelter * (1.0f - pitch * 0.35f);
        float span = 0.22f + cover * 0.73f;
        DrawBox((Vector3){size.x * (1.0f - span) * 0.36f,
                          size.y * 0.5f + 0.035f + cover * 0.045f, 0.0f},
                (Vector3){size.x * span, 0.055f + cover * 0.09f,
                          size.z * (0.67f + cover * 0.29f)},
                BlendColor(BUILDING_SNOW, WORLD_STONE, (1.0f - shelter) * 0.15f));
    }
    rlPopMatrix();
}
