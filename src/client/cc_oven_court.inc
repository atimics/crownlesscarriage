
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
        (void)AdventureWrap("No field note here. Inspect the Oven Court tally beside Silverwick's grain merchant, or ask a present bakery worker.",
            x, y, width, size, MUTED);
    }
    AdventureButton(AdventureBookMineStepBounds(panel, 0), "Earlier note", CcOvenCourtNote(sim, 1) != NULL, local->book_offset == 1);
    AdventureButton(AdventureBookMineStepBounds(panel, 1), "Latest note", CcOvenCourtNote(sim, 0) != NULL, local->book_offset == 0);
}
