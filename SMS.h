#ifndef __SMS_H
#define __SMS_H
#include "MSHR.h"
struct SMS
{
    unsigned theRegionShift,theRegionSize,theRegionMask,theBlocksPerRegion;

    typedef unsigned long long pattern_t;

    struct AGTent {
        CacheAddr_t pc;
        int offset;
        pattern_t pattern;
        AGTent(CacheAddr_t aPC = 0ULL,int aOffset = 0,pattern_t aPattern = 0ULL)
        :   pc(aPC)
        ,   offset(aOffset)
        ,   pattern(aPattern)
        { }
        CacheAddr_t EVICT_DETECTOR;
    };
    struct PHTent {
        pattern_t pattern;
        PHTent(pattern_t aPattern = 0ULL):pattern(aPattern) { }
        CacheAddr_t EVICT_DETECTOR;
    };
    typedef Container<CacheAddr_t,AGTent> AGT; AGT theAGT;
    typedef Container<CacheAddr_t,PHTent> PHT; PHT thePHT;

    SMS()
        : theRegionShift(13) // shift: 12 = 4KB region, 10 = 1KB region
        , theRegionSize(1<<theRegionShift)
        , theRegionMask(theRegionSize-1)
        , theBlocksPerRegion(theRegionSize/theBlockSize)
        , theAGT(8,16,theRegionShift,27-theRegionShift)
        , thePHT(8,16,NoRotation?0:2,14)
    {
    }

    pattern_t rotate(int aBitIndex,int anOffset) {
        debug(stderr,"rotate(%d,%d [%d]) : ",aBitIndex,anOffset,theBlocksPerRegion);
        pattern_t res = 1ULL<<((aBitIndex + anOffset) % theBlocksPerRegion);
        debug(stderr,"%llx > %llx\n",1ULL<<aBitIndex,res);
        assert(!(res & (res-1)));
        return res;
    }

    bool replace( SIGNED_COUNTER cycle, CacheAddr_t addr ) {
        CacheAddr_t region_tag = addr & ~theRegionMask;
        int region_offset = (addr & theRegionMask)>>6;
        AGT::Item agt_evicted;
        //DBG_(Dev, ( << std::dec << theId << "-evict: group=" << std::hex << region_tag << "  offset=" << std::dec << region_offset ) );
        bool erased_something(false);

        AGT::Iter agt_ent = theAGT.find(region_tag);
        if (agt_ent != theAGT.end()) {
            pattern_t new_bit = rotate(region_offset,agt_ent->second.offset);
            if(NoRotation) new_bit = 1ULL << region_offset;
            if (agt_ent->second.pattern & new_bit) {
                agt_evicted = *agt_ent;
                //DBG_(Dev, ( << std::dec << theId << "-end: group=" << std::hex << region_tag << "  key=" << agt_evicted.second.pc << "  " << agt_evicted.second.pattern ) );
                theAGT.erase(region_tag);
                erased_something = true;
            }
        }

        if (agt_evicted.second.pattern) {
            if (thePHT.erase(agt_evicted.second.pc)) C(PHT_erased_previous); // if replacing or if it's a singleton
            if ((agt_evicted.second.pattern-1)&agt_evicted.second.pattern) {// not singleton
                debug(stderr,"learned pattern (block eviction) %llx into PHT, pc=%llx\n",agt_evicted.second.pattern,agt_evicted.second.pc);
                thePHT.insert(agt_evicted.second.pc,PHTent(agt_evicted.second.pattern));
            }
        }

        return erased_something;
    }

    void checkEvictions( SIGNED_COUNTER cycle, MSHRs* mshrs ) {
        for(int n=0;n<theAGT.theHeight;++n) {
invalidated_list:
            AGT::ListType& aList(theAGT.theItems[n]);
            int x=0;
            for(AGT::Iter i=aList.begin();i!=aList.end();i++) {
     //fprintf(stderr,"bla %d n=%d x=%d\n",__LINE__,n,x);
     //fprintf(stderr,"bla %llx\n",i->second.pattern);
                int offset = theBlocksPerRegion-1;
                for(pattern_t pattern = i->second.pattern;pattern;--offset) {
                    pattern_t mask = (1ULL<<offset);
                    if (!(pattern & mask)) continue;
                    EVICTdebug(stderr,"  attempt offset=%d mask=%llx on region_offset=%d with pattern %llx\n",offset,mask,i->second.offset,pattern);
                    pattern &= ~mask;
                    CacheAddr_t prediction = ((-i->second.offset+offset)*theBlockSize);
                    if(NoRotation) prediction = (offset*theBlockSize);
                    EVICTdebug(stderr,"  prediction = %llx\n",prediction);
                    prediction &= theRegionMask;
                    CacheAddr_t anAddress = i->second.EVICT_DETECTOR+prediction;
                    EVICTdebug(stderr,"  prediction&= %llx\n",prediction);
                    EVICTdebug(stderr,"  prediction!= %llx\n",i->first + prediction);
                    if (GetPrefetchBit(0,anAddress)==-1) {
                        if (mshrs->inflight(anAddress)) {
                            MSHRfprintf(stderr,"block still in flight (n=%d, x=%d), region=%llx offset=%d addr=%llx\n",n,x,i->first,i->second.offset,anAddress);
                        } else {
                            MSHRfprintf(stderr,"detected evicted block (n=%d, x=%d), region=%llx offset=%d, evicted addr=%llx\n",n,x,i->first,i->second.offset,anAddress);
                            if (replace(cycle, anAddress)) goto invalidated_list;
                        }
                    }
                }
                ++x;
            }
        }
    }

