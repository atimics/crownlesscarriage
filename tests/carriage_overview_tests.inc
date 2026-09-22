/* The departure controls must remain outside the scrollable reading surface. */
static bool CarriageOverviewContains(Rectangle outer, Rectangle inner)
{
    return inner.x >= outer.x && inner.y >= outer.y &&
        inner.x + inner.width <= outer.x + outer.width &&
        inner.y + inner.height <= outer.y + outer.height;
}

static int RunCarriageOverviewRegression(void)
{
    static CcSim sim;
    static LocalState local;
    CcSimInit(&sim, UINT32_C(0xc0a71a9e));
    local.carriage_tab = CARRIAGE_OVERVIEW;
    CcClientPreferences preferences = {0};
    CcClientPreferences *previous = adventure_preferences;
    adventure_preferences = &preferences;
    uint64_t before = CcSimHash(&sim);
    const int sizes[][2] = {{1040,620}, {1200,700}, {1280,720}, {1600,900}, {1920,1080}};
    for (int size = 0; size < 5; ++size) {
        int width = sizes[size][0], height = sizes[size][1];
        CarriageOverviewLayout layout = CarriageOverviewBounds(width, height);
        Rectangle screen = {0,0,(float)width,(float)height};
        if (!CarriageOverviewContains(screen, layout.viewport) ||
            !CarriageOverviewContains(layout.viewport, layout.cargo) ||
            !CarriageOverviewContains(layout.viewport, layout.details) ||
            !CarriageOverviewContains(screen, layout.up) ||
            !CarriageOverviewContains(screen, layout.down) ||
            CheckCollisionRecs(layout.cargo, layout.details) ||
            CheckCollisionRecs(layout.up, layout.down) ||
            layout.viewport.y + layout.viewport.height > layout.up.y ||
            layout.down.y + layout.down.height > height - 94)
            return ClientRegressionFailure("Carriage layout overlaps the window or departure controls.");
        for (int large = 0; large <= 2; ++large) {
            preferences.text_size = large;
            float maximum = CarriageOverviewMaximumScroll(&sim, &local, layout);
            float top = CarriageOverviewClampScroll(-10, maximum);
            float end = CarriageOverviewClampScroll(100000, maximum);
            if (!isfinite(maximum) || maximum < 0 || top != 0 || end != maximum ||
                CarriageOverviewClampScroll(NAN, maximum) != 0)
                return ClientRegressionFailure("Carriage scroll bounds must be finite and reachable.");
            float cargo_end = layout.cargo.y + 52 +
                (float)((CC_GOOD_COUNT + 1) / 2) * (float)CarriageCargoRowHeight(layout.cargo) - 8;
            if (cargo_end - maximum > layout.viewport.y + layout.viewport.height)
                return ClientRegressionFailure("The final cargo row cannot be reached.");
            int first = CarriageOverviewDetails(&sim, &local, layout.details, 0, false);
            int last = CarriageOverviewDetails(&sim, &local, layout.details, maximum, false);
            if (first != last)
                return ClientRegressionFailure("Scrolling changed measured text layout.");
        }
    }
    if (CcSimHash(&sim) != before)
        return ClientRegressionFailure("Measuring the carriage changed simulation state.");
    for (int good = 0; good < CC_GOOD_COUNT; ++good) sim.player.cargo[good] = good % 3 == 1 ? 4 : 0;
    CcGood goods[CC_GOOD_COUNT];
    bool seen[CC_GOOD_COUNT] = {0};
    bool empty_seen = false;
    if (CarriageCargoOrder(&sim.player, goods) != CC_GOOD_COUNT)
        return ClientRegressionFailure("The manifest lost a cargo type.");
    for (int slot = 0; slot < CC_GOOD_COUNT; ++slot) {
        int good = (int)goods[slot];
        if (good < 0 || good >= CC_GOOD_COUNT || seen[good])
            return ClientRegressionFailure("The manifest duplicates or invents a cargo type.");
        seen[good] = true;
        if (sim.player.cargo[good] <= 0) empty_seen = true;
        else if (empty_seen) return ClientRegressionFailure("Carried goods should precede empty types.");
    }
    /* Queued normal keyboard input must only change presentation state. */
    before = CcSimHash(&sim);
    ClientInputClearPressed();
    queued_key_press[KEY_PAGE_DOWN] = true;
    if (!HandleCarriageOverviewScroll(&sim, &local, VIEW_CARRIAGE) ||
        local.carriage_overview_scroll <= 0)
        return ClientRegressionFailure("Page Down did not scroll the overview.");
    float down = local.carriage_overview_scroll;
    local.carriage_tab = CARRIAGE_PONIES;
    if (HandleCarriageOverviewScroll(&sim, &local, VIEW_CARRIAGE) ||
        local.carriage_overview_scroll != down)
        return ClientRegressionFailure("Overview scrolling consumed pony input.");
    local.carriage_tab = CARRIAGE_OVERVIEW;
    ClientInputClearPressed();
    queued_key_press[KEY_PAGE_UP] = true;
    if (!HandleCarriageOverviewScroll(&sim, &local, VIEW_CARRIAGE) ||
        local.carriage_overview_scroll >= down || CcSimHash(&sim) != before)
        return ClientRegressionFailure("Scrolling must leave cargo, time, coins and promises unchanged.");
    ClientInputClearPressed();
    adventure_preferences = previous;
    puts("PASS carriage overview: responsive bounds, reachable cargo, large text and read-only scrolling");
    return 0;
}
