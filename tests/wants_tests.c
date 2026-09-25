#include "persistence/cc_save.h"
#include "sim/cc_wants.h"
#include "test_support.h"

#include <sqlite3.h>
#include <string.h>

/* Procedural personal item quests (schema 119), design: docs/personal-requests.md.
   These tests drive src/sim/cc_wants.c and cc_wants_codec.c directly and through
   the public command dispatch, without depending on the shape the random world
   generator happens to produce today. Every scenario first isolates the whole
   living cast (everyone travelling, so the daily scan skips them) and then
   promotes only the specific people the scenario needs. */

static CcSim sim, loaded;
static char error[256];

static void IsolateWorld(void)
{
    for (int i = 0; i < sim.character_count; ++i) {
        sim.characters[i].activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    }
}

/* Unlock request generation (wants.initialized becomes 2) while keeping the
   world otherwise empty: the only present character has no occupation and
   their town has no spare tools, so the one-time belonging seeding finds
   nobody eligible. */
static void Prime(void)
{
    IsolateWorld();
    CcCharacter *bystander = &sim.characters[0];
    bystander->activity = CC_CHARACTER_ACTIVITY_WORKING;
    bystander->travel_destination_id = 0;
    bystander->bandit_group_id = 0;
    bystander->hungry_days = 0;
    bystander->occupation = CC_OCCUPATION_NONE;
    sim.player.location_id = bystander->current_settlement_id;
    CcSettlement *town = CcSimSettlementMutable(&sim, bystander->current_settlement_id);
    town->stock[CC_GOOD_TOOLS] = 0;
    CcCommand discover = {.kind = CC_COMMAND_PERSONAL_WANT,
        .target_id = bystander->id, .amount = CC_WANT_DISCOVER};
    CC_CHECK(CcSimApply(&sim, &discover, error, sizeof(error)));
    bystander->activity = CC_CHARACTER_ACTIVITY_TRAVELLING;
    CC_CHECK(sim.wants.initialized == 2);
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) CC_CHECK(sim.wants.wants[i].id == 0);
    for (int i = 0; i < CC_BELONGINGS; ++i) CC_CHECK(sim.wants.items[i].id == 0);
}

/* Force the first living, unclaimed adult found at a settlement into a known
   working state, optionally with a chosen occupation. */
static CcCharacter *AdultAt(CcId settlement_id, CcCharacterOccupation occupation)
{
    for (int i = 0; i < sim.character_count; ++i) {
        CcCharacter *p = &sim.characters[i];
        if (p->death_day > sim.current_day && p->current_settlement_id == settlement_id &&
            p->activity == CC_CHARACTER_ACTIVITY_TRAVELLING && p->bandit_group_id == 0 &&
            CcCharacterAgeYears(&sim, p) >= 16) {
            p->activity = CC_CHARACTER_ACTIVITY_WORKING;
            p->travel_destination_id = 0;
            p->hungry_days = 0;
            p->player_disposition = 50;
            p->stress = 20;
            if (occupation != CC_OCCUPATION_NONE) p->occupation = occupation;
            return p;
        }
    }
    return NULL;
}

static CcId MakeId(CcEntityKind kind)
{
    CcId id = ((uint64_t)kind << 56U) | sim.next_entity_serial;
    if (sim.next_entity_serial < UINT64_MAX) ++sim.next_entity_serial;
    return id;
}

static CcBelonging *MakeBelonging(const CcCharacter *owner, const char *name,
    CcCustodyHolder holder, int32_t condition)
{
    CcBelonging *item = NULL;
    for (int i = 0; i < CC_BELONGINGS && item == NULL; ++i)
        if (sim.wants.items[i].id == 0) item = &sim.wants.items[i];
    CcCustodyEntry *entry = NULL;
    for (int i = 0; i < CC_CUSTODY_CAPACITY && entry == NULL; ++i)
        if (sim.custody.entries[i].id == 0) entry = &sim.custody.entries[i];
    CC_CHECK(item != NULL && entry != NULL);
    CcId item_id = MakeId(CC_ENTITY_BELONGING);
    *item = (CcBelonging){.id = item_id, .custody_id = sim.custody.next_id++,
        .home_id = owner->current_settlement_id,
        .last_place_id = holder.kind == CC_CUSTODY_STORE ? holder.id : owner->current_settlement_id,
        .last_wear_day = sim.current_day};
    (void)snprintf(item->name, sizeof(item->name), "%s", name);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text, sizeof(text), "%s is kept for testing.", item->name);
    CcId event_id = CcSimPushEvent(&sim, CC_EVENT_BELONGING_MOVED, owner->id,
        item->last_place_id, 0, condition, text)->id;
    item->cause_event_id = event_id;
    *entry = (CcCustodyEntry){.id = item->custody_id, .revision = 1, .owner_id = owner->id,
        .last_event_id = event_id, .holder = holder, .kind = CC_CUSTODY_BELONGING,
        .reference_id = item_id, .quantity = 1, .condition = condition, .active = true};
    return item;
}

