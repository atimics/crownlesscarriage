/* Company Book calendar: all dates and passages come from the simulation. */
static Rectangle ScrivenButtonBounds(Rectangle panel, int index)
{
    float width = (panel.width - 64.0f) / 2.0f;
    return (Rectangle){panel.x + 24.0f + (float)(index % 2) * (width + 16.0f),
        panel.y + panel.height - 158.0f + (float)(index / 2) * 44.0f, width, 36.0f};
}
static int ScrivenNextBook(const CcSim *sim, int after)
{
    for (int step = 1; step <= CC_SCRIVEN_BOOKS; ++step) {
        int slot = (after + step + CC_SCRIVEN_BOOKS) % CC_SCRIVEN_BOOKS;
        if (CcScrivenBookAccessible(sim, sim->scriven.books[slot].id, sim->player.location_id)) return slot;
    }
    return -1;
}
static const CcScrivenBook *ScrivenSelectedBook(const CcSim *sim, const LocalState *local)
{
    int slot = (local->book_offset - 1) / 8;
    if (local->book_offset <= 0 || slot >= CC_SCRIVEN_BOOKS) return NULL;
    const CcScrivenBook *book = &sim->scriven.books[slot];
    return CcScrivenBookAccessible(sim, book->id, sim->player.location_id) ? book : NULL;
}
static void DrawAdventureCalendar(const CcSim *sim, const LocalState *local)
{
    Rectangle panel = AdventurePanel();
    DrawAdventureBookHead(panel, "Calendar", 4);
    int x = (int)panel.x + 28, y = (int)panel.y + 128;
    int width = (int)panel.width - 56, size = AdventureTextSize(15);
    const CcScrivenBook *book = ScrivenSelectedBook(sim, local);
    if (book != NULL) {
        const CcTreasure *t = CcSimTreasure(sim, book->id);
        const CcSettlement *owner = t != NULL ? CcSimSettlement(sim, t->owner_id) : NULL;
        y += AdventureWrap(t != NULL ? t->name : "Carried tome", x, y, width, size + 2, CC_GOLD) + 8;
        char terms[176];
        (void)snprintf(terms, sizeof(terms), "Owner: %s. Edition day %d. Condition %d/100.%s",
            owner != NULL ? owner->name : "The company", book->edition_day, book->condition,
            book->borrower_id == sim->player.id ? " On loan to the company." : "");
        if (book->borrower_id == sim->player.id)
            (void)snprintf(terms, sizeof(terms), "Loan from %s. Return this tome by day %d. Edition day %d; condition %d/100.",
                owner != NULL ? owner->name : "the town", book->loan_due_day, book->edition_day, book->condition);
        y += AdventureWrap(terms, x, y, width, size - 2, MUTED) + 12;
        int passage = (local->book_offset - 1) % 8;
        if (passage < 3) (void)AdventureWrap(book->passages[passage], x, y, width, size, INK);
        else if (passage < 7) {
            const CcScrivenNote *note = &book->notes[passage - 3];
            (void)AdventureWrap(note->kind != 0 ? note->text : "This margin is ready for a dated field note.", x, y, width, size, INK);
        } else if (book->almanac_id > 0) {
            const CcScrivenFinding *f = &sim->scriven.almanacs[book->almanac_id - 1];
            char finding[240];
            (void)snprintf(finding, sizeof(finding), "Almanac, agreed day %d: age begins on proposed day %d. Evidence places it between days %d and %d. %d schools signed.",
                f->agreed_day, f->proposed_day, f->earliest_day, f->latest_day, f->schools);
            (void)AdventureWrap(finding, x, y, width, size, INK);
        } else (void)AdventureWrap("Bring this tome to Scrivendays to receive a cited almanac.", x, y, width, size, MUTED);
        const char *labels[6] = {"Next tome", "Next passage", book->borrower_id == sim->player.id ? "Return tome" : t != NULL && t->owner_id == sim->player.id ? "Company tome" : "Borrow tome",
            "Copy: 2 crowns", "Record lair sighting", "Commission tome"};
        for (int i = 0; i < 6; ++i) AdventureButton(ScrivenButtonBounds(panel, i), labels[i],
            i != 2 || t == NULL || t->owner_id != sim->player.id, false);
    } else if (local->book_offset == -1) {
        CcCalendarDate date = CcCalendar(sim->current_day);
        y += AdventureWrap("Thirteen signs, 28 days each. The Wanderer names each year in its thirteen-year circuit.", x, y, width, size, INK) + 14;
        for (int i = 0; i < CC_ZODIAC_SIGNS; ++i) {
            CcOverlayDrawText(CcZodiacName(i), x + (i / 7) * width / 2, y + (i % 7) * 25,
                size, i == date.sign ? CC_GOLD : i == date.year_sign ? TEAL : MUTED);
        }
        AdventureButton(ScrivenButtonBounds(panel, 4), "Back to dates", true, false);
    } else {
        char text[640]; CcScrivenDescribe(sim, text, sizeof(text));
        y += AdventureWrap(text, x, y, width, size, INK) + 14;
        if (y < (int)(panel.y + panel.height - 240.0f))
            (void)AdventureWrap(sim->scriven.player_report[0] != '\0' ? sim->scriven.player_report :
                "Carry a tome, record dated sightings, and bring it to the scribes. Their almanacs keep the evidence beside the agreed date.",
                x, y, width, size - 2, TEAL);
        const char *labels[6] = {ScrivenNextBook(sim, -1) >= 0 ? "Browse local tomes" : "Commission tome",
            "Watch the sky", "Attend hearing", "Deliver almanac", "Spend a watch", "The thirteen signs"};
        for (int i = 0; i < 6; ++i) AdventureButton(ScrivenButtonBounds(panel, i), labels[i], true, false);
    }
}
static void HandleAdventureCalendar(CcJournal *journal, CcSim *sim, LocalState *local, char *message, size_t capacity)
{
    Rectangle panel = AdventurePanel();
    int clicked = -1;
    for (int i = 0; i < 6; ++i) if (AdventureHit(ScrivenButtonBounds(panel, i))) clicked = i;
    if (clicked < 0) return;
    if (local->book_offset == -1) { if (clicked == 4) local->book_offset = 0; return; }
    const CcScrivenBook *book = ScrivenSelectedBook(sim, local);
    CcCommand action = {.kind = CC_COMMAND_SCRIVEN, .target_id = book != NULL ? book->id : 0};
    if (book != NULL) {
        if (clicked == 0) {
            int next = ScrivenNextBook(sim, (local->book_offset - 1) / 8);
            local->book_offset = next < 0 ? 0 : next * 8 + 1;
            if (next >= 0) { action.amount = CC_SCRIVEN_READ; action.target_id = sim->scriven.books[next].id; }
        } else if (clicked == 1) {
            int base = (local->book_offset - 1) / 8 * 8;
            local->book_offset = base + local->book_offset % 8 + 1;
        } else if (clicked == 2) action.amount = book->borrower_id == sim->player.id ? CC_SCRIVEN_RETURN : CC_SCRIVEN_BORROW;
        else if (clicked == 3) action.amount = CC_SCRIVEN_COPY;
        else if (clicked == 4) action.amount = CC_SCRIVEN_OBSERVE;
        else if (clicked == 5) action.amount = CC_SCRIVEN_COMMISSION;
    } else {
        if (clicked == 0) {
            int next = ScrivenNextBook(sim, -1);
            local->book_offset = next < 0 ? 0 : next * 8 + 1;
            action.amount = next < 0 ? CC_SCRIVEN_COMMISSION : CC_SCRIVEN_READ;
            action.target_id = next < 0 ? 0 : sim->scriven.books[next].id;
        } else if (clicked == 1) action.amount = CC_SCRIVEN_SKY;
        else if (clicked == 2) action.amount = CC_SCRIVEN_HEARING;
        else if (clicked == 3) action.amount = CC_SCRIVEN_DELIVER;
        else if (clicked == 4) action.amount = CC_SCRIVEN_WAIT;
        else if (clicked == 5) local->book_offset = -1;
    }
    if (action.amount != 0) (void)ApplyCommand(journal, sim, action, message, capacity);
}
