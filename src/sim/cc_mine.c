#include "sim/cc_mine.h"
#include "sim/cc_sim_custody.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static const int32_t chambers[6][4] = {
    {2,2,7,5}, {12,2,7,5}, {23,2,6,5},
    {12,13,8,6}, {2,13,7,6}, {24,13,5,6}
};
static bool In(int32_t x, int32_t y, int32_t a, int32_t b, int32_t w, int32_t h)
{
    return x >= a && x < a+w && y >= b && y < b+h;
}
int32_t CcMineChamber(int32_t x, int32_t y)
{
    for (int32_t i=0;i<6;++i)
        if (In(x,y,chambers[i][0],chambers[i][1],chambers[i][2],chambers[i][3])) return i;
    return -1;
}
const char *CcMineChamberName(int32_t chamber)
{
    static const char *const names[] = {"Gatehouse", "Lamp room", "Workers' records",
        "Stair hall", "Rope store", "Lower passage"};
    return chamber >= 0 && chamber < 6 ? names[chamber] : "Connecting passage";
}
const CcRoadSite *CcMineSite(const CcSim *sim)
{
    if (sim == NULL || sim->dungeon_count == 0) return NULL;
    for (int32_t i=0;i<sim->road_site_count;++i) {
        const CcRoadSite *site=&sim->road_sites[i];
        if (site->kind == CC_ROAD_SITE_MINE &&
            site->home_settlement_id == sim->dungeons[0].settlement_id) return site;
    }
    return NULL;
}
int32_t CcMineBranchSubtick(const CcSim *sim)
{
    if(sim == NULL || sim->schema_version < 59U || !sim->journey.active) return -1;
    const CcRoadSite *site=CcMineSite(sim);
    if(site == NULL || site->route_id != sim->journey.route_id) return -1;
    int32_t slot=(int32_t)(site-sim->road_sites);
    if((sim->journey.road_site_stop_mask & (UINT32_C(1)<<slot)) != 0) return -1;
    const CcRoute *route=CcSimRoute(sim,site->route_id);
    if(route == NULL) return -1;
    int32_t progress=sim->journey.origin_id == route->from_id ? site->progress_milli : 1000-site->progress_milli;
    return (int32_t)(((int64_t)sim->journey.total_subticks*progress+999)/1000);
}
bool CcMineWalkableState(CcMinePhase phase, int32_t x, int32_t y,
                         bool bar_open)
{
    if (x < 1 || x >= CC_MINE_WIDTH-1 || y < 1 || y >= CC_MINE_HEIGHT-1) return false;
    if (phase == CC_MINE_YARD) {
        if (y < 4) return x == 15 && y == 3;
        return !In(x,y,3,5,6,4) && !In(x,y,22,5,6,4) &&
               !In(x,y,3,11,6,4) && !In(x,y,22,11,6,4);
    }
    if (phase != CC_MINE_LEVEL) return false;
    if (x == 15 && y == 10 && !bar_open) return false;
    return CcMineChamber(x,y) >= 0 || In(x,y,9,4,3,1) || In(x,y,19,4,4,1) ||
        In(x,y,15,7,1,6) || In(x,y,5,7,1,6) || In(x,y,9,15,3,1) || In(x,y,20,15,4,1);
}
bool CcMineWalkable(const CcSim *sim, CcMinePhase phase, int32_t x, int32_t y)
{
    return sim != NULL && CcMineWalkableState(
        phase,x,y,sim->mine.bar_open);
}
int32_t CcMinePackUsed(const CcSim *sim)
{
    if (sim == NULL) return 0;
    int64_t used=0;
    for (int32_t i=0;i<CC_GOOD_COUNT;++i) {
        int32_t quantity=sim->mine.pack[i];
        if (quantity < 0 || used > INT_MAX-quantity) return INT_MAX;
        used+=quantity;
    }
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if (entry->active && entry->kind == CC_CUSTODY_GOODS &&
            entry->holder.kind == CC_CUSTODY_MINE_PACK &&
            entry->holder.id == sim->player.id) {
            if (entry->quantity < 0 || entry->quantity > INT_MAX ||
                used > INT_MAX-entry->quantity) return INT_MAX;
            used += entry->quantity;
        }
    }
    return used > INT_MAX ? INT_MAX : (int32_t)used;
}
static int32_t MineCustodyGood(const CcSim *sim, CcCustodyHolderKind kind,
                               CcId holder_id, CcGood good)
{
    int64_t used=0;
    if (sim == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if (entry->active && entry->kind == CC_CUSTODY_GOODS &&
            entry->holder.kind == kind && entry->holder.id == holder_id && entry->good == (int32_t)good)
            used += entry->quantity;
    }
    return used > INT_MAX ? INT_MAX : (int32_t)used;
}
int32_t CcMinePackGood(const CcSim *sim, CcGood good)
{
    if (sim == NULL || good < 0 || good >= CC_GOOD_COUNT) return 0;
    int64_t total=(int64_t)sim->mine.pack[good]+MineCustodyGood(sim,
        CC_CUSTODY_MINE_PACK,sim->player.id,good);
    return total > INT_MAX ? INT_MAX : (int32_t)total;
}
int32_t CcMineSourceGood(const CcSim *sim, CcGood good)
{
    return sim == NULL ? 0 : MineCustodyGood(sim,CC_CUSTODY_SITE,sim->mine.source_id,good);
}
int32_t CcMineCacheGood(const CcSim *sim, CcGood good)
{
    return sim == NULL ? 0 : MineCustodyGood(sim,CC_CUSTODY_SITE,sim->mine.cache_id,good);
}
int32_t CcMineSourceUsed(const CcSim *sim)
{
    int64_t total=0;
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) total+=CcMineSourceGood(sim,(CcGood)good);
    return total > INT_MAX ? INT_MAX : (int32_t)total;
}
int32_t CcMineCacheUsed(const CcSim *sim)
{
    int64_t total=0;
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) total+=CcMineCacheGood(sim,(CcGood)good);
    return total > INT_MAX ? INT_MAX : (int32_t)total;
}
void CcMineInitializeLoad(CcSim *sim)
{
    if (sim == NULL || sim->dungeon_count == 0 || sim->player.id == 0 ||
        sim->goblins.id == 0 ||
        sim->custody.next_id > UINT64_MAX - 3U) return;
    /* This runs only for the current campaign schema. Legacy journal replay
       keeps the shipped 96-slot table until its runtime upgrade finishes. */
    if (sim->schema_version < 103U) return;
    sim->custody.capacity=CC_CUSTODY_CAPACITY;
    CcMineVisit *mine=&sim->mine;
    if (mine->source_id != 0 || mine->cache_id != 0) {
        return;
    }
    int32_t slots=0;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i)
        if (sim->custody.entries[i].id == 0) ++slots;
    if (slots < 3) return;
    uint64_t authored_serial=((uint64_t)sim->world_seed << 1U) | UINT64_C(1);
    mine->source_id=CcMakeId(CC_ENTITY_MINE_SOURCE,authored_serial);
    mine->source_owner_id=sim->goblins.id;
    mine->cache_id=CcMakeId(CC_ENTITY_MINE_CACHE,authored_serial);
    mine->cache_owner_id=sim->player.id;
    /* The fixed load awaits in the Lower Passage. The Rope Store is a real,
       separate cache location, so a return trip has a recorded choice. */
    mine->source_x=26; mine->source_y=16;
    mine->cache_x=5; mine->cache_y=15;
    mine->source_released=false;
    if (sim->schema_version >= 106U) mine->return_revision=1;
    const CcGood goods[] = {CC_GOOD_IRON,CC_GOOD_GOLD,CC_GOOD_GEMS};
    const int32_t quantities[] = {8,3,2};
    int32_t next=0;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody) && next<3;++i) {
        CcCustodyEntry *entry=&sim->custody.entries[i];
        if (entry->id != 0) continue;
        *entry=(CcCustodyEntry){.id=sim->custody.next_id++,.revision=1,
            .owner_id=mine->source_owner_id,
            .holder={CC_CUSTODY_SITE,mine->source_id},.kind=CC_CUSTODY_GOODS,
            .quantity=quantities[next],.good=(int32_t)goods[next],.condition=100,.active=true};
        if (goods[next] == CC_GOOD_IRON) mine->iron_source_entry_id=entry->id;
        else if (goods[next] == CC_GOOD_GOLD) mine->gold_source_entry_id=entry->id;
        else if (goods[next] == CC_GOOD_GEMS) mine->gems_source_entry_id=entry->id;
        ++next;
    }
}
bool CcMineSettleFallenPack(CcSim *sim)
{
    if (sim == NULL || sim->mine.source_id == 0 ||
        sim->mine.source_owner_id != sim->goblins.id) return false;
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
        int32_t quantity=sim->mine.pack[good];
        if (quantity < 0 || sim->player.cargo[good] < 0 ||
            sim->player.cargo[good] > CC_SIM_MAX_UNITS-quantity) return false;
    }
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if (!entry->active || entry->holder.kind!=CC_CUSTODY_MINE_PACK ||
            entry->holder.id!=sim->player.id) continue;
        if (entry->kind!=CC_CUSTODY_GOODS || entry->owner_id!=sim->goblins.id ||
            entry->quantity<=0) return false;
    }
    /* Personal supplies return to the parked carriage. Goods still owned by
       the haulers return to their recorded Lower Passage holder. */
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
        sim->player.cargo[good]+=sim->mine.pack[good];
        sim->mine.pack[good]=0;
    }
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        CcCustodyEntry *entry=&sim->custody.entries[i];
        if (!entry->active || entry->holder.kind!=CC_CUSTODY_MINE_PACK ||
            entry->holder.id!=sim->player.id) continue;
        entry->holder=(CcCustodyHolder){CC_CUSTODY_SITE,sim->mine.source_id};
        entry->revision+=1;
        entry->last_event_id=(uint64_t)sim->mine.revision+1U;
    }
    return true;
}
static bool Near(const CcMineVisit *mine, int32_t x, int32_t y)
{
    return abs(mine->x-x)+abs(mine->y-y) <= 1;
}
static bool MineLocationReachable(const CcMineVisit *mine, int32_t x, int32_t y)
{
    return mine->phase == CC_MINE_LEVEL && Near(mine,x,y);
}
static bool MineHaulersNeedBread(const CcSim *sim)
{
    /* The source holder is the haulers' present stock. It is the local food
       need used by this offer, rather than a remote town or lair balance. */
    return CcMineSourceGood(sim,CC_GOOD_BREAD) < 2;
}
static bool MineSourceOwnershipReady(const CcSim *sim)
{
    const CcMineVisit *mine=&sim->mine;
    return mine->source_id != 0 && mine->source_owner_id == sim->goblins.id &&
        mine->cache_owner_id == sim->player.id && sim->goblins.id != 0 &&
        sim->player.id != 0;
}
static bool MineCreditSourceGood(CcSim *sim, CcGood good, int32_t quantity)
{
    if (sim == NULL || good < 0 || good >= CC_GOOD_COUNT || quantity <= 0)
        return false;
    CcMineVisit *mine=&sim->mine;
    if (sim->custody.next_id == 0 || sim->custody.next_id == UINT64_MAX)
        return false;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        CcCustodyEntry *entry=&sim->custody.entries[i];
        if (!entry->active || entry->kind != CC_CUSTODY_GOODS ||
            entry->good != (int32_t)good ||
            entry->holder.kind != CC_CUSTODY_SITE ||
            entry->holder.id != mine->source_id ||
            entry->owner_id != mine->source_owner_id) continue;
        if (entry->quantity > CC_SIM_MAX_UNITS-quantity) return false;
        entry->quantity+=quantity;
        entry->revision+=1;
        entry->last_event_id=(uint64_t)mine->revision+1U;
        if (good == CC_GOOD_BREAD && mine->bread_source_entry_id == 0U)
            mine->bread_source_entry_id=entry->id;
        return true;
    }
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        CcCustodyEntry *entry=&sim->custody.entries[i];
        if (entry->id != 0) continue;
        *entry=(CcCustodyEntry){.id=sim->custody.next_id++,.revision=1,
            .owner_id=mine->source_owner_id,
            .last_event_id=(uint64_t)mine->revision+1U,
            .holder={CC_CUSTODY_SITE,mine->source_id},.kind=CC_CUSTODY_GOODS,
            .quantity=quantity,.good=(int32_t)good,.condition=100,.active=true};
        if (good == CC_GOOD_BREAD) mine->bread_source_entry_id=entry->id;
        return true;
    }
    return false;
}
static bool Fail(char *error, size_t capacity, const char *text);
static bool MineTransfer(CcSim *sim, CcCustodyHolder source,
                         CcCustodyHolder destination, CcGood good,
                         int32_t quantity, char *error, size_t capacity)
{
    CcCustodyResult result=CcSimTransferMineGoods(sim,source,destination,good,
        quantity,(uint64_t)sim->mine.revision+1U);
    if (result == CC_CUSTODY_READY) return true;
    if (result == CC_CUSTODY_STALE) return Fail(error,capacity,"Refresh the mine position before moving that load.");
    if (result == CC_CUSTODY_FULL) return Fail(error,capacity,"The destination has no room for that quantity.");
    if (result == CC_CUSTODY_REMOTE) return Fail(error,capacity,"Reach both the carried pack and the located holder first.");
    if (result == CC_CUSTODY_FORBIDDEN) return Fail(error,capacity,"The mine load is not available for that transfer.");
    return Fail(error,capacity,"That holder does not contain the requested quantity.");
}
static bool StowMinePack(CcSim *sim, char *error, size_t capacity)
{
    int32_t carried[CC_GOOD_COUNT]={0};
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        const CcCustodyEntry *entry=&sim->custody.entries[i];
        if (!entry->active || entry->holder.kind != CC_CUSTODY_MINE_PACK ||
            entry->holder.id != sim->player.id) continue;
        if (entry->kind != CC_CUSTODY_GOODS || entry->good < 0 ||
            entry->good >= CC_GOOD_COUNT || entry->quantity > INT32_MAX - carried[entry->good])
            return Fail(error,capacity,"The carried mine manifest is invalid.");
        carried[entry->good]+=(int32_t)entry->quantity;
    }
    if (CcPlayerCargoUsed(&sim->player)+CcMinePackUsed(sim) > sim->player.cargo_capacity)
        return Fail(error,capacity,"Make room in the carriage for the carried pack.");
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
        if (sim->player.cargo[good] > CC_SIM_MAX_UNITS-sim->mine.pack[good]-carried[good])
            return Fail(error,capacity,"The carriage cannot hold the carried mine load.");
    }
    for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
        sim->player.cargo[good]+=sim->mine.pack[good]+carried[good];
        sim->mine.pack[good]=0;
    }
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody);++i) {
        CcCustodyEntry *entry=&sim->custody.entries[i];
        if (entry->active && entry->holder.kind == CC_CUSTODY_MINE_PACK &&
            entry->holder.id == sim->player.id) {
            if (sim->schema_version >= 106U) {
                entry->holder=(CcCustodyHolder){CC_CUSTODY_PLAYER,sim->player.id};
            } else {
                entry->quantity=0; entry->active=false;
            }
            entry->revision+=1;
            entry->last_event_id=(uint64_t)sim->mine.revision+1U;
        }
    }
    return true;
}
static bool UnpackMineGood(CcSim *sim, CcGood good, int32_t quantity,
                           char *error, size_t capacity)
{
    if (quantity <= 0 || CcMinePackGood(sim,good) < quantity ||
        CcPlayerCargoUsed(&sim->player) > sim->player.cargo_capacity-quantity)
        return Fail(error,capacity,"Check the carried goods and carriage space.");
    int32_t legacy=sim->mine.pack[good] < quantity ? sim->mine.pack[good] : quantity;
    int32_t remaining=quantity-legacy;
    CcCustodyState candidate=sim->custody;
    for (int32_t i=0;i<CcCustodyEffectiveCapacity(&sim->custody) && remaining>0;++i) {
        CcCustodyEntry *entry=&candidate.entries[i];
        if (!entry->active || entry->kind != CC_CUSTODY_GOODS || entry->good != (int32_t)good ||
            entry->holder.kind != CC_CUSTODY_MINE_PACK || entry->holder.id != sim->player.id) continue;
        int32_t moved=entry->quantity < remaining ? (int32_t)entry->quantity : remaining;
        entry->quantity-=moved; entry->revision+=1;
        entry->last_event_id=(uint64_t)sim->mine.revision+1U;
        if (entry->quantity == 0) entry->active=false;
        remaining-=moved;
    }
    if (remaining != 0) return Fail(error,capacity,"The carried mine manifest is invalid.");
    sim->custody=candidate;
    sim->mine.pack[good]-=legacy;
    sim->player.cargo[good]+=quantity;
    return true;
}
const char *CcMineAction(const CcSim *sim)
{
    const CcMineVisit *m=&sim->mine;
    if (m->phase == CC_MINE_YARD) {
        if (Near(m,15,18)) return "Board carriage and return to road";
        if (Near(m,15,3)) return "Enter Mine Mouth on foot";
    } else if (m->phase == CC_MINE_LEVEL) {
        if (Near(m,5,3)) return "Step outside to the mine yard";
        if (Near(m,15,10) && !m->bar_open) return "Lift the wooden bar";
        if (Near(m,26,4)) return m->surveyed ?
            "Reread the workers' records" : "Read the workers' records";
        if (Near(m,26,16)) {
            if (m->contest_active) return "Haulers engaged: fight or break contact";
            if (m->encounter_outcome == CC_MINE_ENCOUNTER_CONTESTED)
                return "Take the defeated haulers' load";
            if (m->encounter_outcome == CC_MINE_ENCOUNTER_BARGAINED)
                return "Gold paid from the hauler load";
            return "Hungry goblin haulers: bargain or contest";
        }
    }
    return NULL;
}
static bool Fail(char *error, size_t capacity, const char *text)
{
    if (error != NULL && capacity > 0) (void)snprintf(error,capacity,"%s",text);
    return false;
}
static void SpendMinutes(CcSim *sim, int32_t minutes)
{
    sim->clock.minute_subticks += minutes * CC_WORLD_MINUTE_SUBTICKS;
    while (sim->clock.minute_subticks >= CC_WORLD_DAY_SUBTICKS) {
        sim->clock.minute_subticks -= CC_WORLD_DAY_SUBTICKS;
        CcSimAdvanceDays(sim,1);
    }
}

