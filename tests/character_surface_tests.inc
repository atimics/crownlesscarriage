static Color RenderSurfaceSample(bool indexed, int surface, bool enabled,
                                  float light_yaw)
{
    Mesh mesh = {0};
    mesh.vertexCount = 6;
    mesh.triangleCount = 2;
    const float vertices[] = {
        -1,-1,0, 1,-1,0, 1,1,0, -1,-1,0, 1,1,0, -1,1,0,
    };
    mesh.vertices = MemAlloc(sizeof(vertices));
    mesh.normals = MemAlloc(sizeof(vertices));
    mesh.texcoords = MemAlloc(12 * sizeof(float));
    mesh.colors = MemAlloc(24);
    memcpy(mesh.vertices, vertices, sizeof(vertices));
    memset(mesh.texcoords, 0, 12 * sizeof(float));
    for (int vertex = 0; vertex < 6; ++vertex) {
        mesh.normals[vertex * 3] = 0.0f;
        mesh.normals[vertex * 3 + 1] = 0.0f;
        mesh.normals[vertex * 3 + 2] = 1.0f;
        mesh.colors[vertex * 4] = 14;
        mesh.colors[vertex * 4 + 1] = 128;
        mesh.colors[vertex * 4 + 2] = 0;
        mesh.colors[vertex * 4 + 3] = (unsigned char)(
            ((float)surface + 0.5f) * 255.0f / 8.0f + 0.5f);
    }
    UploadMesh(&mesh, false);
    Material material = LoadMaterialDefault();
    material.shader = indexed ? visual_style.npc : visual_style.character.shader;
    material.maps[MATERIAL_MAP_DIFFUSE].color = indexed ? WHITE : GRAY;
    Camera3D camera = {.position = {0,0,5}, .target = {0,0,0}, .up = {0,1,0},
                       .fovy = 2.4f, .projection = CAMERA_ORTHOGRAPHIC};
    RenderTexture2D target = LoadRenderTexture(64, 64);
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    Color colors[CC_NPC_ARCHETYPE_MATERIAL_COUNT];
    for (int index = 0; index < CC_NPC_ARCHETYPE_MATERIAL_COUNT; ++index) {
        colors[index] = GRAY;
    }
    SetIndexedPalette(colors, 0.0f, false, (Vector3){0}, enabled);
    float vertex_colors = 0.0f;
    SetShaderValue(visual_style.character.shader,
        GetShaderLocation(visual_style.character.shader, "vertexColorsAreAlbedo"),
        &vertex_colors, SHADER_UNIFORM_FLOAT);
    const float direction[] = {sinf(light_yaw), 0, cosf(light_yaw)};
    SetShaderValue(material.shader, GetShaderLocation(material.shader, "lightDirection"),
                    direction, SHADER_UNIFORM_VEC3);
    DrawMesh(mesh, material, MatrixIdentity());
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    Image shot = LoadImageFromTexture(target.texture);
    Color sample = GetImageColor(shot, 32, 32);
    UnloadImage(shot);
    UnloadRenderTexture(target);
    UnloadMesh(mesh);
    material.shader = (Shader){0};
    UnloadMaterial(material);
    vertex_colors = 1.0f;
    SetShaderValue(visual_style.character.shader,
        GetShaderLocation(visual_style.character.shader, "vertexColorsAreAlbedo"),
        &vertex_colors, SHADER_UNIFORM_FLOAT);
    return sample;
}

static void TestCharacterSurfaces(void)
{
    for (int indexed = 0; indexed < 2; ++indexed) {
        Color cloth = RenderSurfaceSample(indexed != 0, 1, true, 0.0f);
        Color metal = RenderSurfaceSample(indexed != 0, 4, true, 0.0f);
        Color skin = RenderSurfaceSample(indexed != 0, 0, true, 0.0f);
        RequireRenderer((int)metal.r > (int)cloth.r + 12,
                        "equal-colored metal must have a stronger highlight than cloth");
        RequireRenderer(skin.r != cloth.r || skin.g != cloth.g || skin.b != cloth.b,
                        "surface labels must distinguish skin from equal-colored cloth");
        (void)printf("PASS %s material samples: cloth %d, metal %d, skin %d\n",
                     indexed ? "indexed" : "hero", cloth.r, metal.r, skin.r);
        float transition = 2.0f * acosf(powf(0.52f, 1.0f / 24.0f));
        Color first = RenderSurfaceSample(indexed != 0, 4, true, transition - 0.002f);
        Color second = RenderSurfaceSample(indexed != 0, 4, true, transition + 0.002f);
        RequireRenderer(abs((int)first.r - (int)second.r) < 12 &&
                        abs((int)first.g - (int)second.g) < 12 &&
                        abs((int)first.b - (int)second.b) < 12,
                        "a small light turn must keep the metal highlight stable");
    }
    Color plain = RenderSurfaceSample(true, 1, false, 0.0f);
    Color reset = RenderSurfaceSample(true, 4, false, 0.0f);
    RequireRenderer(ColorToInt(plain) == ColorToInt(reset),
                    "a later ordinary palette must reset character material styling");
}