    void IssuePrefetches( SIGNED_COUNTER cycle, PrefetchData_t *Data, MSHRs* mshrs ) {
        CacheAddr_t pc(Data->LastRequestAddr);
        assert(pc);
        CacheAddr_t region_tag = Data->DataAddr & ~theRegionMask;
        int region_offset = (Data->DataAddr & theRegionMask)>>6;
        CacheAddr_t key = pc;
        if(NoRotation) key = (pc << (theRegionShift-6)) | region_offset;
        bool miss = (! Data->hit);

        //DBG_(Dev, ( << std::dec << theId << "-access: group=" << std::hex << region_tag << "  key=" << key << "  offset=" << std::dec << region_offset ) );
        debug(stderr,"region=%llx offset=%d bit=%llx pc=%llx\n",region_tag,region_offset,1ULL<<region_offset,pc);
        AGT::Iter agt_ent = theAGT.find(region_tag);
        AGT::Item agt_evicted;
        bool new_gen = false;
        if (agt_ent == theAGT.end()) {
            C(AGT_evict_replacement)
            pattern_t new_bit = 1ULL<<(theBlocksPerRegion-1);
            if(!NoRotation) {
                agt_evicted = theAGT.insert(region_tag,AGTent(key,theBlocksPerRegion-region_offset-1,new_bit));
            } else {
                new_bit = 1ULL << region_offset;
                agt_evicted = theAGT.insert(region_tag,AGTent(key,0,new_bit));
            }
            new_gen = true;
            debug(stderr,"new pattern (from scratch) new_bit=%llx offset=%d->%d\n",new_bit,region_offset,theBlocksPerRegion-region_offset);
        } else {
            pattern_t new_bit = rotate(region_offset,agt_ent->second.offset);
            if(NoRotation) new_bit = 1ULL << region_offset;
            if ((agt_ent->second.pattern & new_bit) && miss) {
                C(AGT_samebit_replacement)
                // FIXME: is same bit repeating the common case or not?
                debug(stderr,"collided on pattern %llx (new_bit=%llx)\n",agt_ent->second.pattern,new_bit);
                /* same-bit ends gen logic
                agt_evicted = *agt_ent;
                if(!NoRotation) {
                    agt_ent->second = AGTent(key,theBlocksPerRegion-region_offset-1);
                } else {
                    agt_ent->second = AGTent(key,0);
                }
                new_gen = true;
                */
            } else {
                C(AGT_addbit)
            }
            agt_ent->second.pattern |= new_bit;
            debug(stderr,"update pattern:%llx new_bit=%llx offset=%d\n",agt_ent->second.pattern,new_bit,agt_ent->second.offset);
        }

        if (agt_evicted.second.pattern) {
            if (thePHT.erase(agt_evicted.second.pc)) C(PHT_erased_previous); // if replacing or if it's a singleton
            if ((agt_evicted.second.pattern-1)&agt_evicted.second.pattern) {
                // not singleton
                debug(stderr,"learned pattern (AGT eviction) %llx into PHT, pc=%llx\n",agt_evicted.second.pattern,agt_evicted.second.pc);
                thePHT.insert(agt_evicted.second.pc,PHTent(agt_evicted.second.pattern));
            }
        }

        if (new_gen) {
            PHT::Iter pht_ent = thePHT.find(key);
            if (pht_ent != thePHT.end()) {
                //DBG_(Dev, ( << std::dec << theId << "-predict: group=" << std::hex << region_tag << "  key=" << key << "  " << pht_ent->second.pattern ) );
                C(L1_Found_Pattern)
                debug(stderr,"prediction pattern for pc=%llx is %llx and region_offset %d\n",key,pht_ent->second.pattern,region_offset);
                int offset = theBlocksPerRegion-2; // extra -1 to avoid prefetch of trigger
                if(NoRotation) offset += 1;
                //assert((pht_ent->second.pattern-1)&pht_ent->second.pattern);
                for(pattern_t pattern = pht_ent->second.pattern;pattern;--offset) {
                    //debug(stderr,"  prediction at offset %d, pattern is %llx and region_offset %d\n",offset,pht_ent->second.pattern,region_offset);
                    pattern_t mask = (1ULL<<offset);
                    debug(stderr,"  attempt offset=%d mask=%llx on region_offset=%d with pattern %llx\n",offset,mask,region_offset,pattern);
                    if (!(pattern & mask)) continue;
                    pattern &= ~mask;
                    if (NoRotation && (offset == region_offset)) continue;
                    debug(stderr,"  prediction at offset %d, pattern is %llx and region_offset %d\n",offset,pht_ent->second.pattern,region_offset);
                    CacheAddr_t prediction = ((region_offset+offset+1)*theBlockSize);
                    if(NoRotation) prediction = (offset*theBlockSize);
                    debug(stderr,"  prediction = %llx\n",prediction);
                    prediction &= theRegionMask;
                    debug(stderr,"  prediction&= %llx\n",prediction);
                    debug(stderr,"  prediction!= %llx\n",region_tag + prediction);
                    IssueL2Prefetch(cycle,region_tag + prediction);
                    //mshrs->allocate(region_tag + prediction, -cycle); only for stats, don't waste MSHRs
                    C(L1_Prefetches_Issued)
                }
            }
        }

    }

};
#endif
