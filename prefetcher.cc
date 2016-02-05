#include "interface.hh"

void prefetch_init(void){
/* Called before any calls to prefetch_access. */
/* This is the place to initialize data structures. */
   DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");
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
