#include "persistence/cc_save.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static CcSim sim;
int main(int argc,char **argv) {
    char error[256];
    if(argc<3) return 2;
    for(int i=2;i<argc;++i) {
        FILE *f=fopen(argv[i],"rb");if(f==NULL) return 3;
        if(fseek(f,0,SEEK_END)!=0) return 3;
        long size=ftell(f);if(size<=0) return 3;
        rewind(f);unsigned char *input=malloc((size_t)size);if(input==NULL) return 3;
        if(fread(input,1,(size_t)size,f)!=(size_t)size) return 3;
        fclose(f);
        if(!CcSaveDecode(input,(size_t)size,&sim,error,sizeof(error))) {
            fprintf(stderr,"%s: %s\n",argv[i],error);return 4;
        }
        free(input);
        unsigned char *output=NULL;size_t length=0;
        if(!CcSaveEncode(&sim,&output,&length,error,sizeof(error))) return 5;
        const char *name=strrchr(argv[i],'/');name=name ? name+1 : argv[i];
        char path[1024];if(snprintf(path,sizeof(path),"%s/%s",argv[1],name)>=(int)sizeof(path)) return 6;
        f=fopen(path,"wb");if(f==NULL) return 6;
        if(fwrite(output,1,length,f)!=length) return 6;
        fclose(f);CcSaveFreeBuffer(output);
        printf("%s %u %u %zu %016" PRIx64,name,sim.schema_version,sim.generator_version,length,CcSimHash(&sim));
        CcSimAdvanceDays(&sim,7);
        printf(" %016" PRIx64 "\n",CcSimHash(&sim));
    }
    return 0;
}
