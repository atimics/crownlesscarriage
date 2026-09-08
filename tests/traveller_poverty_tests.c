#include "persistence/cc_save.h"
#include "test_support.h"
#include <stdio.h>
static CcSim sim, restored;
static int person_slot;
static CcSettlement *place;
static char error[256];
static CcCharacter *person(void) { return &sim.characters[person_slot]; }
static void prepare(void) {
    CcSimInit(&sim,42U);
    person_slot=-1;
    for(int i=0;i<sim.character_count;++i) {
        if(sim.characters[i].role==CC_CHARACTER_TRAVELLER && person_slot<0)person_slot=i;
    }
    CC_CHECK(person_slot>=0);
    const CcRoute *road=CcSimRoute(&sim,sim.bandits[0].route_id);
    CC_CHECK(road!=NULL);
    place=CcSimSettlementMutable(&sim,road->from_id);
    person()->home_settlement_id=road->to_id;
    person()->current_settlement_id=place->id;
    person()->birth_day=sim.current_day-30*365;
    person()->appearance_seed=3U; /* Third hungry night is day 4. */
    person()->activity=CC_CHARACTER_ACTIVITY_SEEKING_AID;
    place->prosperity=0;
    place->service_mask|=UINT32_C(1)<<CC_SERVICE_INN;
    for(int g=0;g<CC_GOOD_COUNT;++g)if(CcGoodNutritionValue((CcGood)g,CC_NUTRITION_CIVILIAN)>0)place->stock[g]=0;
    place->price[CC_GOOD_BREAD]=1;
}
static void fund(CcMoney wanted) {
    CcMoney amount=wanted-person()->travel_coins;
    CC_CHECK(amount>=0 && place->market_coins>=amount);
    place->market_coins-=amount;person()->travel_coins+=amount;
}
static void advance(void) {
    CcMoney gold=CcSimTrackedGold(&sim);
    CcSimAdvanceDays(&sim,1);
    CC_CHECK(CcSimTrackedGold(&sim)==gold);
    if(!CcSimValidate(&sim,error,sizeof(error))){fprintf(stderr,"%s\n",error);CC_CHECK(false);}
}
int main(void) {
    /* Food shortage can recruit a traveller who still has money for a bed. */
    prepare();fund(20);
    int members=sim.bandits[0].members;
    advance();advance();CC_CHECK(person()->bandit_group_id==0U);
    advance();CC_CHECK(person()->bandit_group_id==sim.bandits[0].id);
    CC_CHECK(sim.bandits[0].members==members+1);
    CC_CHECK(person()->hungry_days==3 && person()->unsheltered_nights==0);
    CcId recruit=person()->id;
    const CcEvent *event=CcSimRecentEvent(&sim,0);
    bool found=false;
    for(int i=0;i<sim.event_count;++i){event=CcSimRecentEvent(&sim,i);if(event->kind==CC_EVENT_BANDIT_PRESSURE && event->actor_id==recruit)found=true;}
    CC_CHECK(found);
    const char *path="traveller-poverty.ccsave";
    CC_CHECK(CcSaveWrite(path,&sim,error,sizeof(error)));
    CC_CHECK(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(CcSimHash(&sim)==CcSimHash(&restored));
    sim=restored;advance();advance();
    CC_CHECK(sim.bandits[0].members==members+1);
    CC_CHECK(person()->bandit_group_id==sim.bandits[0].id);
    CcMoney inheritance=person()->travel_coins;
    person()->death_day=sim.current_day+1;advance();
    CC_CHECK(person()->id!=recruit && person()->bandit_group_id==0U);
    CC_CHECK(person()->travel_coins==inheritance);
    CC_CHECK(remove(path)==0);

    /* A traveller who can buy food but cannot pay for lodging seeks a camp. */
    prepare();place->stock[CC_GOOD_BREAD]=100;
    for(int d=0;d<3;++d){fund(1);advance();}
    CC_CHECK(person()->hungry_days==0 && person()->unsheltered_nights==3);
    CC_CHECK(person()->bandit_group_id==sim.bandits[0].id);

    /* Food and a paid bed keep hardship at zero. */
    prepare();place->stock[CC_GOOD_BREAD]=100;fund(20);
    advance();advance();advance();
    CC_CHECK(person()->hungry_days==0 && person()->unsheltered_nights==0);
    CC_CHECK(person()->bandit_group_id==0U);

    /* Work in a prosperous town covers the food and lodging bill. */
    prepare();place->stock[CC_GOOD_BREAD]=100;place->prosperity=50;
    advance();advance();advance();
    CC_CHECK(person()->hungry_days==0 && person()->unsheltered_nights==0);
    CC_CHECK(person()->bandit_group_id==0U);

    /* Relief before the third day clears the pressure. */
    prepare();advance();advance();
    CC_CHECK(person()->hungry_days==2);
    CC_CHECK(CcSaveWrite(path,&sim,error,sizeof(error)));
    CC_CHECK(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(restored.characters[person_slot].hungry_days==2);
    place->stock[CC_GOOD_BREAD]=100;fund(20);advance();
    CC_CHECK(person()->hungry_days==0 && person()->bandit_group_id==0U);
    CC_CHECK(remove(path)==0);

    /* The person's own inhabited home provides the existing household support. */
    prepare();person()->current_settlement_id=person()->home_settlement_id;
    advance();advance();advance();CC_CHECK(person()->bandit_group_id==0U);

    /* Historical saves retain their old rules and upgrade with empty purses. */
    prepare();sim.schema_version=57U;advance();advance();advance();
    CC_CHECK(person()->travel_coins==0 && person()->bandit_group_id==0U);
    CC_CHECK(CcSaveWrite(path,&sim,error,sizeof(error)));
    CC_CHECK(CcSaveRead(path,&restored,error,sizeof(error)));
    CC_CHECK(restored.schema_version==CC_SIM_SCHEMA_VERSION);
    CC_CHECK(restored.characters[person_slot].hungry_days==0);
    CC_CHECK(remove(path)==0);
    puts("Traveller food, lodging, recruitment, inheritance, and save checks passed");
    return 0;
}