static void RecordMineBypass(CcSim *sim)
{
    CcMineVisit *mine=&sim->mine;
    if (sim->schema_version < 106U || mine->bypass_event_id != 0U) return;
    const CcRoadSite *site=CcMineSite(sim);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text,sizeof(text),
        "Day %d: the Crownless Company walks the western store passage around the barred middle passage at Low Silver Pit.",
        sim->current_day);
    CcEvent *event=CcSimPushEvent(sim,CC_EVENT_FACT_REVEALED,
        site != NULL ? site->id : mine->site_id,
        sim->dungeon_count > 0 ? sim->dungeons[0].settlement_id : mine->site_id,
        0U,1,text);
    mine->bypass_event_id=event->id;
    mine->bypass_day=sim->current_day;
}

static void RecordMineSurvey(CcSim *sim, bool earlier_survey)
{
    CcMineVisit *mine=&sim->mine;
    if (sim->schema_version < 106U || mine->survey_event_id != 0U) return;
    const CcRoadSite *site=CcMineSite(sim);
    char text[CC_EVENT_TEXT_CAPACITY];
    (void)snprintf(text,sizeof(text), earlier_survey ?
        "Day %d: the company rereads Low Silver Pit records; they claim the west store route circles the bar. Earlier observation date unknown." :
        "Day %d: The western store passage goes around the barred middle passage. The stair to Lamp Hall is marked blocked.",
        sim->current_day);
    CcEvent *event=CcSimPushEvent(sim,CC_EVENT_LORE_RECORDED,
        site != NULL ? site->id : mine->site_id,
        sim->dungeon_count > 0 ? sim->dungeons[0].settlement_id : mine->site_id,
        0U,1,text);
    mine->survey_source_id=site != NULL ? site->id : mine->site_id;
    mine->survey_event_id=event->id;
    mine->survey_read_day=sim->current_day;
    mine->survey_observed_day=earlier_survey ? 0 : sim->current_day;
}
bool CcMineApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    CcMineVisit *m=&sim->mine;
    if (sim->schema_version < 59U || m->revision >= INT_MAX-1)
        return Fail(error,capacity,"Load this campaign with the current mine rules.");
    if (command->kind == CC_COMMAND_VISIT_MINE) {
        const CcRoadSite *site=CcMineSite(sim);
        /* Reaching the branch is enough; the stop window already says the
           company is there. Requiring an exact subtick left the turn offered
           on one tick and campable on all the others. */
        if (m->phase != CC_MINE_NONE || site == NULL || site->id != command->target_id ||
            CcSimJourneyRoadSiteStop(sim) != site ||
            CcMineBranchSubtick(sim) < 0 ||
            sim->journey.elapsed_subticks < CcMineBranchSubtick(sim))
            return Fail(error,capacity,"Follow the road to the Low Silver Pit branch first.");
        if (sim->dungeon_expedition.active || sim->pony_company.encounter >= 0)
            return Fail(error,capacity,"Finish the current visit before taking the mine branch.");
        /* Turning off anchors the journey at the branch: the company leaves the
           road there whether it reached the mark exactly or a few subticks on,
           and the visit invariant holds the carriage at that anchor. */
        int32_t branch=CcMineBranchSubtick(sim);
        sim->journey.elapsed_subticks=branch;
        sim->carriage.progress_milli=(int32_t)(
            (int64_t)branch*1000/sim->journey.total_subticks);
        m->site_id=site->id; m->phase=CC_MINE_YARD; m->x=15; m->y=17;
        m->return_speed=sim->carriage.speed_milli_per_second;
        sim->carriage.mode=CC_CARRIAGE_STOPPED;
        sim->carriage.speed_milli_per_second=0;
        SpendMinutes(sim,1);
    } else {
        if (m->phase == CC_MINE_NONE || command->target_id != (CcId)m->revision)
            return Fail(error,capacity,"Refresh the mine position before acting.");
        if (command->kind == CC_COMMAND_MINE_STEP) {
            static const int32_t dx[]={0,1,0,-1},dy[]={-1,0,1,0};
            if (command->amount < 0 || command->amount > 3)
                return Fail(error,capacity,"Choose north, east, south, or west.");
            if (m->contest_active)
                return Fail(error,capacity,"Break contact with the haulers before walking away.");
            int32_t x=m->x+dx[command->amount], y=m->y+dy[command->amount];
            if (!CcMineWalkable(sim,m->phase,x,y))
                return Fail(error,capacity,"Stone or a closed passage blocks this step.");
            m->x=x; m->y=y;
            if (m->phase == CC_MINE_LEVEL) {
                m->steps=(m->steps+1)%6;
                if (m->steps == 0) { if(m->light > 0) m->light-=1; SpendMinutes(sim,5); }
                int32_t chamber=CcMineChamber(x,y);
                if (chamber >= 0) m->seen |= UINT32_C(1) << chamber;
                /* The Gatehouse, Rope Store, and Stair Hall form the mine's
                   existing side loop. Passing the central bar cannot create
                   a bypass result; reaching the source-side tile after this
                   loop does. */
                if (x == 9 && y == 15) {
                    if (!m->bypass_route_seen) RecordMineBypass(sim);
                    m->bypass_route_seen=true;
                }
            } else SpendMinutes(sim,1);
        } else if (command->kind == CC_COMMAND_MINE_PACK) {
            if (m->phase != CC_MINE_YARD || !Near(m,15,18))
                return Fail(error,capacity,"Pack supplies beside the carriage.");
            if (command->good < 0 || command->good >= CC_GOOD_COUNT ||
                command->amount == 0 || command->amount < -CC_MINE_PACK_CAPACITY ||
                command->amount > CC_MINE_PACK_CAPACITY)
                return Fail(error,capacity,"Choose up to eight carried goods.");
            int32_t amount=command->amount, good=(int32_t)command->good;
            if (amount > 0 && (sim->player.cargo[good] < amount ||
                CcMinePackUsed(sim)+amount > CC_MINE_PACK_CAPACITY))
                return Fail(error,capacity,"Check the carriage stock and eight-slot pack.");
            if (amount < 0) {
                if (!UnpackMineGood(sim,(CcGood)good,-amount,error,capacity)) return false;
            } else {
                m->pack[good]+=amount; sim->player.cargo[good]-=amount;
            }
        } else if (command->kind == CC_COMMAND_MINE_INSPECT) {
            if (!MineLocationReachable(m,m->source_x,m->source_y))
                return Fail(error,capacity,"Reach the Lower Passage load before inspecting it.");
            if (error != NULL && capacity > 0) error[0]='\0';
            return true;
        } else if (command->kind == CC_COMMAND_MINE_BARGAIN) {
            if (!MineLocationReachable(m,m->source_x,m->source_y))
                return Fail(error,capacity,"Reach the haulers before offering their food.");
            if (m->encounter_outcome == CC_MINE_ENCOUNTER_BARGAINED)
                return Fail(error,capacity,"The haulers have already settled this offer.");
            if (m->encounter_outcome == CC_MINE_ENCOUNTER_CONTESTED)
                return Fail(error,capacity,"The Lower Passage contest has already been settled.");
            if (m->contest_active)
                return Fail(error,capacity,"Break contact before changing the offer.");
            if (!MineSourceOwnershipReady(sim) || !MineHaulersNeedBread(sim) ||
                m->pack[CC_GOOD_BREAD] < 2 || CcMineSourceGood(sim,CC_GOOD_GOLD) < 1)
                return Fail(error,capacity,"Carry two Bread in the mine pack for the haulers.");
            CcMineVisit original_mine=*m;
            CcCustodyState original_custody=sim->custody;
            /* Debit the offered Bread before checking the combined pack.
               This permits an eight-slot pack that already holds the offer. */
            m->pack[CC_GOOD_BREAD]-=2;
            if (!MineCreditSourceGood(sim,CC_GOOD_BREAD,2) ||
                CcMinePackUsed(sim)+1 > CC_MINE_PACK_CAPACITY) {
                *m=original_mine;
                sim->custody=original_custody;
                return Fail(error,capacity,"The haulers cannot receive that offer here.");
            }
            /* Source release is scoped to this atomic exchange. The committed
               bargain closes it again, so it grants no later source access. */
            m->source_released=true;
            if (!MineTransfer(sim,(CcCustodyHolder){CC_CUSTODY_SITE,m->source_id},
                (CcCustodyHolder){CC_CUSTODY_MINE_PACK,sim->player.id},CC_GOOD_GOLD,1,
                error,capacity)) {
                *m=original_mine;
                sim->custody=original_custody;
                return false;
            }
            m->source_released=false;
            m->encounter_outcome=CC_MINE_ENCOUNTER_BARGAINED;
        } else if (command->kind == CC_COMMAND_MINE_CONTEST) {
            if (!MineLocationReachable(m,m->source_x,m->source_y))
                return Fail(error,capacity,"Reach the haulers before challenging their load.");
            if (m->encounter_outcome == CC_MINE_ENCOUNTER_BARGAINED ||
                m->encounter_outcome == CC_MINE_ENCOUNTER_CONTESTED)
                return Fail(error,capacity,"The Lower Passage fight has already ended.");
            if (m->contest_active)
                return Fail(error,capacity,"The Lower Passage contest is already active.");
            m->contest_active=true;
        } else if (command->kind == CC_COMMAND_MINE_BREAK_CONTACT) {
            if (!MineLocationReachable(m,m->source_x,m->source_y))
                return Fail(error,capacity,"Reach the haulers before withdrawing.");
            if (!m->contest_active || m->source_owner_id != sim->goblins.id ||
                command->amount < 0 || command->amount > 100)
                return Fail(error,capacity,"There is no Lower Passage fight to break from.");
            m->contest_active=false;
            m->encounter_outcome=CC_MINE_ENCOUNTER_BROKEN_CONTACT;
            m->player_injury=(uint8_t)command->amount;
        } else if (command->kind == CC_COMMAND_MINE_RESOLVE_CONTEST) {
            if (!MineLocationReachable(m,m->source_x,m->source_y) ||
                !m->contest_active || m->source_owner_id != sim->goblins.id ||
                command->amount < 0 || command->amount > 100)
                return Fail(error,capacity,"Refresh the active Lower Passage contest.");
            m->contest_active=false;
            m->source_released=true;
            m->encounter_outcome=CC_MINE_ENCOUNTER_CONTESTED;
            m->player_injury=(uint8_t)command->amount;
        } else if (command->kind == CC_COMMAND_MINE_TAKE) {
            if (!MineLocationReachable(m,m->source_x,m->source_y))
                return Fail(error,capacity,"Reach the Lower Passage load before taking from it.");
            if (command->good < 0 || command->good >= CC_GOOD_COUNT || command->amount <= 0)
                return Fail(error,capacity,"Choose a positive quantity from the load.");
            if (m->encounter_outcome != CC_MINE_ENCOUNTER_CONTESTED ||
                !m->source_released)
                return Fail(error,capacity,"The haulers have not released this load.");
            if (command->amount > CcMineSourceGood(sim,command->good))
                return Fail(error,capacity,"The haulers have not released that quantity.");
            if (CcMinePackUsed(sim)+command->amount > CC_MINE_PACK_CAPACITY)
                return Fail(error,capacity,"Make room in the eight-slot pack before taking more.");
            if (!MineTransfer(sim,(CcCustodyHolder){CC_CUSTODY_SITE,m->source_id},
                (CcCustodyHolder){CC_CUSTODY_MINE_PACK,sim->player.id},command->good,
                command->amount,error,capacity)) return false;
        } else if (command->kind == CC_COMMAND_MINE_CACHE) {
            if (!MineLocationReachable(m,m->cache_x,m->cache_y))
                return Fail(error,capacity,"Reach the Rope Store cache before moving its goods.");
            if (command->good < 0 || command->good >= CC_GOOD_COUNT || command->amount == 0)
                return Fail(error,capacity,"Choose a good and a non-zero cache quantity.");
            if (command->amount > 0) {
                if (m->pack[command->good] > 0)
                    return Fail(error,capacity,"The Rope Store holds only the haulers' load.");
                if (!MineTransfer(sim,(CcCustodyHolder){CC_CUSTODY_MINE_PACK,sim->player.id},
                    (CcCustodyHolder){CC_CUSTODY_SITE,m->cache_id},command->good,
                    command->amount,error,capacity)) return false;
            } else {
                if (command->amount == INT_MIN)
                    return Fail(error,capacity,"Choose a supported cache quantity.");
                int32_t quantity=-command->amount;
                if (CcMinePackUsed(sim)+quantity > CC_MINE_PACK_CAPACITY)
                    return Fail(error,capacity,"Make room in the eight-slot pack before recovering goods.");
                if (!MineTransfer(sim,(CcCustodyHolder){CC_CUSTODY_SITE,m->cache_id},
                    (CcCustodyHolder){CC_CUSTODY_MINE_PACK,sim->player.id},command->good,
                    quantity,error,capacity)) return false;
            }
        } else if (command->kind == CC_COMMAND_MINE_USE) {
            if (m->phase == CC_MINE_YARD && Near(m,15,3)) {
                const CcDungeon *d=&sim->dungeons[0];
                if (d->state == CC_DUNGEON_SEALED || d->state == CC_DUNGEON_RESEALED)
                    return Fail(error,capacity,"The mine mouth is sealed.");
                if (CcNutritionAvailable(m->pack,CC_NUTRITION_TRAVEL) < CC_NUTRITION_PER_RATION)
                    return Fail(error,capacity,"Carry one Bread or Meat from the carriage first.");
                (void)CcNutritionConsume(m->pack,CC_NUTRITION_TRAVEL,CC_NUTRITION_PER_RATION);
                m->phase=CC_MINE_LEVEL; m->x=5; m->y=4; m->light=18; m->steps=0; m->seen|=1U;
                SpendMinutes(sim,1);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,5,3)) {
                m->phase=CC_MINE_YARD; m->x=15; m->y=4; m->light=0; m->steps=0;
                SpendMinutes(sim,1);
            } else if (m->phase == CC_MINE_YARD && Near(m,15,18)) {
                if (!StowMinePack(sim,error,capacity)) return false;
                const CcRoadSite *site=CcMineSite(sim);
                sim->journey.road_site_stop_mask |= UINT32_C(1) << (int32_t)(site-sim->road_sites);
                sim->carriage.mode=CC_CARRIAGE_MOVING;
                sim->carriage.speed_milli_per_second=m->return_speed;
                m->phase=CC_MINE_NONE; m->site_id=0; m->x=0; m->y=0; m->return_speed=0;
                SpendMinutes(sim,1);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,15,10) && !m->bar_open) {
                m->bar_open=true; SpendMinutes(sim,1);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,26,4)) {
                if (m->surveyed) {
                    if (sim->schema_version >= 106U && m->survey_event_id == 0U) {
                        RecordMineSurvey(sim,true);
                        SpendMinutes(sim,5);
                        m->revision+=1;
                    }
                    if (error != NULL && capacity > 0) error[0]='\0';
                    return true;
                }
                RecordMineSurvey(sim,false);
                m->surveyed=true;
                sim->dungeons[0].rooms[0].state_flags |= CC_DUNGEON_ROOM_SEARCHED | CC_DUNGEON_ROOM_DISCOVERED;
                SpendMinutes(sim,5);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,26,16)) {
                return Fail(error,capacity,"Use Bargain, Contest, or Break contact at the haulers.");
            } else return Fail(error,capacity,"Walk to a doorway, the bar, or the workers' records.");
        } else return Fail(error,capacity,"Choose a mine action.");
    }
    m->revision+=1;
    if (error != NULL && capacity > 0) error[0]='\0';
    return true;
}
bool CcMineValidate(const CcSim *sim)
{
    const CcMineVisit *m=&sim->mine;
    if (m->phase < CC_MINE_NONE || m->phase > CC_MINE_LEVEL || m->revision < 0 ||
        m->light < 0 || m->light > 18 || m->steps < 0 || m->steps >= 6 || (m->seen & ~63U)) return false;
    for (int32_t good=0;good<CC_GOOD_COUNT;++good)
        if (m->pack[good] < 0 || m->pack[good] > CC_MINE_PACK_CAPACITY) return false;
    if (CcMinePackUsed(sim) > CC_MINE_PACK_CAPACITY) return false;
    if (sim->schema_version >= 103U) {
        if (m->source_id == 0 || m->cache_id == 0 || m->source_id == m->cache_id ||
            CcIdKind(m->source_id) != CC_ENTITY_MINE_SOURCE ||
            CcIdKind(m->cache_id) != CC_ENTITY_MINE_CACHE ||
            m->source_owner_id != sim->goblins.id ||
            m->cache_owner_id != sim->player.id ||
            !CcMineWalkable(sim,CC_MINE_LEVEL,m->source_x,m->source_y) ||
            !CcMineWalkable(sim,CC_MINE_LEVEL,m->cache_x,m->cache_y) ||
            (m->source_x == m->cache_x && m->source_y == m->cache_y)) return false;
        if (CcMineCacheUsed(sim) > CC_MINE_CACHE_CAPACITY) return false;
        if (sim->schema_version >= 104U &&
            (m->encounter_outcome > CC_MINE_ENCOUNTER_BROKEN_CONTACT ||
             m->player_injury > 100 ||
             (m->source_released !=
              (m->encounter_outcome == CC_MINE_ENCOUNTER_CONTESTED)) ||
             (m->contest_active &&
              (m->encounter_outcome == CC_MINE_ENCOUNTER_BARGAINED ||
               m->encounter_outcome == CC_MINE_ENCOUNTER_CONTESTED)))) return false;
        if (sim->schema_version >= 106U) {
            bool lead_empty=m->lead_event_id == 0U;
            bool survey_empty=m->survey_event_id == 0U;
            bool bypass_empty=m->bypass_event_id == 0U;
            bool report_empty=m->report_event_id == 0U;
            if (m->return_revision < 1 ||
                m->iron_source_entry_id == 0U || m->gold_source_entry_id == 0U ||
                m->gems_source_entry_id == 0U ||
                m->iron_source_entry_id >= sim->custody.next_id ||
                m->gold_source_entry_id >= sim->custody.next_id ||
                m->gems_source_entry_id >= sim->custody.next_id ||
                m->bread_source_entry_id >= sim->custody.next_id ||
                m->iron_source_entry_id == m->gold_source_entry_id ||
                m->iron_source_entry_id == m->gems_source_entry_id ||
                m->gold_source_entry_id == m->gems_source_entry_id ||
                m->report_kind > CC_MINE_RETURN_INFORMATION ||
                m->reported_encounter_outcome > CC_MINE_ENCOUNTER_BROKEN_CONTACT ||
                (lead_empty != (m->lead_source_id == 0U && m->lead_day == 0 && !m->lead_document)) ||
                (survey_empty != (m->survey_source_id == 0U && m->survey_read_day == 0 &&
                                  m->survey_observed_day == 0)) ||
                (!survey_empty && (m->survey_read_day <= 0 || m->survey_observed_day < 0 ||
                                   m->survey_observed_day > m->survey_read_day)) ||
                (bypass_empty != (m->bypass_day == 0)) ||
                (report_empty != (m->report_recipient_id == 0U && m->report_day == 0 &&
                                  m->report_kind == CC_MINE_RETURN_NONE &&
                                  m->reported_encounter_outcome == CC_MINE_ENCOUNTER_OPEN)) ||
                (!report_empty && (m->report_day <= 0 ||
                                   m->report_kind == CC_MINE_RETURN_NONE))) return false;
        }
    }
    if (m->phase == CC_MINE_NONE)
        return m->site_id == 0 && m->x == 0 && m->y == 0 && m->return_speed == 0 &&
            m->light == 0 && m->steps == 0 && CcMinePackUsed(sim) == 0;
    const CcRoadSite *site=CcMineSite(sim);
    return site != NULL && m->site_id == site->id && sim->journey.active &&
        sim->journey.phase == CC_JOURNEY_PHASE_TRAVELLING &&
        sim->journey.route_id == site->route_id && !sim->dungeon_expedition.active &&
        sim->journey.elapsed_subticks == CcMineBranchSubtick(sim) &&
        CcSimJourneyRoadSiteStop(sim) == site &&
        m->return_speed > 0 && CcMineWalkable(sim,m->phase,m->x,m->y);
}
