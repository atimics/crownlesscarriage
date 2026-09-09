static CcMoney MinimumI64(CcMoney a, CcMoney b) { return a < b ? a : b; }

/* Goblin portage and the mixed-species dragon cult. Included after the
   underroad and event helpers so porters use the same open passages. */
const char *CcGoblinColorName(int32_t color)
{
    static const char *names[] = {"Red", "Purple", "Blue"};
    return color >= 0 && color < CC_GOBLIN_FACTION_COUNT ? names[color] : "Unchosen";
}

const char *CcCultRankName(int32_t rank)
{
    static const char *names[] = {"Initiate", "Bearer", "Keeper", "Voice"};
    return rank >= 0 && rank < CC_CULT_RANK_COUNT ? names[rank] : "Initiate";
}

int32_t CcSimCultMembers(const CcSim *sim, CcCultSpecies species)
{
    if (sim == NULL || species < CC_CULT_HUMAN || species > CC_CULT_GOBLIN) return 0;
    int32_t total = 0;
    for (int32_t r = 0; r < CC_CULT_RANK_COUNT; ++r) total += sim->dragon_cult.ranks[species][r];
    return total;
}

static int32_t DragonRitualMembers(const CcSim *sim)
{
    return sim->schema_version >= 75U ?
        CcSimCultMembers(sim, CC_CULT_HUMAN) + CcSimCultMembers(sim, CC_CULT_GOBLIN) : sim->goblins.members;
}

void CcSimInitializeGoblinPolitics(CcSim *sim)
{
    if (sim == NULL || sim->dungeon_count < 1) return;
    CcGoblinPolitics *p = &sim->goblin_politics;
    *p = (CcGoblinPolitics){0};
    p->dragon_id = sim->dragon.id;
    p->contest_started_day = sim->current_day;
    p->crown_faction = -1;
    p->raid_faction = (int32_t)(sim->world_seed % 3U);
    /* All three lairs share the physical underroad and its treasure. */
    static const int32_t rooms[] = {12, 13, 11};
    for (int32_t i = 0; i < CC_GOBLIN_FACTION_COUNT; ++i) {
        CcGoblinFaction *f = &p->factions[i];
        f->members = sim->goblins.members / 3 + (i < sim->goblins.members % 3 ? 1 : 0);
        f->dungeon_id = sim->dungeons[0].id;
        f->lair_room = rooms[i];
        f->porter_room = f->lair_room;
        f->target_room = f->lair_room;
    }
    memset(sim->dragon_cult.ranks, 0, sizeof(sim->dragon_cult.ranks));
    memset(sim->dragon_cult.service, 0, sizeof(sim->dragon_cult.service));
    const CcSettlement *home = CcSimSettlement(sim, sim->goblins.lair_settlement_id);
    sim->dragon_cult.ranks[CC_CULT_HUMAN][CC_CULT_INITIATE] = home != NULL ? MinimumI32(6, home->population) : 0;
    sim->dragon_cult.ranks[CC_CULT_GOBLIN][CC_CULT_INITIATE] = sim->goblins.members / 4;
}

static void CultService(CcSim *sim, CcCultSpecies species, CcMoney amount)
{
    if (amount <= 0) return;
    CcDragonCult *cult = &sim->dragon_cult;
    cult->service[species] += MinimumI64(amount, CC_SIM_MAX_MONEY - cult->service[species]);
    /* Senior service costs more; each award advances at most one member. */
    for (int32_t rank = CC_CULT_KEEPER; rank >= CC_CULT_INITIATE; --rank) {
        CcMoney cost = (CcMoney)(rank + 1) * 40;
        if (cult->ranks[species][rank] > 0 && cult->service[species] >= cost) {
            cult->service[species] -= cost;
            cult->ranks[species][rank] -= 1;
            cult->ranks[species][rank + 1] += 1;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text), "A %s rises to %s in the dragon cult through service.",
                species == CC_CULT_HUMAN ? "human" : "goblin", CcCultRankName(rank + 1));
            (void)PushEvent(sim, CC_EVENT_GOBLIN_CULT_RALLIED, sim->dragon.id,
                sim->goblins.lair_settlement_id, 0U, rank + 1, text);
            break;
        }
    }
}

