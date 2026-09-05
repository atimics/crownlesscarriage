/* Use the trade commands and the same stock queries as the renderer. */
static void TestPhysicalGoods(void)
{
    static CcSim sim;
    char error[192];
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        CcSimInit(&sim, 42U);
        CcSettlement *place = CcSimSettlementMutable(&sim, sim.player.location_id);
        sim.player.coins = 100000;
        sim.player.cargo_capacity = 100;
        place->market_coins = 100000;
        memset(sim.player.cargo, 0, sizeof(sim.player.cargo));
        place->stock[good] = 2;
        RequireRenderer(CcLocalWarehousePileCountInternal(place, (CcGood)good) == 2,
            "two stock units must appear in their warehouse bay");
        CcCommand trade = {.kind = CC_COMMAND_TRADE, .good = (CcGood)good, .amount = 2};
        RequireRenderer(CcSimApply(&sim, &trade, error, sizeof(error)), error);
        RequireRenderer(place->stock[good] == 0 && sim.player.cargo[good] == 2 &&
            CcLocalWarehousePileCountInternal(place, (CcGood)good) == 0 &&
            CcLocalCargoBoxCountInternal(&sim.player) == 2,
            "buying the last stock must empty the bay and load two carriage units");
        RequireRenderer(CcLocalCargoBoxGoodInternal(&sim.player, 0) == (CcGood)good &&
            CcLocalCargoBoxGoodInternal(&sim.player, 1) == (CcGood)good &&
            CcLocalCargoBoxGoodInternal(&sim.player, 2) == CC_GOOD_COUNT,
            "carriage models must match the purchased good and quantity");
        uint64_t hash = CcSimHash(&sim);
        RequireRenderer(!CcSimApply(&sim, &trade, error, sizeof(error)) && CcSimHash(&sim) == hash,
            "an empty bay must preserve stock and cargo when a purchase fails");
        trade.amount = -1;
        RequireRenderer(CcSimApply(&sim, &trade, error, sizeof(error)), error);
        RequireRenderer(CcLocalWarehousePileCountInternal(place, (CcGood)good) == 1 &&
            CcLocalCargoBoxCountInternal(&sim.player) == 1,
            "selling one unit must refill the warehouse and reduce the carriage load");
        RequireRenderer(CcSimApply(&sim, &trade, error, sizeof(error)), error);
        RequireRenderer(place->stock[good] == 2 && CcLocalCargoBoxCountInternal(&sim.player) == 0,
            "selling the last unit must empty the carriage rack");
        place->stock[good] = INT32_MAX;
        RequireRenderer(CcLocalWarehousePileCountInternal(place, (CcGood)good) == 3,
            "large warehouse stocks must keep a bounded display");
    }
    CcPlayerCompany mixed = {0};
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) mixed.cargo[good] = INT32_MAX;
    RequireRenderer(CcLocalCargoBoxCountInternal(&mixed) == 24, "a full rack must stay within its draw budget");
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
        RequireRenderer(CcLocalCargoBoxGoodInternal(&mixed, good) == (CcGood)good,
            "every carried good must remain visible on a full rack");
    RequireRenderer(CcLocalCargoBoxGoodInternal(NULL, 0) == CC_GOOD_COUNT &&
        CcLocalCargoBoxGoodInternal(&mixed, -1) == CC_GOOD_COUNT &&
        CcLocalWarehousePileCountInternal(NULL, CC_GOOD_FOOD) == 0,
        "missing stock and invalid slots must produce an empty display");
    (void)puts("PASS physical goods: all goods transfer between warehouse and carriage");
}

static Image RenderCargoQuantity(RenderTexture2D target, int32_t quantity)
{
    CcPlayerCompany player = {0};
    player.cargo[CC_GOOD_FOOD] = quantity;
    Camera3D camera = {.position = {4, 6, 5}, .target = {-0.30f, 2.70f, 0},
        .up = {0, 1, 0}, .fovy = 4.2f, .projection = CAMERA_ORTHOGRAPHIC};
    BeginTextureMode(target);
    ClearBackground(BLACK);
    BeginMode3D(camera);
    BeginWorldLighting(camera, &INTERIOR_ART_COMPOSITION);
    DrawCarriedCargoBoxes(&player, (Vector3){0}, 0.0f);
    EndWorldLighting();
    EndMode3D();
    EndTextureMode();
    return LoadImageFromTexture(target.texture);
}

static void TestPhysicalGoodsGraphics(const char *path)
{
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good) {
        EconomicCargoAsset *asset = &economic_cargo_assets[good];
        RequireRenderer(asset->ready && asset->bounds.max.x > asset->bounds.min.x &&
            asset->bounds.max.y > asset->bounds.min.y && asset->bounds.max.z > asset->bounds.min.z,
            "every good needs a loaded model with usable rack bounds");
    }
    RenderTexture2D target = LoadRenderTexture(512, 512);
    RequireRenderer(IsRenderTextureValid(target), "cargo comparison target must load");
    for (int32_t fallback = 0; fallback < 2; ++fallback) {
        economic_cargo_assets[CC_GOOD_FOOD].ready = fallback == 0;
        Image two = RenderCargoQuantity(target, 2);
        Image eight = RenderCargoQuantity(target, 8);
        Color *before = LoadImageColors(two), *after = LoadImageColors(eight);
        RequireRenderer(before != NULL && after != NULL, "cargo pixels must be readable");
        int32_t changed = 0;
        for (int32_t pixel = 0; pixel < two.width * two.height; ++pixel) {
            if (abs((int)before[pixel].r - (int)after[pixel].r) > 8 ||
                abs((int)before[pixel].g - (int)after[pixel].g) > 8 ||
                abs((int)before[pixel].b - (int)after[pixel].b) > 8) changed++;
        }
        RequireRenderer(changed > 1000, "adding six cargo units must visibly fill the rack");
        ImageFlipVertical(&two);
        ImageFlipVertical(&eight);
        RequireRenderer(ExportImage(two, TextFormat("%s-cargo-2-%d.png", path, fallback)) &&
            ExportImage(eight, TextFormat("%s-cargo-8-%d.png", path, fallback)),
            "cargo comparison captures must be written");
        UnloadImageColors(before);
        UnloadImageColors(after);
        UnloadImage(two);
        UnloadImage(eight);
        economic_cargo_assets[CC_GOOD_FOOD].ready = true;
        (void)printf("PASS cargo pixels: %d changed, fallback %d\n", changed, fallback);
    }
    UnloadRenderTexture(target);
}
