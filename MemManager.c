#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<assert.h>
#include<limits.h>
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

FILE *tmpfp = NULL, *fp = NULL, *outfp = NULL, *anafp = NULL;

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

int refnum = 0;

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


void print_tlb()
{
    printf("ref %d: ", refnum);
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
    if(last_pid != pid)
    {
        FOR(MAXTLB)
        {
            tlb[i] = INT_MIN;
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
        pt[pid][vpn].ref = 1;
        if(tlbPlc)  // LRU
        {
            del_intarr(tlbidx, tlb, &tlbCnt);
            ins_intarr(tlbCnt, vpn, tlb, &tlbCnt);
        }
    }

    return ret;
}

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
    if(freeframeidx == -1)
    {
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
        if(tlbCnt == MAXTLB)
        {
            if(tlbidx != -1 && out.pid == pid)
            {
                del_intarr(tlbidx, tlb, &tlbCnt);
            }
            else
            {
                if(tlbPlc)
                {
                    del_intarr(0, tlb, &tlbCnt);
                }
                else
                {
                    del_intarr(rand() % tlbCnt, tlb, &tlbCnt);
                }
            }
        }
        else
        {
            if(tlbidx != -1 && out.pid == pid)
            {
                del_intarr(tlbidx, tlb, &tlbCnt);
            }
        }
        ins_intarr(tlbCnt, vpn, tlb, &tlbCnt);

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

        if(tlbCnt == MAXTLB)
        {
            if(tlbPlc)
            {
                del_intarr(0, tlb, &tlbCnt);
            }
            else
            {
                del_intarr(rand() % tlbCnt, tlb, &tlbCnt);
            }
        }
        ins_intarr(tlbCnt, vpn, tlb, &tlbCnt);
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

    fprintf(outfp, "Page Fault, %d, Evict %d of Process %c to %d, %d<<%d\n",
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
        float alpha = (float)refCnt[i] / (float)tlbRefCnt[i];
        float EAT = 220 - 100 * alpha;
        fprintf(anafp, "Process %c, Effective Access Time = %0.3f\n", pidToChar(i),EAT);
        fprintf(anafp, "Process %c, Page Fault Rate: %0.3f\n", pidToChar(i), (float)pfCnt[i] / (float)refCnt[i]);
    }
}

int main(void)
{
    srand(time(NULL));
    buf = malloc(len);
    //readcfg();
    tlbPlc = 1;
    allocPlc = 0;
    replPlc = 1;
    max_pf = 64;
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
    outfp = fopen("trace_output.txt", "w");
    char delim[] = "(), ";
    int tlbidx = -1;
    while(getline(&buf, &len, fp) != EOF)
    {
        token = strtok(buf, delim);
        token = strtok(NULL, delim);
        last_pid = pid;
        sscanf(token, "%c", &pchar);
        pid = (int)(pchar - 'A');
        token = strtok(NULL, delim);
        sscanf(token, "%d", &vpn);
        refnum++;
        refCnt[pid]++;
        if(update_tlb())  // TLB hit
        {
            tlbRefCnt[pid]++;
        }
        else // TLB miss
        {
            tlbRefCnt[pid] += 2;
            fprintf(outfp, "Process %c, TLB Miss, ", pchar);
            if(pt[pid][vpn].pre == 1)  //if page hit:
            {
                if(tlbCnt == MAXTLB)
                {
                    if(tlbPlc)  // lru
                    {
                        del_intarr(0, tlb, &tlbCnt);
                    }
                    else del_intarr(rand() % tlbCnt, tlb, &tlbCnt);
                }
                ins_intarr(tlbCnt, vpn, tlb, &tlbCnt);
                pt[pid][vpn].ref = 1;
                fprintf(outfp, "Page Hit, %d=>%d\n", vpn, pt[pid][vpn].pfn);
            }
            else
            {
                pf_handler();
            }
        }
        print_tlb();
        fprintf(outfp, "Process %c, TLB Hit, %d=>%d\n", pchar, vpn, pt[pid][vpn].pfn);
    }
    anafp = fopen("analysis.txt", "w");
    analyze();
    fclose(fp);
    fclose(anafp);
    fclose(outfp);
    free(buf);
    return 0;
}