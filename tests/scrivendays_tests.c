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
    if(!Calendar() || !Books() || !HearingFixture() || !Codec() || !DailyReplay()) return 1;
    puts("Calendar, frozen passages, loans, evidence boundaries, codec, and daily replay passed.");
    return 0;
}
