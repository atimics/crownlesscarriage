static bool AdventureHit(Rectangle bounds);

/* The manifest is a reading surface, not a fixed 1280-pixel illustration.
   Keep its viewport separate from the always-visible departure action tray. */
typedef struct CarriageOverviewLayout {
    Rectangle viewport;
    Rectangle cargo;
    Rectangle details;
    Rectangle up;
    Rectangle down;
} CarriageOverviewLayout;

static CarriageOverviewLayout CarriageOverviewBounds(int width, int height)
{
    float available = fmaxf(360.0f, (float)width - 104.0f);
    float cargo_width = floorf((available - 20.0f) * 0.42f);
    Rectangle view = {52.0f, 214.0f, available,
        fmaxf(80.0f, (float)height - 358.0f)};
    return (CarriageOverviewLayout){
        .viewport = view,
        .cargo = {view.x, view.y, cargo_width, view.height},
        .details = {view.x + cargo_width + 20.0f, view.y,
                    available - cargo_width - 20.0f, view.height},
        .up = {view.x + available - 360.0f, (float)height - 134.0f, 172, 32},
        .down = {view.x + available - 180.0f, (float)height - 134.0f, 180, 32},
    };
}

static int CarriageCargoOrder(const CcPlayerCompany *player, CcGood *goods)
{
    int count = 0;
    /* Carried goods first; zero-stock types remain available further down. */
    for (int carried = 1; carried >= 0; --carried)
        for (int good = 0; good < CC_GOOD_COUNT; ++good)
            if ((player->cargo[good] > 0) == (carried != 0))
                goods[count++] = (CcGood)good;
    return count;
}

static int CarriageOverviewParagraph(const char *text, int x, int *y,
                                      int width, int size, Color color,
                                      bool draw)
{
    int height = AdventureText(text, x, *y, width, size, color, draw);
    *y += height + 8;
    return height;
}

