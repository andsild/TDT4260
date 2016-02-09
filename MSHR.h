#ifndef __MSHR_H
#define __MSHR_H
#include <map>
// Miss Status Handling Register
struct MSHRs {
    unsigned int theSize;
    typedef std::map<Addr,SIGNED_COUNTER> tEntries;
    tEntries theEntries;

    MSHRs(unsigned int aSize)
    :   theSize(aSize)
    { }

    void clean(SIGNED_COUNTER cycle) {
        if (cycle<0) cycle = -cycle;
        tEntries::iterator next;
        for(tEntries::iterator i=theEntries.begin();i!=theEntries.end();i = next) {
            next = i;
            ++next;
            if (GetPrefetchBit(0,i->first)>-1) {
                // std::cerr << "removing MSHR entry" i->first << " " << theEntries.size() << " left (i->second=%lld, cycle=%lld)\n",i->first,theEntries.size()-1,i->second,cycle);
                if (i->second > 0) {
                    S(mem_waited,cycle - i->second)
                    assert(cycle >= i->second);
                }
                theEntries.erase(i);
            } else {
                // MSHRfprintf(stderr,"keeping MSHR entry %llx, %d entries\n",i->first,theEntries.size());
            }
        }
    }

    bool inflight(Addr anAddr) {
        anAddr &= ~63ULL;
        bool res = (theEntries.find(anAddr) != theEntries.end());
        // MSHRfprintf(stderr,"MSHR in-flight check for %llx: %d\n",anAddr,res);
        return res;
    }

    bool allocate(Addr anAddr, SIGNED_COUNTER cycle) {
        clean(cycle);
        anAddr &= ~63ULL;
        if (cycle<0 && GetPrefetchBit(0,anAddr)>-1) C(L1_Prefetch_Not_Needed)
        tEntries::iterator i = theEntries.find(anAddr);
        if (i != theEntries.end()) {
            if (cycle<0) {
                C(L1_Prefetch_Already_InFlight)
            } else if (i->second<0) {
                C(L1_Prefetch_PartialHit);
                S(mem_saved,cycle + i->second)
                i->second = cycle;
            } else {
                C(L1_OoO_Miss_Overlap)
            }
            std::cerr << "MSHR for " << anAddr << "exists" << std::endl; 
            return false;
        }
        if (theEntries.size() == theSize) {
            // MSHRfprintf(stderr,"MSHRs are full, can't insert %llx (size=%d)\n",anAddr,theSize);
            C(MSHR_full)
            return true;
        }
        theEntries.insert(std::make_pair(anAddr,cycle));
        // MSHRfprintf(stderr,"added MSHR entry %llx, %d present\n",anAddr,theEntries.size());
        return true;
    }

};

SMS *sms;
MSHRs *mshrs;

void InitPrefetchers() // DO NOT CHANGE THE PROTOTYPE
{
}

void IssuePrefetches( COUNTER cycle, PrefetchData_t *L1Data, PrefetchData_t * L2Data )
{
    if (!cycle) return;
    sms->checkEvictions( cycle, mshrs );
    if (cycle%100000 == 0) {
        fprintf(stdout,"cycle %lld (%.1f%% at IPC=1.0)\n",cycle,cycle/1000000.);
        fflush(stdout);
    }
    for(int i = 0; i < 4; i++) {
        if(cycle == L1Data[i].LastRequestCycle) {
            int pbit = GetPrefetchBit(0,L1Data[i].DataAddr);
            if (!L1Data[i].hit || (pbit==1)) {
                bool mshr_allocated = false;
                if (!L1Data[i].hit) {
                    mshr_allocated = mshrs->allocate(L1Data[i].DataAddr,cycle);
                    if (mshr_allocated)
                        C(L1_Total_Misses)
                    else {
                        C(L1_Total_MSHR_Hits)
                        assert(!L1Data[i].hit);
                        L1Data[i].hit = 1;
                    }
                    debug(stderr,"%lld: %llx missed on %llx (pbit=%d)\n",cycle,L1Data[i].LastRequestAddr,L1Data[i].DataAddr,pbit);
                } else {
                    C(L1_Total_Prefetch_Hits)
                    assert(pbit==1);
                    debug(stderr,"%lld: %llx prefetch hit on %llx (pbit=%d)\n",cycle,L1Data[i].LastRequestAddr,L1Data[i].DataAddr,pbit);
                }
            } else {
                C(L1_Total_Hits)
                debug(stderr,"%lld: %llx hit on %llx (pbit=%d)\n",cycle,L1Data[i].LastRequestAddr,L1Data[i].DataAddr,pbit);
            }
            sms->IssuePrefetches(cycle, &L1Data[i], mshrs);
            UnSetPrefetchBit(0,L1Data[i].DataAddr);
        }
    }

    for(int i = 0; i < 4; i++) {
        if(cycle == L2Data[i].LastRequestCycle) {
            if (L2Data[i].hit) {
                C(L2_Total_Hits)
                if (GetPrefetchBit(1,L2Data[i].DataAddr)==1) C(L2_Total_Prefetch_Hits)
            } else {
                C(L2_Total_Misses)
            }
            if (!L2Data[i].hit || (GetPrefetchBit(1,L2Data[i].DataAddr)==1)) {
                UnSetPrefetchBit(1,L2Data[i].DataAddr);
            }
        }
    }

}

#endif