static const CcPersonalWant *FindRoot(CcId person, CcWantKind kind)
{
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim.wants.wants[i];
        if (w->person_id == person && w->kind == (int32_t)kind && w->parent_id == 0) return w;
    }
    return NULL;
}

/* Same as FindRoot, but only an ACTIVE root: a person can have an old
   settled or closed root still on the books while a new one is active. */
static const CcPersonalWant *FindActiveRoot(CcId person, CcWantKind kind)
{
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim.wants.wants[i];
        if (w->person_id == person && w->kind == (int32_t)kind && w->parent_id == 0 &&
            w->status == CC_WANT_ACTIVE) return w;
    }
    return NULL;
}

static const CcPersonalWant *FindChild(CcId parent)
{
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        const CcPersonalWant *w = &sim.wants.wants[i];
        if (w->parent_id == parent) return w;
    }
    return NULL;
}

static CcCommand WantCommand(CcId target, int32_t amount)
{
    const CcPersonalWant *w = CcWantsFind(&sim, target);
    CC_CHECK(w != NULL);
    return (CcCommand){.kind = CC_COMMAND_PERSONAL_WANT, .target_id = target,
        .secondary_id = (uint64_t)w->revision, .amount = amount};
}

static CcCommand ItemCommand(CcId item_id, int32_t amount)
{
    const CcBelonging *item = CcWantsItem(&sim, item_id);
    CC_CHECK(item != NULL);
    const CcCustodyEntry *e = CcCustodyFind(&sim.custody, item->custody_id);
    CC_CHECK(e != NULL);
    return (CcCommand){.kind = CC_COMMAND_PERSONAL_WANT, .target_id = item_id,
        .secondary_id = e->revision, .amount = amount};
}

/* Two campaigns started from the same seed, driven through the same direct
   calls, must reach byte-identical wants state -- including the belonging
   placement, which is derived from the world seed rather than the shared
   random stream. */
static void TestDeterministicGeneration(void)
{
    static CcSim a, b;
    CcSimInit(&a, 90210U);
    CcSimInit(&b, 90210U);
    /* CcRoyalCarriage carries unused trailing padding after its final bool
       field, so a raw memcmp of two independently generated campaigns is not
       meaningful (the game itself never compares full campaigns this way);
       CcSimHash is the canonical equality check and ignores it. */
    CC_CHECK(CcSimHash(&a) == CcSimHash(&b));
    for (int day = 0; day < 4; ++day) {
        CcWantsAdvance(&a); CcWantsAdvance(&b);
        ++a.current_day; ++b.current_day;
    }
    CcCommand discover = {.kind = CC_COMMAND_PERSONAL_WANT,
        .target_id = a.characters[0].id, .amount = CC_WANT_DISCOVER};
    CC_CHECK(a.characters[0].id == b.characters[0].id);
    a.player.location_id = a.characters[0].current_settlement_id;
    b.player.location_id = b.characters[0].current_settlement_id;
    CC_CHECK(CcSimApply(&a, &discover, error, sizeof(error)));
    CC_CHECK(CcSimApply(&b, &discover, error, sizeof(error)));
    CC_CHECK(memcmp(&a.wants, &b.wants, sizeof(a.wants)) == 0);
    CC_CHECK(CcWantsHash(&a.wants) == CcWantsHash(&b.wants));
    for (int day = 0; day < 6; ++day) {
        ++a.current_day; ++b.current_day;
        CcWantsAdvance(&a); CcWantsAdvance(&b);
        CC_CHECK(memcmp(&a.wants, &b.wants, sizeof(a.wants)) == 0);
    }
}

/* A meal request only appears once bread is reachable, and names the town it
   came from. Closing every road hides the source; reopening one reveals it. */
static void TestMealFromReachableSourcesOnly(void)
{
    CcSimInit(&sim, 4242U);
    Prime();
    CcCharacter *hungry = AdultAt(sim.settlements[0].id, CC_OCCUPATION_NONE);
    CC_CHECK(hungry != NULL);
    hungry->hungry_days = 3;
    hungry->travel_coins = 40;
    for (int i = 0; i < sim.settlement_count; ++i) sim.settlements[i].stock[CC_GOOD_BREAD] = 0;
    sim.settlements[5].stock[CC_GOOD_BREAD] = 20;
    for (int i = 0; i < sim.route_count; ++i) sim.routes[i].closed = true;
    CcWantsAdvance(&sim);
    CC_CHECK(FindRoot(hungry->id, CC_WANT_MEAL) == NULL);
    CC_CHECK(CcWantsValidate(&sim));
    for (int i = 0; i < sim.route_count; ++i) sim.routes[i].closed = false;
    CcWantsAdvance(&sim);
    const CcPersonalWant *meal = FindRoot(hungry->id, CC_WANT_MEAL);
    CC_CHECK(meal != NULL && meal->source_place_id == sim.settlements[5].id &&
        meal->good == CC_GOOD_BREAD && meal->quantity == 1 && meal->escrow == 2);
    CC_CHECK(CcWantsValidate(&sim));
    /* The book and text-client surfaces should describe this request. */
    sim.player.location_id = hungry->current_settlement_id;
    CcWantOffer offers[8];
    int32_t count = CcWantsOffers(&sim, hungry->id, offers, 8);
    CC_CHECK(count == 1 && offers[0].ready && offers[0].command.amount == CC_WANT_LEARN);
    char text[256];
    CC_CHECK(CcWantsRequestText(&sim, meal->id, text, sizeof(text)) && text[0] != '\0');
    CC_CHECK(CcWantsPersonText(&sim, hungry->id, text, sizeof(text)) && text[0] != '\0');
    CC_CHECK(CcSimApply(&sim, &offers[0].command, error, sizeof(error)));
    CcWantsDescribe(&sim, text, sizeof(text));
    CC_CHECK(strstr(text, hungry->name) != NULL);
}