static int CarriageOverviewDetails(const CcSim *sim, const LocalState *local,
                                    Rectangle pane, float scroll, bool draw)
{
    int x = (int)pane.x + 16;
    int start = (int)pane.y + 16 - (int)scroll;
    int y = start;
    int width = (int)pane.width - 32;
    int body = AdventureTextSize(15);
    int heading = AdventureTextSize(19);
    const CcSituation *quest = CcSimAcceptedSituation(sim);
    CarriageOverviewParagraph("PROMISE & NEXT STEP", x, &y, width, heading, TEAL, draw);
    if (quest != NULL) {
        char target[96], next[192];
        SituationTargetLabel(sim, quest, target, sizeof(target));
        SituationNextAction(sim, quest, next, sizeof(next));
        CarriageOverviewParagraph(TextFormat("%s — %s / %s",
            SituationTitle(quest->kind), target, quest->affected_name),
            x, &y, width, body, INK, draw);
        CarriageOverviewParagraph(next, x, &y, width, body, CC_GOLD, draw);
        CarriageOverviewParagraph(TextFormat("Due day %d  /  Reward +%" PRId64 " crowns",
            quest->deadline_day, quest->reward), x, &y, width, body, TEAL, draw);
        if (quest->kind == CC_SITUATION_RELIEF_DELIVERY ||
            quest->kind == CC_SITUATION_BLACK_MARKET_DELIVERY) {
            int remaining = quest->quantity - quest->progress;
            if (remaining < 0) remaining = 0;
            int aboard = sim->player.cargo[quest->good];
            if (aboard > remaining) aboard = remaining;
            CarriageOverviewParagraph(TextFormat("Committed load: %d / %d %s aboard",
                aboard, remaining, CcGoodName(quest->good)), x, &y, width, body,
                aboard >= remaining ? TEAL : DANGER, draw);
        }
    } else {
        CarriageOverviewParagraph("No active promise. The notices and Company Book hold available leads.",
            x, &y, width, body, INK, draw);
    }
    y += 8;
    CarriageOverviewParagraph("TEAM & DEPARTURE", x, &y, width, heading, CC_VIOLET, draw);
    for (int horse = 0; horse < CcSimHorseTeamCount(sim); ++horse)
        CarriageOverviewParagraph(TextFormat("%s — health %d, fatigue %d",
            CcPonyName(CcSimTeamPony(sim, horse)), sim->horse_team[horse].health,
            sim->horse_team[horse].fatigue), x, &y, width, body, INK, draw);
    CarriageOverviewParagraph(TextFormat("Passengers %d / %d  /  Maps %d / %d",
        CarriagePassengerCount(sim), sim->player.passenger_capacity,
        CcPlayerMapCount(sim), sim->player.map_capacity), x, &y, width, body, INK, draw);
    CarriageOverviewParagraph(local->carriage_inspection_road ?
        TextFormat("Route %d%% — carriage %s", sim->carriage.progress_milli / 10,
            RoadCarriageStatus(sim)) : local->site_kind == CC_LOCAL_SITE_NONE ?
        TextFormat("%d roads leave this town.", OutgoingRouteCount(sim)) :
        "Return road is ready.", x, &y, width, body, CC_GOLD, draw);
    if (!local->carriage_inspection_road) CarriageOverviewParagraph(CcSimHorseTeamReadiness(sim) < 30 ?
        "The team needs rest before departure." : "Departure checks ready.",
        x, &y, width, body, CcSimHorseTeamReadiness(sim) < 30 ? DANGER : TEAL, draw);
    const CcSettlement *place = CcSimSettlement(sim, sim->player.location_id);
    CcFoodEconomy food = {0};
    if (!local->carriage_inspection_road && place != NULL &&
        CcSimFoodEconomyAtSettlement(sim, place->id, &food)) {
        y += 8;
        CarriageOverviewParagraph("LOCAL NOURISHMENT", x, &y, width, heading, TEAL, draw);
        const char *state = food.stock < food.weekly_consumption * 2 ?
            "Shortage" : food.stock < food.reserve_target ? "Tight" : "Surplus";
        CarriageOverviewParagraph(TextFormat("%s: %d rations; weekly +%d / -%d; Bread %dc.",
            state, food.stock, food.weekly_production, food.weekly_consumption, food.unit_price),
            x, &y, width, body, INK, draw);
    }
    return y - start + 32;
}

static int CarriageCargoRowHeight(Rectangle pane)
{
    int size = AdventureTextSize(15);
    int width = ((int)pane.width - 44) / 2 - 48;
    int name_height = size + 7;
    for (int good = 0; good < CC_GOOD_COUNT; ++good) {
        int measured = AdventureText(CcGoodName((CcGood)good), 0, 0,
            width, size, INK, false);
        if (measured > name_height) name_height = measured;
    }
    return name_height + size + 22;
}

static float CarriageOverviewMaximumScroll(const CcSim *sim,
    const LocalState *local, CarriageOverviewLayout layout)
{
    int cargo_height = 60 + ((CC_GOOD_COUNT + 1) / 2) * CarriageCargoRowHeight(layout.cargo);
    int detail_height = CarriageOverviewDetails(sim, local, layout.details, 0, false);
    return fmaxf(0.0f, (float)(cargo_height > detail_height ? cargo_height : detail_height) -
                  layout.viewport.height);
}

static float CarriageOverviewClampScroll(float scroll, float maximum)
{
    if (!isfinite(scroll)) return 0.0f;
    return fmaxf(0.0f, fminf(scroll, maximum));
}

static bool HandleCarriageOverviewScroll(const CcSim *sim, LocalState *local,
                                         ClientView view)
{
    if (view != VIEW_CARRIAGE || local->carriage_tab != CARRIAGE_OVERVIEW) return false;
    CarriageOverviewLayout layout = CarriageOverviewBounds(ContextViewportWidth(), ContextViewportHeight());
    float maximum = CarriageOverviewMaximumScroll(sim, local, layout);
    float change = 0.0f;
    if (ClientKeyPressed(KEY_PAGE_UP) || AdventureHit(layout.up)) change -= layout.viewport.height * 0.75f;
    if (ClientKeyPressed(KEY_PAGE_DOWN) || AdventureHit(layout.down)) change += layout.viewport.height * 0.75f;
    if (CheckCollisionPointRec(GetMousePosition(), layout.viewport)) change -= GetMouseWheelMove() * 54.0f;
    local->carriage_overview_scroll = CarriageOverviewClampScroll(
        local->carriage_overview_scroll + change, maximum);
    return change != 0.0f;
}

