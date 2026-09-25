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
static void DrawAdventureWants(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    DrawAdventureBookHead(panel, "Requests", 5);
    WantBookCard card;
    int count = WantBookCards(sim, local->book_offset, &card);
    if (count > 0 && local->book_offset >= count) (void)WantBookCards(sim, count - 1, &card);
    int x = (int)panel.x + 28, y = (int)panel.y + 128, width = (int)panel.width - 56;
    int size = AdventureTextSize(15);
    BeginScissorMode(x, y, width, (int)panel.height - 288);
    if (count == 0) {
        (void)AdventureWrap("Ask people what they need. Their requests appear here. You can also collect belongings from town stores and leave carried items there.", x, y, width, size, INK);
    } else {
        y += AdventureWrap(card.title, x, y, width, size + 2, CC_GOLD) + 10;
        y += AdventureWrap(card.text, x, y, width, size, INK) + 10;
        if (card.count > 0 && !card.actions[0].ready)
            (void)AdventureWrap(card.actions[0].detail, x, y, width, size - 1, TEAL);
    }
    EndScissorMode();
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
    if (count <= 0) { local->book_offset = 0; return; }
    if (local->book_offset >= count) {
        local->book_offset = count - 1;
        (void)WantBookCards(sim, local->book_offset, &card);
    }
    if (ClientKeyPressed(KEY_UP) || AdventureHit(AdventureBookMineStepBounds(panel, 0)))
        local->book_offset = (local->book_offset + count - 1) % count;
    else if (ClientKeyPressed(KEY_DOWN) || AdventureHit(AdventureBookMineStepBounds(panel, 1)))
        local->book_offset = (local->book_offset + 1) % count;
    else for (int i = 0; i < card.count; ++i)
        if (card.actions[i].ready && AdventureHit(WantBookActionBounds(panel, i))) {
            (void)ApplyCommand(journal, sim, card.actions[i].command, message, capacity);
            break;
        }
}