/* Work supplies are requested when the town runs low, and settle by
   themselves once the town restocks -- the "requests change with the world
   each day" behaviour -- without the player ever giving anything. */
static void TestWorkSupplyGenerationAndDailySettlement(void)
{
    CcSimInit(&sim, 99U);
    Prime();
    CcCharacter *baker = AdultAt(sim.settlements[0].id, CC_OCCUPATION_BAKER);
    CC_CHECK(baker != NULL);
    baker->travel_coins = 20;
    CcSettlement *home = CcSimSettlementMutable(&sim, baker->current_settlement_id);
    home->stock[CC_GOOD_WHEAT] = 0;
    for (int i = 0; i < sim.settlement_count; ++i) sim.settlements[i].stock[CC_GOOD_WHEAT] = 0;
    sim.settlements[2].stock[CC_GOOD_WHEAT] = 50;
    CcWantsAdvance(&sim);
    const CcPersonalWant *supply = FindRoot(baker->id, CC_WANT_WORK_SUPPLIES);
    CC_CHECK(supply != NULL && supply->good == CC_GOOD_WHEAT && supply->quantity == 2 &&
        supply->source_place_id == sim.settlements[2].id && supply->status == CC_WANT_ACTIVE);
    CcId want_id = supply->id;
    int32_t created_day = supply->created_day;
    /* Restocking the baker's own town settles the request without a handover. */
    ++sim.current_day;
    home->stock[CC_GOOD_WHEAT] = 5;
    CcWantsAdvance(&sim);
    const CcPersonalWant *settled = CcWantsFind(&sim, want_id);
    CC_CHECK(settled != NULL && settled->status == CC_WANT_SETTLED &&
        settled->settled_day == sim.current_day && settled->escrow == 0);
    CC_CHECK(baker->travel_coins == 20);
    CC_CHECK(created_day < settled->settled_day);
    CC_CHECK(CcWantsValidate(&sim));
    /* A second request for the same worker is refused until the cooldown
       passes, even though the town is short again. */
    home->stock[CC_GOOD_WHEAT] = 0;
    CcWantsAdvance(&sim);
    CC_CHECK(FindRoot(baker->id, CC_WANT_WORK_SUPPLIES) == NULL ||
        FindRoot(baker->id, CC_WANT_WORK_SUPPLIES)->id == want_id);
}

/* A belonging left in another town's stores is a recoverable request. The
   player fetches it and returns it to its owner; trust and memory record the
   completion. */
static void TestRecoveryCustodyToPlayerAndBack(void)
{
    CcSimInit(&sim, 555U);
    Prime();
    CcCharacter *owner = AdultAt(sim.settlements[0].id, CC_OCCUPATION_FARMER);
    CC_CHECK(owner != NULL);
    owner->travel_coins = 30;
    CcBelonging *item = MakeBelonging(owner, "Wren's knife",
        (CcCustodyHolder){CC_CUSTODY_STORE, sim.settlements[2].id}, 100);
    CcWantsAdvance(&sim);
    const CcPersonalWant *recover = FindRoot(owner->id, CC_WANT_RECOVER);
    CC_CHECK(recover != NULL && recover->item_id == item->id &&
        recover->source_place_id == sim.settlements[2].id && recover->escrow == 6);
    CcId want_id = recover->id;
    sim.player.location_id = owner->current_settlement_id;
    CcCommand learn = WantCommand(want_id, CC_WANT_LEARN);
    CC_CHECK(CcSimApply(&sim, &learn, error, sizeof(error)));
    /* The item is not with the owner: it must be collected at its store. */
    sim.player.location_id = sim.settlements[2].id;
    CcCommand take = ItemCommand(item->id, CC_WANT_TAKE);
    CC_CHECK(CcSimApply(&sim, &take, error, sizeof(error)));
    const CcCustodyEntry *held = CcCustodyFind(&sim.custody, item->custody_id);
    CC_CHECK(held->holder.kind == CC_CUSTODY_PLAYER);
    sim.player.location_id = owner->current_settlement_id;
    CcMoney coins_before = sim.player.coins;
    int32_t disposition_before = owner->player_disposition;
    CcCommand give = WantCommand(want_id, CC_WANT_GIVE);
    CC_CHECK(CcSimApply(&sim, &give, error, sizeof(error)));
    const CcPersonalWant *done = CcWantsFind(&sim, want_id);
    CC_CHECK(done->status == CC_WANT_FULFILLED && done->escrow == 0);
    CC_CHECK(sim.player.coins == coins_before + 6);
    CC_CHECK(owner->player_disposition == disposition_before + 6);
    CC_CHECK(CcCharacterRemembers(owner, CC_CHARACTER_MEMORY_PLAYER_HELPED, owner->current_settlement_id));
    const CcCustodyEntry *back = CcCustodyFind(&sim.custody, item->custody_id);
    CC_CHECK(back->holder.kind == CC_CUSTODY_CHARACTER && back->holder.id == owner->id);
    CC_CHECK(CcWantsValidate(&sim));
}

