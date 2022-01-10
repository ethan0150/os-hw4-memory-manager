#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<assert.h>
//#include<stdbool.h>

#define MAXM 1024
#define MAXN 2048
#define MAXP 20
#define MAXTLB 32
#define READL getline(&buf, &len, tmpfp);
#define FOR(n) for(int i = 0;i<n;i++)
#define FORj(n) for(int j = 0;j<n;j++)

typedef struct PTE
{
    int pfn; // -1: page hasn't been allocated. Stores PFN when pre == 1, otherwise stores DBI
    int ref; // toggled to 1 once referenced
    int pre; // 1: in memory 0: on disk
} PTE;

typedef struct replEntry
{
    int pid;
    int vpn;
} RE;

FILE *tmpfp = NULL, *fp = NULL;

PTE pt[MAXP][MAXN]; // pt[VPN]
int tlb[MAXTLB] = {0}, tlbCnt = 0; // TLB[i] cantains VPN

char freeFrame[MAXN] = {0}; // 1: in use 0: not in use
char freeDisk[MAXP * MAXM] = {0}; // 1: in use 0: not in use

int localRepl[MAXP][MAXN]; // localRepl[PID][i] contains VPN
int localCnt[MAXP] = {0};

RE globalRepl[MAXP * MAXN];
int globalCnt = 0;

char tlbPlc = 0, //0: RANDOM 1: LRU
     replPlc = 0, //0: CLOCK 1: FIFO
     allocPlc = 0; //0: GLOBAL 1: LOCAL
int globalclk = 0;
int localclk[MAXP] = {0};
int max_pf, max_vp;

char *buf, *token, str[1000];
size_t len = 1000;

int refCnt[MAXP] = {0}, tlbHitCnt[MAXP] = {0}, pfCnt[MAXP] = {0}, pRefCnt[MAXP] = {0}, tlbRefCnt[MAXP];
int vpn;
int pid = 'z' - 'A', last_pid;
char pchar;
void ins_intarr(int idx, int n, int *arr, int *cur_len)
{
    for(int i=*cur_len; i>idx; i--)
    {
        arr[i] = arr[i-1];
    }
    arr[idx] = n;
    (*cur_len)++;
    return;
}

void del_intarr(int idx, int *arr, int *cur_len)
{
    for(int i=idx; i<*cur_len-1; i++)
        arr[i] = arr[i+1];
    (*cur_len)--;
    return;
}

void ins_REarr(int idx, RE n, RE *arr, int *cur_len)
{
    for(int i=*cur_len; i>idx; i--)
    {
        arr[i] = arr[i-1];
    }
    arr[idx] = n;
    (*cur_len)++;
    return;
}

void del_REarr(int idx, RE *arr, int *cur_len)
{
    for(int i=idx; i<*cur_len-1; i++)
        arr[i] = arr[i+1];
    (*cur_len)--;
    return;
}

int getFreeFrame()
{
    FOR(max_pf)
    {
        if(!freeFrame[i])
        {
            return i;
        }
    }
    return -1;
}

int getFreeDisk()
{
    FOR(MAXP * MAXM)
    {
        if(!freeDisk[i])
            return i;
    }
    return -1;
}

char pidToChar(int pid)
{
    return (char)pid + 'A';
}
/*
void print_pt(){
    FOR(max_vp){

    }
}*/

void print_tlb()
{
    printf("[Debug] ");
    FOR(tlbCnt)
    {
        printf("%d ", tlb[i]);
    }
    printf("\n");
}

int search_tlb(int page)
{
    FOR(tlbCnt)
    {
        if(tlb[i] == page)
            return i;
    }
    return -1;
}

void readcfg()
{
    char delim[] = ":";
    tmpfp = fopen("./TA_testdata/sys_config.txt", "r");

    READL
    token = strtok(buf, delim);
    token = strtok(NULL, delim);
    sscanf(token, "%s", str);
    if(!strcmp(str, "LRU"))
        tlbPlc = 1;

    READL
    token = strtok(buf, delim);
    token = strtok(NULL, delim);
    sscanf(token, "%s", str);
    if(!strcmp(str, "FIFO"))
        replPlc = 1;

    READL
    token = strtok(buf, delim);
    token = strtok(NULL, delim);
    sscanf(token, "%s", str);
    if(!strcmp(str, "LOCAL"))
        allocPlc = 1;

    READL
    READL

    READL
    token = strtok(buf, delim);
    token = strtok(NULL, delim);
    sscanf(token, "%d", &max_pf);

    fclose(tmpfp);
    return;
}

