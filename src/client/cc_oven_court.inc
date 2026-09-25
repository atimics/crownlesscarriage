/* This is a local inspection, not a quest. Mutations use the journaled
 * interaction in cc_adventure.inc; the panel retains the inspected snapshot. */
static Rectangle OvenCourtFooter(Rectangle panel, int32_t index)
{
    float width = (panel.width - 64.0f) / 3.0f;
    return (Rectangle){panel.x + 24.0f + (float)index * (width + 8.0f),
        panel.y + panel.height - 58.0f, width, 40.0f};
}

static void DrawOvenCourt(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    AdventurePanelTitle(panel, "Oven Court");
    AdventureButton(AdventureTab(panel, 0, 2), "Tally  1", true, local->oven_page == 0);
    AdventureButton(AdventureTab(panel, 1, 2), "What helps?  2", true, local->oven_page == 1);
    const CcOvenCourtObservation *o = &local->oven_observation;
    int x = (int)panel.x + 28, y = (int)panel.y + 130;
    int width = (int)panel.width - 56, size = AdventureTextSize(22);
    ClientTouchHeading("Silverwick's Oven Court", "An inspection, not a promise or a completed batch.");
    if (local->oven_page == 0) {
        y += AdventureWrap(TextFormat("PUBLIC TALLY - read on day %d", o->day), x, y, width, size, TEAL) + 12;
        y += AdventureWrap(TextFormat("%d Bread in store. %d Wheat in store.", o->bread, o->wheat),
            x, y, width, size + 2, INK) + 12;
        y += AdventureWrap(CcOvenCourtStatus(o), x, y, width, size, CC_GOLD) + 12;
        (void)AdventureWrap(TextFormat("Next scheduled work: day %d. Supplies can change before then. Read the tally again to update this note.",
            o->next_work_day), x, y, width, size - 1, MUTED);
    } else {
        y += AdventureWrap(CcOvenCourtAdvice(o), x, y, width, size + 1, INK) + 16;
        (void)AdventureWrap("Sell wheat at the grain merchant. The bakery handles support and repairs. Check quantities and payment there; the tally reserves nothing.",
            x, y, width, size - 1, MUTED);
    }
    bool here = sim->player.location_id == o->place_id && !sim->journey.active &&
        sim->mine.phase == CC_MINE_NONE;
    AdventureButton(OvenCourtFooter(panel, 0), "Grain shop  3", here, false);
    AdventureButton(OvenCourtFooter(panel, 1), "Bakery  4", here, false);
    AdventureButton(OvenCourtFooter(panel, 2), "Read note  B", true, false);
}

static bool HandleOvenCourt(const CcSim *sim, LocalState *local, ClientView *view,
    ClientView *return_view, char *message, size_t capacity)
{
    if (*view != VIEW_OVEN_COURT || ClientKeyPressed(KEY_F5)) return false;
    Rectangle panel = AdventurePanel();
    if (ClientKeyPressed(KEY_ESCAPE) || AdventureHit(AdventureClose(panel))) {
        *view = VIEW_LOCAL; return true;
    }
    if (ClientKeyPressed(KEY_ONE) || AdventureHit(AdventureTab(panel, 0, 2))) local->oven_page = 0;
    if (ClientKeyPressed(KEY_TWO) || AdventureHit(AdventureTab(panel, 1, 2))) local->oven_page = 1;
    if (AdventureHit(OvenCourtFooter(panel, 2))) {
        local->book_page = 3; local->book_offset = 0;
        *return_view = VIEW_OVEN_COURT; *view = VIEW_LEDGER;
    }
    bool grain = ClientKeyPressed(KEY_THREE) || AdventureHit(OvenCourtFooter(panel, 0));
    bool bakery = ClientKeyPressed(KEY_FOUR) || AdventureHit(OvenCourtFooter(panel, 1));
    if (grain || bakery) {
        const CcLocalPlaceProfile *profile = CcLocalPlaceProfileForSettlement(
            CcSimSettlement(sim, sim->player.location_id));
        const CcLocalShop *shop = CcLocalShopAt(profile, grain ?
            CC_LOCAL_SHOP_GRAIN_MERCHANT : CC_LOCAL_SHOP_BAKERY);
        const CcInteractionTarget *door = CcInteractionFind(&local->interactions,
            (CcInteractionKey){sim->player.location_id,
                shop != NULL ? (uint64_t)shop->building_index + 1U : 0U,
                CC_INTERACTION_DOOR});
        if (sim->player.location_id != local->oven_observation.place_id || door == NULL ||
            !door->available || !CcInteractionStart(&local->interaction, door,
                local->agent.position.x, local->agent.position.z)) {
            (void)snprintf(message, capacity,
                "Return to the street and find the open %s.",
                grain ? "grain merchant" : "bakery");
        } else {
            *view = VIEW_LOCAL; *return_view = VIEW_LOCAL;
            (void)snprintf(message, capacity,
                "Approaching the %s. No supplies have been committed.",
                grain ? "grain merchant" : "bakery");
        }
    }
    return true;
}

static void DrawAdventureCourtNotes(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    DrawAdventureBookHead(panel, "Court notes", 3);
    int x = (int)panel.x + 28, y = (int)panel.y + 128;
    int width = (int)panel.width - 56, size = AdventureTextSize(22);
    const CcEvent *note = CcOvenCourtNote(sim, local->book_offset);
    y += AdventureWrap("SILVERWICK - field notes", x, y, width, size + 2, CC_GOLD) + 14;
    if (note != NULL) {
        y += AdventureWrap(note->actor_id == sim->player.id ?
            "Copied from the public tally:" : "A bakery worker explained the local tally:",
            x, y, width, size - 1, TEAL) + 10;
        y += AdventureWrap(note->text, x, y, width, size, INK) + 14;
        (void)AdventureWrap("This is what the company learned then, not today's remote stock. A received delivery is not proof of recovery.",
            x, y, width, size - 1, MUTED);
    } else {
        (void)AdventureWrap("No field note here. Inspect the Oven Court tally beside Silverwick's Company store, or ask a present bakery worker.",
            x, y, width, size, MUTED);
    }
    AdventureButton(AdventureBookMineStepBounds(panel, 0), "Earlier note", CcOvenCourtNote(sim, 1) != NULL, local->book_offset == 1);
    AdventureButton(AdventureBookMineStepBounds(panel, 1), "Latest note", CcOvenCourtNote(sim, 0) != NULL, local->book_offset == 0);
}
