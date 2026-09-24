#include "sim/cc_scriven.h"
#include "sim/cc_archive_volumes_internal.h"
#include "persistence/cc_save.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); return false; } } while (0)
static CcSim sim, copy;
static char error[256];
static CcTreasure *Tome(CcSim *world, int town)
{
    if (world->treasure_count >= CC_MAX_TREASURES) return NULL;
    CcTreasure *t = &world->treasures[world->treasure_count++];
    *t = (CcTreasure){.id=CcMakeId(CC_ENTITY_TREASURE,world->next_entity_serial++),
        .maker_settlement_id=world->settlements[town].id, .owner_id=world->settlements[town].id,
        .location_id=world->settlements[town].id,.created_day=world->current_day,.craft_work=1,.appraised_value=2};
    (void)snprintf(t->name,sizeof(t->name),"Annal of %s",world->settlements[town].name);
    CcScrivenFreeze(world,t->id);
    world->archives.lore_stored=CcSimArchivePhysicalLore(world);
    return t;
}
static bool Apply(int action,CcId book)
{
    CcCommand cmd={.kind=CC_COMMAND_SCRIVEN,.amount=action,.target_id=book};
    return CcSimApply(&sim,&cmd,error,sizeof(error));
}
static bool Calendar(void)
{
    CHECK(CcCalendar(0).day_of_sign == 1 && CcCalendar(0).sign == 0);
    CHECK(CcCalendar(363).sign == 12 && CcCalendar(363).day_of_sign == 28);
    CHECK(CcCalendar(364).year == 1 && CcCalendar(364).year_sign == 1);
    CHECK(CcCalendar(-1).year == -1 && CcCalendar(-1).day_of_year == 364);
    CHECK(CcCalendar(INT32_MIN).day_of_year >= 1);
    CHECK(CcCalendar(13*364).year_sign == 0);
    CHECK(CcCalendar(196).sign == 7 && CcCalendar(196).day_of_sign == 1);
    CHECK(CcCalendar(202).day_of_sign == 7);
    CHECK(CcCalendarWanderer(0,0) == CcCalendarWanderer(13*364,0));
    return true;
}
static bool RoundTrip(void)
{
    unsigned char *data=NULL;size_t length=0;
    sim.royal_trade_week=sim.current_day/7;
    CHECK(CcSimValidate(&sim,error,sizeof(error)) || (fprintf(stderr,"%s\n",error),false));
    CHECK(CcSaveEncode(&sim,&data,&length,error,sizeof(error)));
    CHECK(CcSaveDecode(data,length,&copy,error,sizeof(error)) || (fprintf(stderr,"%s\n",error),false));
    CcSaveFreeBuffer(data);
    CHECK(CcSimHash(&sim) == CcSimHash(&copy));
    return true;
}
static bool Books(void)
{
    CcSimInit(&sim,717);
    CcTreasure *t=Tome(&sim,0);CHECK(t != NULL);
    CcId id=t->id,owner=t->owner_id;
    char before[144],after[144];
    CHECK(CcSimTomePassage(&sim,id,0,before,sizeof(before)));
    sim.settlements[0].population+=17;
    CHECK(CcSimTomePassage(&sim,id,0,after,sizeof(after)) && strcmp(before,after)==0);
    sim.player.location_id=owner;
    sim.player.cargo_capacity=100;
    CHECK(Apply(CC_SCRIVEN_BORROW,id));
    CHECK(t->owner_id==owner && t->location_id==sim.player.id);
    uint64_t held=CcSimHash(&sim);
    CHECK(!Apply(CC_SCRIVEN_BORROW,id) && CcSimHash(&sim)==held);
    CHECK(RoundTrip());
    CHECK(Apply(CC_SCRIVEN_READ,id));
    CHECK(Apply(CC_SCRIVEN_RETURN,id));
    CHECK(t->owner_id==owner && t->location_id==owner);
    CHECK(!Apply(CC_SCRIVEN_RETURN,id));
    CHECK(RoundTrip());
    return true;
}
static bool HearingFixture(void)
{
    CcSimInit(&sim,313);
    sim.current_day=196;
    CcTreasure *a=Tome(&sim,0),*b=Tome(&sim,2);CHECK(a && b);
    CcScrivenBook *ba=(CcScrivenBook *)CcScrivenBookById(&sim,a->id);
    CcScrivenBook *bb=(CcScrivenBook *)CcScrivenBookById(&sim,b->id);
    ba->notes[0]=(CcScrivenNote){.dragon_id=sim.dragon.id,.author_id=sim.characters[0].id,
        .place_id=sim.dragon.lair_settlement_id,.source_book_id=a->id,.day=100,.kind=CC_SCRIVEN_NOTE_CROWNED};
    bb->notes[1]=(CcScrivenNote){.dragon_id=sim.dragon.id,.author_id=sim.characters[1].id,
        .place_id=sim.dragon.lair_settlement_id,.source_book_id=b->id,.day=180,.kind=CC_SCRIVEN_NOTE_DEEP};
    (void)snprintf(ba->notes[0].text,144,"Day 100: the field party saw a Crowned Dragon.");
    (void)snprintf(bb->notes[1].text,144,"Day 180: the field party saw a Deep Wyrm.");
    sim.scriven.host_id=sim.settlements[0].id;sim.scriven.status=2;
    sim.scriven.meeting_year=0;sim.scriven.opens_day=196;sim.scriven.closes_day=202;
    sim.player.location_id=sim.scriven.host_id;
    for(int i=0;i<2;++i) {
        CcTreasure *t=i==0?a:b;
        sim.scriven.delegates[i]=(CcScrivenDelegate){.person_id=sim.characters[i].id,.book_id=t->id,
            .home_id=t->owner_id,.place_id=sim.scriven.host_id,.phase=CC_SCRIVEN_ATTENDING,.notice_day=140};
        t->location_id=sim.characters[i].id;
        sim.characters[i].current_settlement_id=sim.scriven.host_id;
        sim.characters[i].travel_destination_id=0;
    }
    CHECK(Apply(CC_SCRIVEN_HEARING,0));
    CHECK(sim.scriven.finding.earliest_day==101 && sim.scriven.finding.latest_day==180);
    CHECK(sim.scriven.finding.proposed_day==140 && sim.scriven.finding.schools==2);
    CHECK(sim.scriven.company.agreed_day==0); /* Written copy needs a carried tome. */
    CcScrivenFinding first=sim.scriven.finding;
    sim.scriven.ages[0]=(CcScrivenAge){sim.dragon.id,175};sim.scriven.age_count=1;
    CHECK(Apply(CC_SCRIVEN_HEARING,0));
    CHECK(memcmp(&first,&sim.scriven.finding,sizeof(first))==0 && sim.scriven.editions==1);
    /* Moving the second book away removes it from a new hearing. */
    sim.scriven.finding=(CcScrivenFinding){0};
    sim.scriven.delegates[1].phase=CC_SCRIVEN_OUTWARD;
    sim.scriven.delegates[1].place_id=sim.settlements[2].id;
    CHECK(Apply(CC_SCRIVEN_HEARING,0));CHECK(sim.scriven.finding.agreed_day==0);
    /* A copy shares the original witness and fails the independent-source test. */
    sim.scriven.delegates[1].phase=CC_SCRIVEN_ATTENDING;
    sim.scriven.delegates[1].place_id=sim.scriven.host_id;
    bb->notes[1].source_book_id=a->id;
    CHECK(Apply(CC_SCRIVEN_HEARING,0));CHECK(sim.scriven.finding.agreed_day==0);
    return true;
}
static bool Expeditions(void)
{
    CcSimInit(&sim,91);
    for(int i=0;i<sim.settlement_count;++i) {
        CcSettlement *town=&sim.settlements[i];
        town->hunger=0;town->security=100;
        town->stock[CC_GOOD_WHEAT]=500;town->stock[CC_GOOD_PAPER]=100;town->stock[CC_GOOD_TOOLS]=100;
        CcCharacter *p=NULL;
        for(int j=0;j<sim.character_count;++j) {
            CcCharacter *candidate=&sim.characters[j];
            bool office=candidate->id==sim.archives.abbot_character_id;
            for(int k=0;k<sim.kingdom_count;++k) office |= candidate->id==sim.kingdoms[k].ruler_character_id || candidate->id==sim.kingdoms[k].monastery_patron_id;
            if(!office && candidate->home_settlement_id==town->id && candidate->birth_day<0) {p=candidate;break;}
        }
        CHECK(p!=NULL);
        p->occupation=CC_OCCUPATION_SCRIBE;p->activity=CC_CHARACTER_ACTIVITY_WORKING;
        p->home_settlement_id=town->id;p->current_settlement_id=town->id;
        p->birth_day=-30*365;p->death_day=2000;p->travel_coins=1000;p->bandit_group_id=0;
        p->travel_destination_id=0;p->travel_arrival_day=0;
    }
    for(int i=0;i<sim.route_count;++i) {sim.routes[i].closed=false;sim.routes[i].condition=100;}
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)sim.diplomacy[i][j]=CC_DIPLOMACY_PEACE;
    sim.current_day=140;sim.dragon.life_stage=CC_DRAGON_STAGE_CROWNED;
    CcScrivenAdvance(&sim);
    CcId first_host=sim.scriven.host_id;CHECK(first_host!=0);
    bool saved_traveller=false;
    for(int day=141;day<=260;++day) {
        sim.current_day=day;
        CcScrivenAdvance(&sim);
        if(!saved_traveller) for(int i=0;i<6;++i) if(sim.scriven.delegates[i].route_id!=0) {
            CHECK(RoundTrip());saved_traveller=true;break;
        }
    }
    CHECK(saved_traveller && sim.scriven.meetings==1 && sim.scriven.returns>=2);
    int noted=0;
    for(int i=0;i<CC_SCRIVEN_BOOKS;++i) if(sim.scriven.books[i].notes[0].kind==CC_SCRIVEN_NOTE_CROWNED) ++noted;
    CHECK(noted>=2);
    sim.current_day=300;sim.dragon.life_stage=CC_DRAGON_STAGE_DEEP_WYRM;CcScrivenAnchor(&sim);
    for(int day=504;day<=624;++day) {sim.current_day=day;CcScrivenAdvance(&sim);}
    CHECK(sim.scriven.host_id!=first_host && sim.scriven.meetings==2);
    CHECK(sim.scriven.finding.agreed_day>0);
    CHECK(sim.scriven.finding.earliest_day<=300 && sim.scriven.finding.latest_day>=300);
    CHECK(sim.scriven.returns>=4 && sim.scriven.editions==1);
    CHECK(sim.scriven.ages[0].first_deep_day==300 && sim.scriven.finding.proposed_day!=300);
    int adopted=0;
    for(int i=0;i<6;++i) if(sim.scriven.local[i].agreed_day>0) ++adopted;
    CHECK(adopted>=2);
    CHECK(RoundTrip());
    return true;
}
static bool WatchAndCopy(void)
{
    CcSimInit(&sim,41);
    sim.player.cargo_capacity=100;sim.player.coins=100;
    sim.settlements[0].stock[CC_GOOD_PAPER]=10;sim.settlements[0].stock[CC_GOOD_WHEAT]=20;sim.settlements[0].stock[CC_GOOD_TOOLS]=10;
    CcTreasure *t=Tome(&sim,0);CHECK(t!=NULL);
    CcId id=t->id;CHECK(Apply(CC_SCRIVEN_BORROW,id));
    CHECK(Apply(CC_SCRIVEN_COPY,id));
    CHECK(CcScrivenBookById(&sim,id)->borrower_id==sim.player.id);
    CHECK(sim.player.treasure_cargo_slots==2);
    CHECK(RoundTrip());
    int day=sim.current_day;
    sim.clock.minute_subticks=0;
    CHECK(Apply(CC_SCRIVEN_SKY,0));
    CHECK(Apply(CC_SCRIVEN_WAIT,0));CHECK(sim.current_day==day);
    CHECK(!Apply(CC_SCRIVEN_SKY,0));
    CHECK(Apply(CC_SCRIVEN_WAIT,0));CHECK(Apply(CC_SCRIVEN_WAIT,0));
    CHECK(sim.current_day==day+1 && sim.clock.minute_subticks==0);
    CHECK(RoundTrip());
    return true;
}
static bool Codec(void)
{
    uint8_t bytes[CC_SCRIVEN_WIRE_CAPACITY],again[CC_SCRIVEN_WIRE_CAPACITY];
    CcScrivenState recovered;
    size_t length=CcScrivenEncode(&sim.scriven,bytes,sizeof(bytes));CHECK(length>0);
    CHECK(CcScrivenDecode(&recovered,bytes,length));
    CHECK(CcScrivenEncode(&recovered,again,sizeof(again))==length && memcmp(bytes,again,length)==0);
    CHECK(!CcScrivenDecode(&recovered,bytes,length-1));
    CHECK(!CcScrivenDecode(&recovered,bytes,length+1));
    bytes[0]=99;CHECK(!CcScrivenDecode(&recovered,bytes,length));
    /* Every encoded field byte contributes to the hash, including empty slots. */
    length=CcScrivenEncode(&sim.scriven,bytes,sizeof(bytes));
    uint64_t expected=CcScrivenHash(&sim.scriven);
    for(size_t i=4;i<length;++i) {
        bytes[i]^=1;CHECK(CcScrivenDecode(&recovered,bytes,length));
        CHECK(CcScrivenHash(&recovered)!=expected);bytes[i]^=1;
    }
    return true;
}
static bool DailyReplay(void)
{
    CcSimInit(&sim,41);copy=sim;
    CcSimAdvanceDays(&sim,400);
    for(int i=0;i<400;++i) CcSimAdvanceDays(&copy,1);
    CHECK(CcSimHash(&sim)==CcSimHash(&copy));
    CHECK(RoundTrip());
    return true;
}
int main(void)
{
    if(!Calendar() || !Books() || !HearingFixture() || !Codec() || !Expeditions() || !WatchAndCopy() || !DailyReplay()) return 1;
    puts("Calendar, frozen passages, loans, evidence boundaries, codec, and daily replay passed.");
    return 0;
}