int update_tlb() // hit: return 1, miss: return 0
{
    int ret, tlbidx = -1;
    tlbRefCnt[pid]++;
    if(last_pid != pid)
    {
        FOR(MAXTLB)
        {
            tlb[i] = -1;
        }
        tlbCnt = 0;
    }

    FOR(tlbCnt)
    {
        if(tlb[i] == vpn)
        {
            tlbidx = i;
            break;
        }
    }
    ret = (tlbidx != -1);
    if(ret)  // hit
    {
        tlbHitCnt[pid]++;
        pt[pid][vpn].ref = 1;
        if(tlbPlc)  // LRU
        {
            del_intarr(tlbidx, tlb, &tlbCnt);
            ins_intarr(tlbCnt, vpn, tlb, &tlbCnt);
        }
    }
    else  // miss
    {
        tlbRefCnt[pid]++;
        if(tlbPlc)
        {
            if(tlbCnt == MAXTLB)
            {
                del_intarr(0, tlb, &tlbCnt);
            }
        }
        else
        {
            if(tlbCnt == MAXTLB)
            {
                del_intarr((rand() % MAXTLB), tlb, &tlbCnt);
            }
        }
        //tlb[tlbCnt-1] = vpn;
        //tlbCnt++;
        ins_intarr(tlbCnt, vpn, tlb, &tlbCnt);
    }

    return ret;
}
/*
void page_out(){// put a page to disk, freeing a frame
    RE out, in;
    int freeidx;
    //remove RE from replList and
    if(allocPlc){// local
        if(replPlc){// FIFO
            out.vpn = localRepl[pid][0];
            freeidx = pt[pid][out.vpn].pfn;
            del_intarr(0, localRepl[pid], &localCnt[pid]);
        }
        else{// CLOCK
            int lastidx;
            if(pt[pid][localRepl[pid][globalclk]].ref){
                while(pt[pid][localRepl[pid][globalclk]].ref){
                    pt[pid][localRepl[pid][globalclk]].ref = 0;
                    lastidx = globalclk;
                    globalclk++;
                    globalclk %= localCnt[pid];
                }
                out.vpn = localRepl[pid][lastidx];
                freeidx = pt[pid][out.vpn].pfn;
                localRepl[pid][lastidx] = vpn;
            }
        }
    }

}
*/
void pf_handler()
{
    pfCnt[pid]++;
    RE out = (RE)
    {
        .pid = pid, .vpn = -1
    };
    RE in = out;
    int src = -1, dst = -1;
    int freeframeidx = getFreeFrame();
    int insidx, tlbidx;
    //printf("%d\n", freeframeidx);
    if(freeframeidx == -1)
    {
        //put page to disk and free frame
        if(allocPlc) // local
        {
            if(replPlc) // FIFO
            {
                out.vpn = localRepl[pid][0];
                del_intarr(0, localRepl[pid], &localCnt[pid]);
                insidx = localCnt[pid];
            }
            else // CLOCK
            {
                insidx = localclk[pid];
                int flag = 0;

                while(pt[pid][localRepl[pid][localclk[pid]]].ref)
                {
                    pt[pid][localRepl[pid][localclk[pid]]].ref = 0;
                    //insidx = localclk[pid];
                    localclk[pid]++;
                    localclk[pid] %= localCnt[pid];
                    flag = 1;
                }

                if(flag)
                {
                    insidx = localclk[pid];
                    localclk[pid]++;
                    localclk[pid] %= localCnt[pid];
                }

                out.vpn = localRepl[pid][insidx];
                del_intarr(insidx, localRepl[pid], &localCnt[pid]);

                if(!flag)
                {
                    localclk[pid]++;
                    localclk[pid] %= localCnt[pid] + 1;
                }
            }
        }
        else  // global
        {
            if(replPlc)  // FIFO
            {
                out = globalRepl[0];
                del_REarr(0, globalRepl, &globalCnt);
                insidx = globalCnt;
            }
            else // CLOCK
            {
                RE cur_RE = globalRepl[globalclk];
                insidx = globalclk;
                int flag = 0;

                while(pt[cur_RE.pid][cur_RE.vpn].ref)
                {
                    pt[cur_RE.pid][cur_RE.vpn].ref = 0;
                    //insidx = globalclk;
                    globalclk++;
                    globalclk %= globalCnt;
                    flag = 1;
                    cur_RE = globalRepl[globalclk];
                }

                if(flag)
                {
                    insidx = globalclk;
                    globalclk++;
                    globalclk %= globalCnt;
                }

                out = globalRepl[insidx];
                del_REarr(insidx, globalRepl, &globalCnt);

                if(!flag)
                {
                    globalclk++;
                    globalclk %= globalCnt + 1;
                }
            }
        }
        tlbidx = search_tlb(out.vpn);
        if(tlbidx != -1 && allocPlc)
        {
            del_intarr(tlbidx, tlb, &tlbCnt);
        }
        freeframeidx = pt[out.pid][out.vpn].pfn;
        pt[out.pid][out.vpn] = (PTE)
        {
            .pfn = getFreeDisk()
        };
        dst = getFreeDisk();
        freeDisk[getFreeDisk()] = 1;
        freeFrame[freeframeidx] = 0;
    }
    else
    {
        if(allocPlc)  // local
        {
            insidx = localCnt[pid];
        }
        else insidx = globalCnt;
    }

    assert(freeframeidx != -1);

    if(pt[pid][vpn].pre == 0)  //page in disk
    {
        src = pt[pid][vpn].pfn;
        freeDisk[pt[pid][vpn].pfn] = 0;
    }

    pt[pid][vpn] = (PTE)
    {
        .pfn = freeframeidx, .pre = 1, .ref = 1
    };
    freeFrame[freeframeidx] = 1;
    if(allocPlc)  // local
    {
        ins_intarr(insidx, vpn, localRepl[pid], &localCnt[pid]);
    }
    else
    {
        ins_REarr(insidx, (RE)
        {
            .pid = pid, .vpn = vpn
        }, globalRepl, &globalCnt);
    }

    printf("Page Fault, %d, Evict %d of Process %c to %d, %d<<%d\n",
           freeframeidx,
           out.vpn,
           pidToChar(out.pid),
           dst,
           vpn,
           src
          );
    /*if(freeframeidx == 63){
        printf("(%d %d %d)\n", globalclk, globalCnt, insidx);
    }*/
    //printf("out.pid: %d\n", out.pid);
}
void pf_handler_new()
{
    pfCnt[pid]++;
    RE out = (RE)
    {
        .pid = pid, .vpn = -1
    };
    RE in = out;
    int src = -1, dst = -1;
    int freeframeidx = getFreeFrame();
    if(freeframeidx == -1)  //page replacement
    {
        if(allocPlc) // local
        {
            if(replPlc)  // fifo
            {
                del_intarr(0, localRepl[pid], &(localCnt[pid]));
                ins_intarr(localCnt[pid], vpn, localRepl[pid], &localCnt[pid]);

            }
            else
            {

            }
        }
        else
        {

        }
    }
    else
    {

    }
    printf("Page Fault, %d, Evict %d of Process %c to %d, %d<<%d\n",
           freeframeidx,
           out.vpn,
           pidToChar(out.pid),
           dst,
           vpn,
           src
          );
}