static void DrawCarriageOverview(const CcSim *sim, const LocalState *local,
                                 Texture2D economic_goods)
{
    CarriageOverviewLayout layout = CarriageOverviewBounds(GetScreenWidth(), GetScreenHeight());
    float maximum = CarriageOverviewMaximumScroll(sim, local, layout);
    float scroll = CarriageOverviewClampScroll(local->carriage_overview_scroll, maximum);
    CcOverlayDrawBodyText(TextFormat("Load %d / %d slots  |  Team %d / 100  |  Wagon %d / 100",
        CcPlayerCargoUsed(&sim->player), sim->player.cargo_capacity,
        CcSimHorseTeamReadiness(sim), sim->carriage.condition), 52, 177,
        AdventureTextSize(16), INK);
    DrawPanel(layout.cargo, Fade(BACKGROUND, 0.76f));
    DrawPanel(layout.details, Fade(BACKGROUND, 0.76f));
    CcOverlayFlush();
    BeginScissorMode((int)layout.viewport.x, (int)layout.viewport.y,
        (int)layout.viewport.width, (int)layout.viewport.height);
    CcOverlayDrawBodyText("CARGO MANIFEST", (int)layout.cargo.x + 16,
        (int)(layout.cargo.y + 16 - scroll), AdventureTextSize(19), CC_GOLD);
    CcGood goods[CC_GOOD_COUNT];
    int count = CarriageCargoOrder(&sim->player, goods);
    int row_height = CarriageCargoRowHeight(layout.cargo);
    float slot_width = (layout.cargo.width - 44.0f) * 0.5f;
    for (int slot = 0; slot < count; ++slot) {
        CcGood good = goods[slot];
        Rectangle box = {layout.cargo.x + 16 + (float)(slot % 2) * (slot_width + 12),
            layout.cargo.y + 52 + (float)(slot / 2) * (float)row_height - scroll,
            slot_width, (float)row_height - 8};
        DrawRectangleRounded(box, 0.10f, 4, Fade(PANEL_HOVER, 0.50f));
        (void)DrawEconomicGoodIcon(economic_goods, good,
            (Rectangle){box.x + 8, box.y + 10, 24, 24},
            sim->player.cargo[good] > 0 ? WHITE : Fade(WHITE, 0.4f));
        int y = (int)box.y + 8;
        int height = AdventureText(CcGoodName(good), (int)box.x + 40, y,
            (int)box.width - 48, AdventureTextSize(15), INK, true);
        CcOverlayDrawBodyText(TextFormat("x%d", sim->player.cargo[good]),
            (int)box.x + 40, y + height, AdventureTextSize(15),
            sim->player.cargo[good] > 0 ? CC_GOLD : MUTED);
    }
    (void)CarriageOverviewDetails(sim, local, layout.details, scroll, true);
    /* Text is batched independently; flush while the clip is still active. */
    CcOverlayFlush();
    EndScissorMode();
    if (maximum > 0) {
        float thumb_height = fmaxf(24, layout.viewport.height * layout.viewport.height /
            (layout.viewport.height + maximum));
        float thumb_y = layout.viewport.y + (layout.viewport.height - thumb_height) * scroll / maximum;
        DrawRectangle((int)(layout.viewport.x + layout.viewport.width - 4),
            (int)thumb_y, 3, (int)thumb_height, TEAL);
    }
    CcOverlayDrawBodyText("Wheel or Page Up / Page Down", 52, GetScreenHeight() - 124,
        AdventureTextSize(13), MUTED);
    AdventureButton(layout.up, "Scroll up  PgUp", scroll > 0, false);
    AdventureButton(layout.down, "Scroll down  PgDn", scroll < maximum, false);
}