static void ReconcileGoblinFactions(CcSim *sim)
{
    CcGoblinPolitics *p = &sim->goblin_politics;
    int32_t total = 0;
    for (int32_t i = 0; i < 3; ++i) total += p->factions[i].members;
    /* Existing births, hunger, defence and player choices still change the
       species total. Assign each gain to the smallest clan and each loss to
       the largest. Direct dragon hunts update both counts together. */
    while (total != sim->goblins.members) {
        bool gain = total < sim->goblins.members;
        int32_t chosen = 0;
        for (int32_t i = 1; i < 3; ++i) {
            if (gain ? p->factions[i].members < p->factions[chosen].members :
                       p->factions[i].members > p->factions[chosen].members) chosen = i;
        }
        p->factions[chosen].members += gain ? 1 : -1;
        total += gain ? 1 : -1;
    }
    const CcSettlement *home = CcSimSettlement(sim, sim->goblins.lair_settlement_id);
    for (int32_t species = 0; species < CC_CULT_SPECIES_COUNT; ++species) {
        int32_t population = species == CC_CULT_GOBLIN ? sim->goblins.members : home != NULL ? home->population : 0;
        int32_t excess = CcSimCultMembers(sim, (CcCultSpecies)species) - population;
        for (int32_t rank = 0; rank < CC_CULT_RANK_COUNT && excess > 0; ++rank) {
            int32_t lost = MinimumI32(excess, sim->dragon_cult.ranks[species][rank]);
            sim->dragon_cult.ranks[species][rank] -= lost;
            excess -= lost;
        }
    }
}

static int32_t GoblinNextRoom(const CcDungeon *dungeon, int32_t start, int32_t target)
{
    int32_t queue[CC_MAX_DUNGEON_ROOMS], previous[CC_MAX_DUNGEON_ROOMS];
    for (int32_t i = 0; i < CC_MAX_DUNGEON_ROOMS; ++i) previous[i] = -1;
    int32_t head = 0, tail = 1;
    queue[0] = start;
    previous[start] = start;
    while (head < tail && previous[target] < 0) {
        int32_t room = queue[head++];
        for (int32_t i = 0; i < dungeon->link_count; ++i) {
            const CcDungeonLink *link = &dungeon->links[i];
            if (!DungeonLinkAllowsTravel(link, room)) continue;
            int32_t next = DungeonLinkOtherRoom(link, room);
            if (next < 0 || previous[next] >= 0) continue;
            previous[next] = room;
            queue[tail++] = next;
        }
    }
    if (previous[target] < 0 || start == target) return start;
    int32_t next = target;
    while (previous[next] != start) next = previous[next];
    return next;
}

static CcMoney GoblinCargoValue(const CcGoblinFaction *f)
{
    return f->carried_coins + (CcMoney)f->carried_gold * 40 + (CcMoney)f->carried_gems * 70;
}

static void StoreGoblinRaid(CcSim *sim)
{
    CcGoblinSociety *g = &sim->goblins;
    CcGoblinFaction *f = &sim->goblin_politics.factions[sim->goblin_politics.raid_faction];
    CcMoney coins = MinimumI64(g->lair_coins, CC_SIM_MAX_MONEY - f->coins);
    int32_t gold = MinimumI32(g->lair_stock[CC_GOOD_GOLD], CC_SIM_MAX_UNITS - f->gold);
    int32_t gems = MinimumI32(g->lair_stock[CC_GOOD_GEMS], CC_SIM_MAX_UNITS - f->gems);
    f->coins += coins; g->lair_coins -= coins;
    f->gold += gold; g->lair_stock[CC_GOOD_GOLD] -= gold;
    f->gems += gems; g->lair_stock[CC_GOOD_GEMS] -= gems;
}