/* A worn belonging generates a repair request for its owner and a linked
   iron request for a reachable smith. Repair only becomes possible once a
   smith and reachable iron exist; the player then carries the tool to the
   smith, pays the iron, and returns the mended tool to its owner. */
static void TestRepairSmithIronChain(void)
{
    CcSimInit(&sim, 7777U);
    Prime();
    CcCharacter *owner = AdultAt(sim.settlements[0].id, CC_OCCUPATION_WOODCUTTER);
    CcCharacter *smith = AdultAt(sim.settlements[1].id, CC_OCCUPATION_SMITH);
    CC_CHECK(owner != NULL && smith != NULL);
    owner->travel_coins = 40;
    CcSettlement *smith_town = CcSimSettlementMutable(&sim, smith->current_settlement_id);
    smith_town->service_mask |= (UINT32_C(1) << CC_SERVICE_SMITHY);
    smith_town->stock[CC_GOOD_TOOLS] = 0;
    smith_town->stock[CC_GOOD_IRON] = 0;
    CcBelonging *item = MakeBelonging(owner, "Aldric's axe",
        (CcCustodyHolder){CC_CUSTODY_CHARACTER, owner->id}, 45);
    CcWantsAdvance(&sim);
    /* No repair yet: the smith's town has no tools stocked and no iron to sell. */
    CC_CHECK(FindRoot(owner->id, CC_WANT_REPAIR) == NULL);
    smith_town->stock[CC_GOOD_TOOLS] = 3;
    smith_town->stock[CC_GOOD_IRON] = 5;
    CcWantsAdvance(&sim);
    const CcPersonalWant *root = FindRoot(owner->id, CC_WANT_REPAIR);
    CC_CHECK(root != NULL && root->item_id == item->id && root->escrow == 6);
    const CcPersonalWant *child = FindChild(root->id);
    CC_CHECK(child != NULL && child->kind == CC_WANT_REPAIR_IRON && child->person_id == smith->id &&
        child->good == CC_GOOD_IRON && child->quantity == 1 && child->escrow == 0);
    CcId root_id = root->id, child_id = child->id;
    CcMoney gold = CcSimTrackedGold(&sim);
    /* Learning the repair also records the smith's linked request. */
    sim.player.location_id = owner->current_settlement_id;
    CcCommand learn = WantCommand(root_id, CC_WANT_LEARN);
    CC_CHECK(CcSimApply(&sim, &learn, error, sizeof(error)));
    CC_CHECK(CcWantsFind(&sim, child_id)->known_day == sim.current_day);
    char text[256];
    CC_CHECK(CcWantsRequestText(&sim, root_id, text, sizeof(text)) && strstr(text, smith->name) != NULL);
    /* Borrow the worn tool directly from its owner. */
    CcCommand take = ItemCommand(item->id, CC_WANT_TAKE);
    CC_CHECK(CcSimApply(&sim, &take, error, sizeof(error)));
    CC_CHECK(CcCustodyFind(&sim.custody, item->custody_id)->holder.kind == CC_CUSTODY_PLAYER);
    /* Travel to the smith with iron in the carriage. */
    sim.player.location_id = smith->current_settlement_id;
    sim.player.cargo[CC_GOOD_IRON] = 1;
    int32_t iron_total = CcSimTrackedGood(&sim, CC_GOOD_IRON);
    CcCommand pay_iron = WantCommand(child_id, CC_WANT_GIVE);
    CC_CHECK(CcSimApply(&sim, &pay_iron, error, sizeof(error)));
    const CcCustodyEntry *repaired = CcCustodyFind(&sim.custody, item->custody_id);
    CC_CHECK(repaired->condition == 100 && sim.player.cargo[CC_GOOD_IRON] == 0);
    CC_CHECK(CcWantsItem(&sim, item->id)->repair_iron == 1);
    CC_CHECK(CcWantsFind(&sim, child_id)->status == CC_WANT_FULFILLED);
    CC_CHECK(CcCharacterRemembers(smith, CC_CHARACTER_MEMORY_PLAYER_HELPED, smith->current_settlement_id));
    /* Iron leaves the carriage but stays tracked inside the mended tool. */
    CC_CHECK(CcSimTrackedGood(&sim, CC_GOOD_IRON) == iron_total);
    /* Return the mended tool to its owner. */
    sim.player.location_id = owner->current_settlement_id;
    CcCommand give_back = WantCommand(root_id, CC_WANT_GIVE);
    CC_CHECK(CcSimApply(&sim, &give_back, error, sizeof(error)));
    CC_CHECK(CcWantsFind(&sim, root_id)->status == CC_WANT_FULFILLED);
    CC_CHECK(CcCustodyFind(&sim.custody, item->custody_id)->holder.kind == CC_CUSTODY_CHARACTER);
    CC_CHECK(CcCharacterRemembers(owner, CC_CHARACTER_MEMORY_PLAYER_HELPED, owner->current_settlement_id));
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcWantsValidate(&sim));
}

