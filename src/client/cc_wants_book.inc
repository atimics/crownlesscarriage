/* One saved request or local belonging per page keeps the next action clear. */
typedef struct WantBookCard {
    char title[128], text[640];
    CcWantOffer actions[2];
    int count;
} WantBookCard;

static int WantBookCards(const CcSim *sim, int selected, WantBookCard *card)
{
    *card = (WantBookCard){0};
    int count = 0;
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim->wants.wants[i];
        if (w->id == 0 || w->known_day == 0) continue;
        if (count++ != selected) continue;
        const CcCharacter *person = CcSimCharacter(sim, w->person_id);
        const CcSettlement *home = CcSimSettlement(sim, w->home_id);
        (void)snprintf(card->title, sizeof(card->title), "%s at %s",
            person != NULL ? person->name : "A former acquaintance", home != NULL ? home->name : "town");
        if (w->status == CC_WANT_ACTIVE) {
            (void)CcWantsRequestText(sim, w->id, card->text, sizeof(card->text));
            size_t used = strlen(card->text);
            (void)snprintf(card->text + used, sizeof(card->text) - used,
                "\nReward: %" PRId64 " crowns and trust.", w->escrow);
            CcWantOffer *offer = &card->actions[card->count++];
            offer->command = (CcCommand){.kind = CC_COMMAND_PERSONAL_WANT,
                .target_id = w->id, .secondary_id = (uint64_t)w->revision, .amount = CC_WANT_GIVE};
            offer->ready = CcWantsPlan(sim, &offer->command, offer->detail, sizeof(offer->detail));
            (void)snprintf(offer->label, sizeof(offer->label), "%s",
                w->kind == CC_WANT_REPAIR_IRON ? "Give iron for the repair" : "Hand over the item");
            const CcBelonging *item = CcWantsItem(sim, w->item_id);
            const CcCustodyEntry *entry = item != NULL ? CcCustodyFind(&sim->custody, item->custody_id) : NULL;
            if (entry != NULL && entry->holder.kind == CC_CUSTODY_CHARACTER && w->parent_id == 0) {
                offer = &card->actions[card->count++];
                offer->command = (CcCommand){.kind = CC_COMMAND_PERSONAL_WANT,
                    .target_id = item->id, .secondary_id = entry->revision, .amount = CC_WANT_TAKE};
                offer->ready = CcWantsPlan(sim, &offer->command, offer->detail, sizeof(offer->detail));
                (void)snprintf(offer->label, sizeof(offer->label), "Borrow %s", item->name);
            }
        } else (void)snprintf(card->text, sizeof(card->text), "%s on day %d.",
            w->status == CC_WANT_FULFILLED ? "You helped" :
            w->status == CC_WANT_SETTLED ? "The town supplied this need" : "The request ended", w->settled_day);
    }
    CcWantOffer local_offers[CC_BELONGINGS];
    int local_count = CcWantsOffers(sim, 0, local_offers, CC_BELONGINGS);
    for (int i = 0; i < local_count; ++i) {
        if (count++ != selected) continue;
        const CcBelonging *item = CcWantsItem(sim, local_offers[i].command.target_id);
        const CcCustodyEntry *entry = item != NULL ? CcCustodyFind(&sim->custody, item->custody_id) : NULL;
        (void)snprintf(card->title, sizeof(card->title), "%s", item != NULL ? item->name : "Belonging");
        (void)snprintf(card->text, sizeof(card->text), "%s\nCondition: %d/100. The satchel holds four belongings.",
            local_offers[i].command.amount == CC_WANT_LEAVE ? "Carried in your satchel." : "Kept in this town's stores.",
            entry != NULL ? entry->condition : 0);
        card->actions[0] = local_offers[i]; card->count = 1;
    }
    return count;
}
static Rectangle WantBookActionBounds(Rectangle panel, int index)
{
    return (Rectangle){panel.x + 24.0f, panel.y + panel.height - 150.0f + (float)index * 44.0f,
        panel.width - 48.0f, 36.0f};
}
static Rectangle WantBookTextBounds(Rectangle panel)
{
    return (Rectangle){panel.x + 28.0f, panel.y + 128.0f, panel.width - 56.0f,
        fmaxf(24.0f, panel.height - 322.0f)};
}
static Rectangle WantBookScrollBounds(Rectangle panel, bool down)
{
    return (Rectangle){panel.x + 24.0f + (down ? (panel.width - 48.0f) / 2.0f : 0.0f),
        panel.y + panel.height - 184.0f, (panel.width - 48.0f) / 2.0f, 28.0f};
}
static float WantBookMaximumScroll(const WantBookCard *card, Rectangle pane)
{
    int size = AdventureTextSize(15), width = (int)pane.width;
    int height = AdventureText(card->title, 0, 0, width, size + 2, CC_GOLD, false) + 10;
    height += AdventureText(card->text, 0, 0, width, size, INK, false) + 10;
    if (card->count > 0 && !card->actions[0].ready)
        height += AdventureText(card->actions[0].detail, 0, 0, width, size - 1, TEAL, false);
    return fmaxf(0.0f, (float)height - pane.height);
}
static void DrawAdventureWants(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    DrawAdventureBookHead(panel, "Requests", 5);
    WantBookCard card;
    int count = WantBookCards(sim, local->book_offset, &card);
    if (count > 0 && local->book_offset >= count) (void)WantBookCards(sim, count - 1, &card);
    if (count == 0) {
        (void)snprintf(card.title, sizeof(card.title), "People and belongings");
        (void)snprintf(card.text, sizeof(card.text), "Ask people what they need. Their requests appear here. Collect belongings from town stores and leave carried items there.");
    }
    Rectangle pane = WantBookTextBounds(panel);
    float maximum = WantBookMaximumScroll(&card, pane);
    float scroll = fmaxf(0.0f, fminf(local->want_scroll, maximum));
    int x = (int)pane.x, y = (int)(pane.y - scroll), width = (int)pane.width;
    int size = AdventureTextSize(15);
    BeginScissorMode((int)pane.x, (int)pane.y, width, (int)pane.height);
    {
        y += AdventureWrap(card.title, x, y, width, size + 2, CC_GOLD) + 10;
        y += AdventureWrap(card.text, x, y, width, size, INK) + 10;
        if (card.count > 0 && !card.actions[0].ready)
            (void)AdventureWrap(card.actions[0].detail, x, y, width, size - 1, TEAL);
    }
    EndScissorMode();
    AdventureButton(WantBookScrollBounds(panel, false), "Read up  PgUp", scroll > 0.0f, false);
    AdventureButton(WantBookScrollBounds(panel, true), "Read down  PgDn", scroll < maximum, false);
    for (int i = 0; i < card.count; ++i)
        AdventureButton(WantBookActionBounds(panel, i), card.actions[i].label, card.actions[i].ready, false);
    AdventureButton(AdventureBookMineStepBounds(panel, 0), "Previous request", count > 1, false);
    AdventureButton(AdventureBookMineStepBounds(panel, 1), "Next request", count > 1, false);
}
static void HandleAdventureWants(CcJournal *journal, CcSim *sim, LocalState *local, char *message, size_t capacity)
{
    Rectangle panel = AdventurePanel();
    WantBookCard card;
    int count = WantBookCards(sim, local->book_offset, &card);
    if (count <= 0) {
        local->book_offset = 0;
        (void)snprintf(card.title, sizeof(card.title), "People and belongings");
        (void)snprintf(card.text, sizeof(card.text), "Ask people what they need. Their requests appear here. Collect belongings from town stores and leave carried items there.");
    }
    if (count > 0 && local->book_offset >= count) {
        local->book_offset = count - 1;
        (void)WantBookCards(sim, local->book_offset, &card);
    }
    Rectangle pane = WantBookTextBounds(panel);
    float change = 0.0f;
    if (ClientKeyPressed(KEY_PAGE_UP) || AdventureHit(WantBookScrollBounds(panel, false))) change -= pane.height * 0.75f;
    if (ClientKeyPressed(KEY_PAGE_DOWN) || AdventureHit(WantBookScrollBounds(panel, true))) change += pane.height * 0.75f;
    if (CheckCollisionPointRec(ClientPointerPosition(), pane)) change -= GetMouseWheelMove() * 54.0f;
    local->want_scroll = fmaxf(0.0f, fminf(local->want_scroll + change, WantBookMaximumScroll(&card, pane)));
    if (count <= 0) return;
    if (ClientKeyPressed(KEY_UP) || AdventureHit(AdventureBookMineStepBounds(panel, 0))) {
        local->book_offset = (local->book_offset + count - 1) % count;
        local->want_scroll = 0.0f;
    } else if (ClientKeyPressed(KEY_DOWN) || AdventureHit(AdventureBookMineStepBounds(panel, 1))) {
        local->book_offset = (local->book_offset + 1) % count;
        local->want_scroll = 0.0f;
    } else for (int i = 0; i < card.count; ++i)
        if (card.actions[i].ready && AdventureHit(WantBookActionBounds(panel, i))) {
            (void)ApplyCommand(journal, sim, card.actions[i].command, message, capacity);
            local->want_scroll = 0.0f;
            break;
        }
}
