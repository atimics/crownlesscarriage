static Color *CaptureGoodsIcon(Texture2D atlas, RenderTexture2D target)
{
    BeginTextureMode(target);
    ClearBackground(BLANK);
    bool drawn = DrawEconomicGoodIcon(atlas, CC_GOOD_BREAD,
                                      (Rectangle){0, 0, 32, 32}, WHITE);
    EndTextureMode();
    if (!drawn) return NULL;
    Image image = LoadImageFromTexture(target.texture);
    Color *pixels = LoadImageColors(image);
    UnloadImage(image);
    return pixels;
}

static int RunMapTextureLifetimeRegression(void)
{
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(64, 64, "Goods atlas lifetime");
    if (!IsWindowReady()) return ClientRegressionFailure("Graphics window required.");
    ClientMapTextures textures = {0};
    RenderTexture2D target = LoadRenderTexture(32, 32);
    Color *expected = NULL;
    Color *actual = NULL;
    int result = 1;
    if (!IsRenderTextureValid(target) ||
        !LoadMapTexture(&textures.economic_goods,
                        &textures.economic_goods_attempted,
                        CC_ECONOMIC_GOODS_ATLAS_ASSET, "economic goods atlas")) {
        (void)ClientRegressionFailure("Goods atlas and target must load.");
        goto cleanup;
    }
    SetTextureFilter(textures.economic_goods, TEXTURE_FILTER_POINT);
    unsigned int atlas_id = textures.economic_goods.id;
    expected = CaptureGoodsIcon(textures.economic_goods, target);
    int visible = 0;
    if (expected != NULL) {
        for (int i = 0; i < 32 * 32; ++i) {
            if (expected[i].a > 0) ++visible;
        }
    }
    if (visible < 32) {
        (void)ClientRegressionFailure("Bread icon must contain visible pixels.");
        goto cleanup;
    }
    /* Exercise both page slots and repeated map-close/local/cargo frames. */
    for (int frame = 0; frame < 8; ++frame) {
        Image page = GenImageColor(4, 4, frame % 2 == 0 ? RED : BLUE);
        if (frame % 2 == 0) {
            textures.illustrated = LoadTextureFromImage(page);
            textures.illustrated_attempted = true;
        } else {
            textures.collectible_atlas = LoadTextureFromImage(page);
            textures.collectible_atlas_attempted = true;
        }
        UnloadImage(page);
        ReleaseMapPageTextures(&textures);
        ReleaseMapPageTextures(&textures);
        if (textures.illustrated.id != 0U || textures.collectible_atlas.id != 0U ||
            textures.illustrated_attempted || textures.collectible_atlas_attempted ||
            textures.economic_goods.id != atlas_id || !textures.economic_goods_attempted) {
            (void)ClientRegressionFailure("Map eviction must retain the goods atlas.");
            goto cleanup;
        }
        actual = CaptureGoodsIcon(textures.economic_goods, target);
        if (actual == NULL || memcmp(actual, expected, 32U * 32U * sizeof(Color)) != 0) {
            (void)ClientRegressionFailure("Bread icon pixels must survive map eviction.");
            goto cleanup;
        }
        UnloadImageColors(actual);
        actual = NULL;
    }
    ReleaseMapTextures(&textures);
    ReleaseMapTextures(&textures);
    if (textures.economic_goods.id != 0U || textures.economic_goods_attempted) {
        (void)ClientRegressionFailure("Shutdown must clear the goods atlas handle.");
        goto cleanup;
    }
    result = 0;
    (void)puts("PASS goods atlas: rendered bread survives repeated map-page eviction.");
cleanup:
    if (actual != NULL) UnloadImageColors(actual);
    if (expected != NULL) UnloadImageColors(expected);
    ReleaseMapTextures(&textures);
    UnloadRenderTexture(target);
    CloseWindow();
    return result;
}