/* When a recipient dies before a plain request is settled, it closes and its
   reserved reward returns to the town rather than being lost or paid out --
   and no help is remembered, since none was given. When a smith dies before
   a repair, the repair itself closes too. */
static void TestRequestEndsOnDeathAndSmithDeath(void)
{
    CcSimInit(&sim, 31415U);
    Prime();
    CcCharacter *hungry = AdultAt(sim.settlements[0].id, CC_OCCUPATION_NONE);
    CC_CHECK(hungry != NULL);
    hungry->hungry_days = 2;
    hungry->travel_coins = 10;
    sim.settlements[1].stock[CC_GOOD_BREAD] = 10;
    CcWantsAdvance(&sim);
    const CcPersonalWant *meal = FindRoot(hungry->id, CC_WANT_MEAL);
    CC_CHECK(meal != NULL && meal->escrow == 2);
    CcId meal_id = meal->id;
    CcSettlement *home = CcSimSettlementMutable(&sim, hungry->current_settlement_id);
    CcMoney town_coins_before = home->market_coins;
    CcMoney gold = CcSimTrackedGold(&sim);
    hungry->death_day = sim.current_day;
    ++sim.current_day;
    CcWantsAdvance(&sim);
    const CcPersonalWant *closed = CcWantsFind(&sim, meal_id);
    CC_CHECK(closed != NULL && closed->status == CC_WANT_CLOSED && closed->escrow == 0);
    CC_CHECK(home->market_coins == town_coins_before + 2);
    CC_CHECK(CcSimTrackedGold(&sim) == gold);
    CC_CHECK(CcWantsValidate(&sim));

    CcSimInit(&sim, 271828U);
    Prime();
    CcCharacter *owner = AdultAt(sim.settlements[0].id, CC_OCCUPATION_QUARRYMAN);
    CcCharacter *smith = AdultAt(sim.settlements[1].id, CC_OCCUPATION_SMITH);
    CC_CHECK(owner != NULL && smith != NULL);
    owner->travel_coins = 20;
    CcSettlement *smith_town = CcSimSettlementMutable(&sim, smith->current_settlement_id);
    smith_town->service_mask |= (UINT32_C(1) << CC_SERVICE_SMITHY);
    smith_town->stock[CC_GOOD_TOOLS] = 3;
    smith_town->stock[CC_GOOD_IRON] = 5;
    CcBelonging *item = MakeBelonging(owner, "Petra's pick",
        (CcCustodyHolder){CC_CUSTODY_CHARACTER, owner->id}, 20);
    CcWantsAdvance(&sim);
    const CcPersonalWant *root = FindRoot(owner->id, CC_WANT_REPAIR);
    const CcPersonalWant *child = root != NULL ? FindChild(root->id) : NULL;
    CC_CHECK(root != NULL && child != NULL);
    CcId root_id = root->id, child_id = child->id;
    smith->death_day = sim.current_day;
    ++sim.current_day;
    CcWantsAdvance(&sim);
    CC_CHECK(CcWantsFind(&sim, child_id)->status == CC_WANT_CLOSED);
    CC_CHECK(CcWantsFind(&sim, root_id)->status == CC_WANT_CLOSED);
    CC_CHECK(CcCustodyFind(&sim.custody, item->custody_id)->condition == 20);
    CC_CHECK(CcWantsValidate(&sim));
}

/* The reserved reward never exceeds -- and never overdraws -- the giver's
   purse: a person with less than the usual reward only reserves what they
   have, and their purse never goes negative. */