static void AdvanceGoblinPorter(CcSim *sim, int32_t color)
{
    CcGoblinFaction *f = &sim->goblin_politics.factions[color];
    CcDungeon *dungeon = DungeonMutable(sim, f->dungeon_id);
    if (dungeon == NULL || f->members <= 0) return;
    bool alive = !sim->dragon.slain && sim->dragon.life_stage != CC_DRAGON_STAGE_EGG;
    if (!alive && f->target_room == 19) f->target_room = f->lair_room;
    if (f->porter_room != f->target_room) {
        f->porter_room = GoblinNextRoom(dungeon, f->porter_room, f->target_room);
        return;
    }
    if (f->porter_room == 19 && GoblinCargoValue(f) > 0 && alive) {
        CcMoney coins = MinimumI64(f->carried_coins, CC_SIM_MAX_MONEY - sim->dragon.hoard);
        int32_t gold = MinimumI32(f->carried_gold, CC_SIM_MAX_UNITS - sim->dragon.hoard_goods[CC_GOOD_GOLD]);
        int32_t gems = MinimumI32(f->carried_gems, CC_SIM_MAX_UNITS - sim->dragon.hoard_goods[CC_GOOD_GEMS]);
        CcMoney value = coins + (CcMoney)gold * 40 + (CcMoney)gems * 70;
        sim->dragon.hoard += coins; sim->dragon.hoard_goods[CC_GOOD_GOLD] += gold;
        sim->dragon.hoard_goods[CC_GOOD_GEMS] += gems;
        f->carried_coins -= coins; f->carried_gold -= gold; f->carried_gems -= gems;
        if (value > 0) {
            f->tribute += MinimumI64(value, CC_SIM_MAX_MONEY - f->tribute);
            f->deliveries = MinimumI32(CC_SIM_MAX_UNITS, f->deliveries + 1);
            sim->goblins.tributes_delivered = MinimumI32(CC_SIM_MAX_UNITS, sim->goblins.tributes_delivered + 1);
            sim->dragon_cult.devotion = MinimumI32(100, sim->dragon_cult.devotion + 1);
            CultService(sim, CC_CULT_GOBLIN, value);
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text), "%s porters deliver %" PRId64 " crowns of treasure through the Underroad to %s.",
                CcGoblinColorName(color), value, sim->dragon.name);
            CcEvent *event = PushEvent(sim, CC_EVENT_GOBLIN_TRIBUTE_DELIVERED, sim->goblins.id,
                sim->dragon.lair_settlement_id, f->journey_event_id, (int32_t)value, text);
            sim->dragon.hoard_event_id = event->id;
        }
        f->target_room = f->lair_room;
        return;
    }
    if (f->porter_room == f->lair_room) {
        CcMoney coins = MinimumI64(f->carried_coins, CC_SIM_MAX_MONEY - f->coins);
        int32_t gold = MinimumI32(f->carried_gold, CC_SIM_MAX_UNITS - f->gold);
        int32_t gems = MinimumI32(f->carried_gems, CC_SIM_MAX_UNITS - f->gems);
        f->coins += coins; f->carried_coins -= coins;
        f->gold += gold; f->carried_gold -= gold;
        f->gems += gems; f->carried_gems -= gems;
        if (alive && (f->coins > 0 || f->gold > 0 || f->gems > 0)) {
            int32_t capacity = MaximumI32(1, f->members / 4);
            coins = MinimumI64(f->coins, capacity * 10);
            gold = MinimumI32(f->gold, capacity);
            gems = MinimumI32(f->gems, MaximumI32(0, capacity - gold));
            f->coins -= coins; f->carried_coins += coins;
            f->gold -= gold; f->carried_gold += gold;
            f->gems -= gems; f->carried_gems += gems;
            f->target_room = 19;
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text), "%s porters take treasure from their lair towards %s, hoping to win favour.",
                CcGoblinColorName(color), sim->dragon.name);
            CcEvent *event = PushEvent(sim, CC_EVENT_GOBLIN_TRIBUTE_DEPARTED, sim->goblins.id,
                sim->dragon.lair_settlement_id, sim->goblins.tribute_event_id, (int32_t)GoblinCargoValue(f), text);
            f->journey_event_id = event->id;
            return;
        }
        /* Choose reachable treasure; each item leaves the shared room. */
        for (int32_t n = 0; n < dungeon->room_count; ++n) {
            int32_t r = (n + color * 7) % dungeon->room_count;
            CcDungeonRoom *room = &dungeon->rooms[r];
            if ((room->loot_good == CC_GOOD_GOLD || room->loot_good == CC_GOOD_GEMS) &&
                room->loot_quantity > 0 && r != f->lair_room &&
                GoblinNextRoom(dungeon, f->porter_room, r) != f->porter_room) {
                f->target_room = r;
                return;
            }
        }
    } else {
        CcDungeonRoom *room = &dungeon->rooms[f->porter_room];
        int32_t taken = MinimumI32(room->loot_quantity, MaximumI32(1, f->members / 4));
        if (room->loot_good == CC_GOOD_GOLD) f->carried_gold += taken;
        else if (room->loot_good == CC_GOOD_GEMS) f->carried_gems += taken;
        else taken = 0;
        room->loot_quantity -= taken;
        f->target_room = f->lair_room;
    }
}

