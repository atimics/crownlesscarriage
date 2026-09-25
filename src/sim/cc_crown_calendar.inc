/* Crown dates are witnessed dates. This register is an audit of observations;
   hearings and the company use only the pages they have actually received. */
static void CrownRecord(CcSim *sim, const CcScrivenNote *note)
{
    if (sim->schema_version < 115U || note->kind != CC_SCRIVEN_NOTE_CROWNED) return;
    CcCrownCalendar *c = &sim->crown_calendar;
    for (int i = 0; i < c->sighting_count; ++i) {
        if (c->sightings[i].dragon_id != note->dragon_id) continue;
        if (note->day < c->sightings[i].day) c->sightings[i] = *note;
        return;
    }
    if (c->sighting_count < CC_SCRIVEN_AGES) c->sightings[c->sighting_count++] = *note;
}
void CcCrownCalendarInit(CcSim *sim)
{
    if (sim == NULL) return;
    memset(&sim->crown_calendar, 0, sizeof(sim->crown_calendar));
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i)
        if (sim->scriven.books[i].id != 0) CrownRecord(sim, &sim->scriven.books[i].notes[0]);
}
static uint64_t CrownEdition(const CcSim *sim, const CcScrivenBook *book)
{
    if (sim->schema_version < 115U || book == NULL) return 0;
    return sim->crown_calendar.book_editions[book - sim->scriven.books];
}
static void CrownRead(CcSim *sim, const CcScrivenBook *book)
{
    if (sim->schema_version < 115U || book == NULL) return;
    CcCrownCalendar *c = &sim->crown_calendar;
    const CcScrivenNote *note = &book->notes[0];
    CcScrivenNote *known = &c->company_sighting;
    if (note->kind == CC_SCRIVEN_NOTE_CROWNED &&
        (known->kind == 0 || (known->dragon_id == note->dragon_id ? note->day < known->day : note->day > known->day)))
        *known = *note;
    uint64_t edition = CrownEdition(sim, book);
    if (edition > 0 && edition <= (uint64_t)c->editions) c->company = c->almanacs[edition - 1];
}
static bool CrownAttends(const CcSim *sim, const CcScrivenDelegate *d)
{
    const CcCharacter *p = CcSimCharacter(sim, d->person_id);
    return d->phase == CC_SCRIVEN_ATTENDING && d->place_id == sim->scriven.host_id &&
        d->route_id == 0 && Alive(sim, p) && p->current_settlement_id == d->place_id &&
        p->travel_destination_id == 0 && CcScrivenBookAccessible(sim, d->book_id, d->place_id);
}
static bool CrownHearing(CcSim *sim)
{
    CcScrivenState *s = &sim->scriven;
    CcCrownCalendar *c = &sim->crown_calendar;
    const CcScrivenBook *first = NULL, *second = NULL;
    CcId dragon = 0;
    int newest = 0;
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        const CcScrivenBook *b = &s->books[i];
        const CcScrivenNote *n = &b->notes[0];
        if (CcScrivenBookAccessible(sim, b->id, s->host_id) &&
            n->kind == CC_SCRIVEN_NOTE_CROWNED && n->day > newest) {
            newest = n->day; dragon = n->dragon_id;
        }
    }
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        const CcScrivenBook *b = &s->books[i];
        const CcScrivenNote *n = &b->notes[0];
        if (n->kind == CC_SCRIVEN_NOTE_CROWNED && n->dragon_id == dragon &&
            CcScrivenBookAccessible(sim, b->id, s->host_id) &&
            (first == NULL || n->day < first->notes[0].day)) first = b;
    }
    if (first == NULL) return false;
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        const CcScrivenBook *b = &s->books[i];
        const CcScrivenNote *n = &b->notes[0];
        if (n->kind == CC_SCRIVEN_NOTE_CROWNED && n->dragon_id == dragon &&
            n->source_book_id != first->notes[0].source_book_id && n->author_id != first->notes[0].author_id &&
            CcScrivenBookAccessible(sim, b->id, s->host_id) &&
            (second == NULL || n->day < second->notes[0].day)) second = b;
    }
    if (second == NULL) return false;
    CcScrivenFinding f = {.dragon_id = dragon, .proposed_day = first->notes[0].day,
        .earliest_day = first->notes[0].day, .latest_day = first->notes[0].day, .agreed_day = sim->current_day};
    CcId schools[CC_SCRIVEN_DELEGATES] = {0};
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &s->delegates[i];
        const CcScrivenBook *b = CcScrivenBookById(sim, d->book_id);
        if (b == NULL || !CrownAttends(sim, d) || School(sim, b->school_id) == NULL) continue;
        bool duplicate = false;
        for (int j = 0; j < f.schools; ++j) duplicate |= schools[j] == b->school_id;
        if (!duplicate) schools[f.schools++] = b->school_id;
        f.signers[i] = d->person_id;
    }
    if (f.schools < 2) return false;
    f.votes = f.schools;
    f.book_ids[0] = first->id; f.book_ids[1] = second->id;
    f.school_ids[0] = first->school_id; f.school_ids[1] = second->school_id;
    (void)snprintf(f.citations[0], CC_SCRIVEN_TEXT, "%s", first->notes[0].text);
    (void)snprintf(f.citations[1], CC_SCRIVEN_TEXT, "%s", second->notes[0].text);
    /* Earlier dates travel in editions too. A host may also keep its own copy. */
    int host = TownSlot(sim, s->host_id);
    if (host >= 0 && c->local[host].dragon_id == dragon && c->local[host].proposed_day < f.proposed_day)
        f = c->local[host];
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        uint64_t carried = CrownEdition(sim, &s->books[i]);
        if (carried == 0 || !CcScrivenBookAccessible(sim, s->books[i].id, s->host_id)) continue;
        const CcScrivenFinding *prior = &c->almanacs[carried - 1];
        if (prior->dragon_id == dragon && prior->proposed_day < f.proposed_day) f = *prior;
    }
    /* Equal dates share an edition; each changed date keeps a separate edition. */
    uint64_t edition = 0;
    for (int i = 0; i < c->editions; ++i)
        if (c->almanacs[i].dragon_id == dragon && c->almanacs[i].proposed_day == f.proposed_day &&
            c->almanacs[i].book_ids[0] == f.book_ids[0] && c->almanacs[i].book_ids[1] == f.book_ids[1]) {
            f = c->almanacs[i]; edition = (uint64_t)i + 1;
        }
    if (edition == 0) {
        if (c->editions == 32) return false;
        c->almanacs[c->editions++] = f; edition = (uint64_t)c->editions;
    }
    if (host >= 0) c->local[host] = f;
    CcSettlement *town = CcSimSettlementMutable(sim, s->host_id);
    if (town != NULL) for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        const CcScrivenDelegate *d = &s->delegates[i];
        CcScrivenBook *b = Book(sim, d->book_id);
        if (b == NULL || !CrownAttends(sim, d) || CrownEdition(sim, b) == edition || town->stock[CC_GOOD_PAPER] == 0) continue;
        --town->stock[CC_GOOD_PAPER]; c->book_editions[b - s->books] = edition;
    }
    (void)snprintf(s->report, sizeof(s->report),
        "The Crown Age begins on day %d, the first sighting in the cited tomes. %d schools agreed on day %d.",
        f.proposed_day, f.schools, f.agreed_day);
    return true;
}
static void CrownCarryFromHearing(CcSim *sim)
{
    if (sim->schema_version < 115U) return;
    CcCrownCalendar *c = &sim->crown_calendar;
    int host = TownSlot(sim, sim->scriven.host_id);
    if (host < 0 || c->local[host].agreed_day == 0) return;
    uint64_t edition = 0;
    for (int i = 0; i < c->editions; ++i)
        if (c->almanacs[i].dragon_id == c->local[host].dragon_id &&
            c->almanacs[i].agreed_day == c->local[host].agreed_day) edition = (uint64_t)i + 1;
    CcSettlement *town = &sim->settlements[host];
    for (int i = 0; i < CC_SCRIVEN_BOOKS && edition > 0; ++i) {
        CcScrivenBook *b = &sim->scriven.books[i];
        const CcTreasure *t = CcSimTreasure(sim, b->id);
        if (t == NULL || t->destroyed || (t->owner_id != sim->player.id && b->borrower_id != sim->player.id) ||
            !CcScrivenBookAccessible(sim, b->id, town->id)) continue;
        if (CrownEdition(sim, b) != edition) {
            if (town->stock[CC_GOOD_PAPER] == 0) continue;
            --town->stock[CC_GOOD_PAPER]; c->book_editions[i] = edition;
        }
        CrownRead(sim, b); return;
    }
}
static bool CrownDeliver(CcSim *sim, int town)
{
    if (sim->schema_version < 115U || town < 0) return false;
    CcCrownCalendar *c = &sim->crown_calendar;
    bool delivered = false;
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        const CcScrivenBook *b = &sim->scriven.books[i];
        const CcTreasure *t = CcSimTreasure(sim, b->id);
        uint64_t edition = CrownEdition(sim, b);
        if (edition == 0 || edition > (uint64_t)c->editions || t == NULL || t->destroyed ||
            (t->owner_id != sim->player.id && b->borrower_id != sim->player.id) ||
            !CcScrivenBookAccessible(sim, b->id, sim->settlements[town].id)) continue;
        const CcScrivenFinding *f = &c->almanacs[edition - 1];
        if (f->agreed_day >= c->local[town].agreed_day) c->local[town] = *f;
        delivered = true;
    }
    return delivered;
}