static void TestRewardEscrowCappedByPurse(void)
{
    CcSimInit(&sim, 606U);
    Prime();
    CcCharacter *hungry = AdultAt(sim.settlements[0].id, CC_OCCUPATION_NONE);
    CC_CHECK(hungry != NULL);
    hungry->hungry_days = 1;
    hungry->travel_coins = 1;
    sim.settlements[1].stock[CC_GOOD_BREAD] = 10;
    CcWantsAdvance(&sim);
    const CcPersonalWant *meal = FindRoot(hungry->id, CC_WANT_MEAL);
    CC_CHECK(meal != NULL && meal->escrow == 1);
    CC_CHECK(hungry->travel_coins == 0);
    sim.player.location_id = hungry->current_settlement_id;
    CcCommand learn = WantCommand(meal->id, CC_WANT_LEARN);
    CC_CHECK(CcSimApply(&sim, &learn, error, sizeof(error)));
    sim.player.cargo[CC_GOOD_BREAD] = 1;
    CcMoney coins_before = sim.player.coins;
    CcCommand give = WantCommand(meal->id, CC_WANT_GIVE);
    CC_CHECK(CcSimApply(&sim, &give, error, sizeof(error)));
    CC_CHECK(sim.player.coins == coins_before + 1);
    CC_CHECK(hungry->travel_coins == 0);
    CC_CHECK(CcWantsValidate(&sim));
}

/* The satchel holds four named belongings at once; a fifth must wait until
   one is left in town storage, which is itself trackable through custody. */
static void TestSatchelFourItemLimitAndTownStorage(void)
{
    CcSimInit(&sim, 808U);
    Prime();
    CcCharacter *owner = AdultAt(sim.settlements[0].id, CC_OCCUPATION_FARMER);
    CC_CHECK(owner != NULL);
    sim.player.location_id = sim.settlements[1].id;
    CcBelonging *items[5];
    char name[16];
    for (int i = 0; i < 5; ++i) {
        (void)snprintf(name, sizeof(name), "Trinket %d", i);
        items[i] = MakeBelonging(owner, name,
            (CcCustodyHolder){CC_CUSTODY_STORE, sim.settlements[1].id}, 100);
    }
    for (int i = 0; i < 4; ++i) {
        CcCommand take = ItemCommand(items[i]->id, CC_WANT_TAKE);
        CC_CHECK(CcSimApply(&sim, &take, error, sizeof(error)));
        CC_CHECK(CcCustodyFind(&sim.custody, items[i]->custody_id)->holder.kind == CC_CUSTODY_PLAYER);
    }
    CcCommand blocked = ItemCommand(items[4]->id, CC_WANT_TAKE);
    CC_CHECK(!CcSimApply(&sim, &blocked, error, sizeof(error)));
    CC_CHECK(CcCustodyFind(&sim.custody, items[4]->custody_id)->holder.kind == CC_CUSTODY_STORE);
    /* Leaving one carried belonging in the current town's stores makes room. */
    CcCommand leave = ItemCommand(items[0]->id, CC_WANT_LEAVE);
    CC_CHECK(CcSimApply(&sim, &leave, error, sizeof(error)));
    const CcCustodyEntry *left = CcCustodyFind(&sim.custody, items[0]->custody_id);
    CC_CHECK(left->holder.kind == CC_CUSTODY_STORE && left->holder.id == sim.player.location_id);
    CcCommand take_fifth = ItemCommand(items[4]->id, CC_WANT_TAKE);
    CC_CHECK(CcSimApply(&sim, &take_fifth, error, sizeof(error)));
    CC_CHECK(CcCustodyFind(&sim.custody, items[4]->custody_id)->holder.kind == CC_CUSTODY_PLAYER);
    CC_CHECK(CcWantsValidate(&sim));
}

/* A person waits at least 14 days before a new request; a settled or closed
   request (with no open child) is forgotten after 28 days, freeing its slot.
   This is the "requests change with the world each day" lifecycle. */
