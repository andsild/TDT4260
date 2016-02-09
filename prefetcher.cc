#include <string>
#include <stdio.h>
#include <iostream>
#include <map>
#include "interface.hh"
#include "definitions.cc"

#define SIGNED_COUNTER long long
#define C(n) do { ++(theStats.theCounters[(#n)]); } while(0);
#define S(n,v) do { (theStats.theCounters[(#n)])+=(v); } while(0);


const int theBlockSize(64);

const bool NoRotation = false;

struct Stats
{
    std::map<std::string, long long> theCounters;
    ~Stats() {
        for(std::map<std::string, long long>::iterator
                i=theCounters.begin();
                i!=theCounters.end();
                ++i) {
            std::cerr << i->first << "," << i->second << std::endl;
        }
    }
} theStats;

#include "SMS.h"
#include "MSHR.h"

void prefetch_init(void){
   DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
    sms = new SMS();
    mshrs = new MSHRs(16);
}


void prefetch_access(AccessStat stat){
/* pf_addr is now an address within the _next_ cache block */
   Addr pf_addr = stat.mem_addr + BLOCK_SIZE;
/*
* Issue a prefetch request if a demand miss occured,
* and the block is not already in cache.
*/
   if (stat.miss && !in_cache(pf_addr)) {
      issue_prefetch(pf_addr);

   }
}

void prefetch_complete(Addr addr) {
/*
* Called when a block requested by the prefetcher has been loaded.
*/
}