void analyze()
{
    for(int i=0; i<MAXP && tlbRefCnt[i]; i++)
    {
        float hit_r = (float)tlbHitCnt[i] / (float)tlbRefCnt[i];
        float EAT = (hit_r * 120) + ((1 - hit_r) * (2 * 100) + 20);
        printf("Process %c, Effective Access Time = %0.3f\n", pidToChar(i),EAT);
        printf("Process %c, Page Fault Rate: %0.3f\n", pidToChar(i), (float)pfCnt[i] / (float)pRefCnt[i]);
    }

}

int main(void)
{
    srand(time(NULL));
    buf = malloc(len);
    readcfg();
    //tlbPlc = 1; allocPlc = 1; replPlc = 1; max_pf = 64;
    FOR(MAXP)  // initialize pt
    {
        FORj(MAXN)
        {
            pt[i][j] = (PTE)
            {
                .pfn = -1, .pre = -1
            };
        }
    }

    fp = fopen("trace.txt", "r");
    freopen("trace_output.txt", "w", stdout);
    char delim[] = "(), ";
    //printf("fuck\n");
    while(getline(&buf, &len, fp) != EOF)
    {
        //read a line of trace
        token = strtok(buf, delim);
        token = strtok(NULL, delim);
        last_pid = pid;
        sscanf(token, "%c", &pchar);
        pid = (int)(pchar - 'A');
        token = strtok(NULL, delim);
        sscanf(token, "%d", &vpn);

        refCnt[pid]++;
        if(update_tlb())  // TLB hit
        {
            //printf("Process %c, TLB Hit, %d=>%d\n", pchar, vpn, pt[pid][vpn].pfn);
        }
        else // TLB miss
        {
            pRefCnt[pid]++;
            printf("Process %c, TLB Miss, ", pchar);
            if(pt[pid][vpn].pre == 1)  //if page hit:
            {

                pt[pid][vpn].ref = 1;
                printf("Page Hit, %d=>%d\n", vpn, pt[pid][vpn].pfn);
            }
            else
            {
                pf_handler();
                if(vpn == 63)
                {
                    //print
                }
            }
        }
        printf("Process %c, TLB Hit, %d=>%d\n", pchar, vpn, pt[pid][vpn].pfn);
        if(vpn == 63)
        {

        }
    }
    freopen("analysis.txt", "w", stdout);
    analyze();

    fclose(fp);
    free(buf);
    return 0;
}