static void TestRequestLifecycleCooldownAndCleanup(void)
{
    CcSimInit(&sim, 2024U);
    Prime();
    CcCharacter *baker = AdultAt(sim.settlements[0].id, CC_OCCUPATION_BAKER);
    CC_CHECK(baker != NULL);
    baker->travel_coins = 20;
    CcSettlement *home = CcSimSettlementMutable(&sim, baker->current_settlement_id);
    home->stock[CC_GOOD_WHEAT] = 0;
    sim.settlements[2].stock[CC_GOOD_WHEAT] = 50;
    CcWantsAdvance(&sim);
    const CcPersonalWant *first = FindRoot(baker->id, CC_WANT_WORK_SUPPLIES);
    CC_CHECK(first != NULL);
    CcId first_id = first->id;
    /* Restocking settles it the same way TestWorkSupplyGenerationAndDailySettlement
       demonstrates; the interesting part here is what happens afterward. */
    home->stock[CC_GOOD_WHEAT] = 5;
    CcWantsAdvance(&sim);
    const CcPersonalWant *settled = CcWantsFind(&sim, first_id);
    CC_CHECK(settled != NULL && settled->status == CC_WANT_SETTLED);
    int32_t settled_day = settled->settled_day;
    home->stock[CC_GOOD_WHEAT] = 0;
    /* Still inside the 14-day cooldown (days settled_day+1 .. settled_day+13):
       no second request, even though the town is short again and the source
       is still reachable. */
    for (int offset = 1; offset <= 13; ++offset) {
        ++sim.current_day;
        CcWantsAdvance(&sim);
        CC_CHECK(FindActiveRoot(baker->id, CC_WANT_WORK_SUPPLIES) == NULL);
        CC_CHECK(CcWantsFind(&sim, first_id) != NULL);
        CC_CHECK(CcWantsValidate(&sim));
    }
    CC_CHECK(sim.current_day - settled_day == 13);
    /* One more day and the cooldown lifts: a fresh request appears in a new
       slot while the settled one is still recorded in the book. */
    ++sim.current_day;
    CcWantsAdvance(&sim);
    CC_CHECK(sim.current_day - settled_day == 14);
    const CcPersonalWant *second = FindActiveRoot(baker->id, CC_WANT_WORK_SUPPLIES);
    CC_CHECK(second != NULL && second->id != first_id);
    CcId second_id = second->id;
    CC_CHECK(CcWantsFind(&sim, first_id) != NULL);
    /* Past 28 days since the first one settled, its slot is forgotten, while
       the new, still-active one is untouched. */
    while (sim.current_day - settled_day <= 28) {
        ++sim.current_day;
        CcWantsAdvance(&sim);
        CC_CHECK(CcWantsValidate(&sim));
    }
    CC_CHECK(CcWantsFind(&sim, first_id) == NULL);
    CC_CHECK(CcWantsFind(&sim, second_id) != NULL);
}

/* The wire codec is the exact and only thing that crosses a save: round trip
   every field, and reject a state whose escrow could not have been produced
   by the simulation (a negative reward reservation). */
static void TestWantsCodecRoundTripAndGuards(void)
{
    CcWantsState state = {0};
    state.initialized = 2;
    state.last_day = 12345;
    state.wants[0] = (CcPersonalWant){.id = 1, .person_id = 2, .item_id = 3, .parent_id = 0,
        .source_place_id = 4, .home_id = 5, .cause_event_id = 6, .outcome_event_id = 0,
        .escrow = 6, .kind = CC_WANT_REPAIR, .status = CC_WANT_ACTIVE, .good = CC_GOOD_TOOLS,
        .quantity = 1, .created_day = 10, .known_day = 11, .settled_day = 0, .revision = 3};
    state.wants[1] = (CcPersonalWant){.id = 7, .person_id = 8, .item_id = 3, .parent_id = 1,
        .source_place_id = 4, .home_id = 5, .cause_event_id = 9, .outcome_event_id = 12,
        .escrow = 0, .kind = CC_WANT_REPAIR_IRON, .status = CC_WANT_FULFILLED, .good = CC_GOOD_IRON,
        .quantity = 1, .created_day = 10, .known_day = 11, .settled_day = 15, .revision = 2};
    state.items[0] = (CcBelonging){.id = 3, .custody_id = 30, .home_id = 5, .last_place_id = 4,
        .cause_event_id = 6, .repair_iron = 1, .last_wear_day = 10,
        .name = "A very long belonging name near the field limit"};
    uint8_t bytes[CC_WANTS_WIRE_CAPACITY];
    size_t length = CcWantsEncode(&state, bytes, sizeof(bytes));
    CC_CHECK(length > 0 && length <= sizeof(bytes));
    CcWantsState decoded = {0};
    CC_CHECK(CcWantsDecode(&decoded, bytes, length));
    CC_CHECK(memcmp(&state, &decoded, sizeof(state)) == 0);
    CC_CHECK(CcWantsHash(&state) == CcWantsHash(&decoded));
    /* Truncated or oversized payloads are rejected outright. */
    CC_CHECK(!CcWantsDecode(&decoded, bytes, length - 1));
    CC_CHECK(CcWantsEncode(&state, bytes, length - 1) == 0);
    /* A negative escrow could never come from the simulation; the wire
       format's shared field walk refuses it in either direction, so it
       cannot even be encoded, let alone loaded back. */
    CcWantsState corrupt = state;
    corrupt.wants[0].escrow = -1;
    CC_CHECK(CcWantsEncode(&corrupt, bytes, sizeof(bytes)) == 0);
}

/* The whole save/load path (both the in-memory encoder and the sqlite file)
   preserves every request and belonging field, and the hash the save
   records still matches after loading back. */
