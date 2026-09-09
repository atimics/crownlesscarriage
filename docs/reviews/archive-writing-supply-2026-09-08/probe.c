#include "sim/cc_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

typedef struct Load { CcId id, seat; int good, units; } Load;
static Load loads[CC_MAX_SHIPMENTS];
static int64_t ordered[CC_GOOD_COUNT], delivered[CC_GOOD_COUNT], redirected[CC_GOOD_COUNT], lost[CC_GOOD_COUNT];
static int64_t spent, purchases, unresolved;
static CcSim sim;
void StudyStaff(const CcSim *s)
{
    fprintf(stderr,"staff,%d,0,%" PRId64 ",%d,0,0\n",s->current_day,s->iron_ledger_reserve,s->archives.scribes);
}
void StudyPurchase(const CcSim *s,const CcShipment *shipment,CcMoney charge)
{
    int slot=-1;
    for(int i=0;i<CC_MAX_SHIPMENTS;i++)if(loads[i].id==0){slot=i;break;}
    if(slot<0)exit(3);
    loads[slot]=(Load){shipment->id,shipment->final_destination_id,shipment->good,shipment->quantity};
    ordered[shipment->good]+=shipment->quantity; spent+=charge; purchases++;
    fprintf(stderr,"purchase,%d,%" PRIu64 ",%" PRId64 ",%d,%d,%" PRId64 "\n",s->current_day,shipment->id,s->iron_ledger_reserve+charge,s->archives.scribes,shipment->quantity,charge);
}
void StudyEvent(const CcSim *s,CcEventKind kind,CcId id,CcId location,int32_t quantity)
{
    if(kind!=CC_EVENT_SHIPMENT_ARRIVED && kind!=CC_EVENT_SHIPMENT_LOST)return;
    for(int i=0;i<CC_MAX_SHIPMENTS;i++)if(loads[i].id==id){
        Load *load=&loads[i];
        if(quantity!=load->units)exit(4);
        if(kind==CC_EVENT_SHIPMENT_LOST)lost[load->good]+=quantity;
        else if(location==load->seat)delivered[load->good]+=quantity;
        else redirected[load->good]+=quantity;
        fprintf(stderr,"%s,%d,%" PRIu64 ",%" PRId64 ",%d,%d,0\n",kind==CC_EVENT_SHIPMENT_LOST?"lost":location==load->seat?"delivered":"redirected",s->current_day,id,s->iron_ledger_reserve,s->archives.scribes,quantity);
        *load=(Load){0};return;
    }
}
static void Array(const char *name,const int64_t *values)
{
    printf(",\"%s\":[",name);
    for(int i=0;i<CC_GOOD_COUNT;i++)printf("%s%" PRId64,i?",":"",values[i]);
    printf("]");
}
int main(int argc,char **argv)
{
    if(argc!=3)return 2;
    unsigned ordinal=(unsigned)strtoul(argv[1],NULL,0);int years=atoi(argv[2]);
    CcSimInit(&sim,ordinal*UINT32_C(0x9e3779b9));
    int zero=0,low=0,ready=0,weeks=0,failed=0;int64_t staff=0,lore=0;char error[256]="";
    printf("{\"ordinal\":%u,\"schema\":%u,\"annual_hashes\":[",ordinal,sim.schema_version);
    for(int day=1;day<=years*365;day++){
        CcSimAdvanceDays(&sim,1);
        if(sim.current_day%7==0){weeks++;zero+=sim.archives.scribes==0;low+=sim.iron_ledger_reserve<50;ready+=CcSimArchiveWorkPlan(&sim).recording_ready;staff+=sim.archives.scribes;lore+=sim.archives.lore_stored;}
        if(day%365==0){printf("%s\"%016" PRIx64 "\"",day==365?"":",",CcSimHash(&sim));if(!CcSimValidate(&sim,error,sizeof(error))){failed=day/365;break;}}
    }
    for(int i=0;i<CC_MAX_SHIPMENTS;i++)unresolved+=loads[i].units;
    printf("],\"failed_year\":%d,\"error\":\"%s\",\"weeks\":%d,\"zero_staff\":%d,\"low_reserve\":%d,\"ready\":%d,\"staff_sum\":%" PRId64 ",\"lore_sum\":%" PRId64 ",\"end_staff\":%d,\"end_lore\":%d,\"purchases\":%" PRId64 ",\"spent\":%" PRId64 ",\"unresolved_units\":%" PRId64,failed,error,weeks,zero,low,ready,staff,lore,sim.archives.scribes,sim.archives.lore_stored,purchases,spent,unresolved);
    Array("ordered",ordered);Array("delivered",delivered);Array("redirected",redirected);Array("lost",lost);puts("}");
    return failed?1:0;
}
