#include "sim/cc_scriven.h"
#include "sim/cc_census.h"
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
    char town_name[sizeof(world->settlements[town].name)];
    memcpy(town_name,world->settlements[town].name,sizeof(town_name));
    town_name[sizeof(town_name)-1]='\0';
    (void)snprintf(t->name,sizeof(t->name),"Annal of %s",town_name);
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
    CcCensusReconcile(&sim);
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
    /* A closer account in the first source still permits a second, independent source. */
    ba->notes[1]=bb->notes[1];ba->notes[1].source_book_id=a->id;ba->notes[1].day=170;
    (void)snprintf(ba->notes[1].text,144,"Day 170: the first field party saw a Deep Wyrm.");
    sim.scriven.finding=(CcScrivenFinding){0};
    CHECK(Apply(CC_SCRIVEN_HEARING,0));
    CHECK(sim.scriven.finding.earliest_day==101 && sim.scriven.finding.latest_day==180);
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
            CHECK(CcSimReadableTome(&sim,sim.scriven.delegates[i].person_id)==sim.scriven.delegates[i].book_id);
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
    CHECK(!Apply(CC_SCRIVEN_SKY,0));
    CHECK(Apply(CC_SCRIVEN_WAIT,0));CHECK(sim.current_day==day);
    CHECK(!Apply(CC_SCRIVEN_SKY,0));
    CHECK(Apply(CC_SCRIVEN_WAIT,0));CHECK(Apply(CC_SCRIVEN_SKY,0));
    CHECK(CcScrivenBookById(&sim,id)->notes[3].kind==CC_SCRIVEN_NOTE_SKY);
    CHECK(Apply(CC_SCRIVEN_WAIT,0));
    CHECK(sim.current_day==day+1 && sim.clock.minute_subticks==0);
    CHECK(RoundTrip());
    return true;
}
static bool FieldNotes(void)
{
    CcSimInit(&sim,41);
    sim.player.cargo_capacity=100;
    sim.dragon.lair_settlement_id=sim.player.location_id;
    sim.dragon.life_stage=CC_DRAGON_STAGE_CROWNED;
    CcTreasure *t=Tome(&sim,0);CHECK(t!=NULL);
    uint64_t before=CcSimHash(&sim);
    CHECK(!Apply(CC_SCRIVEN_OBSERVE,t->id) && CcSimHash(&sim)==before);
    CHECK(Apply(CC_SCRIVEN_BORROW,t->id));
    int day=sim.current_day;
    CHECK(Apply(CC_SCRIVEN_OBSERVE,t->id));
    const CcScrivenBook *b=CcScrivenBookById(&sim,t->id);
    CHECK(sim.current_day==day+1 && b->notes[0].day==day);
    CHECK(sim.crown_calendar.sighting_count==1 && sim.crown_calendar.sightings[0].day==day);
    CHECK(sim.crown_calendar.company_sighting.day==day && sim.crown_calendar.editions==0);
    char description[512]; CcScrivenDescribe(&sim,description,sizeof(description));
    CHECK(strstr(description,"Crown Age year 1") && strstr(description,"Scribes will compare"));
    CHECK(b->notes[2].kind==CC_SCRIVEN_NOTE_GOBLINS);
    before=CcSimHash(&sim);
    CHECK(!Apply(CC_SCRIVEN_OBSERVE,t->id) && CcSimHash(&sim)==before);
    CHECK(RoundTrip());
    sim.current_day=CC_SIM_MAX_DAY;
    before=CcSimHash(&sim);
    CHECK(!Apply(CC_SCRIVEN_COMMISSION,0) && CcSimHash(&sim)==before);
    CHECK(!Apply(CC_SCRIVEN_COPY,t->id) && CcSimHash(&sim)==before);
    CHECK(!Apply(CC_SCRIVEN_OBSERVE,t->id) && CcSimHash(&sim)==before);
    return true;
}
static bool Schema112(void)
{
    CcSimInit(&sim,717);
    CcTreasure *t=Tome(&sim,0);CHECK(t!=NULL);
    CcId id=t->id;
    sim.schema_version=112;
    memset(&sim.scriven,0,sizeof(sim.scriven));
    uint64_t before=CcSimHash(&sim);
    unsigned char *data=NULL;size_t length=0;
    CHECK(CcSaveEncode(&sim,&data,&length,error,sizeof(error)));
    CHECK(CcSaveDecode(data,length,&copy,error,sizeof(error)) || (fprintf(stderr,"%s\n",error),false));
    CcSaveFreeBuffer(data);
    CHECK(copy.schema_version==CC_SIM_SCHEMA_VERSION);
    CHECK(CcScrivenBookById(&copy,id)!=NULL && copy.scriven.age_count==0);
    copy.schema_version=112;
    copy.next_entity_serial -= CcCensusIssuedIdCount(&copy.census);
    CHECK(CcSimHash(&copy)==before);
    return true;
}
static bool CrownAges(void)
{
    CcSimInit(&sim,313); sim.current_day=196;
    sim.settlements[0].stock[CC_GOOD_PAPER]=100;
    sim.settlements[0].stock[CC_GOOD_WHEAT]=100;
    sim.settlements[0].stock[CC_GOOD_TOOLS]=100;
    sim.player.cargo_capacity=100; sim.player.coins=100;
    CcTreasure *a=Tome(&sim,0),*b=Tome(&sim,2),*held=Tome(&sim,0),*lost=Tome(&sim,3);
    CHECK(a && b && held && lost);
    CcScrivenBook *ba=(CcScrivenBook *)CcScrivenBookById(&sim,a->id);
    CcScrivenBook *bb=(CcScrivenBook *)CcScrivenBookById(&sim,b->id);
    CcScrivenBook *bl=(CcScrivenBook *)CcScrivenBookById(&sim,lost->id);
    ba->notes[0]=(CcScrivenNote){.dragon_id=sim.dragon.id,.author_id=sim.characters[0].id,
        .place_id=sim.dragon.lair_settlement_id,.source_book_id=a->id,.day=100,.kind=CC_SCRIVEN_NOTE_CROWNED};
    bb->notes[0]=ba->notes[0];
    bl->notes[0]=ba->notes[0]; bl->notes[0].day=40;
    bl->notes[0].source_book_id=lost->id; bl->notes[0].author_id=sim.characters[2].id;
    sim.scriven.host_id=sim.settlements[0].id; sim.scriven.status=2;
    sim.scriven.meeting_year=0; sim.scriven.opens_day=196; sim.scriven.closes_day=202;
    sim.player.location_id=sim.scriven.host_id;
    held->owner_id=sim.player.id; ++sim.player.treasure_cargo_slots;
    for(int i=0;i<2;++i) {
        CcTreasure *t=i==0?a:b;
        sim.scriven.delegates[i]=(CcScrivenDelegate){.person_id=sim.characters[i].id,.book_id=t->id,
            .home_id=t->owner_id,.place_id=sim.scriven.host_id,.phase=CC_SCRIVEN_ATTENDING,.notice_day=140};
        t->location_id=sim.characters[i].id;
        sim.characters[i].current_settlement_id=sim.scriven.host_id;
        sim.characters[i].travel_destination_id=0;
    }
    CHECK(Apply(CC_SCRIVEN_HEARING,0) && sim.crown_calendar.editions==0);
    bb->notes[0].source_book_id=b->id;
    CHECK(Apply(CC_SCRIVEN_HEARING,0) && sim.crown_calendar.editions==0); /* Same witness. */
    bb->notes[0].author_id=sim.characters[1].id; bb->notes[0].day=120;
    sim.scriven.delegates[1].place_id=sim.settlements[2].id;
    CHECK(Apply(CC_SCRIVEN_HEARING,0) && sim.crown_calendar.editions==0);
    sim.scriven.delegates[1].place_id=sim.scriven.host_id;
    CHECK(Apply(CC_SCRIVEN_HEARING,0) && sim.crown_calendar.editions==1);
    CHECK(sim.crown_calendar.company.proposed_day==100 && sim.crown_calendar.company.agreed_day==196);
    CHECK(sim.crown_calendar.company.earliest_day==100 && sim.crown_calendar.company.latest_day==100);
    CHECK(sim.crown_calendar.local[2].agreed_day==0);
    /* The older page is still in another town. The register carries no public authority. */
    CcCrownCalendarInit(&sim);
    CHECK(sim.crown_calendar.sightings[0].day==40);
    CHECK(Apply(CC_SCRIVEN_HEARING,0) && sim.crown_calendar.company.proposed_day==100);
    CcScrivenFinding old=sim.crown_calendar.company;
    CHECK(Apply(CC_SCRIVEN_COPY,held->id));
    CcId copy_id=sim.treasures[sim.treasure_count-1].id;
    const CcScrivenBook *copied=CcScrivenBookById(&sim,copy_id);
    CHECK(sim.crown_calendar.book_editions[copied-sim.scriven.books]==1);
    /* An earlier brought original revises the date and preserves the prior edition. */
    lost->location_id=sim.scriven.host_id;
    CHECK(Apply(CC_SCRIVEN_HEARING,0));
    CHECK(sim.crown_calendar.editions==2 && sim.crown_calendar.company.proposed_day==40);
    CHECK(memcmp(&old,&sim.crown_calendar.almanacs[0],sizeof(old))==0);
    CHECK(sim.crown_calendar.book_editions[copied-sim.scriven.books]==1);
    CHECK(Apply(CC_SCRIVEN_READ,copy_id) && sim.crown_calendar.company.proposed_day==100);
    CHECK(Apply(CC_SCRIVEN_READ,held->id) && sim.crown_calendar.company.proposed_day==40);
    bb->notes[1]=bb->notes[0]; bb->notes[1].kind=CC_SCRIVEN_NOTE_DEEP; bb->notes[1].day=180;
    CHECK(Apply(CC_SCRIVEN_HEARING,0));
    CHECK(sim.scriven.finding.agreed_day>0 && sim.crown_calendar.company.proposed_day==40);
    sim.scriven.company=sim.scriven.finding;
    char description[1024]; CcScrivenDescribe(&sim,description,sizeof(description));
    CHECK(strstr(description,"Crown Age year 1") && strstr(description,"Deep Wyrm Epoch year 1"));
    CHECK(RoundTrip());
    sim.player.location_id=sim.settlements[1].id;
    uint64_t before=CcSimHash(&sim);
    CHECK(!Apply(CC_SCRIVEN_DELIVER,0) && CcSimHash(&sim)==before);
    held->location_id=sim.player.location_id;
    CHECK(Apply(CC_SCRIVEN_DELIVER,0));
    CHECK(sim.crown_calendar.local[1].proposed_day==40);
    CHECK(sim.scriven.local[1].proposed_day==sim.scriven.company.proposed_day);
    held->location_id=sim.scriven.host_id; sim.player.location_id=sim.scriven.host_id;
    sim.current_day=203;
    sim.scriven.delegates[1].place_id=sim.settlements[2].id;
    sim.characters[1].current_settlement_id=sim.settlements[2].id;
    sim.scriven.delegates[1].phase=CC_SCRIVEN_RETURNING;
    CcScrivenAdvance(&sim);
    CHECK(sim.crown_calendar.local[2].proposed_day==40);
    /* Reusing a destroyed tome's object slot clears its carried edition. */
    CcId old_id=a->id;
    int slot=(int)(ba-sim.scriven.books);
    CHECK(sim.crown_calendar.book_editions[slot]>0);
    sim.scriven.delegates[0].phase=CC_SCRIVEN_FINISHED; a->destroyed=true;
    sim.dragon.life_stage=CC_DRAGON_STAGE_CROWNED; sim.dragon.age_days=1200*365;
    sim.dragon.crown_continuity_days=800*365; sim.dragon.territory_stability=100;
    sim.dragon.memory_integrity=100; sim.dragon_cult.devotion=100;
    sim.dragon.hoard=5000; sim.dragon.hoard_goods[CC_GOOD_GOLD]=10; sim.dragon.hoard_goods[CC_GOOD_GEMS]=10;
    CcSimAdvanceDays(&sim,1);
    CHECK(sim.dragon.life_stage==CC_DRAGON_STAGE_DEEP_WYRM);
    CHECK(CcScrivenBookById(&sim,old_id)==NULL && sim.crown_calendar.book_editions[slot]==0);
    return true;
}
static bool CrownCodecAndLegacy(void)
{
    uint8_t bytes[CC_SCRIVEN_WIRE_CAPACITY],again[CC_SCRIVEN_WIRE_CAPACITY];
    CcCrownCalendar recovered;
    size_t length=CcCrownCalendarEncode(&sim.crown_calendar,bytes,sizeof(bytes)); CHECK(length>0);
    CHECK(CcCrownCalendarDecode(&recovered,bytes,length));
    CHECK(CcCrownCalendarEncode(&recovered,again,sizeof(again))==length && memcmp(bytes,again,length)==0);
    CHECK(!CcCrownCalendarDecode(&recovered,bytes,length-1));
    CHECK(!CcCrownCalendarDecode(&recovered,bytes,length+1));
    bytes[0]=99; CHECK(!CcCrownCalendarDecode(&recovered,bytes,length)); bytes[0]=1;
    uint64_t expected=CcCrownCalendarHash(&sim.crown_calendar);
    for(size_t i=4;i<length;++i) {
        bytes[i]^=1; CHECK(CcCrownCalendarDecode(&recovered,bytes,length));
        CHECK(CcCrownCalendarHash(&recovered)!=expected); bytes[i]^=1;
    }
    CcSimInit(&sim,42); sim.schema_version=114; CcSimAdvanceDays(&sim,364);
    CHECK(CcSimHash(&sim)==UINT64_C(13544121058959254427)); /* Captured from schema 114 code. */
    unsigned char *data=NULL; size_t size=0;
    CHECK(CcSaveEncode(&sim,&data,&size,error,sizeof(error)));
    CHECK(CcSaveDecode(data,size,&copy,error,sizeof(error))); CcSaveFreeBuffer(data);
    CHECK(copy.schema_version==CC_SIM_SCHEMA_VERSION && copy.crown_calendar.editions==0);
    copy.schema_version=114; CHECK(CcSimHash(&copy)==CcSimHash(&sim));
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
    if(!Calendar() || !Books() || !HearingFixture() || !Codec() || !Expeditions() || !WatchAndCopy() || !FieldNotes() || !Schema112() || !CrownAges() || !CrownCodecAndLegacy() || !DailyReplay()) return 1;
    puts("Calendar, frozen passages, loans, evidence boundaries, codec, and daily replay passed.");
    return 0;
}