static void AdvanceGoblinPolitics(CcSim *sim)
{
    if (sim->schema_version < 75U) return;
    CcGoblinPolitics *p = &sim->goblin_politics;
    ReconcileGoblinFactions(sim);
    if (sim->dragon.slain || p->dragon_id != sim->dragon.id) {
        p->dragon_id = sim->dragon.id;
        p->contest_started_day = sim->current_day;
        p->crown_faction = -1;
        for (int32_t i = 0; i < 3; ++i) p->factions[i].tribute = 0;
    }
    /* A dragon has one founding tribute year. Ties extend the contest until
       a delivery creates a clear winner. Its colour lasts for its life. */
    if (!sim->dragon.slain && p->crown_faction < 0 &&
        sim->current_day - p->contest_started_day >= 365) {
        int32_t winner = 0;
        for (int32_t i = 1; i < 3; ++i)
            if (p->factions[i].tribute > p->factions[winner].tribute) winner = i;
        bool unique = p->factions[winner].tribute > 0;
        for (int32_t i = 0; i < 3; ++i)
            if (i != winner && p->factions[i].tribute == p->factions[winner].tribute) unique = false;
        if (unique) {
            p->crown_faction = winner;
            static const CcDragonHairColor colors[] = {
                CC_DRAGON_HAIR_RED, CC_DRAGON_HAIR_PURPLE, CC_DRAGON_HAIR_BLUE
            };
            sim->dragon.hair_color = colors[winner];
            char text[CC_EVENT_TEXT_CAPACITY];
            (void)snprintf(text, sizeof(text), "%s leads the tribute and %s bears a %s crown. All three factions hope their next offering will win favour.",
                CcGoblinColorName(winner), sim->dragon.name, CcGoblinColorName(winner));
            (void)PushEvent(sim, CC_EVENT_DRAGON_CROWNED, sim->dragon.id,
                sim->dragon.lair_settlement_id, sim->dragon.hoard_event_id, winner, text);
        }
    }
    /* Rotate first access each day so shared treasure has fair contention. */
    for (int32_t i = 0; i < 3; ++i) AdvanceGoblinPorter(sim, (sim->current_day + i) % 3);
    if (sim->current_day % 28 == 0) {
        CcDragonCult *cult = &sim->dragon_cult;
        int32_t humans = CcSimCultMembers(sim, CC_CULT_HUMAN);
        const CcSettlement *home = CcSimSettlement(sim, sim->goblins.lair_settlement_id);
        if (cult->devotion >= 50 && home != NULL && !CcSettlementIsAbandoned(home)) {
            if (humans < MinimumI32(60, home->population)) cult->ranks[CC_CULT_HUMAN][0] += 1;
            if (CcSimCultMembers(sim, CC_CULT_GOBLIN) < sim->goblins.members / 2)
                cult->ranks[CC_CULT_GOBLIN][0] += 1;
        }
        CcSettlement *town = CcSimSettlementMutable(sim, sim->goblins.lair_settlement_id);
        if (town != NULL && !CcSettlementIsAbandoned(town) && humans > 0) {
            CcMoney gift = MinimumI64(town->market_coins, MaximumI32(1, humans / 4));
            CcMoney *chest = sim->dragon.slain ? &cult->offering_coins : &sim->dragon.hoard;
            gift = MinimumI64(gift, CC_SIM_MAX_MONEY - *chest);
            town->market_coins -= gift;
            *chest += gift;
            CultService(sim, CC_CULT_HUMAN, gift);
        }
    }
}

static bool HuntGoblinFaction(CcSim *sim)
{
    if (sim->schema_version < 75U || sim->goblin_politics.crown_faction < 0 ||
        sim->goblin_politics.dragon_id != sim->dragon.id) return false;
    CcGoblinPolitics *p = &sim->goblin_politics;
    ReconcileGoblinFactions(sim);
    for (int32_t i = 0; i < 3; ++i) {
        int32_t color = (p->next_hunt_faction + i) % 3;
        CcGoblinFaction *f = &p->factions[color];
        if (color == p->crown_faction || f->members <= 0) continue;
        int32_t killed = MinimumI32(f->members, MinimumI32(sim->goblins.members - 1, MaximumI32(1, sim->goblins.members / 24)));
        if (killed <= 0) continue;
        f->members -= killed;
        sim->goblins.members -= killed;
        f->hunted = MinimumI32(CC_SIM_MAX_UNITS, f->hunted + killed);
        p->next_hunt_faction = (color + 1) % 3;
        sim->dragon.body_condition = MinimumI32(100, sim->dragon.body_condition + killed * 5);
        sim->dragon.hunt_cooldown_days = 42;
        sim->dragon.hunts += 1;
        sim->dragon.activity = CC_DRAGON_ACTIVITY_HUNTING;
        char text[CC_EVENT_TEXT_CAPACITY];
        (void)snprintf(text, sizeof(text), "%s hunts %d %s goblins. Their porters hope more treasure will earn the %s crown's favour.",
            sim->dragon.name, killed, CcGoblinColorName(color), CcGoblinColorName(p->crown_faction));
        CcEvent *event = PushEvent(sim, CC_EVENT_DRAGON_HUNT, sim->dragon.id,
            sim->goblins.lair_settlement_id, sim->dragon.hoard_event_id, killed, text);
        sim->dragon.lifecycle_event_id = event->id;
        ReconcileGoblinFactions(sim);
        return true;
    }
    return false;
}

