/* Frozen measurements for the 364-day long-history comparison. */
#define main original_metrics_main
#include "sim_metrics.c"
#undef main
#define main original_species_main
#include "species_metrics.c"
#undef main
#include "sim/cc_calendar.h"
#include "persistence/cc_save.h"
static void Header(void) { bool campaign_metrics = true;
    (void)printf(
        "seed_number,world_seed,year,average_hunger,maximum_hunger,"
        "average_prosperity,minimum_prosperity,maximum_prosperity,"
        "average_security,minimum_security,maximum_security,"
        "average_food_price,maximum_food_price,closed_routes,"
        "active_situations,bandit_influence,monster_pressure,"
        "average_legitimacy,shipment_slots,event_count,goblin_tributes,"
        "dragon_hoard,dragon_stolen,dragon_retaliations,average_inequality,"
        "maximum_inequality,average_war_burden,maximum_war_burden,"
        "hoard_raids,social_hoard_raids,war_hoard_raids,tracked_gold,"
        "market_coins,war_chests,total_food_stock,average_war_supply_crisis,"
        "maximum_war_supply_crisis,total_kingdom_treasury,treasure_count,"
        "goblin_lair_food,goblin_lair_weapons,dragon_raw_gold,dragon_gems,"
        "tracked_iron,tracked_tools,tracked_weapons,iron_ledger_reserve,"
        "iron_ledger_debt,smuggler_routes,goblin_hoard_defenses,wars,"
        "alliances,active_couriers,lost_couriers,distorted_couriers,"
        "dragons_slain,dragon_campaign_attempts,dragon_campaign_victories,"
        "dragon_campaign_defeats,dragon_stage,dragon_age_years,"
        "dragon_crown_strength,dragon_body_condition,dragon_memory_integrity,"
        "dragon_territory_stability,dragon_regional_influence,dragon_eggs,"
        "dragon_hunts,dragon_broods,dragon_whelps_dispersed,"
        "dragon_afterdeath_days,active_settlements,abandoned_settlements,"
        "total_population,climate_factor,dragon_campaign_experience,"
        "minimum_active_settlements,maximum_closed_routes,"
        "years_all_routes_closed,years_with_abandoned_settlement,"
        "route_closures,settlement_abandonments,years_hunger_40_plus,"
        "years_hunger_60_plus,years_at_war,years_allied,"
        "years_dragon_campaign,years_goblin_raid,years_bandit_raid,"
        "years_bandit_influence_70_plus,goblin_members_end,"
        "goblin_devotion_end,goblin_cohesion_end,goblin_interceptions_end,"
        "bandit_members_end,bandit_supplies_end,bandit_influence_end,"
        "bandit_raids_end,dragon_campaign_phase_end,days_at_war,"
        "days_allied,days_dragon_campaign,days_goblin_raid,days_bandit_raid,"
        "days_bandit_influence_70_plus,dragon_egg_days,dragon_whelp_days,"
        "dragon_wanderer_days,dragon_crowned_days,dragon_deep_wyrm_days,"
        "dragon_uncrowned_days,dragon_afterdragon_days,archive_scribes,"
        "lore_stored,lore_lost_total,archive_stewardship,"
        "archive_last_recorded_day,lore_ceiling,archive_tool_wear,"
        "archive_abbot_present,population_weighted_hunger,metrics_version,day,"
        "schema_version,generator_version,state_hash,inhabited_hunger,"
        "inhabited_prosperity,inhabited_security,weighted_hunger,weighted_prosperity,"
        "weighted_security,years_population_weighted_hunger_40_plus,years_without_population,first_bandit_id,"
        "bandit_raid_group_days,bandit_influence_70_plus_group_days,"
        "bandit_raid_group_year_samples,bandit_influence_70_plus_group_year_samples");
    (void)printf(",goblin_crown_color,cult_human_members,cult_goblin_members,cult_devotion");
    static const char *colors[] = {"red", "purple", "blue"};
    for (int32_t color = 0; color < CC_GOBLIN_FACTION_COUNT; ++color)
        (void)printf(",goblin_%s_members,goblin_%s_lair_value,goblin_%s_tribute,goblin_%s_hunted",
            colors[color], colors[color], colors[color], colors[color]);
    for (int32_t species = 0; species < CC_CULT_SPECIES_COUNT; ++species)
        for (int32_t rank = 0; rank < CC_CULT_RANK_COUNT; ++rank)
            (void)printf(",cult_%s_rank_%d", species == CC_CULT_HUMAN ? "human" : "goblin", rank);
    (void)printf(",hoard_share_permille,treasury_share_permille,"
                 "goblin_share_permille,circulating_share_permille");
    if (campaign_metrics) {
        (void)printf(",live_treasures,live_treasure_value,newest_treasure_day,"
                     "oldest_treasure_day,treasures_from_ruins,treasures_in_ruins,"
                     "treasure_identity_hash,next_entity_serial,"
                     "character_coins,hungry_travellers,unsheltered_travellers,named_bandits");
    }
    (void)putchar('\n');

}
static CcSim s, restored;
static FILE *events, *counts, *extra, *bands, *towns;
static uint64_t annual[CC_EVENT_KIND_COUNT], all[CC_EVENT_KIND_COUNT];
static int samples[CC_EVENT_KIND_COUNT], last_era=-1;
static CcId last_event=0;
static uint64_t empty_arrivals, loaded_arrivals;
static bool old_abandoned[CC_MAX_SETTLEMENTS];
static bool FullEvent(CcEventKind k) {
    return k == CC_EVENT_DRAGON_SLAIN || k == CC_EVENT_DRAGON_SUCCESSOR ||
        k == CC_EVENT_DRAGON_BROOD || k == CC_EVENT_DRAGON_WHELP_DISPERSED ||
        k == CC_EVENT_DRAGON_UNCROWNED || k == CC_EVENT_DRAGON_TERRITORY_LOST ||
        k == CC_EVENT_GOBLIN_DRAGON_SEED || k == CC_EVENT_WAR_DECLARED ||
        k == CC_EVENT_PEACE_DECLARED || k == CC_EVENT_ALLIANCE_DECLARED;
}
static FILE *Open(const char *prefix, const char *suffix) {
    char path[512]; snprintf(path,sizeof(path),"%s-%s",prefix,suffix);
    FILE *f=fopen(path,"w"); if (!f) {perror(path); exit(2);} return f;
}
static void EventLine(int year, const char *kind, int day, const char *text) {
    fprintf(events,"%d\t%d\t%s\t%s\n",year,day,kind,text);
}
static void Daily(int seed, History *species) {
    Observe(&s,species);
    int year=CcCalendar(s.current_day).year, era=year/100;
    if (era!=last_era) { memset(samples,0,sizeof(samples));last_era=era; }
    int fresh=0;
    for (int offset=0; offset<s.event_count; ++offset) {
        const CcEvent *e=CcSimRecentEvent(&s,offset);
        if (e==NULL || e->id<=last_event) break;
        ++fresh;
    }
    for (int offset=fresh-1; offset>=0; --offset) {
        const CcEvent *e=CcSimRecentEvent(&s,offset);
        if (e->kind<0 || e->kind>=CC_EVENT_KIND_COUNT) continue;
        ++annual[e->kind];++all[e->kind];
        if (e->kind == CC_EVENT_SHIPMENT_ARRIVED) {
            if (e->magnitude > 0) ++loaded_arrivals;
            else ++empty_arrivals;
        }
        if (FullEvent(e->kind) || samples[e->kind] < 2) {
            EventLine(year,CcEventKindName(e->kind),e->day,e->text);
            ++samples[e->kind];
        }
    }
    const CcEvent *latest=CcSimRecentEvent(&s,0);
    if (latest) last_event=latest->id;
    for (int i=0;i<s.settlement_count;++i) {
        bool abandoned=CcSettlementIsAbandoned(&s.settlements[i]);
        if (abandoned!=old_abandoned[i]) {
            char text[192]; snprintf(text,sizeof(text),"%s %s; population %d",s.settlements[i].name,
                abandoned?"becomes abandoned":"becomes inhabited",s.settlements[i].population);
            EventLine(year,"town-status",s.current_day,text);
            old_abandoned[i]=abandoned;
        }
    }
    (void)seed;
}
static void Year(int seed,int year,const History *h) {
    int cows=0,sheep=0,town_tools=0,paper=0,bandit_members=0,bandit_raids=0,named=0;
    for (int i=0;i<s.settlement_count;++i) {
        CcSettlement *t=&s.settlements[i];
        cows+=t->cow_adults+t->cow_calves; sheep+=t->sheep_adults+t->sheep_lambs;
        town_tools+=t->stock[CC_GOOD_TOOLS]; paper+=t->stock[CC_GOOD_PAPER];
        fprintf(towns,"%d,%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%lld\n",seed,year,s.current_day,
            t->name,t->population,CcSettlementIsAbandoned(t),t->hunger,t->prosperity,t->security,
            t->stock[CC_GOOD_WHEAT],t->stock[CC_GOOD_TOOLS],t->stock[CC_GOOD_PAPER],
            t->cow_adults+t->cow_calves,(long long)t->market_coins);
    }
    for (int i=0;i<s.bandit_count;++i) {
        CcBanditGroup *b=&s.bandits[i];
        bandit_members+=b->members;bandit_raids+=b->raids_completed;
        fprintf(bands,"%d,%d,%d,%llu,%s,%d,%d,%d,%lld,%d,%d,%d\n",seed,year,s.current_day,
            (unsigned long long)b->id,b->name,b->members,b->supplies,b->influence,
            (long long)b->coins,b->camp_size,b->raid_phase,b->raids_completed);
    }
    for (int i=0;i<s.character_count;++i) named+=s.characters[i].bandit_group_id!=0;
    for (int k=0;k<CC_EVENT_KIND_COUNT;++k) {
        if (annual[k]) fprintf(counts,"%d,%d,%d,%s,%llu,%llu\n",seed,year,k,
            CcEventKindName((CcEventKind)k),(unsigned long long)annual[k],(unsigned long long)all[k]);
        annual[k]=0;
    }
    CcRitualOfferingPlan ritual=CcSimRitualOfferingPlan(&s);
    fprintf(extra,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u",
        seed,year,s.current_day,cows,sheep,CcSimCommonPonyCount(&s),town_tools,paper,
        bandit_members,bandit_raids,named,s.character_births,s.character_deaths,
        s.scriven.meetings,s.scriven.editions,s.scriven.age_count,
        ritual.food_rations,ritual.relics,(unsigned)ritual.blocked);
#define X(field) fprintf(extra,",%llu",(unsigned long long)h->field)
    X(active_days); X(hunger_days); X(equipment_days); X(tribute_days);
    X(floor_days); X(divided_days); X(afterdragon_days); X(ritual_due_days);
    X(blocked_members); X(blocked_devotion); X(blocked_cohesion); X(blocked_food);
    X(blocked_coins); X(blocked_relics); X(blocked_tools); X(blocked_weapons);
    X(prepared); X(raids); X(empty_raids); X(tribute_events); X(rallies);
    X(rumors); X(offerings); X(seeds); X(defenses); X(saturated_days);
#undef X
    fprintf(extra,",%llu,%llu\n",(unsigned long long)empty_arrivals,
        (unsigned long long)loaded_arrivals);
}
int main(int argc,char **argv) {
    if (argc!=4) return 2;
    int32_t seed, years; const char *prefix=argv[3];
    if (!Positive(argv[1], INT32_MAX, &seed) || !Positive(argv[2], 10000, &years) ||
        strlen(prefix) > 480U) return 2;
    events=Open(prefix,"events.tsv"); counts=Open(prefix,"counts.csv");
    extra=Open(prefix,"extra.csv");bands=Open(prefix,"bands.csv");towns=Open(prefix,"towns.csv");
    fputs("year\tday\tkind\ttext\n",events);
    fputs("seed,year,kind_id,kind,annual,total\n",counts);
    fputs("seed,year,day,id,name,members,supplies,influence,coins,camp_size,raid_phase,raids\n",bands);
    fputs("seed,year,day,name,population,abandoned,hunger,prosperity,security,wheat,tools,paper,cows,market_coins\n",towns);
    fputs("seed,year,day,cows,sheep,common_ponies,town_tools,paper,bandit_members,bandit_raids,named_bandits,births,deaths,meetings,editions,true_ages,ritual_food,ritual_relics,ritual_blocked,active_days,hunger_days,equipment_days,tribute_days,floor_days,divided_days,afterdragon_days,ritual_due_days,blocked_members,blocked_devotion,blocked_cohesion,blocked_food,blocked_coins,blocked_relics,blocked_tools,blocked_weapons,prepared,raids,empty_raids,tribute_events,rallies,rumors,offerings,seeds,defenses,saturated_days,empty_arrivals,loaded_arrivals\n",extra);
    CcSimInit(&s,(uint32_t)seed);
    CcMetricsHistory history={0};History species={0};
    const CcEvent *latest=CcSimRecentEvent(&s,0);
    if (latest) last_event=species.last_event=latest->id;
    history.minimum_active_settlements=s.settlement_count;
    for(int i=0;i<s.settlement_count;++i)
        history.settlement_was_abandoned[i]=old_abandoned[i]=CcSettlementIsAbandoned(&s.settlements[i]);
    for(int i=0;i<s.route_count;++i) history.route_was_closed[i]=s.routes[i].closed;
    Header(); PrintYear(&s,&history,seed,0,true); Year(seed,0,&species);
    char error[256]={0};
    for(int year=1;year<=years;++year) {
        for(int day=0;day<CC_SOLAR_DAYS;++day) {
            CcDragonLifeStage stage=s.dragon.life_stage;
            int32_t crown=s.goblin_politics.crown_faction;
            CcSimAdvanceDays(&s,1);UpdateDailyHistory(&s,&history);Daily(seed,&species);
            if (stage!=s.dragon.life_stage) EventLine(CcCalendar(s.current_day).year,"dragon-stage",s.current_day,CcDragonLifeStageName(s.dragon.life_stage));
            if (crown!=s.goblin_politics.crown_faction) EventLine(CcCalendar(s.current_day).year,"goblin-crown",s.current_day,CcGoblinColorName(s.goblin_politics.crown_faction));
            if (day%7==0 && !CcSimValidate(&s,error,sizeof(error))) {fprintf(stderr,"seed=%d day=%d invalid=%s\n",seed,s.current_day,error);return 1;}
        }
        UpdateHistory(&s,&history); PrintYear(&s,&history,seed,year,true); Year(seed,year,&species);
        if (year%500==0) {fflush(stdout);fflush(extra);fprintf(stderr,"seed=%d completed_year=%d\n",seed,year);}
    }
    unsigned char *bytes=NULL; size_t length=0;
    bool valid=CcSimValidate(&s,error,sizeof(error)) && CcSaveEncode(&s,&bytes,&length,error,sizeof(error)) &&
        CcSaveDecode(bytes,length,&restored,error,sizeof(error));
    CcSaveFreeBuffer(bytes);
    bool same=valid && CcSimHash(&s)==CcSimHash(&restored);
    char path[512];snprintf(path,sizeof(path),"%s-final.db",prefix);
    if (same) valid=CcSaveWrite(path,&s,error,sizeof(error));
    fprintf(stderr,"seed=%d years=%d valid=%d save_reload_equal=%d event_saturation_days=%llu hash=%llu error=%s\n",
        seed,years,valid,same,(unsigned long long)species.saturated_days,(unsigned long long)CcSimHash(&s),error);
    bool write_error=ferror(stdout)||ferror(events)||ferror(counts)||ferror(extra)||ferror(bands)||ferror(towns);
    fclose(events);fclose(counts);fclose(extra);fclose(bands);fclose(towns);
    return valid&&same&&!write_error?0:1;
}
