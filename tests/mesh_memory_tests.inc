static size_t MeshCpuVertexBytes(const Mesh *mesh)
{
    size_t per_vertex = 0;
    if (mesh->vertices != NULL) per_vertex += 3U * sizeof(float);
    if (mesh->normals != NULL) per_vertex += 3U * sizeof(float);
    if (mesh->texcoords != NULL) per_vertex += 2U * sizeof(float);
    if (mesh->texcoords2 != NULL) per_vertex += 2U * sizeof(float);
    if (mesh->tangents != NULL) per_vertex += 4U * sizeof(float);
    if (mesh->colors != NULL) per_vertex += 4U;
    if (mesh->boneIndices != NULL) per_vertex += 4U;
    if (mesh->boneWeights != NULL) per_vertex += 4U * sizeof(float);
    if (mesh->animVertices != NULL) per_vertex += 3U * sizeof(float);
    if (mesh->animNormals != NULL) per_vertex += 3U * sizeof(float);
    return (size_t)mesh->vertexCount * per_vertex;
}

static Color *CaptureMemoryModel(Model model, RenderTexture2D target)
{
    Camera3D camera = {
        .position = {4, 3, 5}, .target = {0, 0.7f, 0},
        .up = {0, 1, 0}, .fovy = 4.0f, .projection = CAMERA_ORTHOGRAPHIC,
    };
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    DrawModel(model, (Vector3){0}, 1.0f, WHITE);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    Image shot = LoadImageFromTexture(target.texture);
    Color *pixels = LoadImageColors(shot);
    UnloadImage(shot);
    RequireRenderer(pixels != NULL, "mesh memory capture must be readable");
    int visible = 0;
    for (int i = 0; i < 128 * 128; ++i) {
        if (pixels[i].r > 20 || pixels[i].g > 20 || pixels[i].b > 20) ++visible;
    }
    RequireRenderer(visible > 100, "mesh memory capture must contain geometry");
    return pixels;
}

static void TestUploadedMeshRelease(void)
{
    RenderTexture2D target = LoadRenderTexture(128, 128);
    RequireRenderer(IsRenderTextureValid(target), "mesh memory target must load");
    Model cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));
    Mesh *mesh = &cube.meshes[0];
    size_t cube_bytes = MeshCpuVertexBytes(mesh);
    RequireRenderer(cube_bytes > 0 && mesh->indices != NULL,
                    "cube must exercise CPU buffers and indexed drawing");
    unsigned short *indices = mesh->indices;
    Color *before = CaptureMemoryModel(cube, target);

    /* Keep the CPU animation path usable when it owns animated vertices. */
    mesh->animVertices = MemAlloc((unsigned int)(mesh->vertexCount * 3) *
                                 (unsigned int)sizeof(float));
    float *vertices = mesh->vertices;
    ReleaseUploadedModelCpuData(&cube);
    RequireRenderer(mesh->vertices == vertices && mesh->animVertices != NULL,
                    "CPU animated geometry must stay available");
    MemFree(mesh->animVertices);
    mesh->animVertices = NULL;

    /* Meshes awaiting upload retain their source data. */
    unsigned int position_buffer = mesh->vboId[0];
    mesh->vboId[0] = 0;
    ReleaseUploadedModelCpuData(&cube);
    RequireRenderer(mesh->vertices == vertices,
                    "pending mesh upload must retain its source");
    mesh->vboId[0] = position_buffer;

    ReleaseUploadedModelCpuData(&cube);
    ReleaseUploadedModelCpuData(&cube);
    RequireRenderer(MeshCpuVertexBytes(mesh) == 0 && mesh->indices == indices &&
                    IsModelValid(cube), "uploaded cube must keep its GPU draw data");
    Color *after = CaptureMemoryModel(cube, target);
    RequireRenderer(memcmp(before, after, 128U * 128U * sizeof(Color)) == 0,
                    "indexed mesh pixels must match after CPU release");
    UnloadImageColors(before);
    UnloadImageColors(after);
    UnloadModel(cube);

    CreatureModelCache *horse = &creature_models[CC_CREATURE_HORSE][CC_CREATURE_POSE_IDLE];
    RequireRenderer(horse->ready, "horse skin must load for memory checks");
    Model resident = horse->model;
    char path[1024];
    RequireRenderer(ResolveAssetPath(CcCreatureAssetPath(CC_CREATURE_HORSE,
        CC_CREATURE_POSE_IDLE), path, sizeof(path)), "horse asset must resolve");
    horse->model = LoadModel(path);
    RequireRenderer(IsModelValid(horse->model), "fresh horse skin must load");
    ApplyNpcBodyStyle(&horse->model);
    const Color palette[CC_NPC_ARCHETYPE_MATERIAL_COUNT] = {
        WHITE, BROWN, BEIGE, RED, BROWN, WHITE, GRAY, GOLD, BLACK,
    };
    SetIndexedPalette(palette, 0.54f, false, (Vector3){0}, false);
    size_t horse_bytes = MeshCpuVertexBytes(&horse->model.meshes[0]);
    RequireRenderer(horse_bytes > 0, "fresh horse must contain CPU geometry");
    Color *poses[2];
    for (int frame = 0; frame < 2; ++frame) {
        RequireRenderer(PoseQuadrupedCreature(horse, CC_CREATURE_HORSE,
            (float)frame * PI * 0.5f, true, NULL), "horse pose must resolve");
        poses[frame] = CaptureMemoryModel(horse->model, target);
    }
    RequireRenderer(memcmp(poses[0], poses[1], 128U * 128U * sizeof(Color)) != 0,
                    "horse walking poses must move the rendered skin");
    ReleaseUploadedModelCpuData(&horse->model);
    RequireRenderer(MeshCpuVertexBytes(&horse->model.meshes[0]) == 0,
                    "GPU skin must release CPU vertex buffers");
    for (int frame = 0; frame < 2; ++frame) {
        RequireRenderer(PoseQuadrupedCreature(horse, CC_CREATURE_HORSE,
            (float)frame * PI * 0.5f, true, NULL), "released horse pose must resolve");
        after = CaptureMemoryModel(horse->model, target);
        RequireRenderer(memcmp(poses[frame], after, 128U * 128U * sizeof(Color)) == 0,
                        "walking skin pixels must match after CPU release");
        UnloadImageColors(poses[frame]);
        UnloadImageColors(after);
    }
    UnloadModel(horse->model);
    horse->model = resident;
    UnloadRenderTexture(target);
    (void)printf("PASS mesh memory: released %zu cube bytes and %zu horse bytes; pixels match\n",
                 cube_bytes, horse_bytes);
}