static void TestSaveLoadRoundTrip(void)
{
    CcSimInit(&sim, 13U);
    Prime();
    CcCharacter *owner = AdultAt(sim.settlements[0].id, CC_OCCUPATION_MILLER);
    CcCharacter *smith = AdultAt(sim.settlements[1].id, CC_OCCUPATION_SMITH);
    CcCharacter *hungry = AdultAt(sim.settlements[2].id, CC_OCCUPATION_NONE);
    CC_CHECK(owner != NULL && smith != NULL && hungry != NULL);
    owner->travel_coins = 40;
    hungry->hungry_days = 2;
    hungry->travel_coins = 20;
    CcSettlement *smith_town = CcSimSettlementMutable(&sim, smith->current_settlement_id);
    smith_town->service_mask |= (UINT32_C(1) << CC_SERVICE_SMITHY);
    smith_town->stock[CC_GOOD_TOOLS] = 3;
    smith_town->stock[CC_GOOD_IRON] = 5;
    sim.settlements[3].stock[CC_GOOD_BREAD] = 10;
    sim.settlements[3].stock[CC_GOOD_WHEAT] = 50;
    (void)MakeBelonging(owner, "Saved tool",
        (CcCustodyHolder){CC_CUSTODY_CHARACTER, owner->id}, 30);
    (void)MakeBelonging(hungry, "Stored keepsake",
        (CcCustodyHolder){CC_CUSTODY_STORE, sim.settlements[3].id}, 100);
    CcWantsAdvance(&sim);
    CC_CHECK(FindRoot(owner->id, CC_WANT_REPAIR) != NULL);
    CC_CHECK(FindRoot(hungry->id, CC_WANT_MEAL) != NULL);
    CC_CHECK(CcSimValidate(&sim, error, sizeof(error)));

    unsigned char *bytes = NULL;
    size_t length = 0;
    CC_CHECK(CcSaveEncode(&sim, &bytes, &length, error, sizeof(error)));
    CC_CHECK(CcSaveDecode(bytes, length, &loaded, error, sizeof(error)));
    CcSaveFreeBuffer(bytes);
    CC_CHECK(memcmp(&sim.wants, &loaded.wants, sizeof(sim.wants)) == 0);
    CC_CHECK(CcWantsHash(&sim.wants) == CcWantsHash(&loaded.wants));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));

    const char *path = "wants-tests.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)));
    CC_CHECK(memcmp(&sim.wants, &loaded.wants, sizeof(sim.wants)) == 0);
    CC_CHECK(CcWantsHash(&sim.wants) == CcWantsHash(&loaded.wants));
    CC_CHECK(CcSimHash(&sim) == CcSimHash(&loaded));
    CC_CHECK(CcSimValidate(&loaded, error, sizeof(error)));
    (void)remove(path);
}

/* A save written by schema 118 (before personal requests existed) loads
   cleanly: the upgrade starts the feature in its unseeded state, and the
   saved wants table stays empty until the player actually asks. */
static void TestLegacySchema118Load(void)
{
    CcSimInit(&sim, 555000111U);
    sim.schema_version = 118U;
    uint64_t legacy_hash = CcSimHash(&sim);
    /* Below schema 119 the wants payload is not part of the canonical state:
       mutating it does not move the hash. */
    sim.wants.wants[0] = (CcPersonalWant){.id = 999, .person_id = 999,
        .kind = CC_WANT_MEAL, .status = CC_WANT_ACTIVE, .revision = 1};
    CC_CHECK(CcSimHash(&sim) == legacy_hash);
    sim.wants.wants[0] = (CcPersonalWant){0};
    const char *path = "wants-legacy-118.ccsave";
    CC_CHECK(CcSaveWrite(path, &sim, error, sizeof(error)));
    sqlite3 *database = NULL;
    CC_CHECK(sqlite3_open(path, &database) == SQLITE_OK);
    sqlite3_stmt *statement = NULL;
    CC_CHECK(sqlite3_prepare_v2(database, "SELECT COUNT(*) FROM wants_state;", -1, &statement, NULL) == SQLITE_OK);
    CC_CHECK(sqlite3_step(statement) == SQLITE_ROW);
    CC_CHECK(sqlite3_column_int(statement, 0) == 0);
    sqlite3_finalize(statement);
    sqlite3_close(database);
    CC_CHECK(CcSaveRead(path, &loaded, error, sizeof(error)));
    CC_CHECK(loaded.schema_version == CC_SIM_SCHEMA_VERSION);
    CC_CHECK(loaded.wants.initialized == 1);
    CC_CHECK(loaded.wants.last_day == loaded.current_day);
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) CC_CHECK(loaded.wants.wants[i].id == 0);
    for (int i = 0; i < CC_BELONGINGS; ++i) CC_CHECK(loaded.wants.items[i].id == 0);
    CC_CHECK(CcSimValidate(&loaded, error, sizeof(error)));
    (void)remove(path);
}

int main(void)
{
    TestDeterministicGeneration();
    TestMealFromReachableSourcesOnly();
    TestWorkSupplyGenerationAndDailySettlement();
    TestRecoveryCustodyToPlayerAndBack();
    TestRepairSmithIronChain();
    TestRequestEndsOnDeathAndSmithDeath();
    TestRewardEscrowCappedByPurse();
    TestSatchelFourItemLimitAndTownStorage();
    TestRequestLifecycleCooldownAndCleanup();
    TestWantsCodecRoundTripAndGuards();
    TestSaveLoadRoundTrip();
    TestLegacySchema118Load();
    return 0;
}