static bool ValidateGoblinPolitics(const CcSim *sim)
{
    const CcGoblinPolitics *p = &sim->goblin_politics;
    if (CcIdKind(p->dragon_id) != CC_ENTITY_DRAGON ||
        p->contest_started_day < 1 || p->contest_started_day > sim->current_day ||
        p->crown_faction < -1 || p->crown_faction >= CC_GOBLIN_FACTION_COUNT ||
        p->raid_faction < 0 || p->raid_faction >= CC_GOBLIN_FACTION_COUNT ||
        p->next_hunt_faction < 0 || p->next_hunt_faction >= CC_GOBLIN_FACTION_COUNT) return false;
    if (sim->dragon_cult.offering_coins < 0 || sim->dragon_cult.offering_coins > CC_SIM_MAX_MONEY) return false;
    for (int32_t good = 0; good < CC_GOOD_COUNT; ++good)
        if (sim->dragon_cult.offering_stock[good] < 0 || sim->dragon_cult.offering_stock[good] > CC_SIM_MAX_UNITS) return false;
    int32_t total = 0;
    for (int32_t i = 0; i < CC_GOBLIN_FACTION_COUNT; ++i) {
        const CcGoblinFaction *f = &p->factions[i];
        const CcDungeon *d = CcSimDungeon(sim, f->dungeon_id);
        if (d == NULL || d->room_count <= 19 || f->members < 0 || f->members > 120 ||
            f->lair_room < 0 || f->lair_room >= d->room_count || f->lair_room == 19 ||
            f->porter_room < 0 || f->porter_room >= d->room_count ||
            f->target_room < 0 || f->target_room >= d->room_count ||
            f->coins < 0 || f->coins > CC_SIM_MAX_MONEY ||
            f->carried_coins < 0 || f->carried_coins > CC_SIM_MAX_MONEY ||
            f->gold < 0 || f->gold > CC_SIM_MAX_UNITS ||
            f->gems < 0 || f->gems > CC_SIM_MAX_UNITS ||
            f->carried_gold < 0 || f->carried_gold > CC_SIM_MAX_UNITS ||
            f->carried_gems < 0 || f->carried_gems > CC_SIM_MAX_UNITS ||
            f->tribute < 0 || f->tribute > CC_SIM_MAX_MONEY ||
            f->deliveries < 0 || f->deliveries > CC_SIM_MAX_UNITS ||
            f->hunted < 0 || f->hunted > CC_SIM_MAX_UNITS ||
            (f->journey_event_id != 0U && (CcIdKind(f->journey_event_id) != CC_ENTITY_EVENT ||
             (f->journey_event_id & CC_ID_SERIAL_MASK) >= sim->next_entity_serial))) return false;
        total += f->members;
    }
    if (total != sim->goblins.members) return false;
    const CcSettlement *home = CcSimSettlement(sim, sim->goblins.lair_settlement_id);
    for (int32_t species = 0; species < CC_CULT_SPECIES_COUNT; ++species) {
        for (int32_t rank = 0; rank < CC_CULT_RANK_COUNT; ++rank) {
            if (sim->dragon_cult.ranks[species][rank] < 0 ||
                sim->dragon_cult.ranks[species][rank] > 120) return false;
        }
        int32_t members = CcSimCultMembers(sim, (CcCultSpecies)species);
        int32_t population = species == CC_CULT_GOBLIN ? sim->goblins.members : home != NULL ? home->population : 0;
        if (members > population || members > 120 || sim->dragon_cult.service[species] < 0 ||
            sim->dragon_cult.service[species] > CC_SIM_MAX_MONEY) return false;
    }
    return true;
}
