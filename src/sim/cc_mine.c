#include "sim/cc_mine.h"
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
    if(sim == NULL || sim->schema_version < 58U || !sim->journey.active) return -1;
    const CcRoadSite *site=CcMineSite(sim);
    if(site == NULL || site->route_id != sim->journey.route_id) return -1;
    int32_t slot=(int32_t)(site-sim->road_sites);
    if((sim->journey.road_site_stop_mask & (UINT32_C(1)<<slot)) != 0) return -1;
    const CcRoute *route=CcSimRoute(sim,site->route_id);
    if(route == NULL) return -1;
    int32_t progress=sim->journey.origin_id == route->from_id ? site->progress_milli : 1000-site->progress_milli;
    return (int32_t)(((int64_t)sim->journey.total_subticks*progress+999)/1000);
}
bool CcMineWalkable(const CcSim *sim, CcMinePhase phase, int32_t x, int32_t y)
{
    if (x < 1 || x >= CC_MINE_WIDTH-1 || y < 1 || y >= CC_MINE_HEIGHT-1) return false;
    if (phase == CC_MINE_YARD) {
        if (y < 4) return x == 15 && y == 3;
        return !In(x,y,3,5,6,4) && !In(x,y,22,5,6,4) &&
               !In(x,y,3,11,6,4) && !In(x,y,22,11,6,4);
    }
    if (phase != CC_MINE_LEVEL) return false;
    if (x == 15 && y == 10 && !sim->mine.bar_open) return false;
    return CcMineChamber(x,y) >= 0 || In(x,y,9,4,3,1) || In(x,y,19,4,4,1) ||
        In(x,y,15,7,1,6) || In(x,y,5,7,1,6) || In(x,y,9,15,3,1) || In(x,y,20,15,4,1);
}
int32_t CcMinePackUsed(const CcSim *sim)
{
    int64_t used=0;
    for (int32_t i=0;i<CC_GOOD_COUNT;++i) used += sim->mine.pack[i];
    return used > INT_MAX ? INT_MAX : (int32_t)used;
}
static bool Near(const CcMineVisit *mine, int32_t x, int32_t y)
{
    return abs(mine->x-x)+abs(mine->y-y) <= 1;
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
        if (Near(m,26,4) && !m->surveyed) return "Read the workers' survey";
        if (Near(m,26,16)) return "Inspect the lower passage";
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
bool CcMineApply(CcSim *sim, const CcCommand *command, char *error, size_t capacity)
{
    CcMineVisit *m=&sim->mine;
    if (sim->schema_version < 58U || m->revision >= INT_MAX-1)
        return Fail(error,capacity,"Load this campaign with the current mine rules.");
    if (command->kind == CC_COMMAND_VISIT_MINE) {
        const CcRoadSite *site=CcMineSite(sim);
        if (m->phase != CC_MINE_NONE || site == NULL || site->id != command->target_id ||
            CcSimJourneyRoadSiteStop(sim) != site || sim->journey.elapsed_subticks != CcMineBranchSubtick(sim))
            return Fail(error,capacity,"Follow the road to the Low Silver Pit branch first.");
        if (sim->dungeon_expedition.active || sim->pony_company.encounter >= 0)
            return Fail(error,capacity,"Finish the current visit before taking the mine branch.");
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
            int32_t x=m->x+dx[command->amount], y=m->y+dy[command->amount];
            if (!CcMineWalkable(sim,m->phase,x,y))
                return Fail(error,capacity,"Stone or a closed passage blocks this step.");
            m->x=x; m->y=y;
            if (m->phase == CC_MINE_LEVEL) {
                m->steps=(m->steps+1)%6;
                if (m->steps == 0) { if(m->light > 0) m->light-=1; SpendMinutes(sim,5); }
                int32_t chamber=CcMineChamber(x,y);
                if (chamber >= 0) m->seen |= UINT32_C(1) << chamber;
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
            if (amount < 0 && (m->pack[good] < -amount ||
                CcPlayerCargoUsed(&sim->player)-amount > sim->player.cargo_capacity))
                return Fail(error,capacity,"Check the carried goods and carriage space.");
            m->pack[good]+=amount; sim->player.cargo[good]-=amount;
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
                if (CcPlayerCargoUsed(&sim->player)+CcMinePackUsed(sim) > sim->player.cargo_capacity)
                    return Fail(error,capacity,"Make room in the carriage for the carried pack.");
                for (int32_t good=0;good<CC_GOOD_COUNT;++good) {
                    sim->player.cargo[good]+=m->pack[good]; m->pack[good]=0;
                }
                const CcRoadSite *site=CcMineSite(sim);
                sim->journey.road_site_stop_mask |= UINT32_C(1) << (int32_t)(site-sim->road_sites);
                sim->carriage.mode=CC_CARRIAGE_MOVING;
                sim->carriage.speed_milli_per_second=m->return_speed;
                m->phase=CC_MINE_NONE; m->site_id=0; m->x=0; m->y=0; m->return_speed=0;
                SpendMinutes(sim,1);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,15,10) && !m->bar_open) {
                m->bar_open=true; SpendMinutes(sim,1);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,26,4) && !m->surveyed) {
                m->surveyed=true;
                sim->dungeons[0].rooms[0].state_flags |= CC_DUNGEON_ROOM_SEARCHED | CC_DUNGEON_ROOM_DISCOVERED;
                SpendMinutes(sim,5);
            } else if (m->phase == CC_MINE_LEVEL && Near(m,26,16)) {
                return Fail(error,capacity,"Rubble fills the stair to Lamp Hall. The workers' survey is in the east records room.");
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
