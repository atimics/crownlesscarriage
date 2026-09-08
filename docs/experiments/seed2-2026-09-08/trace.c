/* Read-only daily observer for the frozen long-sweep simulation. */
#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static void quoted(const char *s) {
    putchar('"');
    for (; *s; ++s) {
        if (*s == '"' || *s == '\\') putchar('\\');
        if ((unsigned char)*s < 32) printf("\\u%04x", (unsigned char)*s);
        else putchar(*s);
    }
    putchar('"');
}
static void state(const CcSim *s, const char *kind) {
    const CcDragon *d = &s->dragon;
    printf("{\"kind\":\"%s\",\"day\":%d,\"stage\":", kind, s->current_day);
    quoted(CcDragonLifeStageName(d->life_stage));
    printf(",\"memory\":%d,\"crown\":%d,\"territory\":%d,\"continuity\":%d,\"debt\":%" PRId64 ",\"treasures\":%d,\"serial\":%" PRIu64 "}\n",
        d->memory_integrity, d->crown_strength, d->territory_stability,
        d->crown_continuity_days, d->stolen_outstanding, s->treasure_count, s->next_entity_serial);
}
static void treasure(const CcSim *s, const CcTreasure *t, const char *kind) {
    printf("{\"kind\":\"%s\",\"day\":%d,\"id\":%" PRIu64 ",\"name\":", kind,s->current_day,t->id);
    quoted(t->name);
    printf(",\"created_day\":%d,\"value\":%d,\"owner\":%" PRIu64 ",\"location\":%" PRIu64 ",\"maker\":%" PRIu64 ",\"destroyed\":%s}\n",t->created_day,t->appraised_value,t->owner_id,t->location_id,t->maker_settlement_id,t->destroyed?"true":"false");
}
int main(int argc, char **argv) {
    int years = 140000;
    if (argc == 2) {
        char *end = NULL;
        long value = strtol(argv[1], &end, 10);
        if (*end || value < 1 || value > 140000) return 2;
        years = (int)value;
    } else if (argc != 1) return 2;
    CcSim *s=calloc(1,sizeof(*s));
    if (!s) return 2;
    CcSimInit(s,UINT32_C(2)*UINT32_C(0x9e3779b9));
    for(int i=0;i<s->settlement_count;++i) {
        printf("{\"kind\":\"place\",\"id\":%" PRIu64 ",\"name\":",s->settlements[i].id);
        quoted(s->settlements[i].name); puts("}");
    }
    printf("{\"kind\":\"dragon\",\"id\":%" PRIu64 ",\"name\":",s->dragon.id); quoted(s->dragon.name); puts("}");
    for(int i=0;i<s->treasure_count;++i) treasure(s,&s->treasures[i],"initial");
    state(s,"initial_state");
    for(int year=1;year<=years;++year) {
        for(int day=0;day<365;++day) {
            CcTreasure before[CC_MAX_TREASURES];
            int count=s->treasure_count;
            for(int i=0;i<count;++i) before[i]=s->treasures[i];
            CcDragonLifeStage stage=s->dragon.life_stage;
            /* Observe the inputs before the first territory-loss cycle. */
            if (years == 8000 && year >= 7200 && year <= 7330 &&
                (s->current_day + 1) % 28 == 0) {
                const CcSettlement *lair = CcSimSettlement(s,s->dragon.lair_settlement_id);
                bool war = false;
                for (int i=0;lair && i<s->kingdom_count;++i)
                    if(s->kingdoms[i].id != lair->kingdom_id &&
                       CcSimKingdomsAtWar(s,lair->kingdom_id,s->kingdoms[i].id)) war=true;
                printf("{\"kind\":\"ecology\",\"day\":%d,\"territory\":%d,\"bread\":%d,\"wheat\":%d,\"meat\":%d,\"tools\":%d,\"devotion\":%d,\"cohesion\":%d,\"war\":%s}\n",
                    s->current_day,s->dragon.territory_stability,
                    s->goblins.lair_stock[CC_GOOD_BREAD],s->goblins.lair_stock[CC_GOOD_WHEAT],
                    s->goblins.lair_stock[CC_GOOD_MEAT],s->goblins.lair_stock[CC_GOOD_TOOLS],
                    s->goblins.devotion,s->goblins.cohesion,war?"true":"false");
            }
            CcSimAdvanceDays(s,1);
            bool changed=stage!=s->dragon.life_stage;
            if(changed) state(s,"stage");
            for(int i=0;i<s->treasure_count;++i) {
                const CcTreasure *t=&s->treasures[i];
                if(i>=count || t->id!=before[i].id) {treasure(s,t,"created"); changed=true;}
                else if(t->destroyed!=before[i].destroyed || t->owner_id!=before[i].owner_id || t->location_id!=before[i].location_id) treasure(s,t,"moved_or_destroyed");
            }
            if(changed) for(int i=0;i<12;++i) {
                const CcEvent *e=CcSimRecentEvent(s,i);
                if(!e || e->day<s->current_day) break;
                printf("{\"kind\":\"event\",\"day\":%d,\"text\":",e->day); quoted(e->text); puts("}");
            }
        }
        char error[192];
        if(!CcSimValidate(s,error,sizeof(error))) {fprintf(stderr,"year=%d %s\n",year,error);free(s);return 1;}
        if(year%1000==0) {state(s,"checkpoint");fflush(stdout);}
        if(year==128000 || year==140000) for(int i=0;i<s->treasure_count;++i) treasure(s,&s->treasures[i],"inventory");
    }
    free(s); return 0;
}
