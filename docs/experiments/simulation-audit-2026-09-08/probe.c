#include "sim/cc_sim.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void quote(const char *s) {
    putchar('"');for(;*s;++s){if(*s=='"'||*s=='\\')putchar('\\');if((unsigned char)*s<32)printf("\\u%04x",(unsigned char)*s);else putchar(*s);}putchar('"');
}
static void snapshot(const CcSim *s,const char *label) {
    CcMaterialChainSnapshot a=CcSimMaterialChainSnapshot(s);
    printf("{\"kind\":\"snapshot\",\"label\":\"%s\",\"day\":%d,\"reserve\":%" PRId64 ",\"scribes\":%d,\"lore\":%d,\"archive_blocker\":%d,\"archive_wheat\":%d,\"archive_paper\":%d,\"archive_tools\":%d,\"treasures\":%d,\"goblin_members\":%d,\"goblin_tributes\":%d,\"bandit_members\":%d,\"bandit_supplies\":%d,\"bandit_raids\":%d,\"dragon_influence\":%d}\n",label,s->current_day,s->iron_ledger_reserve,a.scribes,s->archives.lore_stored,(int)a.blocker,a.wheat,a.paper,a.tools,s->treasure_count,s->goblins.members,s->goblins.tributes_delivered,s->bandits[0].members,s->bandits[0].supplies,s->bandits[0].raids_completed,s->dragon.regional_influence);
    for(int i=0;i<s->settlement_count;++i){const CcSettlement *p=&s->settlements[i];printf("{\"kind\":\"town\",\"label\":\"%s\",\"name\":",label);quote(p->name);printf(",\"abandoned\":%s,\"population\":%d,\"hunger\":%d,\"security\":%d,\"is_scriptorium\":%s}\n",CcSettlementIsAbandoned(p)?"true":"false",p->population,p->hunger,p->security,p->id==a.scriptorium_id?"true":"false");}
    for(int i=0;i<s->kingdom_count;++i)for(int j=i+1;j<s->kingdom_count;++j)printf("{\"kind\":\"diplomacy\",\"label\":\"%s\",\"first\":%d,\"second\":%d,\"state\":%d,\"since_day\":%d}\n",label,i,j,(int)s->diplomacy[i][j],s->diplomacy_changed_day[i][j]);
    for(int i=0;i<s->treasure_count;++i){const CcTreasure *t=&s->treasures[i];if(t->destroyed)continue;printf("{\"kind\":\"treasure\",\"label\":\"%s\",\"name\":",label);quote(t->name);printf(",\"owner\":%" PRIu64 ",\"location\":%" PRIu64 ",\"value\":%d}\n",t->owner_id,t->location_id,t->appraised_value);}
}
static void window(const CcSim *initial,const char *label,int treatment) {
    CcSim *s=malloc(sizeof(*s));if(!s)exit(2);*s=*initial;
    int counts[256]={0},positive[256]={0};long long magnitude[256]={0};
    int observed_days=0; bool valid=true; int war_days=0,heard_days=0,staffed_days=0; CcId last_event=0;
    const CcEvent *e=CcSimRecentEvent(s,0);if(e)last_event=e->id;
    for(int d=0;d<36500;++d){
        if(treatment>=1)for(int i=0;i<s->route_count;++i){s->routes[i].closed=false;s->routes[i].condition=100;s->routes[i].security=100;}
        if(treatment==2){
            /* Diagnostic maintained support: transfer crowns and top up the archive kit. */
            if(s->iron_ledger_reserve<300)for(int k=0;k<s->kingdom_count && s->iron_ledger_reserve<300;++k){CcMoney n=300-s->iron_ledger_reserve;if(n>s->kingdoms[k].treasury)n=s->kingdoms[k].treasury;s->kingdoms[k].treasury-=n;s->iron_ledger_reserve+=n;}
            CcMaterialChainSnapshot a=CcSimMaterialChainSnapshot(s);CcSettlement *p=CcSimSettlementMutable(s,a.scriptorium_id);
            if(p){p->stock[CC_GOOD_WHEAT]=10000;p->stock[CC_GOOD_PAPER]=20;p->stock[CC_GOOD_TOOLS]=20;}
        }
        CcSimAdvanceDays(s,1);observed_days++;
        bool war=false;for(int i=0;i<s->kingdom_count;++i)for(int j=i+1;j<s->kingdom_count;++j)if(s->diplomacy[i][j]==CC_DIPLOMACY_WAR)war=true;
        war_days+=war;staffed_days+=s->archives.scribes>0;
        bool heard=false;for(int i=0;i<CC_MAX_GOSSIP;++i)if(s->gossip[i].heard_day>=initial->current_day)heard=true;heard_days+=heard;
        CcId newest=last_event;
        for(int i=0;i<s->event_count;++i){const CcEvent *ev=CcSimRecentEvent(s,i);if(!ev||ev->id<=last_event)break;if(ev->id>newest)newest=ev->id;int k=(int)ev->kind;if(k<0||k>=256)exit(2);counts[k]++;positive[k]+=ev->magnitude>0;magnitude[k]+=ev->magnitude;
            if(counts[k]<=2 && (k==CC_EVENT_GOBLIN_RAIDED || k==CC_EVENT_COURIER_LOST || k==CC_EVENT_COURIER_ARRIVED || k==CC_EVENT_PEACE_DECLARED || k==CC_EVENT_LORE_RECORDED)){printf("{\"kind\":\"example\",\"label\":\"%s\",\"event\":",label);quote(CcEventKindName(ev->kind));printf(",\"text\":");quote(ev->text);puts("}");}}
        last_event=newest;
        if(d%365==364){char error[192];if(!CcSimValidate(s,error,sizeof(error))){printf("{\"kind\":\"validation_failure\",\"label\":\"%s\",\"day\":%d,\"error\":",label,s->current_day);quote(error);puts("}");valid=false;break;}}
    }
    for(int k=0;k<256;++k)if(counts[k]){printf("{\"kind\":\"event_count\",\"label\":\"%s\",\"event\":",label);quote(CcEventKindName((CcEventKind)k));printf(",\"count\":%d,\"positive\":%d,\"magnitude_sum\":%lld}\n",counts[k],positive[k],magnitude[k]);}
    printf("{\"kind\":\"window\",\"label\":\"%s\",\"days\":%d,\"valid\":%s,\"war_days\":%d,\"days_with_new_heard_gossip\":%d,\"staffed_days\":%d}\n",label,observed_days,valid?"true":"false",war_days,heard_days,staffed_days);snapshot(s,label);free(s);
}
int main(void){
    CcSim *s=calloc(1,sizeof(*s));if(!s)return 2;char error[192];
    for(int k=0;k<4;++k){CcSimInit(s,42U);if(k==0)s->royal_carriages[0].trips_completed=1000001;if(k==1)s->bandits[0].raids_completed=1000001;if(k==2)s->hoard_raiders.raids_completed=1000001;if(k==3)s->goblins.tributes_delivered=1000001;bool valid=CcSimValidate(s,error,sizeof(error));printf("{\"kind\":\"counter_probe\",\"field\":%d,\"value\":1000001,\"valid\":%s,\"error\":",k,valid?"true":"false");quote(error);puts("}");}
    CcSimInit(s,UINT32_C(2)*UINT32_C(0x9e3779b9));
    for(int y=1;y<=16000;++y){for(int d=0;d<365;++d)CcSimAdvanceDays(s,1);if(!CcSimValidate(s,error,sizeof(error))){fprintf(stderr,"year %d %s\n",y,error);return 1;}}
    snapshot(s,"start");fflush(stdout);
    window(s,"control",0);window(s,"roads_maintained",1);window(s,"roads_and_archive_supported",2);free(s);return 0;
}
